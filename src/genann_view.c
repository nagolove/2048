#include "genann_view.h"

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_structs.h"
#include "chipmunk/chipmunk_types.h"
#include "genann.h"
#include "koh_common.h"
#include "koh_routine.h"
#include "koh_table.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ANN_NAME    48

struct NeuronInfo {
    char   name[50];
    cpBody *body;
    int    layer, index;
    double weight;
};

struct NeuronLinks {
    // Номер слоя, номер нейрона
    /*int layer, index;*/
    int    num;
    double *weights;
};

struct genann_view {
    char    name[MAX_ANN_NAME];
    float   neuron_radius;
    Vector2 position;
    Color   neuron_color;
    cpSpace *space;
    // Хэштаблица, хранит имена нейронов по типу "[номер_слоя,номер_нейрона]"
    HTable  *h_name2physobj, *h_name2weight;
    genann  *ann;
    struct NeuronInfo *current_neuron;
};

static void link_free(const void *key, int key_len, void *data, int data_len) {
    struct NeuronLinks *nl = data;
    printf("link_free: %s, %p\n", (char*)key, data);
    if (nl->weights) {
        free(nl->weights);
        nl->weights = NULL;
    }
}

struct genann_view *genann_view_new(const char *ann_name) {
    struct genann_view *view = calloc(1, sizeof(*view));
    assert(view);
    strncpy(view->name, ann_name, MAX_ANN_NAME);
    view->neuron_radius = 10.;
    view->neuron_color = BLUE;
    view->position = (Vector2) { 0., 0., };
    view->space = cpSpaceNew();
    view->h_name2weight = htable_new(&(struct HTableSetup) {
        .cap = 256, .on_remove = link_free,
    });
    view->h_name2physobj = htable_new(NULL);
    return view;
}

void genann_view_position_set(struct genann_view *view, Vector2 p) {
    assert(view);
    view->position = p;
}

// {{{ Chipnumnk shutdown iterators
void ShapeFreeWrap(cpSpace *space, cpShape *shape, void *unused){
    cpSpaceRemoveShape(space, shape);
    cpShapeFree(shape);
}

void PostShapeFree(cpShape *shape, cpSpace *space){
    cpSpaceAddPostStepCallback(space, (cpPostStepFunc)ShapeFreeWrap, shape, NULL);
}

void ConstraintFreeWrap(cpSpace *space, cpConstraint *constraint, void *unused){
    cpSpaceRemoveConstraint(space, constraint);
    cpConstraintFree(constraint);
}

void PostConstraintFree(cpConstraint *constraint, cpSpace *space){
    cpSpaceAddPostStepCallback(space, (cpPostStepFunc)ConstraintFreeWrap, constraint, NULL);
}

void BodyFreeWrap(cpSpace *space, cpBody *body, void *unused){
    cpSpaceRemoveBody(space, body);
    free(body->userData);
    cpBodyFree(body);
}

static void PostBodyFree(cpBody *body, cpSpace *space){
    cpSpaceAddPostStepCallback(space, (cpPostStepFunc)BodyFreeWrap, body, NULL);
}
// }}}

void genann_view_free(struct genann_view *v) {
    assert(v);
    htable_free(v->h_name2weight);
    htable_free(v->h_name2physobj);
    cpSpaceEachShape(
            v->space, 
            (cpSpaceShapeIteratorFunc)PostShapeFree, 
            v->space
    );
    cpSpaceEachConstraint(
            v->space, 
            (cpSpaceConstraintIteratorFunc)PostConstraintFree, 
            v->space
    );
    cpSpaceEachBody(
            v->space, (cpSpaceBodyIteratorFunc)PostBodyFree, 
            v->space
    );
    space_shutdown((struct SpaceShutdownCtx){
        .space = v->space,
        .free_bodies = true,
        .free_shapes = true,
        .free_constraints = true,
    });
    cpSpaceFree(v->space);
    if (v->ann)
        genann_free(v->ann);
    memset(v, 0, sizeof(*v));
    free(v);
}

static void *add_neuron_info(
    struct genann_view *view, Vector2 place, int index, int layer
) {
    assert(view);
    float mass = 100.;
    float moment = cpMomentForCircle(
        mass,
        view->neuron_radius, 
        view->neuron_radius, 
        cpvzero
    );
    cpBody *body = cpBodyNew(mass, moment);
    cpShape *shape = cpCircleShapeNew(body, view->neuron_radius, cpvzero);

    cpSpaceAddBody(view->space, body);
    cpSpaceAddShape(view->space, shape);

    struct NeuronInfo *ni = calloc(1, sizeof(struct NeuronInfo));
    sprintf(ni->name, "[%d,%d]", index, layer);
    ni->body = body;
    ni->layer = layer;
    ni->index = index;

    body->userData = ni;
    cpBodySetPosition(body, from_Vector2(place));

    htable_add_s(view->h_name2physobj, ni->name, &body, sizeof(void*));

    return body;
}

static void neuron_links_init(struct NeuronLinks *nl, int num) {
    assert(nl);
    nl->num = num;
    nl->weights = calloc(num, sizeof(double));
}

static HTableAction iter_neuron_link(
    const void *key, int key_len, void *value, int value_len,
    void *udata
) {
    struct NeuronLinks *nl = value;
    printf("iter_neuron_link: %s, %d\n", (char*)key, nl->num);
    return HTABLE_ACTION_NEXT;
}

static HTableAction iter_neuron_info(
    const void *key, int key_len, void *value, int value_len,
    void *udata
) {
    //struct NeuronInfo *ni = value;
    cpBody *b = value;
    printf("iter_neuron_info: %s, %p\n", (char*)key, b);
    return HTABLE_ACTION_NEXT;
}

double *genann_weights_input(genann *ann) {
    assert(ann);
    assert(ann->inputs > 0);
    return ann->weight;
}

double *genann_weights_hidden(genann *ann, int layer) {
    assert(ann);
    assert(layer >= 0 && layer < ann->hidden_layers);
    /*return ann->weight + ann->inputs + ann->hidden * layer;*/
    return ann->weight + ann->hidden + ann->hidden * layer;
}

double *genann_weights_output(genann *ann) {
    assert(ann);
    assert(ann->outputs > 0);
    return ann->weight + ann->hidden + ann->hidden * ann->hidden_layers;
}

void genann_viewer_fill_hash(genann_view *view, genann const *ann) {
    if (view->ann) 
        genann_free(view->ann);
    view->ann = genann_copy(ann);

    double const *w = ann->weight;
    int h, j, k;

    /*
    if (!ann->hidden_layers) {
        double *ret = o;
        for (j = 0; j < ann->outputs; ++j) {
            double sum = *w++ * -1.0;
            for (k = 0; k < ann->inputs; ++k) {
                sum += *w++ * i[k];
            }
            // *o++ = genann_act_output(ann, sum);
        }

        return ret;
    }
    */
    char key[64] = {0};
    int layer = 0;

    printf("input:\n");
    /* Figure input layer */
    for (j = 0; j < ann->hidden; ++j) {
        struct NeuronLinks nl = {0};
        neuron_links_init(&nl, ann->inputs);
        w++;
        for (k = 0; k < ann->inputs; ++k) {
            nl.weights[k] = *w;
            w++;
        }
        ++layer;
        /*sprintf(key, "[%d,%d]", j);*/
        htable_add_s(view->h_name2weight, key, &nl, sizeof(nl));
    }
    printf("\n");

    printf("hidden:\n");

    /* Figure hidden layers, if any. */
    for (h = 1; h < ann->hidden_layers; ++h) {
        for (j = 0; j < ann->hidden; ++j) {
            struct NeuronLinks nl = {0};
            neuron_links_init(&nl, ann->hidden);
            w++;
            for (k = 0; k < ann->hidden; ++k) {
                nl.weights[k] = *w;
                w++;
            }
            ++layer;
            sprintf(key, "hidden[%d,%d]", h, j);
            htable_add_s(view->h_name2weight, key, &nl, sizeof(nl));
        }
    }

    printf("output:\n");
    /* Figure output layer. */
    for (j = 0; j < ann->outputs; ++j) {
        printf("w %f\n", *w);
        struct NeuronLinks nl = {0};
        neuron_links_init(&nl, ann->hidden);
        w++;
        //double sum = *w++ * -1.0;
        for (k = 0; k < ann->hidden; ++k) {
            sprintf(key, "output[%d,%d]", j, k);
            htable_add_s(view->h_name2weight, key, &nl, sizeof(nl));
            w++;
        }
        printf("\n");
    }

    /* Sanity check that we used all weights and wrote all outputs. */
    assert(w - ann->weight == ann->total_weights);
    //assert(o - ann->output == ann->total_neurons);
    htable_print(view->h_name2weight);
    /*htable_each(view->h_name2weight, iter_neuron_link, NULL);*/
}

void genann_print_run(genann const *ann) {
    double const *w = ann->weight;
    int h, j, k;

    /*
    if (!ann->hidden_layers) {
        double *ret = o;
        for (j = 0; j < ann->outputs; ++j) {
            double sum = *w++ * -1.0;
            for (k = 0; k < ann->inputs; ++k) {
                sum += *w++ * i[k];
            }
            // *o++ = genann_act_output(ann, sum);
        }

        return ret;
    }
    */

    printf("input:\n");
    /* Figure input layer */
    for (j = 0; j < ann->hidden; ++j) {
        printf("\nw [%d] %f\n", j, *w);
        w++;
        for (k = 0; k < ann->inputs; ++k) {
            printf(" %-8.4f", *w);
            w++;
        }
    }
    printf("\n");

    printf("hidden:\n");

    /* Figure hidden layers, if any. */
    for (h = 1; h < ann->hidden_layers; ++h) {
        printf("w [%d] %f\n", h, *w);
        for (j = 0; j < ann->hidden; ++j) {
            w++;
            for (k = 0; k < ann->hidden; ++k) {
                printf(" %-8.4f", *w);
                w++;
            }
            printf("\n");
        }

    }

    printf("output:\n");
    /* Figure output layer. */
    for (j = 0; j < ann->outputs; ++j) {
        printf("w %f\n", *w);
        w++;
        //double sum = *w++ * -1.0;
        for (k = 0; k < ann->hidden; ++k) {
            printf(" %-8.4f", *w);
            w++;
        }
        printf("\n");
    }

    /* Sanity check that we used all weights and wrote all outputs. */
    assert(w - ann->weight == ann->total_weights);
    //assert(o - ann->output == ann->total_neurons);
}

void make_chipmunk_objects(struct genann_view *view, const genann *net) {
    Vector2 corner;
    //double const *weight = net->weight;
    int layer = 0;
    const float shift_coef = 4.;

    corner = view->position;
    for (int i = 0; i < net->inputs; i++) {
        //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

        add_neuron_info(view, corner, i, layer);

        corner.y += view->neuron_radius * shift_coef;
        /*char text[128] = {0};*/
        /*sprintf(text, "%.4f", net->inputs*/
        /*DrawText(text, corner.x, corner.y, view->neuron_radius, RED);*/
    }

    layer++;

    Vector2 position = view->position;
    Vector2 right_shift = { view->neuron_radius * 10., 0. };
    for (int i = 0; i < net->hidden_layers; i++) {
        position = Vector2Add(position, right_shift);
        corner = position;
        for (int j = 0; j < net->hidden; j++) {
            //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

            add_neuron_info(view, corner, j, layer);

            corner.y += view->neuron_radius * shift_coef;
        }
        ++layer;
    }

    corner = Vector2Add(position, right_shift);
    for (int i = 0; i < net->outputs; i++) {
        //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

        add_neuron_info(view, corner, i, layer);

        corner.y += view->neuron_radius * shift_coef;
    }

    htable_each(view->h_name2physobj, iter_neuron_info, NULL);
}

void genann_view_prepare(struct genann_view *view, const genann *net) {
    assert(view);
    if (!net)
        return;

    make_chipmunk_objects(view, net);
    genann_viewer_fill_hash(view, net);
}

void draw_links(struct genann_view *view, int layer) {
    if (!view->ann) {
        return;
    }

    struct NeuronInfo *ni = view->current_neuron;

    if (!ni) {
        return;
    }

    /*printf("ni->name %s\n", ni->name);*/
    /*printf("index %d, layer %d\n", ni->index, ni->layer);*/

    /*struct NeuronLinks *nl = htable_get(view->h_name2weight, */

    int num = 0;
    double *w = NULL;
    if (layer == 0) {
        num = view->ann->hidden;
        w = genann_weights_input(view->ann);
    } else if (layer == view->ann->hidden + 1) {
        num = view->ann->hidden;
        w = genann_weights_hidden(view->ann, layer);
    } else {
        num = view->ann->hidden;
        w = genann_weights_output(view->ann);
    }

    assert(w);
    for (int i = 0; i < num; ++i) {
        char key[32] = {0};
        sprintf(key, "[%d,%d]", i, layer);
        cpBody **body = htable_get_s(view->h_name2physobj, key, NULL);
        if (!body)
            continue;

        ((struct NeuronInfo*)(*body)->userData)->weight = w[i];

        Vector2 b = from_Vect((*body)->p);
        Vector2 a = from_Vect(ni->body->p);
        float thick = 2.;
        DrawLineEx(
            a, b,
            thick,
            BLACK
        );
        Vector2 tmp = Vector2Scale(Vector2Subtract(b, a), 0.5);
        Vector2 middle = Vector2Add(a, tmp);
        char text[32] = {0};
        sprintf(text, "%f", w[i]);
        DrawText(text, middle.x, middle.y, view->neuron_radius, RED);
    }

}

static void iter_body(cpBody *body, void *data) {
    struct genann_view *view = data;
    DrawCircle(body->p.x, body->p.y, view->neuron_radius, PURPLE);
    const int segs_num = view->neuron_radius * 5;
    double w = ((struct NeuronInfo*)body->userData)->weight;
    if (w > 1.)
        w = 1.;
    printf("w %f\n", w);
    float degree = (1. + w) / (360. * 2.);
    DrawCircleSector(
        from_Vect(body->p), view->neuron_radius, 0., degree, segs_num, DARKBLUE
    );
}

static void space_draw(struct genann_view *view) {
    assert(view);
    cpSpaceEachBody(view->space, iter_body, view);
}

void genann_view_draw(struct genann_view *view) {
    assert(view);

    space_draw(view);
    /*space_debug_draw(view->space, view->neuron_color);*/
    //struct NeuronInfo *info = NULL;

    if (!view->current_neuron) {
        return;
    }
    struct NeuronInfo *ni = view->current_neuron;

    draw_links(view, ni->layer - 1);
    draw_links(view, ni->layer + 1);

    DrawText(
        ni->name,
        ni->body->p.x,
        ni->body->p.y,
        view->neuron_radius,
        GREEN
    );
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

void point_query(
    cpShape *shape, cpVect point, cpFloat distance, 
    cpVect gradient, void *data
) {
    cpBody *body = shape->body;
    if (!body->userData)
        return;

    *(struct NeuronInfo **)data = (struct NeuronInfo*)body->userData;
}

void genann_view_update(genann_view *v, Vector2 mouse_point) {
    assert(v);
    v->current_neuron = NULL;
    cpSpaceStep(v->space, 1. / 60.);
    cpSpacePointQuery(
        v->space, from_Vector2(mouse_point), 0., 
        CP_SHAPE_FILTER_ALL, point_query, &v->current_neuron
    );
}
