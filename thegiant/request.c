#include <Python.h>
#include <cStringIO.h>
#include "request.h"
#include "filewrapper.h"
#include "assert.h"

static inline void PyDict_ReplaceKey(PyObject* dict, PyObject* k1, PyObject* k2);
static PyObject* wsgi_base_dict = NULL;

/* Non-public type from cStringIO I abuse in on_body */
typedef struct {
  PyObject_HEAD
  char *buf;
  Py_ssize_t pos, string_size;
  PyObject *pbuf;
} Iobject;

Request* Request_new(int client_fd, const char* client_addr)
{  
  Request* request = malloc(sizeof(Request));
#ifdef DEBUG
  static unsigned long request_id = 0;
  request->id = request_id++;
#endif
  request->client_fd = client_fd;
  request->client_addr = PyString_FromString(client_addr);
  Request_reset(request);
  return request;
}

void Request_reset(Request* request)
{
  memset(&request->state, 0, sizeof(Request) - (size_t)&((Request*)NULL)->state);
  request->state.response_length_unknown = true;  
  request->parse_phase = RDS_PHASE_CONNECT;
  request->multibulklen = 0;  
  // we make room for at least buffer size...
  request->requestbuffer = malloc(100);
  request->requestbufferlen = 0;
}

void Request_free(Request* request)
{
    Request_clean(request);
    Py_DECREF(request->client_addr);
    free(request);
}

void Request_clean(Request* request)
{  
  free(request->requestbuffer);

  if(request->iterable) {
    /* Call 'iterable.close()' if available */
    PyObject* close_method = PyObject_GetAttr(request->iterable, _close);
    if(close_method == NULL) {
      if(PyErr_ExceptionMatches(PyExc_AttributeError))
        PyErr_Clear();
    } else {
      PyObject_CallObject(close_method, NULL);
      Py_DECREF(close_method);
    }
    if(PyErr_Occurred()) PyErr_Print();
    Py_DECREF(request->iterable);
  }
  Py_XDECREF(request->iterator);
  Py_XDECREF(request->headers);
  Py_XDECREF(request->status);
  Py_XDECREF(request->current_chunk);
}

static int parse_multi_line_message(Request* request, const char* data, const size_t data_len){  
    char *newline = NULL;
    int pos = 0, ok;
    long long ll;
    int partialdone = 0;

    if (request->parse_phase == RDS_PHASE_CONNECT){
        if (request->multibulklen == 0) {        
            newline = strchr(data,'\r');
            if (newline == NULL) {
                // if (sdslen(c->querybuf) > REDIS_INLINE_MAX_SIZE) {
                //     addReplyError(c,"Protocol error: too big mbulk count string");
                //     setProtocolError(c,0);
                // }

                // we will come back here,                
                return -1;
            }

            ok = string2ll(data+1, newline-(data+1), &ll);
            if (!ok || ll > 1024*1024) {
                puts("couldnt find data length... ");
                return -2;
            }
            pos = (newline - data)+2;

            if (ll <= 0) {
                // TODO: handle *-1\r\n ?
                // c->querybuf = sdsrange(c->querybuf,pos,-1);
                return true;
            }        

            request->cmd_list = PyList_New(ll);

            request->multibulklen = ll;     
            request->arg_cnt = 0;   
            request->parse_phase = RDS_PHASE_START;
            // now send the remainder to start line...
            request->lastpos = pos;                        
        }
    }    
    

    while (request->multibulklen){        
        // since we found the start line, here we parse it...
        if (request->parse_phase == RDS_PHASE_START){      
              if (data[request->lastpos] == '$'){
                      // 
                      newline = strchr(data+request->lastpos,'\r');
                      if (!newline){
                        return -1;
                      }

                      ok = string2ll(data+request->lastpos+1, newline - (data+ request->lastpos+1),&ll);
                      if (!ok || ll < 0 || ll > 512*1024*1024) {
                          return -2;
                      }

                        // now parse data line...            
                        pos = (newline - data)+2;

                        if (ll < 0) {
                            // handle $-1\r\n ?
                            // protocol error !!!
                            // c->querybuf = sdsrange(c->querybuf,pos,-1);                
                            return -2;
                        }

                        // now send the remainder to start line...
                        request->lastpos = pos;                
                        request->parse_phase = RDS_PHASE_DATA;
                        request->bulklen = ll;
              } else {
                puts("ERR: protocol error");
                return -2;
              }
          }
          // 
          if (request->parse_phase == RDS_PHASE_DATA){      

                if ((int)(data_len - request->lastpos) < 0){
                  return -1;
                }

                // do we have enough data ???
                if ( (int)(data_len - request->lastpos) < (int)(request->bulklen+2)) {
                    /* Not enough data (+2 == trailing \r\n) */                    
                    return -1;
                    break;
                } else {                                        
                    char *str2 = malloc(request->bulklen + 1);
                    memcpy(str2, data + request->lastpos, request->bulklen);                  
                    str2[request->bulklen] = '\0';

                    PyObject* str = PyString_FromStringAndSize(str2, request->bulklen);                  
                    PyList_SetItem(request->cmd_list, request->arg_cnt++, str);                   
                    // NOTE: as far as i understand, PyList_SetItem doesnt incref
                    // http://stackoverflow.com/questions/3512414/does-this-pylist-appendlist-py-buildvalue-leak
                    // Py_DECREF(str); <- TODO: why ? if i do this weird things happen
                    free(str2);                  
                }                                

              
              request->lastpos = request->lastpos + request->bulklen + 2;
              request->parse_phase = RDS_PHASE_START;              
              request->multibulklen--;                              
          
          } // if RDS_PHASE_DATA
    } // while bulklen

    if (request->multibulklen == 0){      
        return 1;
    }
    
    return -1;
}




static int parse_single_line_message(Request* request, const char* data, const size_t data_len){
    return -2;
}


void Request_parse(Request* request, const char* data, const size_t data_len)
{
  int parse_result;


  // unless we do have a \r in the buffer, dont try to parse it
  request->requestbuffer = realloc(request->requestbuffer, request->requestbufferlen+data_len);
  memcpy(request->requestbuffer + request->requestbufferlen, data, data_len);
  request->requestbufferlen += data_len;

  if (request->requestbuffer[0] == '*'){
      parse_result = parse_multi_line_message(request, request->requestbuffer, request->requestbufferlen);
  } else {
      parse_result = parse_single_line_message(request, request->requestbuffer, request->requestbufferlen);
  }
  
  // -2 is for an unrecoverable protocol error, we send a bad request and return...
  if (parse_result == -2){
      request->state.error_code = HTTP_BAD_REQUEST;
      return;
  }


  // if parse result == -1 then we need more data, so dont do anything yet..
  if (parse_result == -1){
      request->lastpos = 0;
      request->multibulklen = 0;
      request->parse_phase = RDS_PHASE_CONNECT;
      return;    
  }


  if (parse_result == 1){      
      on_line_complete(request);  
  }

  // puts(">>>> d1");
  
  // puts(">>>> d2");
  // request->state.error_code = HTTP_BAD_REQUEST;

  // assert(data_len);
  // size_t nparsed = http_parser_execute((http_parser*)&request->parser,
  //                                      &parser_settings, data, data_len);
  // if(nparsed != data_len)
  //   request->state.error_code = HTTP_BAD_REQUEST;
}



#define REQUEST ((Request*)parser->data)
#define PARSER  ((bj_parser*)parser)
#define UPDATE_LENGTH(name) \
  /* Update the len of a header field/value.
   *
   * Short explaination of the pointer arithmetics fun used here:
   *
   *   [old header data ] ...stuff... [ new header data ]
   *   ^-------------- A -------------^--------B--------^
   *
   * A = XXX- PARSER->XXX.data
   * B = len
   * A + B = old header start to new header end
   */ \
  do { PARSER->name.len = (name - PARSER->name.data) + len; } while(0)

#define _set_header(k, v) PyDict_SetItem(REQUEST->headers, k, v);
#define _set_header_free_value(k, v) \
  do { \
    PyObject* val = (v); \
    _set_header(k, val); \
    Py_DECREF(val); \
  } while(0)
#define _set_header_free_both(k, v) \
  do { \
    PyObject* key = (k); \
    PyObject* val = (v); \
    _set_header(key, val); \
    Py_DECREF(key); \
    Py_DECREF(val); \
  } while(0)



int
on_line_complete(Request* request)
{
  // puts(">>> headers init");
  request->headers = PyDict_New();
  PyDict_Update(request->headers, wsgi_base_dict);
  
  PyDict_SetItemString(request->headers, "REDIS_CMD", request->cmd_list);
  if (request->cmd_list == NULL){    
    puts("WHATTTTTT !!!!!!!!!!!!!");
  } else {
    // TODO: ???
    // Py_DECREF(request->cmd_list);      
  }
  

  // puts(">>> headers init 2");
  // /* HTTP_CONTENT_{LENGTH,TYPE} -> CONTENT_{LENGTH,TYPE} */
  // PyDict_SetItem(request->headers, _HTTP_CONTENT_LENGTH, PyString_FromString(120));
  // PyDict_ReplaceKey(request->headers, _HTTP_CONTENT_LENGTH, _CONTENT_LENGTH);
  // PyDict_ReplaceKey(REQUEST->headers, _HTTP_CONTENT_TYPE, _CONTENT_TYPE);

  // /* SERVER_PROTOCOL (REQUEST_PROTOCOL) */
  // _set_header(_SERVER_PROTOCOL, parser->http_minor == 1 ? _HTTP_1_1 : _HTTP_1_0);

  // /* REQUEST_METHOD */
  // if(parser->method == HTTP_GET) {
  //   /* I love useless micro-optimizations. */
  // _set_header(_REQUEST_METHOD, _GET);
  // } else {
  //   _set_header_free_value(_REQUEST_METHOD,
  //     PyString_FromString(http_method_str(parser->method)));
  // }

  // /* REMOTE_ADDR */
  // _set_header(_REMOTE_ADDR, REQUEST->client_addr);

  // PyObject* body = PyDict_GetItem(REQUEST->headers, _wsgi_input);
  // if(body) {
  //   /* We abused the `pos` member for tracking the amount of data copied from
  //    * the buffer in on_body, so reset it to zero here. */
  //   ((Iobject*)body)->pos = 0;
  // } else {
  //   /* Request has no body */
  // _set_header_free_value(_wsgi_input, PycStringIO->NewInput(_empty_string));
  // }

  // PyDict_Update(REQUEST->headers, wsgi_base_dict);

  request->state.parse_finished = true;
  return 0;
}



static inline void
PyDict_ReplaceKey(PyObject* dict, PyObject* old_key, PyObject* new_key)
{
  PyObject* value = PyDict_GetItem(dict, old_key);
  if(value) {
    Py_INCREF(value);
    PyDict_DelItem(dict, old_key);
    PyDict_SetItem(dict, new_key, value);
    Py_DECREF(value);
  }
}

void _initialize_request_module(const char* server_host, const int server_port)
{
  
  puts("init req. module");

  if (wsgi_base_dict == NULL) {
    PycString_IMPORT;
    wsgi_base_dict = PyDict_New();

    /* dct['wsgi.file_wrapper'] = FileWrapper */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.file_wrapper",
      (PyObject*)&FileWrapper_Type
    );

    /* dct['SCRIPT_NAME'] = '' */
    PyDict_SetItemString(
      wsgi_base_dict,
      "SCRIPT_NAME",
      _empty_string
    );

    /* dct['wsgi.version'] = (1, 0) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.version",
      PyTuple_Pack(2, PyInt_FromLong(1), PyInt_FromLong(0))
    );

    /* dct['wsgi.url_scheme'] = 'http'
     * (This can be hard-coded as there is no TLS support in bjoern.) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.url_scheme",
      PyString_FromString("http")
    );

    /* dct['wsgi.errors'] = sys.stderr */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.errors",
      PySys_GetObject("stderr")
    );

    /* dct['wsgi.multithread'] = True
     * If I correctly interpret the WSGI specs, this means
     * "Can the server be ran in a thread?" */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multithread",
      Py_True
    );

    /* dct['wsgi.multiprocess'] = True
     * ... and this one "Can the server process be forked?" */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multiprocess",
      Py_True
    );

    /* dct['wsgi.run_once'] = False (bjoern is no CGI gateway) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.run_once",
      Py_False
    );
  }

  PyDict_SetItemString(
    wsgi_base_dict,
    "SERVER_NAME",
    PyString_FromString(server_host)
  );

  PyDict_SetItemString(
    wsgi_base_dict,
    "SERVER_PORT",
    PyString_FromFormat("%d", server_port)
  );
}
