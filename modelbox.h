#pragma once

#include "raylib.h"
#include <stdbool.h>

#define FIELD_SIZE  4
#define WIN_VALUE   2048

enum Direction {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
};

enum ModelState {
    MS_PROCESS,
    MS_WIN,
    MS_GAMEOVER,
};

struct ModelBox {
    // x:y
    int  field[FIELD_SIZE][FIELD_SIZE];
    int  scores;
    enum ModelState state;
    void (*update)(struct ModelBox *mb, enum Direction dir);
};

struct ModelView {
    int     field_prev[FIELD_SIZE][FIELD_SIZE];
    int     sorted[FIELD_SIZE * FIELD_SIZE];
    Vector2 pos;
    void    (*draw)(struct ModelView *mv, struct ModelBox *mb);
};

void modelbox_init(struct ModelBox *mb);
void modelview_init(struct ModelView *mv, Vector2 *pos, struct ModelBox *mb);

