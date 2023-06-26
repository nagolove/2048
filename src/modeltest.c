#include "modeltest.h"

#include "koh_logger.h"
#include "modelview.h"
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

void modeltest_put(struct ModelTest *mb, int x, int y, int value) {
    assert(mb);
    //assert(mb->field[x][y].value != 0);
    mb->field[x][y].value = value;
}

static int find_max(struct ModelTest *mb) {
    int max = 0;
    for (int i = 0; i < mb->field_size; i++)
        for (int j = 0; j < mb->field_size; j++)
            if (mb->field[i][j].value > max)
                max = mb->field[i][j].value;

    return max;
}

static bool is_over(struct ModelTest *mb) {
    int num = 0;
    for (int i = 0; i < mb->field_size; i++)
        for (int j = 0; j < mb->field_size; j++)
            if (mb->field[i][j].value > 0)
                num++;

    return mb->field_size * mb->field_size == num;
}

// Складывает значения плиток в определенном направлении.
// Все действия попадают в очередь действий.
static void sum(
        struct ModelTest *mb,
        enum Direction dir,
        struct Cell **field_copy,
        int x, int y,
        bool *moved
) {
    // {{{
    /*
    if (i < 0 || i >= mb->field_size) {
        return;
    }
    if (j < 0 || j >= mb->field_size) {
        return;
    }
    */

    switch (dir) {
        case DIR_UP: {
            if (y > 0 && field_copy[x][y - 1].value == field_copy[x][y].value) {
                
                field_copy[x][y - 1].value = field_copy[x][y].value * 2;
                mb->scores += field_copy[x][y].value;
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_DOWN: {
            if (y + 1 < mb->field_size && field_copy[x][y + 1].value == field_copy[x][y].value) {

                field_copy[x][y + 1].value = field_copy[x][y].value * 2;
                mb->scores += field_copy[x][y].value;
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_LEFT: {
            if (x > 0 && field_copy[x - 1][y].value == field_copy[x][y].value) {

                field_copy[x - 1][y].value = field_copy[x][y].value * 2;
                mb->scores += field_copy[x][y].value;
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_RIGHT: {
            if (x + 1 < mb->field_size && field_copy[x + 1][y].value == field_copy[x][y].value) {

                field_copy[x + 1][y].value = field_copy[x][y].value * 2;
                mb->scores += field_copy[x][y].value;
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        default:
            break;
        }
    }
    // }}}
}


// Двигает плитки в определенном направлении.
// Все действия попадают в очередь действий.
static void move(
        struct ModelTest *mb,
        enum Direction dir,
        struct Cell **field_copy,
        int x, int y,
        bool *moved
) {
    // {{{
    //if (i < 0 || i >= mb->field_size || j < 0 || j >= mb->field_size) {
        //return;
    //}

    switch (dir) {
        case DIR_UP: {
            if (y > 0 && field_copy[x][y - 1].value == 0) {

                field_copy[x][y - 1] = field_copy[x][y];
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_DOWN: {
            if (y + 1 < mb->field_size && field_copy[x][y + 1].value == 0) {

                field_copy[x][y + 1] = field_copy[x][y];
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_LEFT: {
            if (x > 0 && field_copy[x - 1][y].value == 0) {

                field_copy[x - 1][y] = field_copy[x][y];
                field_copy[x][y].value = 0;
                *moved = true;
                //printf("moved horizontal left\n");
            }
            break;
        }
        case DIR_RIGHT: {
            if (x + 1 < mb->field_size && field_copy[x + 1][y].value == 0) {

                field_copy[x + 1][y] = field_copy[x][y];
                field_copy[x][y].value = 0;
                *moved = true;
            }
            break;
        default:
            break;
        }
    }
    // }}}
}

struct Cell **copy_alloc(int field_size) {
    assert(field_size > 0);
    struct Cell **arr = calloc(field_size, sizeof(arr[0]));
    assert(arr);
    for (int i = 0; i < field_size; ++i)
        arr[i] = calloc(field_size, sizeof(arr[0][0]));
    return arr;
}

void copy_free(struct Cell **arr, int field_size) {
    assert(arr);
    for (int i = 0; i < field_size; ++i)
        free(arr[i]);
    free(arr);
}

void copy_from_modeltest(struct Cell **arr, struct ModelTest *mb) {
    assert(arr);
    assert(mb);

    for (int y = 0; y < mb->field_size; ++y)
        for (int x = 0; x < mb->field_size; ++x) {
            arr[y][x] = mb->field[y][x];
        }
}

void copy_to_modeltest(struct Cell **arr, struct ModelTest *mb) {
    assert(arr);
    assert(mb);

    for (int y = 0; y < mb->field_size; ++y)
        for (int x = 0; x < mb->field_size; ++x) {
            mb->field[y][x] = arr[y][x];
        }
}

void modeltest_update(struct ModelTest *mb, enum Direction dir) {
    assert(mb);
    if (mb->state == MVS_GAMEOVER)
        return;

    mb->last_dir = dir;

    struct Cell **field_copy = copy_alloc(mb->field_size);
    //memmove(field_copy, mb->field, field_size_bytes);
    copy_from_modeltest(field_copy, mb);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int y = 0; y < mb->field_size; y++) {
            for (int x = 0; x < mb->field_size; x++) {
                if (field_copy[x][y].value == 0) continue;
                move(mb, dir, field_copy, x, y, &moved);
            }
        }

        for (int y = 0; y < mb->field_size; y++) {
            for (int x = 0; x < mb->field_size; x++) {
                if (field_copy[x][y].value == 0) continue;
                sum(mb, dir, field_copy, x, y, &moved);
            }
        }

        iter++;
        int iter_limit = mb->field_size * mb->field_size;
        if (iter > iter_limit) {
            trace("update: iter_limit was reached\n");
            exit(EXIT_FAILURE);
        }
    } while (moved);

    //memmove(mb->field, field_copy, field_size_bytes);
    copy_to_modeltest(field_copy, mb);

    if (is_over(mb))
        mb->state = MVS_GAMEOVER;

    if (find_max(mb) == WIN_VALUE)
        mb->state = MVS_WIN;

    copy_free(field_copy, mb->field_size);
}

void modeltest_init(struct ModelTest *mb, int field_size) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));

    mb->field_size = field_size;
    mb->field = calloc(field_size, sizeof(mb->field[0]));
    assert(mb->field);
    for (int j = 0; j < field_size; j++) {
        mb->field[j] = calloc(field_size, sizeof(mb->field[0][0]));
        assert(mb->field[j]);
    }

    mb->last_dir = DIR_NONE;
    mb->state = MVS_READY;
}


void modeltest_shutdown(struct ModelTest *mb) {
    assert(mb);
    if (mb->field) {
        for (int j = 0; j < mb->field_size; j++)
            free(mb->field[j]);
        free(mb->field);
        mb->field = NULL;
    }
    memset(mb, 0, sizeof(*mb));
}
