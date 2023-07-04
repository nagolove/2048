#pragma once

#include "koh_destral_ecs.h"

typedef struct ecs_circ_buf {
    de_ecs  *ecs_tmp;
    de_ecs  **ecs;
    int     num, cap;
    int     i, j, cur;
} ecs_circ_buf;

void ecs_circ_buf_init(ecs_circ_buf *b, int cap);
void ecs_circ_buf_shutdown(ecs_circ_buf *b);
void ecs_circ_buf_push(ecs_circ_buf *b, de_ecs *r);//, void *udata, size_t udata_sz);
de_ecs *ecs_circ_buf_prev(ecs_circ_buf *b);
de_ecs *ecs_circ_buf_next(ecs_circ_buf *b);
de_ecs *ecs_circ_buf_last(struct ecs_circ_buf *ecs_buf);
de_ecs *ecs_circ_buf_first(struct ecs_circ_buf *ecs_buf);
void ecs_circ_buf_window(ecs_circ_buf *ecs_buf, de_ecs **r);
