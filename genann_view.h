#pragma once

#include "genann.h"
#include "raylib.h"

typedef struct genann_view genann_view;

genann_view *genann_view_new();
void genann_view_free(genann_view *v);
void genann_view_draw(genann_view *view);
void genann_print(const genann *net);
void genann_view_position_set(struct genann_view *view, Vector2 p);
void genann_view_prepare(struct genann_view *view, const genann *net);

