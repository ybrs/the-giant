#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifdef WANT_SIGINT_HANDLING
#include <sys/signal.h>
#endif

#ifdef WANT_SENDFILE
#include <sys/sendfile.h>
#endif

#include <ev.h>
#include "common.h"
#include "wsgi.h"
#include "server.h"
#include "thegiantmodule.h"

#define LISTEN_BACKLOG  1024
// xxx
/*
#define READ_BUFFER_SIZE 10
*/
#define READ_BUFFER_SIZE 64*1024

#define Py_XCLEAR(obj) do { if(obj) { Py_DECREF(obj); obj = NULL; } } while(0)

static int sockfd;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static bool send_chunk(Request*);
#ifdef WANT_SENDFILE
static bool do_sendfile(Request*);
#endif

static bool handle_nonzero_errno(Request*);



void ev_io_on_timer(struct ev_loop *loop, ev_timer *w, int revents){
    // DBG("timer called");
}

extern PyObject *pydeferqueue; 
ev_idle *idle_watcher;

void idle_cb(struct ev_loop *loop, ev_idle *w, int revents)
{
    int listsize;
    GIL_LOCK(0);
       
    listsize=PyList_Size(pydeferqueue);
    if (listsize>0)
    {
        PyObject *pyelem = PySequence_GetItem(pydeferqueue,0); 
        PyObject *pyfct = PySequence_GetItem(pyelem,0);
        PyObject *pyfctargs = PySequence_GetItem(pyelem,1);
        //execute the python code
        DBG("Execute 1 python function in defer mode:%i ", listsize);

        // 
        PyObject *response = PyObject_CallFunctionObjArgs(pyfct, pyfctargs, NULL); 

        DBG("2");
     
        if (response==NULL) 
        {
            printf("ERROR!!!! Defer callback function as a problem. \nI remind that it takes always one argumet\n");
            PyErr_Print();
            //exit(1);
        }
        DBG("3");
        Py_XDECREF(response);
        Py_DECREF(pyfct);
        Py_DECREF(pyfctargs);
        Py_DECREF(pyelem);
        DBG("4");
        //remove the element
        PySequence_DelItem(pydeferqueue,0); // don't ask me why, but the delitem has to be after the decrefs
        DBG("5");
    } else
    {
        //stop idle if queue is empty
        DBG("stop ev_idle");
        ev_idle_stop(loop, w);
        Py_DECREF(pydeferqueue);
        pydeferqueue=NULL;
    }
    
    GIL_UNLOCK(0);
}

void server_run(void)//(const char* hostaddr, const int port)
{
    int i;
    struct TimerObj *timer;
    struct ev_loop* mainloop = ev_default_loop(0);
    ev_io accept_watcher;
    ev_io_init(&accept_watcher, ev_io_on_request, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    #if WANT_SIGINT_HANDLING
        ev_signal signal_watcher;
        ev_signal_init(&signal_watcher, ev_signal_on_sigint, SIGINT);
        ev_signal_start(mainloop, &signal_watcher);
    #endif  

    // ev_timer_init(&mytimer, ev_io_on_timer, 1., 1.);
    // ev_timer_start(mainloop, &mytimer);

    if (list_timers_i>=0)
    {
        for (i=0; i<list_timers_i; i++)
        {
            timer=list_timers[i];
            ev_timer_init(&timer->timerwatcher, timer_cb, timer->delay, timer->delay);
            ev_timer_start(mainloop, &timer->timerwatcher);
        }
    }

    idle_watcher = malloc(sizeof(ev_idle));
    ev_idle_init(idle_watcher, idle_cb);

    /* This is the program main loop */
    Py_BEGIN_ALLOW_THREADS
    ev_loop(mainloop, 0);
    Py_END_ALLOW_THREADS
}

#if WANT_SIGINT_HANDLING
static void ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
    /* Clean up and shut down this thread.
    * (Shuts down the Python interpreter if this is the main thread) */
    ev_unloop(mainloop, EVUNLOOP_ALL);
    PyErr_SetInterrupt();
}
#endif

bool server_init(const char* hostaddr, const int port)
{
    if((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        return false;
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = PF_INET;
    inet_pton(AF_INET, hostaddr, &sockaddr.sin_addr);
    sockaddr.sin_port = htons(port);

    /* Set SO_REUSEADDR t make the IP address available for reuse */
    int optval = true;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0){
        return false;    
    }
    

    if(listen(sockfd, LISTEN_BACKLOG) < 0){
        return false;
    }
       
    DBG("Listening on %s:%d...", hostaddr, port);
    return true;
}




static void ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    int client_fd;
    struct sockaddr_in sockaddr;
    socklen_t addrlen;

    addrlen = sizeof(struct sockaddr_in);
    client_fd = accept(watcher->fd, (struct sockaddr*)&sockaddr, &addrlen);
    if (client_fd < 0) {
        DBG("Could not accept() client: errno %d", errno);
        return;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    if (fcntl(client_fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) == -1) {
        DBG("Could not set_nonblocking() client %d: errno %d", client_fd, errno);
        return;
    }
  
    GIL_LOCK(0);
    Request* request = Request_new(client_fd, inet_ntoa(sockaddr.sin_addr));
    GIL_UNLOCK(0);

    DBG_REQ(request, "Accepted client %s:%d on fd %d",
          inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port), client_fd);

    ev_io_init(&request->ev_watcher, &ev_io_on_read, client_fd, EV_READ);
    ev_io_start(mainloop, &request->ev_watcher);
}

static void ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    static char read_buf[READ_BUFFER_SIZE];
    Request* request = REQUEST_FROM_WATCHER(watcher);
  
    Py_ssize_t read_bytes = read(
        request->client_fd,
        read_buf,
        READ_BUFFER_SIZE
    );

    GIL_LOCK(0);
  
    if (read_bytes <= 0) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            if(read_bytes == 0)
                DBG_REQ(request, "Client disconnected");
            else
                DBG_REQ(request, "Hit errno %d while read()ing", errno);
            close(request->client_fd);
            
            ev_io_stop(mainloop, &request->ev_watcher);
            Request_free(request);
        }
        goto out;
    }

    Request_parse(request, read_buf, read_bytes);

    if (request->state.error_code) {
        DBG_REQ(request, "Parse error");
        request->current_chunk = PyString_FromString("-ERR parse error \r\n");    
        assert(request->iterator == NULL);
    } else if(request->state.parse_finished) {    
        // Here we call our python application
        if (!wsgi_call_application(request)) {      
            assert(PyErr_Occurred());
            // PyErr_Print();
            assert(!request->state.chunked_response);
            Py_XCLEAR(request->iterator);          

            // show the real error message
            // TODO: this is a little bit messed up
            PyObject *ptype, *pvalue, *ptraceback, *pystring;
            PyErr_Fetch(&ptype, &pvalue, &ptraceback);
            assert(ptype != NULL);
            pystring = PyObject_Str(pvalue);

            char *pStrErrorMessage = PyString_AsString(pystring);
            char buf[200];
            sprintf(buf, "-ERR %s\r\n", pStrErrorMessage);
            request->current_chunk = PyString_FromString(buf);
            Py_XDECREF(pystring);
            Py_XDECREF(ptype); 
            Py_XDECREF(pvalue); 
            Py_XDECREF(ptraceback); 
            // request->current_chunk = PyString_FromString("-ERR exception in python\r\n");
        }    
    } else {
        /* Wait for more data */
        goto out;
    }

    ev_io_stop(mainloop, &request->ev_watcher);
    ev_io_init(&request->ev_watcher, &ev_io_on_write, request->client_fd, EV_WRITE);
    ev_io_start(mainloop, &request->ev_watcher);

out:  
    GIL_UNLOCK(0);
    return;
}

static void ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    Request* request = REQUEST_FROM_WATCHER(watcher);
    GIL_LOCK(0);
    if (request->state.use_sendfile) {
        /* sendfile */
        // if(request->current_chunk && send_chunk(request))
        //   goto out;
        // /* abuse current_chunk_p to store the file fd */
        // request->current_chunk_p = PyObject_AsFileDescriptor(request->iterable);
        // if(do_sendfile(request))
        //   goto out;
    } else {
        /* iterable */
        if (send_chunk(request)){             
            goto out;
        }
      
        if (request->iterator) {      
            PyObject* next_chunk;      
            next_chunk = response_iterable_get_next_chunk(request);      
            if (next_chunk != NULL) {          
                request->current_chunk = wrap_redis_chunk(next_chunk, false, 0);
                if (PyErr_Occurred()) {
                    assert(false);
                }
                assert(request->current_chunk_p == 0);
                goto out;
            } else {        
                if (PyErr_Occurred()) {
                    PyErr_Print();
                    /* We can't do anything graceful here because at least one
                     * chunk is already sent... just close the connection */
                    DBG_REQ(request, "Exception in iterator, can not recover");
                    ev_io_stop(mainloop, &request->ev_watcher);
                    close(request->client_fd);
                    Request_free(request);
                    goto out;
                } 
                Py_CLEAR(request->iterator);
            }
        }

        if (request->state.chunked_response) {
            assert(false);
            /* We have to send a terminating empty chunk + \r\n */
            request->current_chunk = PyString_FromString("0\r\n\r\n");
            assert(request->current_chunk_p == 0);
            request->state.chunked_response = false;
            goto out;
        }
    }

    ev_io_stop(mainloop, &request->ev_watcher);
    DBG_REQ(request, "done, keep-alive");
    Request_clean(request);
    Request_reset(request);
    ev_io_init(&request->ev_watcher, &ev_io_on_read,
               request->client_fd, EV_READ);
    ev_io_start(mainloop, &request->ev_watcher);

out:
    GIL_UNLOCK(0);
}

static bool send_chunk(Request* request)
{
    Py_ssize_t chunk_length;
    Py_ssize_t bytes_sent;  
    assert(request->current_chunk != NULL);
    assert(!(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)
         && PyString_GET_SIZE(request->current_chunk) != 0));
    
    bytes_sent = write(
        request->client_fd,
        PyString_AS_STRING(request->current_chunk) + request->current_chunk_p,
        PyString_GET_SIZE(request->current_chunk) - request->current_chunk_p
    );
    
    if(bytes_sent == -1){
        return handle_nonzero_errno(request);
    }
    
    request->current_chunk_p += bytes_sent;
    
    if(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)) {
    Py_CLEAR(request->current_chunk);
    request->current_chunk_p = 0;
    return false;
  }
  return true;
}

#define SENDFILE_CHUNK_SIZE 16*1024

#ifdef WANT_SENDFILE
static bool do_sendfile(Request* request)
{
    Py_ssize_t bytes_sent = sendfile(
        request->client_fd,
        request->current_chunk_p, /* current_chunk_p stores the file fd */
        NULL, SENDFILE_CHUNK_SIZE
    );
    if(bytes_sent == -1)
        return handle_nonzero_errno(request);
    return bytes_sent != 0;
}
#endif

static bool handle_nonzero_errno(Request* request)
{
  if(errno == EAGAIN || errno == EWOULDBLOCK) {
    /* Try again later */
    return true;
  } else {
    /* Serious transmission failure. Hang up. */
    fprintf(stderr, "Client %d hit errno %d\n", request->client_fd, errno);
    Py_XDECREF(request->current_chunk);
    Py_XCLEAR(request->iterator);
    request->state.keep_alive = false;
    return false;
  }
}
