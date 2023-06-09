#pragma once

#include <stdbool.h>
#include <stddef.h>

struct Timer {
    double  start_time; // GetTime()
    double  duration;   // in seconds
    double  amount;     // 0..1
    bool    expired;
    size_t  id;
    void    *data;
    void    (*update)(struct Timer *tmr);
};

typedef struct TimerMan TimerMan;

struct TimerDef {
    void *udata;
    size_t sz;
    double duration;
    void (*update)(struct Timer*);
};

struct TimerMan *timerman_new();
void timerman_free(struct TimerMan *tm);

void timerman_add(struct TimerMan *tm, struct TimerDef td);
int timerman_update(struct TimerMan *tm);
void timerman_pause(struct TimerMan *tm, bool is_paused);
void timerman_window(struct TimerMan *tm);
