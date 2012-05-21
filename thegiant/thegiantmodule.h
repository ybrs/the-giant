#ifndef thegiantmoduleh
#define thegiantmoduleh

struct TimerObj {
        ev_timer timerwatcher;
        float delay;
        int num;
        PyObject *py_cb;
};

// timers...
#define MAX_TIMERS 10 //maximum number of running timers
struct TimerObj *list_timers[MAX_TIMERS];
int list_timers_i; //number of values entered in the array list_timers


PyObject* wsgi_app;
void timer_cb(struct ev_loop *loop, ev_timer *w, int revents);
#endif