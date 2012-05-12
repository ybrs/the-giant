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

static PyMethodDef TheGiant_FunctionTable[] = {
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
