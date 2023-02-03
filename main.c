#include <assert.h>
#include <stddef.h>
#include <raylib.h>

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

void trained_load() {
    FILE *file = fopen("trained.binary", "r");
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

void auto_play() {
    assert(trained);

    double inputs[field_size * field_size] = {0};
    normalize_inputs(&main_model, inputs);
    const double *outputs = genann_run(trained, inputs);

    double max = 0.;
    int max_index = 0;
    for (int j = 0; j < 4; j++)
        if (outputs[j] > max) {
            max = outputs[j];
            max_index = j;
        }

    printf("max_index %d\n", max_index);
    main_model.update(&main_model, max_index);
}

void update() {
    if (IsKeyPressed(KEY_A)) {
        automode = !automode;
    }

    if (!automode) {
        if (!main_model.gameover)
            input();
        else if (IsKeyPressed(KEY_SPACE)) {
            main_model.reset(&main_model);
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
