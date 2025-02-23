#pragma once

#include "genann.h"
#include "raylib.h"

typedef struct genann_view genann_view;

genann_view *genann_view_new(const char *ann_name);
void genann_view_free(genann_view *v);
void genann_view_update(genann_view *v, Vector2 mouse_point);
void genann_view_draw(genann_view *view);
void genann_print(const genann *net);
void genann_view_position_set(struct genann_view *view, Vector2 p);
void genann_view_prepare(struct genann_view *view, const genann *net);
void genann_print_run(genann const *ann);
