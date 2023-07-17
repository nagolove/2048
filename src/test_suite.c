// vim: set colorcolumn=85
// vim: fdm=marker

#include "test_suite.h"

#include "koh_common.h"
#include "modelview.h"
#include "raylib.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
            const struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
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
    struct ModelView *mv, int x, int y, const int reference[5][5],
    const struct TestCtx *ctx
) {
    if (!reference[y][x])
        return true;

    const struct Cell *cell = modelview_get_cell(mv, x, y, NULL);

    if (!cell) {
        if (ctx) 
            printf(
                    "\033[1;31mcheck_field: suite \"%s\", %d, step %d"
                    ", cell == NULL\n\033[0m",
                    ctx->name, ctx->test_suite_index, ctx->test_index
            );
        else 
            printf("\033[1;31mcheck_field: cell == NULL\n\033[0m");

        printf(
            "check_field: reference[%d][%d] %d\n",
            y, x, reference[y][x]
        );

        print_field(mv);
        print_field5(reference);
        /*abort();*/
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

bool check_field(
    struct ModelView *mv, const int reference[5][5], const struct TestCtx *ctx
) {
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            if (!check_cell(mv, x, y, reference, ctx))
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
    printf(
        "\033[1;35test_modelview_arr: \"%s\" %d, steps num %d\n\033[m",
        input.name, test_index, input.steps_num
    );
    struct ModelView mv = {};
    modelview_init(&mv, modelview_test_setup);
    setup_field(&mv, input.field_setup);
    print_field5(input.field_setup);

    if (!input.steps[input.steps_num - 1].last) {
        printf("test_modelview_arr: last step is false\n");
        koh_trap();
    }

    for (int i = 0; i < input.steps_num; ++i) {

        struct Step *step = &input.steps[i];

        if (step->msg)
            printf("step '%s'\n", step->msg);

        if (step->new_cell) {
            modelview_put_manual(
                &mv,
                step->new_cell->x,
                step->new_cell->y,
                step->new_cell->value
            );
            term_color_set(TERM_BLUE);
            print_field(&mv);
            term_color_reset();
        }

        modelview_input(&mv, step->dir);

        BeginDrawing();
        while (mv.dir != DIR_NONE) { // условие цикла DIR_NONE ?
            modelview_draw(&mv);
            usleep(1000000 / 100); // 100Hz
        }
        EndDrawing();

        struct TestCtx ctx = {
            .test_index = i,
            .test_suite_index = test_index,
            .name = input.name,
        };

        if (!check_field(&mv, step->field, &ctx)) {
            printf("\033[1;31mcheck_field: step %d failed \n\033[0m", i);
            goto _exit;
        }

    }

    printf(
        "\033[1;32mtest_modelview_arr: test \"%s\" %d passed\n\033[0m",
        input.name, test_index++
    );
_exit:
    modelview_shutdown(&mv);
}

void test_modelviews_multiple() {

    // test 0
    test_modelview_arr((struct TestInput) {
        .name = "пустое поле",
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
                .last = true,
            },
        },
        .steps_num = 1,
    });

    // test 1
    test_modelview_arr((struct TestInput){
        .name = "статичная проверка поля",
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
                .last = true,
            },
        },
        .steps_num = 1,
    });

    // test 2
    test_modelview_arr((struct TestInput){
        .name = "одно движение вниз",
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .dir = DIR_UP,
                .field = {
                    {0, 2, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                },
                .last = true,
            },
        },
        .steps_num = 1,
    });

    // test 3
    test_modelview_arr((struct TestInput){
        .name = "серия",
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
                .last = true,
            },
        }
    });

    // test 4
    test_modelview_arr((struct TestInput){
        .name = "неправильное схлопывание",
        .field_setup = {
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
            {0, 1, 0, 0, 0,},
        },

        /* {{{
        // Предполагаемый желательный порядок

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

        }}} */

        .steps_num = 1,
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
                    .last = true,
            },
        }
    });

    // test 5
    test_modelview_arr((struct TestInput){
        .name = "схлопывание с появлением",
        .field_setup = {
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 4,
                    .y = 1,
                },
                .dir = DIR_DOWN,
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 8, 0, 0, 5,},
                },
            },
            {
                .dir = DIR_DOWN,
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 4,
                    .y = 0,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00,  8, 00, 00, 10,},
                },
            },
            {
                .dir = DIR_DOWN,
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 3,
                    .y = 0,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00,  8, 00,  5, 10,},
                },
            },
            {
                .dir = DIR_RIGHT,
                /*
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 4,
                    .y = 1,
                },
                */
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00,  8,  5, 10,},
                },
                .last = true,
            },
        },
        .steps_num = 4,
    });

    // test 6
    test_modelview_arr((struct TestInput){
        .name = "еще одна серия с появлениями",
        .field_setup = {
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
            {0, 2, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .value = 1,
                    .x = 0,
                    .y = 0,
                },
                .dir = DIR_DOWN,
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {1, 8, 0, 0, 0,},
                },
            },
            {
                .dir = DIR_DOWN,
                .new_cell = &(struct Cell) {
                    .value = 1,
                    .x = 0,
                    .y = 0,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {02,  8, 00, 00, 00,},
                },
            },
            {
                .dir = DIR_DOWN,
                .new_cell = &(struct Cell) {
                    .value = 2,
                    .x = 0,
                    .y = 0,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {04,  8, 00,  5, 00,},
                },
            },
            {
                .dir = DIR_RIGHT,
                /*
                .new_cell = &(struct Cell) {
                    .value = 5,
                    .x = 4,
                    .y = 1,
                },
                */
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 04,  8,  5, 10,},
                },
                .last = true,
            },
        },
        .steps_num = 4,
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
        }
        },
        .steps_num = 1,
    });
    */

}

