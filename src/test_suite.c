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

/*
static struct Setup modelview_test_setup = {
    //.pos            = NULL,
    .cam            = NULL,
    .field_size     = 5,
    .tmr_block_time = 0.01,
    .tmr_put_time   = 0.01,
    .use_gui        = false,
    .auto_put       = false,
    .use_bonus      = false,
};
static int test_index = 0;
*/

/*static const bool use_io_supression = false;*/

/*
static void io_2null() {
    if (!use_io_supression) 
        return;
    stdout = freopen("/dev/null", "w", stdout);
    assert(stdout);
    stderr = freopen("/dev/null", "w", stderr);
    assert(stderr);
}
*/

/*
static void io_restore() {
    if (!use_io_supression) 
        return;
    stdout = freopen("/dev/tty", "w", stdout);
    assert(stdout);
    stderr = freopen("/dev/tty", "w", stderr);
    assert(stderr);
}
*/

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

        //modelview_field_print(mv);
        print_field5(reference);
        /*abort();*/
        return false;
    }

    if (cell->value != reference[y][x]) {
        printf(
            "\033[1;31mcheck_field: cell->value %d != reference[%d][%d] %d\n\033[0m",
            cell->value, y, x, reference[y][x]
        );
        /*modelview_field_print(mv);*/
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
    /*modelview_field_print(mv);*/
    return true;
}


