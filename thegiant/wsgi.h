#include <Python.h>
#include "request.h"

bool wsgi_call_application(Request*);
PyObject* response_iterable_get_next_chunk(Request*);
PyObject* wrap_http_chunk_cruft_around(PyObject* chunk);
PyObject* wrap_redis_chunk(PyObject* chunk, bool with_header, int total_elements_count);

PyTypeObject StartResponse_Type;
