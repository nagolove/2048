#pragma once

#include "modelview.h"

struct TestInput {
    int             field_setup[5][5], field_check[5][5];
    enum Direction  dir;
};

/*
// TODO: Сделать тестовую структуру для серии проверок с одним начальным 
// состоянием ModelView
struct TestInput {
    int             field_setup[5][5];
    struct Step {
        int field_check[5][5];
        enum Direction  dir;
    };
    struct Step *steps;
    int         steps_num;
};
*/

void test_modelviews_multiple();
void setup_field(struct ModelView *mv, const int values[5][5]);
void check_field(struct ModelView *mv, const int values[5][5]);
void test_modelviews_one();
