#pragma once

#include "koh_destral_ecs.h"

struct ecs_circ_buf {
    de_ecs  *ecs_tmp;
    de_ecs  **ecs;
    int     num, cap;
    int     i, j, cur;
};

void ecs_circ_buf_init(struct ecs_circ_buf *b, int cap);
void ecs_circ_buf_shutdown(struct ecs_circ_buf *b);
void ecs_circ_buf_push(struct ecs_circ_buf *b, de_ecs *r);
de_ecs *ecs_circ_buf_prev(struct ecs_circ_buf *b);
de_ecs *ecs_circ_buf_next(struct ecs_circ_buf *b);
void ecs_circ_buf_window(struct ecs_circ_buf *ecs_buf, de_ecs **r);
