#include "common.h"
#include "thegiantmodule.h"
#include "filewrapper.h"
#include "wsgi.h"

static inline bool inspect_headers(Request*);

bool wsgi_call_application(Request* request)
{
    /* From now on, `headers` stores the _response_ headers
    * (passed by the WSGI app) rather than the _request_ headers */
    PyObject* request_headers = request->headers;  
    request->headers = NULL;
  
    /* application(environ) call */
    PyObject* retval = PyObject_CallFunctionObjArgs(
        wsgi_app,
        request_headers,
        NULL /* sentinel */
    );

    Py_DECREF(request_headers);    
    
    if(retval == NULL){
        return false;
    }        

    /* The following code is somewhat magic, so worth an explanation.
    *
    * If the application we called was a generator, we have to call .next() on
    * it before we do anything else because that may execute code that
    * invokes `start_response` (which might not have been invoked yet).
    * Think of the following scenario:
    *
    *   def app(environ, start_response):
    *     start_response('200 Ok', ...)
    *     yield 'Hello World'
    *
    * That would make `app` return an iterator (more precisely, a generator).
    * Unfortunately, `start_response` wouldn't be called until the first item
    * of that iterator is requested; `start_response` however has to be called
    * _before_ the wsgi body is sent, because it passes the HTTP headers.
    *
    * If the application returned a list this would not be required of course,
    * but special-handling is painful - especially in C - so here's one generic
    * way to solve the problem:
    *
    * Look into the returned iterator in any case. This allows us to do other
    * optimizations, for example if the returned value is a list with exactly
    * one string in it, we can pick the string and throw away the list so bjoern
    * does not have to come back again and look into the iterator a second time.
    */
    PyObject* first_chunk;
    // TODO..... 
    // if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1 &&
    //     PyString_Check(PyList_GET_ITEM(retval, 0)))
    // {
    //     /* Optimize the most common case, a single string in a list: */
    //     PyObject* tmp = PyList_GET_ITEM(retval, 0);
    //     Py_INCREF(tmp);
    //     Py_DECREF(retval);
    //     retval = tmp;
    //     goto string; /* eeevil */
    // } else 

    if (PyString_Check(retval)) {
    /* According to PEP 333 strings should be handled like any other iterable,
     * i.e. sending the response item for item. "item for item" means
     * "char for char" if you have a string. -- I'm not that stupid. */    
        string:
            if(PyString_GET_SIZE(retval)) {
                first_chunk = retval;
            } else {
                Py_DECREF(retval);
                first_chunk = NULL;
            }
    } else if(FileWrapper_CheckExact(retval)) {
        assert(false);
        // request->state.use_sendfile = true;
        // request->iterable = ((FileWrapper*)retval)->file;
        // Py_INCREF(request->iterable);
        // Py_DECREF(retval);
        // request->iterator = NULL;
        // first_chunk = NULL;
    } else if (PyInt_Check(retval)){
        // integer replies dont have length of elements....
        first_chunk = wrap_redis_chunk(retval, false, 0 ) ;        
        if (first_chunk == NULL && PyErr_Occurred()){
            return false;
        }

    } else if (retval == Py_None){
        // assert(false);        
        first_chunk = wrap_redis_chunk(retval, false, 0 ) ;        
        if (first_chunk == NULL && PyErr_Occurred()){
            return false;
        }

    } else {
        /* Generic iterable (list of length != 1, generator, ...) */
        request->iterable = retval;
        request->iterator = PyObject_GetIter(retval);
        if(request->iterator == NULL){
            return false;
        }
        first_chunk = wrap_redis_chunk( response_iterable_get_next_chunk(request), true, PyObject_Size(retval) ) ;        
        if (first_chunk == NULL && PyErr_Occurred()){
            return false;
        }
            
    }

    request->state.keep_alive = false;


  /* Get the headers and concatenate the first body chunk.
   * In the first place this makes the code more simple because afterwards
   * we can throw away the first chunk PyObject; but it also is an optimization:
   * At least for small responses, the complete response could be sent with
   * one send() call (in server.c:ev_io_on_write) which is a (tiny) performance
   * booster because less kernel calls means less kernel call overhead. */
    PyObject* buf = PyString_FromStringAndSize(NULL, 1024);
    Py_ssize_t length = 0; 

  // if(length == 0) {
  //   printf(">>>> length: %s\n", length);
  //   Py_DECREF(first_chunk);
  //   Py_DECREF(buf);
  //   return false;
  // }

    if(first_chunk == NULL) {
        _PyString_Resize(&buf, length);
        goto out;
    }

    if(request->state.chunked_response) {
        PyObject* new_chunk = wrap_http_chunk_cruft_around(first_chunk);
        Py_DECREF(first_chunk);
        assert(PyString_GET_SIZE(new_chunk) >= PyString_GET_SIZE(first_chunk) + 5);
        first_chunk = new_chunk;
    }

    assert(buf);
    _PyString_Resize(&buf, length + PyString_GET_SIZE(first_chunk));
    memcpy(PyString_AS_STRING(buf)+length, PyString_AS_STRING(first_chunk),
    PyString_GET_SIZE(first_chunk));

    Py_DECREF(first_chunk);

out:
    request->state.wsgi_call_done = true;    
    request->current_chunk = buf;
    request->current_chunk_p = 0;
    return true;
}

static inline bool inspect_headers(Request* request)
{
  Py_ssize_t i;
  PyObject* tuple;

  for(i=0; i<PyList_GET_SIZE(request->headers); ++i) {
    tuple = PyList_GET_ITEM(request->headers, i);

    if(!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 2)
      goto err;

    PyObject* field = PyTuple_GET_ITEM(tuple, 0);
    PyObject* value = PyTuple_GET_ITEM(tuple, 1);

    if(!PyString_Check(field) || !PyString_Check(value))
      goto err;

    if(!strncasecmp(PyString_AS_STRING(field), "Content-Length", PyString_GET_SIZE(field)))
      request->state.response_length_unknown = false;
  }
  return true;

err:
  TYPE_ERROR_INNER("start_response argument 2", "a list of 2-tuples",
    "(found invalid '%.200s' object at position %zd)", Py_TYPE(tuple)->tp_name, i);
  return false;
}


inline PyObject* response_iterable_get_next_chunk(Request* request)
{    
  /* Get the next item out of ``request->iterable``, skipping empty ones. */
    PyObject* next;
    while(true) {
        next = PyIter_Next(request->iterator);
        if (next == NULL){        
            return NULL;
        }   
        return next;
        Py_DECREF(next);
    }
}

static inline void restore_exception_tuple(PyObject* exc_info, bool incref_items)
{
    if(incref_items) {
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 0));
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 1));
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 2));
    }
    PyErr_Restore(
        PyTuple_GET_ITEM(exc_info, 0),
        PyTuple_GET_ITEM(exc_info, 1),
        PyTuple_GET_ITEM(exc_info, 2)
    );
}



#define F_KEEP_ALIVE 1<<1
#define have_http11(parser) (parser.http_major > 0 && parser.http_minor > 0)


PyObject* wrap_redis_chunk(PyObject* chunk, bool with_header, int total_elements_count)
{
    /* 
    * for every iterator chunk we need to wrap with
    * $10\r\n
    * data\r\n
    */
    if (chunk == NULL){  
        // if its a null value, we should send 
        // $-1\r\n    
        char length_line[100];
        size_t length_line_size;    
        if (with_header){
            length_line_size = sprintf(length_line, "*%i\r\n$-1\r\n", total_elements_count);  
        } else {
            length_line_size = sprintf(length_line, "$-1\r\n");  
        } 

        PyObject* new_chunk = PyString_FromStringAndSize(length_line, length_line_size);
        return new_chunk;

    } else if (PyInt_Check(chunk)){
        long num = PyInt_AsLong(chunk);    
        char length_line[100];
        size_t length_line_size;    
        
        if (with_header){
            length_line_size = sprintf(length_line, "*%i\r\n:%d\r\n", total_elements_count, num);  
        } else {
            // single integer replies doesnt have multibulk len
            length_line_size = sprintf(length_line, ":%d\r\n", num);
        } 

        PyObject* new_chunk = PyString_FromStringAndSize(length_line, length_line_size);
        return new_chunk;
    } else {
        char length_line[100];
        size_t length_line_size;
        Py_ssize_t chunklen;
        if (chunk == Py_None){
            chunklen = -1;
        } else {
            chunklen = PyString_Size(chunk);    
        }
    
        if (chunklen == -1){        
            // sending null value...      
            if (with_header){
                length_line_size = sprintf(length_line, "*%i\r\n$-1\r\n", total_elements_count);  
            } else {
                length_line_size = sprintf(length_line, "$-1\r\n");  
            }         
            PyObject* new_chunk = PyString_FromStringAndSize(length_line, length_line_size);
            return new_chunk;
        } else {
            assert(chunklen>=0);    
            if (with_header){
                length_line_size = sprintf(length_line, "*%i\r\n$%i\r\n", total_elements_count,chunklen);  
            } else {
                length_line_size = sprintf(length_line, "$%i\r\n", chunklen);  
            } 
      
            char *buf = malloc(length_line_size+chunklen+2);  
            memcpy(buf, length_line, length_line_size);  
            // add the actual chunk 
            memcpy(buf + length_line_size,  PyString_AS_STRING(chunk), chunklen);
            memcpy(buf + length_line_size + chunklen,  "\r\n", 2);
            PyObject* new_chunk = PyString_FromStringAndSize(buf, length_line_size+chunklen+2);
            free(buf);
            return new_chunk;      
        }
    }  
}


PyObject*
wrap_http_chunk_cruft_around(PyObject* chunk)
{
  /* Who the hell decided to use decimal representation for Content-Length
   * but hexadecimal representation for chunk lengths btw!?! Fuck W3C */
  size_t chunklen = PyString_GET_SIZE(chunk);
  assert(chunklen);
  char buf[strlen("ffffffff") + 2];
  size_t n = sprintf(buf, "%x\r\n", (unsigned int)chunklen);
  PyObject* new_chunk = PyString_FromStringAndSize(NULL, n + chunklen + 2);
  char* new_chunk_p = PyString_AS_STRING(new_chunk);
  memcpy(new_chunk_p, buf, n);
  new_chunk_p += n;
  memcpy(new_chunk_p, PyString_AS_STRING(chunk), chunklen);
  new_chunk_p += chunklen;
  *new_chunk_p++ = '\r'; *new_chunk_p = '\n';
  assert(new_chunk_p == PyString_AS_STRING(new_chunk) + n + chunklen + 1);
  return new_chunk;
}
