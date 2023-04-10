#include "koh_logger.h"
#include "modelbox.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "koh_console.h"
#include "koh_hotkey.h"

static struct ModelBox     main_model;
static struct ModelView    main_view;

static bool automode = false;
static struct genann *trained = NULL;
static struct genann_view *net_viewer = NULL;
static const int screen_width = 1920;
static const int screen_height = 1080;
static const char *trained_fname = "trained.binary";

static Camera2D camera = { 0 };
static const double MAX_VALUE = 2048;

/*
genann *load() {
    FILE *file = fopen(trained_fname, "r");
    genann *ret = NULL;
    if (!file)
        return NULL;

    ret = genann_read(file);
    fclose(file);
    return ret;
}
*/

/*
void trained_free() {
    if (trained) {
        genann_free(trained);
        trained = NULL;
    }
}
*/

void input() {
    if (IsKeyPressed(KEY_R)) {
        modelbox_shutdown(&main_model);
        modelview_shutdown(&main_view);
        modelbox_init(&main_model);
        modelview_init(&main_view, NULL, &main_model);
    }


    main_model.queue_size = 0;

    if (main_view.state != MVS_READY)
        return;

    if (IsKeyPressed(KEY_LEFT)) {
        main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_LEFT);
    } else if (IsKeyPressed(KEY_RIGHT)) {
        main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_RIGHT);
    } else if (IsKeyPressed(KEY_UP)) {
        main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_UP);
    } else if (IsKeyPressed(KEY_DOWN)) {
        main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_DOWN);
    }
}

Vector2 place_center(const char *text, int fontsize) {
    float width = MeasureText(text, fontsize);
    return (Vector2) {
        .x = (screen_width - width) / 2.,
        .y = (screen_height - fontsize) / 2.,
    };
}

void draw_win() {
    const char *msg = "WIN";
    const int fontsize = 220;
    Vector2 pos = place_center(msg, fontsize);
    DrawText(msg, pos.x, pos.y, fontsize, MAROON);
}

void draw_over() {
    const char *msg = "GAMEOVER";
    const int fontsize = 190;
    Vector2 pos = place_center(msg, fontsize);
    DrawText(msg, pos.x, pos.y, fontsize, GREEN);
}

void draw_scores() {
    char msg[64] = {0};
    const int fontsize = 70;
    sprintf(msg, "scores: %d", main_model.scores);
    Vector2 pos = place_center(msg, fontsize);
    pos.y = 1.2 * fontsize;
    DrawText(msg, pos.x, pos.y, fontsize, BLUE);
}

void print_inputs(const double *inputs, int inputs_num) {
    printf("print_inputs: ");
    for (int i = 0; i < inputs_num; i++) {
        printf("%f ", inputs[i]);
    }
    printf("\n");
}

void write_normalized_inputs(struct ModelBox *mb, double *inputs) {
    assert(mb);
    assert(inputs);

    int inputs_num = FIELD_SIZE * FIELD_SIZE;
    int idx = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++) 
            inputs[idx++] = mb->field[j][i].value;

    for (int i = 0; i < inputs_num; i++) {
        inputs[i] = inputs[i] / MAX_VALUE;
    }

    //print_inputs(inputs, inputs_num);
}

// Как протестировать функцию?
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

/*
void auto_play() {
    assert(trained);

    double inputs[FIELD_SIZE * FIELD_SIZE] = {0};
    write_normalized_inputs(&main_model, inputs);
    const double *outputs = genann_run(trained, inputs);

    //printf("direction i %d\n", out2dir(outputs));
    main_model.update(&main_model, out2dir(outputs));
}

const double    ERROR_EPSILON = 0.01;
const int       STEPS_NUM = 4;

void ann_shake(genann *net) {
     //Take a random guess at the ANN weights. 
    for (int i = 0; i < net->total_weights; ++i) {
        net->weight[i] += ((double)rand())/RAND_MAX-0.5;
    }
}
*/

/*
void train() {
    printf("training\n");
    genann *net = genann_init(
        FIELD_SIZE * FIELD_SIZE,
        1, 
        FIELD_SIZE * FIELD_SIZE, 
        4
    );

    int count = 0;

    double inputs[FIELD_SIZE * FIELD_SIZE] = {0};

    struct ModelBox mb = {0};

    modelbox_init(&mb);
    genann_randomize(net);

    int succ_run = 0;
    int fail_run = 0;
    do {
        count++;
        if (count % 10000 == 0) {
            printf("iteration %d\n", count);
        }
        if (count % 15000000 == 0) {
            printf("resetting\n");
            break;
        }

        genann *save = genann_copy(net);
        int scores = mb.scores;

        for (int j = 0; j < STEPS_NUM; j++) {
            //int dir = rand() % 4;
            //mb.update(&mb, dir);
            write_normalized_inputs(&mb, inputs);
            const double *outs = genann_run(net, inputs);
            mb.update(&mb, out2dir(outs));
        }

        if (mb.scores > scores) {
            // Успех
            genann_free(save);
            ann_shake(net);
            //printf("success %d\n", count);
            succ_run++;
            fail_run = 0;
        } else {
            genann_free(net);
            succ_run = 0;
            fail_run++;
            net = save;
        }

        if (succ_run > 250 || mb.state == MS_WIN) {
            printf("great\n");
            break;
        }
        if (fail_run > 1) {
            modelbox_init(&mb);
            genann_randomize(net);
            succ_run = 0;
            fail_run = 0;
        }

    } while (true);

    main_model = mb;

    printf("evolution for %d cycles\n", count);

    FILE *file = fopen(trained_fname, "w");
    if (file) {
        genann_write(net, file);
        fclose(file);
    }

    genann_free(net);
}
*/

/*
void train() {
    printf("training\n");
    // Стратегия обучения?
    struct Set *s = set_new(10005);
    for (int cycles = 0; cycles < 2500; cycles++) {
        printf("cycles %d\n", cycles);
        set_train(s);
        struct Set *new = set_select(s);
        set_shutdown(s);
        s = new;
    }
    set_write(s);

    genann_print(s->nets[0]);

    set_shutdown(s);
}
*/

void camera_process() {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        Vector2 delta = GetMouseDelta();
        delta = Vector2Scale(delta, -1.0f/camera.zoom);

        camera.target = Vector2Add(camera.target, delta);
    }

    // Zoom based on mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        // Get the world point that is under the mouse
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

        // Set the offset to where the mouse is
        camera.offset = GetMousePosition();

        // Set the target to match, so that the camera maps the world space point 
        // under the cursor to the screen space point under the cursor at any zoom
        camera.target = mouseWorldPos;

        // Zoom increment
        const float zoomIncrement = 0.125f;

        camera.zoom += (wheel*zoomIncrement);
        if (camera.zoom < zoomIncrement) camera.zoom = zoomIncrement;
    }
}

/*
static genann_view *view_test = NULL;

genann_view *printing_test() {
    genann *ann = genann_init(5, 3, 5, 4);
    genann_randomize(ann);
    //genann *ann = genann_init(4, 2, 7, 3);
    genann_print(ann);
    genann_print_run(ann);
    genann_view *view = genann_view_new("randomized[5, 3, 5, 4]");
    genann_view_prepare(view, ann);
    genann_free(ann);
    return view;
}
*/

void update() {
    /*camera_process();*/
    /*
    if (IsKeyPressed(KEY_A)) {
        automode = !automode;
    } else if (IsKeyPressed(KEY_T)) {
        train();
    }
    */

    if (!automode) {
        if (main_model.state != MBS_GAMEOVER)
            input();
    } else {
        //auto_play();
    }

    if (IsKeyPressed(KEY_SPACE)) {
        modelbox_init(&main_model);
        automode = false;
    }

    console_check_editor_mode();

    BeginDrawing();
    BeginMode2D(camera);
    ClearBackground(RAYWHITE);
    main_view.draw(&main_view, &main_model);
    draw_scores();
    switch (main_model.state) {
        case MBS_GAMEOVER: {
            draw_over();
            break;
        }
        case MBS_WIN: {
            draw_win();
            break;
        }
        default: break;
    }

    //genann_view_prepare(net_viewer, trained);
    //genann_view_draw(net_viewer);

    /*
    Vector2 mouse_point = GetScreenToWorld2D(GetMousePosition(), camera);
    genann_view_draw(view_test);
    genann_view_update(view_test, mouse_point);
    */

    console_update();

    EndMode2D();
    EndDrawing();

    //usleep(10000);
}

static HotkeyStorage hk = {0};

int main(void) {
    camera.zoom = 1.0f;
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "2048");

    /*
    net_viewer = genann_view_new("viewer");
    genann_view_position_set(net_viewer, (Vector2) { 0., -1000. });
    */

    modelbox_init(&main_model);
    modelview_init(&main_view, NULL, &main_model);

    /*
    trained = load();
    genann_print(trained);
    genann_print_run(trained);
    */

    logger_init();
    hotkey_init(&hk);
    console_init(&hk, &(struct ConsoleSetup) {
        .on_enable = NULL,
        .on_disable = NULL,
        .udata = NULL,
        .color_cursor = BLUE,
        .color_text = BLACK,
        .fnt_path = "assets/djv.ttf",
        .fnt_size = 50,
    });
    console_immediate_buffer_enable(true);

    //view_test = printing_test();

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        update();
    }

    CloseWindow();

    /*
    genann_view_free(net_viewer);
    trained_free();

    genann_view_free(view_test);
    */

    modelbox_shutdown(&main_model);
    modelview_shutdown(&main_view);

    hotkey_shutdown(&hk);
    logger_shutdown();

    return 0;
}
