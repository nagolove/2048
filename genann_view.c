#include "genann_view.h"

#include "genann.h"
#include <assert.h>
#include <raylib.h>
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>

struct genann_view *genann_view_new() {
    struct genann_view *view = calloc(1, sizeof(*view));
    assert(view);
    view->neuron_radius = 10.;
    view->neuron_color = BLUE;
    return view;
}

void genann_view_free(struct genann_view *v) {
    assert(v);
    free(v);
}

void genann_view_draw(struct genann_view *view, genann *net) {
    assert(view);
    if (!net)
        return;

    Vector2 corner;
    double const *weight = net->weight;

    corner = view->position;
    for (int i = 0; i < net->inputs; i++) {
        DrawCircleV(corner, view->neuron_radius, view->neuron_color);
        corner.y += view->neuron_radius * 3.;
        /*char text[128] = {0};*/
        /*sprintf(text, "%.4f", net->inputs*/
        /*DrawText(text, corner.x, corner.y, view->neuron_radius, RED);*/
    }

    Vector2 position = view->position;
    Vector2 right_shift = { view->neuron_radius * 10., 0. };
    for (int i = 0; i < net->hidden_layers; i++) {
        position = Vector2Add(position, right_shift);
        corner = position;
        for (int j = 0; j < net->hidden; j++) {
            DrawCircleV(corner, view->neuron_radius, view->neuron_color);
            corner.y += view->neuron_radius * 3.;
        }
    }

    corner = Vector2Add(position, right_shift);
    for (int i = 0; i < net->outputs; i++) {
        DrawCircleV(corner, view->neuron_radius, view->neuron_color);
        corner.y += view->neuron_radius * 3.;
    }
}

void genann_print(const genann *net) {
    assert(net);
    printf("genann_print:\n");
    printf("inputs: %d\n", net->inputs);
    printf("hidden_layers: %d\n", net->hidden_layers);
    printf("hidden: %d\n", net->hidden);
    printf("outputs: %d\n", net->outputs);
    printf("total_weights: %d\n", net->total_weights);
    printf("total_neurons: %d\n", net->total_neurons);
}
