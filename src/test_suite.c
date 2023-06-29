#include "test_suite.h"

#include <assert.h>

void setup_field(struct ModelView *mv, int values[5][5]) {
    assert(mv);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            modelview_put_manual(mv, x, y, values[y][x]);
}

void check_field(struct ModelView *mv, int values[5][5]) {
    for (int x = 0; x < 5; ++x)
        for (int y = 0; y < 5; ++y) {
            if (values[y][x]) {
                struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
                assert(cell);
                assert(cell->value == values[y][x]);
            }
        }
}

static void test_modelview_arr(struct TestInput *inputs, int inputs_num) {
    struct ModelView mv = {};
    modelview_init(&mv, (struct Setup) {
        .pos = NULL,
        .cam = NULL,
        .field_size = 5,
        .tmr_block_time = 0.01,
        .tmr_put_time = 0.01,
    });
    for (int i = 0; i < inputs_num; ++i) {
        setup_field(&mv, inputs[i].field_setup);
        modelview_input(&mv, inputs[i].dir);
        while (!modelview_draw(&mv));
        check_field(&mv, inputs[i].field_check);
    }
    modelview_shutdown(&mv);
}

void test_modelviews_multiple() {
    test_modelview_arr((struct TestInput[]){
        {
            .field_setup = {
                {0, 1, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .field_check = {
                {0, 1, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .dir = DIR_NONE,
        },
    }, 1);

    test_modelview_arr((struct TestInput[]){
        {
            .field_setup = {
                {0, 1, 0, 0, 0,},
                {0, 1, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .field_check = {
                {0, 2, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .dir = DIR_UP,
        },
    }, 1);

    test_modelview_arr((struct TestInput[]){
        {
            .field_setup = {
                {0, 1, 0, 0, 0,},
                {0, 1, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .field_check = {
                {1, 0, 0, 0, 0,},
                {1, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .dir = DIR_LEFT,
        },
    }, 1);

    test_modelview_arr((struct TestInput[]){
        {
            .field_setup = {
                {0, 2, 0, 0, 0,},
                {0, 2, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 2, 0, 0, 0,},
                {0, 2, 0, 0, 0,},
            },
            .field_check = {
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 8, 0, 0, 0,},
            },
            .dir = DIR_DOWN,
        },
    }, 1);
}
