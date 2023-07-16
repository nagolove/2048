#include "test_suite.h"
#include "modelview.h"
#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define TERM_BLACK        30         
#define TERM_RED          31         
#define TERM_GREEN        32         
#define TERM_YELLOW       33         
#define TERM_BLUE         34         
#define TERM_MAGENTA      35         
#define TERM_CYAN         36         
#define TERM_WHITE        37         

static void term_color_set(int color) {
    assert(color >= 30 && color <= 37);
    printf("\033[1;%dm", color);
}
static void term_color_reset() {
    printf("\033[0m");
}

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
static int test_index = 0;

void setup_field(struct ModelView *mv, const int values[5][5]) {
    assert(mv);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            if (values[y][x])
                modelview_put_manual(mv, x, y, values[y][x]);
}

void print_field(struct ModelView *mv) {
    assert(mv);
    printf("print_field:\n");
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

static bool check_cell(
    struct ModelView *mv, int x, int y, const int reference[5][5]
) {
    if (!reference[y][x])
        return true;

    const struct Cell *cell = modelview_get_cell(mv, x, y, NULL);

    if (!cell) {
        printf("\033[1;31mcheck_field: cell == NULL\n\033[0m");
        printf(
            "check_field: reference[%d][%d] %d\n",
            y, x, reference[y][x]
        );
        print_field(mv);
        print_field5(reference);
        abort();
        return false;
    }

    if (cell->value != reference[y][x]) {
        printf(
            "\033[1;31mcheck_field: cell->value %d != reference[%d][%d] %d\n\033[0m",
            cell->value, y, x, reference[y][x]
        );
        print_field(mv);
        print_field5(reference);
        return false;
    }

    return true;
}

bool check_field(struct ModelView *mv, const int reference[5][5]) {
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            if (!check_cell(mv, x, y, reference))
                return false;
        }
    }
    print_field(mv);
    return true;
}

static void test_modelview_arr(struct TestInput input) {
    assert(input.steps);
    assert(input.steps_num >= 0);
    assert(input.field_setup);
    printf("test_modelview_arr:\n");
    struct ModelView mv = {};
    modelview_init(&mv, modelview_test_setup);
    setup_field(&mv, input.field_setup);
    print_field5(input.field_setup);

    for (int i = 0; i < input.steps_num; ++i) {

        if (input.steps[i].new_cell) {
            modelview_put_manual(
                &mv,
                input.steps[i].new_cell->x,
                input.steps[i].new_cell->y,
                input.steps[i].new_cell->value
            );
            term_color_set(TERM_BLUE);
            print_field(&mv);
            term_color_reset();
        }

        modelview_input(&mv, input.steps[i].dir);

        BeginDrawing();
        while (mv.dir != DIR_NONE) { // условие цикла DIR_NONE ?
            modelview_draw(&mv);
            usleep(1000000 / 100); // 100Hz
        }
        EndDrawing();

        if (!check_field(&mv, input.steps[i].field)) {
            printf("\033[1;31mcheck_field: step %d failed \n\033[0m", i);
            goto _exit;
        }

    }

    printf(
        "\033[1;32mtest_modelview_arr: test %d passed\n\033[0m",
        test_index++
    );
_exit:
    modelview_shutdown(&mv);
}

void test_modelviews_multiple() {

    test_modelview_arr((struct TestInput) {
        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) 
        {
            {
                .new_cell = NULL,
                .dir = DIR_NONE,
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                },
            },
        },
        .steps_num = 1,
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {1, 0, 0, 0, 4,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {2, 0, 0, 0, 3,},
        },
        .steps = (struct Step[]) 
        {
            {
                .field = {
                    {1, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {2, 0, 0, 0, 3,},
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
        .steps_num = 4,
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
            {
                .dir = DIR_RIGHT,
                .field = {
                    {0, 0, 0, 0, 1,},
                    {0, 0, 0, 0, 1,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                },
            },
            {
                .dir = DIR_DOWN,
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
            },
            {
                .dir = DIR_DOWN,
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 4,
                    .y = 1,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 5,},
                    {0, 0, 0, 0, 2,},
                },
            },
        }
    });

    test_modelview_arr((struct TestInput){
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
        },

        /*

        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },

        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },

        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 4, 0, 0, 0,},
        },

         */

        .steps_num = 4,
        .steps = (struct Step[]) {
            {
                    .dir = DIR_DOWN,
                    .field = {
                        {0, 0, 0, 0, 0,},
                        {0, 0, 0, 0, 0,},
                        {0, 0, 0, 0, 0,},
                        {0, 1, 0, 0, 0,},
                        {0, 4, 0, 0, 0,},
                    },
            },
        }
    });


    /*
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
    */

    /*
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
    */

}

