#include "genann_view.h"

#include <string.h>
#include <stdlib.h>
#include "routine.h"
#include "genann.h"
#include <assert.h>
#include <raylib.h>
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>
#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_structs.h"
#include "chipmunk/chipmunk_types.h"
#include "table.h"
#include "common.h"

struct NeuronInfo {
    char   name[50];
    cpBody *body;
};

struct NeuronLinks {
    int    num;
    double *weights;
};

struct genann_view {
    float   neuron_radius;
    Vector2 position;
    Color   neuron_color;
    cpSpace *space;
    // Хэштаблица, хранит имена нейронов по типу
    // "input[0]", "input[1]",
    // "hidden[1,1]" - слой, номер
    HTable  *h_name2physobj, *h_name2weight;
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

struct genann_view *genann_view_new() {
    struct genann_view *view = calloc(1, sizeof(*view));
    assert(view);
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
    space_shutdown(v->space);
    cpSpaceFree(v->space);
    free(v);
}

static void *add_neuron(struct genann_view *view, Vector2 place, char *key) {
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
    strcpy(ni->name, key);
    body->userData = ni;
    ni->body = body;
    cpBodySetPosition(body, from_Vector2(place));
    htable_add_s(view->h_name2physobj, key, &body, sizeof(void*));
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
    struct NeuronInfo *ni = value;
    printf("iter_neuron_info: %s, %p\n", (char*)key, ni->body);
    return HTABLE_ACTION_NEXT;
}

void genann_viewer_fill_hash(genann_view *view, genann const *ann) {
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
        sprintf(key, "input[%d,%d]", j, k);
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
            sprintf(key, "hidden[%d,%d,%d]", h, j, k);
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
    //htable_print(view->hneurons);
    htable_each(view->h_name2weight, iter_neuron_link, NULL);
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
    char key[32] = {0};
    Vector2 corner;
    //double const *weight = net->weight;

    corner = view->position;
    for (int i = 0; i < net->inputs; i++) {
        //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

        sprintf(key, "input[%d]", i);
        add_neuron(view, corner, key);

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
            //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

            sprintf(key, "hidden[%d,%d]", i, j);
            add_neuron(view, corner, key);

            corner.y += view->neuron_radius * 3.;
        }
    }

    corner = Vector2Add(position, right_shift);
    for (int i = 0; i < net->outputs; i++) {
        //DrawCircleV(corner, view->neuron_radius, view->neuron_color);

        sprintf(key, "out[%d]", i);
        add_neuron(view, corner, key);

        corner.y += view->neuron_radius * 3.;
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

void genann_view_draw(struct genann_view *view) {
    assert(view);
    space_debug_draw(view->space, view->neuron_color);

    if (view->current_neuron) {
        struct NeuronInfo *ni = view->current_neuron;
        DrawText(
            ni->name,
            ni->body->p.x,
            ni->body->p.y,
            view->neuron_radius,
            BLACK
        );
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
