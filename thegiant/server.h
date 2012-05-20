#include "request.h"
struct ev_timer mytimer;

bool server_init(const char* hostaddr, const int port);
void server_run(void);
// void server_run(const char* hostaddr, const int port)
