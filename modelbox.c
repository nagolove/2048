#include "modelbox.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static const int quad_width = 128 + 64;

static Color colors[5] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    GRAY,
};

static bool is_over(struct ModelBox *mb) {
    int num = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++)
            if (mb->field[i][j] > 0)
                num++;

    printf("is_over: %d\n", field_size * field_size == num);
    return field_size * field_size == num;
}

static void put(struct ModelBox *mb) {
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

    if (!is_over(mb))
        put(mb);
    else
        mb->gameover = true;
}

/*
static void reset(struct ModelBox *mb) {
    assert(mb);
    mb->gameover = false;
    memset(mb->field, 0, sizeof(mb->field[0][0]) * field_size * field_size);
    mb->scores = 0;
}
*/

void modelbox_init(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    put(mb);
    mb->update = update;
    //mb->reset = reset;
}

static int cmp(const void *pa, const void *pb) {
    const int *a = pa, *b = pb;
    return *a < *b;
}

static void sort_numbers(struct ModelView *mv, struct ModelBox *mb) {
    int tmp[field_size * field_size] = {0};
    int idx = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++)
            tmp[idx++] = mb->field[j][i];

    qsort(tmp, field_size * field_size, sizeof(mb->field[0][0]), cmp);
    memmove(mv->sorted, tmp, sizeof(mv->sorted));
}

static void draw_field(struct ModelView *mv) {
    const int field_width = field_size * quad_width;
    Vector2 start = mv->pos;
    const float thick = 3.;

    Vector2 tmp = start;
    for (int u = 0; u <= field_size; u++) {
        Vector2 end = tmp;
        end.y += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.x += quad_width;
    }

    tmp = start;
    for (int u = 0; u <= field_size; u++) {
        Vector2 end = tmp;
        end.x += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.y += quad_width;
    }
}

static Color get_color(struct ModelView *mv, int value) {
    int colors_num = sizeof(colors) / sizeof(colors[0]);
    /*printf("colors_num %d\n", colors_num);*/
    for (int k = 0; k < colors_num; k++) {
        if (value == mv->sorted[k]) {
            return colors[k];
        }
    }
    return colors[colors_num - 1];
}

static void draw_numbers(struct ModelView *mv, struct ModelBox *mb) {
    assert(mv);
    assert(mb);
    int fontsize = 90;
    float spacing = 2.;
    //const int field_width = field_size * quad_width;
    Vector2 start = mv->pos;
    for (int i = 0; i < field_size; i++) {
        for (int j = 0; j < field_size; j++) {
            if (mb->field[j][i] == 0)
                continue;

            char msg[64] = {0};
            sprintf(msg, "%d", mb->field[j][i]);
            int textw = 0;

            do {
                textw = MeasureTextEx(GetFontDefault(), msg, fontsize--, spacing).x;
                /*printf("fontsize %d\n", fontsize);*/
            } while (textw > quad_width);

            Vector2 pos = start;
            pos.x += j * quad_width + (quad_width - textw) / 2.;
            pos.y += i * quad_width + (quad_width - fontsize) / 2.;
            Color color = get_color(mv, mb->field[j][i]);
            DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);
        }
    }
}

static void draw(struct ModelView *mv, struct ModelBox *mb) {
    assert(mv);
    assert(mb);
    sort_numbers(mv, mb);
    draw_field(mv);
    draw_numbers(mv, mb);
}

void modelview_init(struct ModelView *mv, Vector2 *pos) {
    assert(mv);
    const int field_width = field_size * quad_width;
    if (!pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *pos;
    mv->draw = draw;
}

