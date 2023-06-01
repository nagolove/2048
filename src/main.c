#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "koh_console.h"
#include "koh_hotkey.h"
#include "koh_logger.h"
#include "koh_script.h"
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
#include "cimgui.h"
#include "cimgui_impl.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static struct ModelBox     main_model;
static struct ModelView    main_view;

static bool automode = false;
static const int screen_width = 1920;
static const int screen_height = 1080;

static Camera2D camera = { 0 };
static const double MAX_VALUE = 2048;

static HotkeyStorage hk = {0};


void input() {
    main_view.queue_size = 0;

    // Закончилась-ли анимация?
    if (main_view.state != MVS_READY)
        return;

    if (IsKeyPressed(KEY_LEFT)) {
        //main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_LEFT, &main_view);
    } else if (IsKeyPressed(KEY_RIGHT)) {
        //main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_RIGHT, &main_view);
    } else if (IsKeyPressed(KEY_UP)) {
        //main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_UP, &main_view);
    } else if (IsKeyPressed(KEY_DOWN)) {
        //main_view.expired_cells_num = 0;
        main_model.update(&main_model, DIR_DOWN, &main_view);
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

static void update(void *arg) {
    /*camera_process();*/
    if (!automode) {
        if (main_model.state != MBS_GAMEOVER)
            input();
    } else {
        //auto_play();
    }

    if (IsKeyPressed(KEY_R)) {
        modelbox_shutdown(&main_model);
        modelview_shutdown(&main_view);
        modelbox_init(&main_model);
        modelview_init(&main_view, NULL, &main_model);

        model_global_shutdown();
        model_global_init();

        main_model.start(&main_model, &main_view);
    }

    /*
    if (IsKeyPressed(KEY_SPACE)) {
        modelbox_init(&main_model);
        automode = false;
    }
    */

    hotkey_process(&hk);
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

int main(void) {
#if 0
    printf("test run\n");
    test_divide_slides();
    exit(EXIT_FAILURE);
#endif

    camera.zoom = 1.0f;
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "2048");
    //SetTargetFPS(999);
    SetTargetFPS(60);

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
    });
    /*
    net_viewer = genann_view_new("viewer");
    genann_view_position_set(net_viewer, (Vector2) { 0., -1000. });
    */

    /*
    trained = load();
    genann_print(trained);
    genann_print_run(trained);
    */

    logger_init();
    sc_init();
    logger_register_functions();
    sc_init_script();

    modelbox_init(&main_model);
    modelview_init(&main_view, NULL, &main_model);
    main_model.start(&main_model, &main_view);

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

    model_global_init();

    //view_test = printing_test();
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update, NULL, 60, 1);
#else
    SetTargetFPS(60); 
    while (!WindowShouldClose()) {
        update(NULL);
    }
#endif

    CloseWindow();

    model_global_shutdown();

    modelbox_shutdown(&main_model);
    modelview_shutdown(&main_view);

    rlImGuiShutdown();

    hotkey_shutdown(&hk);
    sc_shutdown();
    logger_shutdown();

    return 0;
}
