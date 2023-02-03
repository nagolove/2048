#include "modelbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static bool is_over(struct ModelBox *mb) {
    int num = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++)
            if (mb->field[i][j] > 0)
                num++;

    printf("is_over: %d\n", field_size * field_size == num);
    return field_size * field_size == num;
}

void put(struct ModelBox *mb) {
    assert(mb);
    int x = rand() % field_size;
    int y = rand() % field_size;

    while (mb->field[x][y] != 0) {
        x = rand() % field_size;
        y = rand() % field_size;
    }

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9)
        mb->field[x][y] = 2;
    else 
        mb->field[x][y] = 4;
}

static void update(struct ModelBox *mb, enum Direction dir) {
    assert(mb);
    if (mb->gameover)
        return;

    const int field_size_bytes = sizeof(mb->field[0][0]) * field_size * field_size;
    int field_copy[field_size][field_size] = {0};
    memmove(field_copy, mb->field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] == 0) continue;

                switch (dir) {
                    case DIR_UP: {
                        if (i > 0 && field_copy[j][i - 1] == 0) {
                            field_copy[j][i - 1] = field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("moved vertical up\n");
                        }
                        break;
                    }
                    case DIR_DOWN: {
                        if (i + 1 < field_size && field_copy[j][i + 1] == 0) {
                            field_copy[j][i + 1] = field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("moved vertical down\n");
                        }
                        break;
                    }
                    case DIR_LEFT: {
                        if (j > 0 && field_copy[j - 1][i] == 0) {
                            field_copy[j - 1][i] = field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("moved horizontal left\n");
                        }
                        break;
                    }
                    case DIR_RIGHT: {
                        if (j + 1 < field_size && field_copy[j + 1][i] == 0) {
                            field_copy[j + 1][i] = field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("moved horizontal right\n");
                        }
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] == 0) 
                    continue;

                switch (dir) {
                    case DIR_UP: {
                        if (i > 0 && field_copy[j][i - 1] == field_copy[j][i]) {
                            field_copy[j][i - 1] = field_copy[j][i] * 2;
                            mb->scores += field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("summarized vertical up\n");
                        }
                        break;
                    }
                    case DIR_DOWN: {
                        if (i + 1 < field_size && 
                            field_copy[j][i + 1] == field_copy[j][i]) {

                            field_copy[j][i + 1] = field_copy[j][i] * 2;
                            mb->scores += field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("summarized vertical down\n");
                        }
                        break;
                    }
                    case DIR_LEFT: {
                        if (j > 0 && field_copy[j - 1][i] == field_copy[j][i]) {
                            field_copy[j - 1][i] = field_copy[j][i] * 2;
                            mb->scores += field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("summarized horizontal left\n");
                        }
                        break;
                    }
                    case DIR_RIGHT: {
                        if (j + 1 < field_size &&
                            field_copy[j + 1][i] == field_copy[j][i]) {

                            field_copy[j + 1][i] = field_copy[j][i] * 2;
                            mb->scores += field_copy[j][i];
                            field_copy[j][i] = 0;
                            moved = true;
                            printf("summarized horizontal right\n");
                        }
                        break;
                    }
                }
            }
        }

        iter++;
        printf("iter %d\n", iter);
    } while (moved);

    memmove(mb->field, field_copy, field_size_bytes);
}


void modelbox_init(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    mb->update = update;
}


