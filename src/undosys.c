#include "undosys.h"
#include "timers.h"
#include <assert.h>
#include <stdlib.h>

struct UndoState {
    de_ecs      *r;
    TimerMan    *tm;
};

struct UndoSys {
    struct UndoState  *st_tmp;
    struct UndoState  *states;
    int     num, cap;
    int     i, j, cur;
};

UndoSys *undosys_new(int cap) {
    UndoSys *us = calloc(1, sizeof(UndoSys));
    assert(us);
    return us;
}

void undosys_free(UndoSys *us) {
    assert(us);
    for (int j = 0; j < us->cap; ++j) {
        if (us->states[j].r) {
            de_ecs_destroy(us->states[j].r);
            us->states[j].r = NULL;
        }
        if (us->states[j].tm) {
            timerman_free(us->states[j].tm);
            us->states[j].tm = NULL;
        }
    }
    free(us);
}

void undosys_push(UndoSys *us, de_ecs *r, TimerMan *tm) {
}

void undosys_prev(UndoSys *us) {
}

void undosys_next(UndoSys *us) {
}

void undosys_last(UndoSys *us) {
}

void undosys_first(UndoSys *us) {
}

void undosys_window(UndoSys *us);
