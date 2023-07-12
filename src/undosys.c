#include "undosys.h"
#include "koh_destral_ecs.h"
#include "timers.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct UndoSys {
    struct UndoState  st;
    struct UndoState  *states;
    int     num, cap;
    int     i, j, cur;
};

UndoSys *undosys_new(int cap) {
    UndoSys *us = calloc(1, sizeof(UndoSys));
    assert(us);
    assert(cap > 0);
    us->cap = cap;
    us->states = calloc(us->cap, sizeof(us->states[0]));
    assert(us->states);
    return us;
}

void undosys_free(UndoSys *us) {
    assert(us);
    for (int j = 0; j < us->cap; ++j) {
        if (us->states[j].r) {
            de_ecs_destroy(us->states[j].r);
            us->states[j].r = NULL;
        }

        if (us->states[j].timers) {
            for (int i = 0; i < us->states[i].timers_num; ++i) {
                timerman_free(us->states[j].timers[i]);
                us->states[j].timers[i] = NULL;
            }
        }

    }
    free(us);
}

void undosys_push(UndoSys *us, struct UndoState st) {
    /*
    if (us->states[us->i].r) {
        de_ecs_destroy(us->states[us->i].r);
        us->states[us->i].r = NULL;
    }
    if (us->states[us->i].tm) {
        timerman_free(us->states[us->i].tm);
        us->states[us->i].tm = NULL;
    }
    if (us->states[us->i].udata) {
        if (us->states[us->i].sz) {
            free(us->states[us->i].udata);
        }
        us->states[us->i].udata = NULL;
        us->states[us->i].sz = 0;
    }

    us->states[us->i].r = de_ecs_clone(st.r);
    assert(us->states[us->i].r);

    us->states[us->i].tm = timerman_clone(st.tm);
    assert(us->states[us->i].tm);

    if (!st.sz) 
        us->states[us->i].udata = st.udata;
    else {
        assert(st.sz > 0);
        us->states[us->i].udata = malloc(st.sz);
        memcpy(us->states[us->i].udata, st.udata, st.sz);
        us->states[us->i].sz = st.sz;
    }
    */
}

struct UndoState *undosys_get(UndoSys *us) {
    return NULL;
}

void undosys_prev(UndoSys *us) {
}

void undosys_next(UndoSys *us) {
}

void undosys_last(UndoSys *us) {
}

void undosys_first(UndoSys *us) {
}

void undosys_window(UndoSys *us) {
}
