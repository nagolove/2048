#pragma once

#include "genann.h"
#include "raylib.h"

struct genann_view {
    float   neuron_radius;
    Vector2 position;
    Color   neuron_color;
};

struct genann_view *genann_view_new();
void genann_view_free(struct genann_view *v);
void genann_view_draw(struct genann_view *view, genann *net);
void genann_print(const genann *net);

