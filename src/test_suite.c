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

static const bool use_io_supression = false;

static void io_2null() {
    if (!use_io_supression) 
        return;
    stdout = freopen("/dev/null", "w", stdout);
    assert(stdout);
    stderr = freopen("/dev/null", "w", stderr);
    assert(stderr);
}

static void io_restore() {
    if (!use_io_supression) 
        return;
    stdout = freopen("/dev/tty", "w", stdout);
    assert(stdout);
    stderr = freopen("/dev/tty", "w", stderr);
    assert(stderr);
}

void setup_field(struct ModelView *mv, const int values[5][5]) {
    assert(mv);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            if (values[y][x])
                modelview_put_manual(mv, x, y, values[y][x]);
}

/*
void print_field(struct ModelView *mv) {
    assert(mv);
    printf("print_field:\n");
    bool prev_printing_state = _use_field_printing;
    _use_field_printing = false;

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

    _use_field_printing = prev_printing_state;
}
*/

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

    if (ctx && ctx->trap) {
        if (ctx->trap->x == x && ctx->trap->y == y)
            koh_trap();
    }

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

        modelview_field_print(mv);
        print_field5(reference);
        /*abort();*/
        return false;
    }

    if (cell->value != reference[y][x]) {
        printf(
            "\033[1;31mcheck_field: cell->value %d != reference[%d][%d] %d\n\033[0m",
            cell->value, y, x, reference[y][x]
        );
        modelview_field_print(mv);
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
    modelview_field_print(mv);
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

    io_2null();
    modelview_init(&mv, modelview_test_setup);
    io_restore();

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

            io_2null();
            modelview_put_manual(
                &mv,
                step->new_cell->x,
                step->new_cell->y,
                step->new_cell->value
            );
            io_restore();

            term_color_set(TERM_BLUE);
            printf("new cell was added\n");
            modelview_field_print(&mv);
            term_color_reset();
        }

        io_2null();

        struct TestCtx ctx = {
            .test_index = i,
            .test_suite_index = test_index,
            .name = input.name,
            .trap = step->trap,
        };

        if (ctx.trap && ctx.trap->x == -1 && ctx.trap->y == -1)
            koh_trap();

        io_restore();
        /*itoa();*/
        _FIELD_PRINT((&mv));

        io_2null();

        modelview_input(&mv, step->dir);
        BeginDrawing();
        while (mv.dir != DIR_NONE) { // условие цикла DIR_NONE ?
            modelview_draw(&mv);
            usleep(1000000 / 100); // 100Hz
        }
        EndDrawing();

        io_restore();

        if (!check_field(&mv, step->field, &ctx)) {
            printf("\033[1;31mcheck_field: step %d failed \n\033[0m", i);
            goto _exit;
        }

    }

    printf(
        "\033[1;32mtest_modelview_arr: test %d \"%s\" passed\n\033[0m",
        test_index++, input.name 
    );
_exit:
    io_2null();
    modelview_shutdown(&mv);
    io_restore();
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
                    {04,  8, 00, 00, 00,},
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
                    {00, 00, 00, 04,  8,},
                },
                .last = true,
            },
        },
        .steps_num = 4,
    });

    /*
    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо, укороченный случай",
        .field_setup = {
            {4, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 2,},
            {0, 0, 0, 0, 0,},
            {2, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .field = {
                    {0, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
                .dir = DIR_RIGHT, 
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 2,
                    .value =2,
                },
                .field = {
                    {0, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 2,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 1,
                    .y = 1,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 3,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .msg = "двойка на позиции (4, 3) исчезает",
                .trap = &(struct Pair) {
                    .x = 4,
                    .y = 3,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 7,
    });
    // */

    /*
    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо, укороченный случай",
        .field_setup = {
            {0, 0, 0, 0, 4,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 2,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 2,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 2,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 2,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 1,
                    .y = 1,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 3,
                    .value = 2,
                },
                .msg = "trap ловушка",
                .trap = &(struct Pair) {
                    .x = -1,
                    .y = -1,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .msg = "двойка на позиции (4, 3) исчезает",
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 6,
    });
    //*/

    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо, укороченный случай",
        .field_setup = {
            {0, 0, 0, 0, 8,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 4,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 4,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 2,
                    .value = 4,
                },
                .field = {
                    {0, 0, 0, 0, 8,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 4, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 4,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 04, 16,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 1,
                    .y = 1,
                    .value = 4,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 04, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 04, 16,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 04, 00, 04, 16,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 3,
                    .value = 4,
                },
                .msg = "trap ловушка",
                .trap = &(struct Pair) {
                    .x = -1,
                    .y = -1,
                },
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00,  4, 00,},
                    {00,  4, 00,  4, 16,},
                },
                .dir = DIR_NONE,
            },
            {
                .msg = "двойка на позиции (4, 3) исчезает",
                /*
                .trap = &(struct Pair) {
                    .x = 4,
                    .y = 3,
                },
                */
                .field = {
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00, 00,},
                    {00, 00, 00, 00,  4,},
                    {00, 00, 00,  8, 16,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 6,
    });
    // */
    /*
    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо",
        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 4,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                },
                .dir = DIR_NONE,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 0, 
                    .y = 0,
                    .value = 4,
                },
                .field = {
                    {4, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {4, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {2, 0, 0, 0, 0,},
                },
                .dir = DIR_LEFT,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 4,
                    .y = 2,
                    .value = 2,
                },
                .field = {
                    {4, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 0, 0,},
                    {2, 0, 0, 0, 0,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
                .dir = DIR_RIGHT, 
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 2,
                    .value =2,
                },
                .field = {
                    {0, 0, 0, 0, 4,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 2,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 1,
                    .y = 1,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_DOWN,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 3,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .msg = "двойка на позиции (4, 3) исчезает",
                .trap = &(struct Pair) {
                    .x = 4,
                    .y = 3,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 11,
    });
    // */

    /*
    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо"
                ", попытка изменить поведение, поле выставлено сразу",
        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 2, 0,},
            {0, 2, 0, 2, 8,},
        },
        .steps = (struct Step[]) {
            //////
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 2, 0, 2, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 2,
    });
    // */

    /*
    test_modelview_arr((struct TestInput){
        .name = "плитка пропадает при движении вправо"
                ", попытка изменить поведение, поле выставлено по шагам",
        .field_setup = {
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
            {0, 0, 0, 0, 0,},
        },
        .steps = (struct Step[]) {
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 3,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 0, 0, 0, 0,},
                },
                .dir = DIR_NONE,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 4,
                    .y = 4,
                    .value = 8,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 0, 0, 0, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 3,
                    .y = 4,
                    .value = 4,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .new_cell = &(struct Cell) {
                    .x = 1,
                    .y = 4,
                    .value = 2,
                },
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 2, 0,},
                    {0, 2, 0, 4, 8,},
                },
                .dir = DIR_NONE,
            },
            {
                .field = {
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 0,},
                    {0, 0, 0, 0, 2,},
                    {0, 0, 0, 4, 8,},
                },
                .dir = DIR_RIGHT,
                .last = true,
            },
        },
        .steps_num = 5,
    });
    // */

}

