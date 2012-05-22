#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "thegiantmodule.h"
#include "filewrapper.h"


PyDoc_STRVAR(listen_doc,
"listen(application, host, port) -> None\n\n \
\
Makes bjoern listen to host:port and use application as WSGI callback. \
(This does not run the server mainloop.)");
static PyObject*
listen(PyObject* self, PyObject* args)
{
  const char* host;
  int port;

  if(wsgi_app) {
    PyErr_SetString(
      PyExc_RuntimeError,
      "Only one bjoern server per Python interpreter is allowed"
    );
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "Osi:run/listen", &wsgi_app, &host, &port))
    return NULL;

  _initialize_request_module(host, port);

  if(!server_init(host, port)) {
    PyErr_Format(
      PyExc_RuntimeError,
      "Could not start server on %s:%d", host, port
    );
    return NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(run_doc,
"run(application, host, port) -> None\n \
Calls listen(application, host, port) and starts the server mainloop.\n \
\n\
run() -> None\n \
Starts the server mainloop. listen(...) has to be called before calling \
run() without arguments.");
static PyObject*
run(PyObject* self, PyObject* args)
{
  if(PyTuple_GET_SIZE(args) == 0) {
    /* bjoern.run() */
    if(!wsgi_app) {
      PyErr_SetString(
        PyExc_RuntimeError,
        "Must call bjoern.listen(app, host, port) before "
        "calling bjoern.run() without arguments."
      );
      return NULL;
    }
  } else {
    /* bjoern.run(app, host, port) */
    if(!listen(self, args))
      return NULL;
  }

  server_run();
  wsgi_app = NULL;
  Py_RETURN_NONE;
}


list_timers_i = 0;

static PyObject *add_timer(PyObject *self, PyObject *args)
{

    DBG("addtimer called");

    struct TimerObj *timer;
    if (list_timers_i<MAX_TIMERS)
    {
        timer = calloc(1,sizeof(struct TimerObj));
        if (!PyArg_ParseTuple(args, "fO", &timer->delay, &timer->py_cb)){
          return NULL;          
        } 

        timer->num = list_timers_i;

        PyObject *pystring;
        pystring = PyObject_Str(timer->py_cb);
        DBG("timer called - 2.2.1");
        char *pStrErrorMessage = PyString_AsString(pystring);
        DBG("=============== err message ==================");
        DBG("%s", pStrErrorMessage);
        DBG("=============== err message ==================");        
            
        list_timers[list_timers_i]=timer;
        list_timers_i++;
    } 
    else
    {
        printf("Limit of maximum %i timers has been reached\n", list_timers_i);
    }
    
    return PyInt_FromLong(list_timers_i);    
}


void timer_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
    GIL_LOCK(0);
    struct TimerObj *timer= ((struct TimerObj*) (((char*)w) - offsetof(struct TimerObj,timerwatcher)));

    assert(timer->py_cb);
    if (PyCallable_Check(timer->py_cb) != 1){
        return;      
    } 

    PyObject *pystring, *objtype;
    objtype = PyObject_Type(timer->py_cb);
    assert(objtype);
    if (objtype == NULL){
      assert(false);
    }

    PyObject* resp = PyObject_CallFunctionObjArgs(
        timer->py_cb,
        NULL /* sentinel */
    );

    if (resp==NULL)
    {
        if (PyErr_Occurred()) 
        { 
             PyErr_Print();
        }
        ev_timer_stop(loop, w);
    }
    if (resp==Py_False)
    {
        ev_timer_stop(loop, w);
    }
    Py_XDECREF(resp);
    GIL_UNLOCK(0);
}


static PyMethodDef TheGiant_FunctionTable[] = {
  {"add_timer", add_timer, METH_VARARGS, "Add a timer"},
  {"run", run, METH_VARARGS, run_doc},
  {"listen", listen, METH_VARARGS, listen_doc},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initthegiant()
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  PyObject* thegiant_module = Py_InitModule("thegiant", TheGiant_FunctionTable);
  PyModule_AddObject(thegiant_module, "version", Py_BuildValue("(ii)", 1, 2));
}
