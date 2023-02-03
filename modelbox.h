#pragma once

#include "raylib.h"
#include <stdbool.h>

#define field_size 4

enum Direction {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
};

struct ModelBox {
    // x:y
    int  field[field_size][field_size];
    int  scores;
    bool gameover;
    void (*update)(struct ModelBox *mb, enum Direction dir);
    //void (*reset)(struct ModelBox *mb);
};

struct ModelView {
    int     sorted[field_size * field_size];
    Vector2 pos;
    void    (*draw)(struct ModelView *mv, struct ModelBox *mb);
};

void modelbox_init(struct ModelBox *mb);
void modelview_init(struct ModelView *mv, Vector2 *pos);

