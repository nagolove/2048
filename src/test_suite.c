#include "test_suite.h"
#include "modelview.h"
#include "raylib.h"

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static struct Setup modelview_test_setup = {
    .pos            = NULL,
    .cam            = NULL,
    .field_size     = 5,
    .tmr_block_time = 0.01,
    .tmr_put_time   = 0.01,
    .use_gui        = false,
    .auto_put       = false,
    .use_bonus      = false,
};

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
                    print_field5(values);
                    abort();
                }
            }
        }
}

static void test_modelview_arr(struct TestInput input) {
    assert(input.steps);
    assert(input.steps_num >= 0);
    assert(input.field_setup);
    printf("test_modelview_arr:\n");
    struct ModelView mv = {};
    modelview_init(&mv, modelview_test_setup);
    //setup_field(&mv, input->steps[i].field);
    print_field5(input.field_setup);
    for (int i = 0; i < input.steps_num; ++i) {
        modelview_input(&mv, input.steps[i].dir);
        BeginDrawing();
        while (mv.dir != DIR_NONE) { // условие цикла DIR_NONE ?
            modelview_draw(&mv);
            usleep(1000000 / 100); // 100Hz
        }
        EndDrawing();
        check_field(&mv, input.steps[i].field);
    }
    modelview_shutdown(&mv);
}

void test_modelviews_multiple() {

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) 
        {{
            .field = {
                {0, 1, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .dir = DIR_NONE,
        },
        },
        .steps_num = 1,
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .field = {
                    {0, 2, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                },
                .dir = DIR_UP,
            },
        },
        .steps_num = 1,
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
        {
            .field = {
                {1, 0, 0, 0, 0,},
                {1, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
            },
            .dir = DIR_LEFT,
        },
    },
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
        {
            .field = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 8, 0, 0, 0,},
            },
            .dir = DIR_DOWN,
        },
        },
        .steps_num = 1,
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
        {
            .field = {
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 0, 0, 0, 0,},
                {0, 8, 0, 0, 0,},
            },
            .dir = DIR_DOWN,
        }
        },
        .steps_num = 1,
    });

}

void test_modelviews_one() {
    struct ModelView mv = {};
    modelview_init(&mv, modelview_test_setup);
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
    /*while (mv.dir != DIR_NONE) {*/
    /*while (mv.state == MVS_ANIMATION) {*/
    for (i = 0; i < 1000; ++i) {
        /*printf("test_modelviews_one: i %d\n", i++);*/
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
