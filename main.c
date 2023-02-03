#include <assert.h>
#include <stddef.h>
#include <raylib.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "genann.h"
#include "modelbox.h"

static struct ModelBox     main_model;
static struct ModelView    main_view;

static bool automode = false;
static struct genann *trained = NULL;
static const int screen_width = 1920;
static const int screen_height = 1080;
static const char *trained_fname = "trained.binary";

void trained_load() {
    FILE *file = fopen(trained_fname, "r");
    if (!file)
        return;

    trained = genann_read(file);
    fclose(file);
}

void trained_free() {
    if (trained) {
        genann_free(trained);
        trained = NULL;
    }
}

void input() {
    if (IsKeyPressed(KEY_LEFT)) {
        main_model.update(&main_model, DIR_LEFT);
    } else if (IsKeyPressed(KEY_RIGHT)) {
        main_model.update(&main_model, DIR_RIGHT);
    } else if (IsKeyPressed(KEY_UP)) {
        main_model.update(&main_model, DIR_UP);
    } else if (IsKeyPressed(KEY_DOWN)) {
        main_model.update(&main_model, DIR_DOWN);
    }
}

void draw_over() {
    const char *msg = "GAMEOVER";
    const int fontsize = 190;
    float width = MeasureText(msg, fontsize);
    Vector2 pos = {
        .x = (screen_width - width) / 2.,
        .y = (screen_height - fontsize) / 2.,
    };
    DrawText(msg, pos.x, pos.y, fontsize, GREEN);
}

void draw_scores() {
    char msg[64] = {0};
    const int fontsize = 70;
    sprintf(msg, "scores: %d", main_model.scores);
    Vector2 pos = {
        .x = (screen_width - MeasureText(msg, fontsize)) / 2.,
        .y = 1.2 * fontsize,
    };
    DrawText(msg, pos.x, pos.y, fontsize, BLUE);
}

const double MAX_VALUE = 100000.;

void normalize_inputs(struct ModelBox *mb, double *inputs) {
    assert(mb);
    assert(inputs);

    int inputs_num = field_size * field_size;
    int idx = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++) 
            inputs[idx++] = mb->field[j][i];

    for (int i = 0; i < inputs_num; i++) {
        inputs[i] = inputs[i] / MAX_VALUE;
    }
}

int out2dir(const double *outputs) {
    assert(outputs);
    double max = 0.;
    int max_index = 0;
    for (int j = 0; j < 4; j++)
        if (outputs[j] > max) {
            max = outputs[j];
            max_index = j;
        }
    return max_index;
}

void auto_play() {
    assert(trained);

    double inputs[field_size * field_size] = {0};
    normalize_inputs(&main_model, inputs);
    const double *outputs = genann_run(trained, inputs);

    printf("directioni %d\n", out2dir(outputs));
    main_model.update(&main_model, out2dir(outputs));
}

const double    ERROR_EPSILON = 0.01;
const int       STEPS_NUM = 4;

void train() {
    genann *net = genann_init(
        field_size * field_size,
        1, 
        field_size * field_size, 
        4
    );

    double err = 0.;
    double last_err = 1000.;
    int count = 0;

    double inputs[field_size * field_size] = {0};

    struct ModelBox mb = {0};
    modelbox_init(&mb);

    do {
        count++;
        if (count % 1000 == 0) {
            printf("iteration %d\n", count);
        }
        if (count % 5000 == 0) {
            genann_randomize(net);
            last_err = 1000;
        }

        genann *save = genann_copy(net);

        /* Take a random guess at the ANN weights. */
        for (int i = 0; i < net->total_weights; ++i) {
            net->weight[i] += ((double)rand())/RAND_MAX-0.5;
        }

        err = 0;

        int scores = mb.scores;

        for (int j = 0; j < STEPS_NUM; j++) {
            int dir = rand() % 4;
            mb.update(&mb, dir);
            normalize_inputs(&mb, inputs);
            //const double *out = genann_run(net, inputs);
            genann_run(net, inputs);
        }

        if (mb.scores > scores) {
            // Успех
        }

        /*
        err += pow(*genann_run(net, input[0]) - output[0], 2.0);
        err += pow(*genann_run(net, input[1]) - output[1], 2.0);
        err += pow(*genann_run(net, input[2]) - output[2], 2.0);
        err += pow(*genann_run(net, input[3]) - output[3], 2.0);
        */

        /* Keep these weights if they're an improvement. */
        if (err < last_err) {
            genann_free(save);
            last_err = err;
        } else {
            genann_free(net);
            net = save;
        }

    } while (err > ERROR_EPSILON);

    FILE *file = fopen(trained_fname, "w");
    if (file) {
        genann_write(net, file);
        fclose(file);
    }
}

void update() {
    if (IsKeyPressed(KEY_A)) {
        automode = !automode;
    } else if (IsKeyPressed(KEY_T)) {
        train();
    }

    if (!automode) {
        if (!main_model.gameover)
            input();
        if (IsKeyPressed(KEY_SPACE)) {
            modelbox_init(&main_model);
        }
    } else {
        auto_play();
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);
    main_view.draw(&main_view, &main_model);
    draw_scores();
    if (main_model.gameover)
        draw_over();
    EndDrawing();
}

int main(void) {
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "2048");

    modelview_init(&main_view, NULL);
    modelbox_init(&main_model);
    trained_load();

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        update();
    }

    CloseWindow();
    trained_free();

    return 0;
}
