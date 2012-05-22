#include "request.h"
struct ev_timer mytimer;

#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)


bool server_init(const char* hostaddr, const int port);
void server_run(void);
// void server_run(const char* hostaddr, const int port)
