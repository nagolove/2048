#include "test_suite.h"
#include "modelview.h"
#include "raylib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void setup_field(struct ModelView *mv, const int values[5][5]) {
    assert(mv);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            if (values[y][x])
                modelview_put_manual(mv, x, y, values[y][x]);
}

void print_field(struct ModelView *mv) {
    assert(mv);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
            if (cell) {
                printf("[%.3d] ", cell->value);
            } else {
                printf("[---] ");
            }
        }
        printf("\n");
    }
}

void print_field5(const int values[5][5]) {
    assert(values);
    printf("print_field5:\n");
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            if (values[y][x]) {
                printf("[%.3d] ", values[y][x]);
            } else {
                printf("[---] ");
            }
        }
        printf("\n");
    }
}

void check_field(struct ModelView *mv, const int values[5][5]) {
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x)
            if (values[y][x]) {
                struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
                if (!cell) {
                    printf("check_field: cell == NULL\n");
                    printf(
                        "check_field: values[%d][%d] %d\n",
                        y, x, values[y][x]
                    );
                    print_field(mv);
                    print_field5(values);
                    abort();
                }
                if (cell->value != values[y][x]) {
                    printf(
                        "check_field: cell->value %d, values[%d][%d] %d\n",
                        cell->value, y, x, values[y][x]
                    );
                    print_field(mv);
                    abort();
                }
            }
        }
}

static void test_modelview_arr(struct TestInput *inputs, int inputs_num) {
    printf("test_modelview_arr:\n");
    struct ModelView mv = {};
    modelview_init(&mv, (struct Setup) {
        .pos = NULL,
        .cam = NULL,
        .field_size = 5,
        .tmr_block_time = 0.01,
        .tmr_put_time = 0.01,
    });
    mv.use_gui = false;
    for (int i = 0; i < inputs_num; ++i) {
        print_field5(inputs[i].field_setup);
        setup_field(&mv, inputs[i].field_setup);
        modelview_input(&mv, inputs[i].dir);
        BeginDrawing();
        while (mv.dir != DIR_NONE) {
            modelview_draw(&mv);
            usleep(1000000 / 100); // 100Hz
        }
        EndDrawing();
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

void test_modelviews_one() {
    struct ModelView mv = {};
    modelview_init(&mv, (struct Setup) {
        .pos = NULL,
        .cam = NULL,
        .field_size = 5,
        .tmr_block_time = 0.01,
        .tmr_put_time = 0.01,
    });
    mv.use_gui = false;
    printf("test_modelviews_one:\n");
    const int field_1[5][5] = { 
        {0, 0, 0, 0, 0}, 
        {0, 2, 0, 0, 0}, 
        {0, 1, 0, 0, 0}, 
        {0, 0, 0, 0, 0}, 
        {0, 0, 0, 0, 0}, 
    };
    print_field5(field_1);
    setup_field(&mv, field_1);
    modelview_input(&mv, DIR_DOWN);
    BeginDrawing();
    int i = 0;
    printf(
        "test_modelviews_one: mv->state %s\n",
        modelview_state2str(mv.state)
    );
    while (mv.dir != DIR_NONE) {
        printf("test_modelviews_one: i %d\n", i++);
        modelview_draw(&mv);
        usleep(1000000 / 100); // 100Hz
    }
    EndDrawing();
    check_field(&mv, (int[5][5]) {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 3, 0, 0, 0},
    });
    //print_field(&mv);
    modelview_shutdown(&mv);
}