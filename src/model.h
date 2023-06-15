#pragma once

#include "modelbox.h"

typedef struct Cell Field[FIELD_SIZE][FIELD_SIZE];

struct ModelBox {
    int                 scores;
    Field               field;
    enum ModelViewState state;
    enum Direction      last_dir;
};

void modelbox_init(struct ModelBox *mb);
void modelbox_shutdown(struct ModelBox *mb);
void modelbox_put(struct ModelBox *mb, int x, int y, int value);
void modelbox_update(struct ModelBox *mb, enum Direction dir);
