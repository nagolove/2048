#pragma once

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
    int field[field_size][field_size];
    int scores;
    bool gameover;

    void (*update)(struct ModelBox *mb, enum Direction dir);
};

void modelbox_init(struct ModelBox *mb);

