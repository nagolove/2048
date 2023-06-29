#pragma once

#include "modelview.h"

struct TestInput {
    int             field_setup[5][5], field_check[5][5];
    enum Direction  dir;
};

void test_modelviews_multiple();
void setup_field(struct ModelView *mv, int values[5][5]);
void check_field(struct ModelView *mv, int values[5][5]);
