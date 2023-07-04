#pragma once

#include "koh_destral_ecs.h"
#include "timers.h"

typedef struct UndoSys UndoSys;

UndoSys *undosys_new(int cap);
void undosys_free(UndoSys *us);
void undosys_push(UndoSys *us, de_ecs *r, TimerMan *tm);
void undosys_prev(UndoSys *us);
void undosys_next(UndoSys *us);
void undosys_last(UndoSys *us);
void undosys_first(UndoSys *us);
void undosys_window(UndoSys *us);
