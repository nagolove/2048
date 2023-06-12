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

static struct ModelView    main_view;

//static const int screen_width = 1920;
//static const int screen_height = 1080;

static const int screen_width = 1920 * 2;
static const int screen_height = 1080 * 2;

static Camera2D camera = { 0 };

static HotkeyStorage hk = {0};


void input() {
    // Закончилась-ли анимация?
    if (main_view.state != MVS_READY)
        return;

    if (IsKeyPressed(KEY_LEFT)) {
        //main_view.expired_cells_num = 0;
        main_view.update(&main_view, DIR_LEFT);
    } else if (IsKeyPressed(KEY_RIGHT)) {
        //main_view.expired_cells_num = 0;
        main_view.update(&main_view, DIR_RIGHT);
    } else if (IsKeyPressed(KEY_UP)) {
        //main_view.expired_cells_num = 0;
        main_view.update(&main_view, DIR_UP);
    } else if (IsKeyPressed(KEY_DOWN)) {
        //main_view.expired_cells_num = 0;
        main_view.update(&main_view, DIR_DOWN);
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
    sprintf(msg, "scores: %d", main_view.scores);
    Vector2 pos = place_center(msg, fontsize);
    pos.y = 1.2 * fontsize;
    DrawText(msg, pos.x, pos.y, fontsize, BLUE);
}

/*
void print_inputs(const double *inputs, int inputs_num) {
    printf("print_inputs: ");
    for (int i = 0; i < inputs_num; i++) {
        printf("%f ", inputs[i]);
    }
    printf("\n");
}
*/

/*
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
*/

void camera_process() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        camera.zoom = 1.;
        camera.offset = (Vector2) { 0., 0. };
    }

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

bool is_paused = false;

static void update(void *arg) {
    camera_process();

    if (IsKeyPressed(KEY_P)) {
        is_paused = !is_paused;
    }

    timerman_pause(main_view.timers, is_paused);

    if (IsKeyPressed(KEY_R)) {
        /*modelbox_shutdown(&main_model);*/
        modelview_shutdown(&main_view);
        /*modelbox_init(&main_model);*/
        modelview_init(&main_view, NULL, &camera);

        /*main_view.start(&main_view, &main_view);*/
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

    if (main_view.state != MVS_GAMEOVER)
        input();
    main_view.draw(&main_view);

    draw_scores();
    switch (main_view.state) {
        case MVS_GAMEOVER: {
            draw_over();
            break;
        }
        case MVS_WIN: {
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
    SetWindowMonitor(1);
    //SetTargetFPS(999);
    SetTargetFPS(60);

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
    });

    logger_init();
    sc_init();
    logger_register_functions();
    sc_init_script();

    modelview_init(&main_view, NULL, &camera);

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
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update, NULL, 60, 1);
#else
    SetTargetFPS(60); 
    while (!WindowShouldClose()) {
        update(NULL);
    }
#endif

    CloseWindow();

    modelview_shutdown(&main_view);

    // TODO: Падает тут 
    rlImGuiShutdown();

    hotkey_shutdown(&hk);
    sc_shutdown();
    logger_shutdown();

    return 0;
}
