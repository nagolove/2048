#pragma once

#include "modelview.h"

struct ModelTest {
    int                 scores, field_size;
    struct Cell         **field;
    enum ModelViewState state;
    enum Direction      last_dir;
};

void modeltest_init(struct ModelTest *mb, int field_size);
void modeltest_shutdown(struct ModelTest *mb);
void modeltest_put(struct ModelTest *mb, int x, int y, int value);
void modeltest_update(struct ModelTest *mb, enum Direction dir);
