#pragma once

#include "modelview.h"

struct Step {
    struct Cell     *new_cell;
    enum Direction  dir;
    int             field[5][5];
};

struct TestInput {
    int         field_setup[5][5];
    /*
    int         **field_setup;
    */
    struct Step *steps;
    int         steps_num;
};

struct TestCtx {
    int test_suite_index, test_index;
};

void test_modelviews_multiple();
void setup_field(struct ModelView *mv, const int values[5][5]);
bool check_field(
    struct ModelView *mv, const int values[5][5], struct TestCtx *ctx
);
