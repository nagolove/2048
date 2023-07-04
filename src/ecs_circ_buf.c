#include "ecs_circ_buf.h"
#include "koh_destral_ecs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include <assert.h>
#include <stdlib.h>

void ecs_circ_buf_init(struct ecs_circ_buf *b, int cap) {
    assert(cap > 0);
    assert(b);
    b->cap = cap;
    b->ecs = calloc(b->cap, sizeof(b->ecs[0]));
    b->num = 0;
    b->cur = 0;
}

void ecs_circ_buf_shutdown(struct ecs_circ_buf *b) {
    assert(b);
    for (int j = 0; j < b->num; ++j) {
        if (b->ecs[j]) {
            de_ecs_destroy(b->ecs[j]);
            b->ecs[j] = NULL;
        }
    }
    if (b->ecs) {
        free(b->ecs);
        b->ecs = NULL;
    }
}

void ecs_circ_buf_push(struct ecs_circ_buf *b, de_ecs *r) {
    if (b->ecs[b->i]) {
        de_ecs_destroy(b->ecs[b->i]);
    }
    b->ecs[b->i] = de_ecs_clone(r);
    b->i = (b->i + 1) % b->cap;
    b->num++;
    if (b->num >= b->cap)
        b->num = b->cap;
}

de_ecs *ecs_circ_buf_prev(struct ecs_circ_buf *b) {
    assert(b);
    de_ecs *ret = NULL;
    if (b->cur > 0) {
        if (b->ecs[b->cur - 1]) {
            ret = b->ecs[b->cur - 1];
            b->cur--;
        }
    }
    return ret;
}

de_ecs *ecs_circ_buf_next(struct ecs_circ_buf *b) {
    assert(b);
    de_ecs *ret = NULL;
    if (b->cur < b->cap) {
        if (b->ecs[b->cur + 1]) {
            ret = b->ecs[b->cur + 1];
            b->cur++;
        }
    }
    return ret;
}

de_ecs *ecs_circ_buf_first(struct ecs_circ_buf *ecs_buf) {
    de_ecs *cur = NULL, *prev = NULL;
    do {
        prev = cur;
        cur = ecs_circ_buf_next(ecs_buf);
    } while (cur);
    return prev;
}

de_ecs *ecs_circ_buf_last(struct ecs_circ_buf *ecs_buf) {
    de_ecs *cur = NULL, *prev = NULL;
    do {
        prev = cur;
        cur = ecs_circ_buf_prev(ecs_buf);
    } while (cur);
    return prev;
}

void ecs_circ_buf_window(struct ecs_circ_buf *ecs_buf, de_ecs **r) {
    assert(ecs_buf);
    assert(r);
    assert(*r);
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("ecs", &open, flags);

    if (igButton("|<<", (ImVec2) {0})) {
        *r = ecs_circ_buf_last(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton("<<", (ImVec2) {0})) {
        *r = ecs_circ_buf_prev(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton("play/pause", (ImVec2) {0})) {
        if (ecs_buf->ecs_tmp)
            *r = ecs_buf->ecs_tmp;
    }
    igSameLine(0, 10);
    if (igButton(">>", (ImVec2) {0})) {
        *r = ecs_circ_buf_next(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton(">>|", (ImVec2) {0})) {
        *r = ecs_circ_buf_first(ecs_buf);
    }
    igText("captured %d systems", ecs_buf->num);
    igText("cur %d system", ecs_buf->cur);

    igEnd();
}

