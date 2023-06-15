#include "model.h"

#include "koh_logger.h"
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

void modelbox_put(struct ModelBox *mb, int x, int y, int value) {
    assert(mb);
    //assert(mb->field[x][y].value != 0);
    mb->field[x][y].value = value;
}

static int find_max(struct ModelBox *mb) {
    int max = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (mb->field[i][j].value > max)
                max = mb->field[i][j].value;

    return max;
}

static bool is_over(struct ModelBox *mb) {
    int num = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (mb->field[i][j].value > 0)
                num++;

    return FIELD_SIZE * FIELD_SIZE == num;
}

// Складывает значения плиток в определенном направлении.
// Все действия попадают в очередь действий.
static void sum(
        struct ModelBox *mb,
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int x, int y,
        bool *moved
) {
    // {{{
    /*
    if (i < 0 || i >= FIELD_SIZE) {
        return;
    }
    if (j < 0 || j >= FIELD_SIZE) {
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
            if (y + 1 < FIELD_SIZE && field_copy[x][y + 1].value == field_copy[x][y].value) {

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
            if (x + 1 < FIELD_SIZE && field_copy[x + 1][y].value == field_copy[x][y].value) {

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
        struct ModelBox *mb,
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int x, int y,
        bool *moved
) {
    // {{{
    //if (i < 0 || i >= FIELD_SIZE || j < 0 || j >= FIELD_SIZE) {
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
            if (y + 1 < FIELD_SIZE && field_copy[x][y + 1].value == 0) {

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
            if (x + 1 < FIELD_SIZE && field_copy[x + 1][y].value == 0) {

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


void modelbox_update(struct ModelBox *mb, enum Direction dir) {
    assert(mb);
    if (mb->state == MVS_GAMEOVER)
        return;

    mb->last_dir = dir;

    const int field_size_bytes = sizeof(mb->field[0][0]) * 
                                 FIELD_SIZE * FIELD_SIZE;
    struct Cell field_copy[FIELD_SIZE][FIELD_SIZE] = {0};
    memmove(field_copy, mb->field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int y = 0; y < FIELD_SIZE; y++) {
            for (int x = 0; x < FIELD_SIZE; x++) {
                if (field_copy[x][y].value == 0) continue;
                move(mb, dir, field_copy, x, y, &moved);
            }
        }

        for (int y = 0; y < FIELD_SIZE; y++) {
            for (int x = 0; x < FIELD_SIZE; x++) {
                if (field_copy[x][y].value == 0) continue;
                sum(mb, dir, field_copy, x, y, &moved);
            }
        }

        iter++;
        int iter_limit = FIELD_SIZE * FIELD_SIZE;
        if (iter > iter_limit) {
            trace("update: iter_limit was reached\n");
            exit(EXIT_FAILURE);
        }
    } while (moved);

    memmove(mb->field, field_copy, field_size_bytes);

    if (is_over(mb))
        mb->state = MVS_GAMEOVER;

    if (find_max(mb) == WIN_VALUE)
        mb->state = MVS_WIN;
}

void modelbox_init(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    mb->last_dir = DIR_NONE;
    mb->state = MVS_READY;
}


void modelbox_shutdown(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
}
