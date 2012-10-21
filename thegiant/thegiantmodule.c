#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "thegiantmodule.h"
#include "filewrapper.h"


PyDoc_STRVAR(listen_doc,
"listen(application, host, port) -> None\n\n \
\
Makes thegiant listen to host:port and use application as WSGI callback. \
(This does not run the server mainloop.)");
static PyObject*
listen(PyObject* self, PyObject* args)
{
  const char* host;
  int port;

  if(wsgi_app) {
    PyErr_SetString(
      PyExc_RuntimeError,
      "Only one giant server per Python interpreter is allowed"
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
        "Must call server.listen(app, host, port) before "
        "calling server.run() without arguments."
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
        char *pStrErrorMessage = PyString_AsString(pystring);
        list_timers[list_timers_i]=timer;
        list_timers_i++;
    } 
    else
    {
        printf("Limit of maximum %i timers has been reached\n", list_timers_i);
    }
    
    return PyInt_FromLong(list_timers_i);    
}

/*
Procedure exposed in Python to stop a running timer: i
*/
static PyObject *stop_timer(PyObject *self, PyObject *args)
{
    int i;
    struct TimerObj *timer;
    struct ev_loop *loop = ev_default_loop(0);
    if (!PyArg_ParseTuple(args, "i", &i))
        return NULL;
    timer=list_timers[i];
    ev_timer_stop(loop, &timer->timerwatcher);
    
    return Py_None;
}

/*
Procedure exposed in Python to restart a running timer: i
*/
static PyObject *restart_timer(PyObject *self, PyObject *args)
{
    int i;
    struct TimerObj *timer;
    struct ev_loop *loop = ev_default_loop(0);
    if (!PyArg_ParseTuple(args, "i", &i))
        return NULL;
    if (i<=list_timers_i)
    {
        timer=list_timers[i];
        ev_timer_again(loop, &timer->timerwatcher);
    }
    else
    {
        printf("index out of range\n");
    }
    return Py_None;
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


PyObject *pydeferqueue;
ev_idle *idle_watcher;

/*
Register a python function to execute when idle
*/
PyObject *py_defer(PyObject *self, PyObject *args)
{
    struct ev_loop *loop = ev_default_loop(0);
    PyObject *pyfct, *pycombined, *pyfctargs;
    int startidle=0;
    int toadd=1;
    int listsize=0;
    
    if (!PyArg_ParseTuple(args, "OOO", &pyfct, &pyfctargs, &pycombined))
        return NULL;
    //if queue is empty, trigger a start of idle
    
    if (!pydeferqueue) 
    {
        pydeferqueue=PyList_New(0);
    }
    listsize=PyList_Size(pydeferqueue);
    if (listsize==0)
    {
        //it has been stopped by the idle_cb
        startidle=1;    
    }
    //add fct cb into the defer queue
    PyObject *pyelem=PyList_New(0);
    PyList_Append(pyelem, pyfct);
    PyList_Append(pyelem, pyfctargs);   
    if (pycombined==Py_True)
    {
        //check if the fucntion is already in the queue
        if (PySequence_Contains(pydeferqueue, pyelem))
        {
            toadd=0;
        }
    }
    
    if (toadd==1)
    {
        PyList_Append(pydeferqueue, pyelem);
        //start the idle
        if (startidle==1)
        {
            //we create a new idle watcher and we start it
            DBG("trigger idle_start");
            ev_idle_start(loop, idle_watcher);
        }
    }
    Py_DECREF(pyelem);
    return Py_None;
}

/*
Return the defer queue size
*/
PyObject *py_defer_queue_size(PyObject *self, PyObject *args)
{
    int listsize;
    if (pydeferqueue)
    {
        listsize = PyList_Size(pydeferqueue);  
        return Py_BuildValue("i", listsize);
    } 
    else
    {
        return Py_None;
    }
}


static PyMethodDef TheGiant_FunctionTable[] = {
    {"add_timer", add_timer, METH_VARARGS, "Add a timer"},
    {"stop_timer", stop_timer, METH_VARARGS, "Stop a timer"},
    {"restart_timer", restart_timer, METH_VARARGS, "Restart a timer"},
    
    /** on idle **/
    {"defer", py_defer, METH_VARARGS, "defer the execution of a python function."},
    {"defer_queue_size", py_defer_queue_size, METH_VARARGS, "Get the size of the defer queue"},

    {"run", run, METH_VARARGS, run_doc},
    {"listen", listen, METH_VARARGS, listen_doc},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initserver()
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  PyObject* thegiant_module = Py_InitModule("thegiant.server", TheGiant_FunctionTable);
  PyModule_AddObject(thegiant_module, "version", Py_BuildValue("(ii)", 1, 2));
}
