#pragma once

#include "koh_destral_ecs.h"
#include "timers.h"
#include <stddef.h>

typedef struct UndoSys UndoSys;

struct UndoState {
    de_ecs      *r;
    TimerMan    **timers;
    int         timers_num;
    void        *udata;
    size_t      sz;
};

UndoSys *undosys_new(int cap);
void undosys_free(UndoSys *us);
void undosys_push(UndoSys *us, struct UndoState st);
void undosys_prev(UndoSys *us);
void undosys_next(UndoSys *us);
void undosys_last(UndoSys *us);
void undosys_first(UndoSys *us);
struct UndoState *undosys_get(UndoSys *us);
void undosys_window(UndoSys *us);
