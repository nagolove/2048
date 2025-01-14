#pragma once

#include "koh_ecs.h"

// XXX: Что за функции? Для чего нужен интерфейс? Чем он занимается?

typedef struct ecs_circ_buf {
    ecs_t   *ecs_tmp;
    ecs_t   **ecs;
    int     num, cap;
    int     i, j, cur;
} ecs_circ_buf;

void ecs_circ_buf_init(ecs_circ_buf *b, int cap);
void ecs_circ_buf_shutdown(ecs_circ_buf *b);
void ecs_circ_buf_push(ecs_circ_buf *b, ecs_t *r);//, void *udata, size_t udata_sz);
ecs_t *ecs_circ_buf_prev(ecs_circ_buf *b);
ecs_t *ecs_circ_buf_next(ecs_circ_buf *b);
ecs_t *ecs_circ_buf_last(struct ecs_circ_buf *ecs_buf);
ecs_t *ecs_circ_buf_first(struct ecs_circ_buf *ecs_buf);
void ecs_circ_buf_window(ecs_circ_buf *ecs_buf, ecs_t **r);
