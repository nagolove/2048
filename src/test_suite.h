#pragma once

#include "modelview.h"

#include "stdbool.h"

typedef struct TestPayload {
    bool do_trap;
} TestPayload;

/*
 .x == -1, .y == -1 - koh_trap()
 .x == -11, .y == -11 - koh_trap() через test_payload
*/
struct Pair {
    int x, y;
};

struct Step {
    const char      *msg;
    struct Cell     *new_cell;
    enum Direction  dir;
    int             field[5][5];
    bool            last;
    struct Pair     *trap;
};

struct TestInput {
    const char  *name;
    int         field_setup[5][5];
    /*
    int         **field_setup;
    */
    struct Step *steps;
    int         steps_num;
};

struct TestCtx {
    int         test_suite_index, test_index;
    const char  *name;
    struct Pair *trap;
};

void test_modelviews_multiple();
void setup_field(struct ModelView *mv, const int values[5][5]);
bool check_field(
    struct ModelView *mv, const int values[5][5], const struct TestCtx *ctx
);
