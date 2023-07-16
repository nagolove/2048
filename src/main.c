#include "timers.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "timers.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_console.h"
#include "koh_hotkey.h"
#include "koh_logger.h"
#include "koh_script.h"
#include "modelview.h"
#include "raylib.h"
#include "raymath.h"
#include "test_suite.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "undosys.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static struct ModelView    main_view;
static UndoSys *undo_system = NULL;

#if defined(PLATFORM_WEB)
static const int screen_width = 1920;
static const int screen_height = 1080;
#else
static const int screen_width = 1920 * 2;
static const int screen_height = 1080 * 2;
#endif

static Camera2D camera = { 0 };
static HotkeyStorage hk = {0};

static struct Setup main_view_setup = {
    .pos = NULL,
    .use_bonus = true,
    .cam = &camera,
    .field_size = 6,
    .tmr_block_time = 0.3,
    .tmr_put_time = 0.3,
    .use_gui = true,
    .auto_put = true,
};

float maxf(float a, float b) {
    return a > b ? a : b;
}

void mouse_swipe_cell(enum Direction *dir) {
    assert(dir);
    static Vector2 handle1 = {0}, handle2 = {0};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        handle1 = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        handle2 = GetMousePosition();
        Vector2 diff = Vector2Subtract(handle1, handle2);
        if (fabs(diff.x) > fabs(diff.y)) {
            if (diff.x < 0) *dir = DIR_RIGHT;
            else if (diff.x > 0) *dir = DIR_LEFT;
        } else { 
            if (diff.y > 0) *dir = DIR_UP;
            else if (diff.y < 0) *dir = DIR_DOWN;
        }
    }
}

static void keyboard_swipe_cell(enum Direction *dir) {
    struct {
        enum Direction  dir;
        int             key;
    } keys_dirs[] = {
        {DIR_LEFT, KEY_LEFT},
        {DIR_RIGHT, KEY_RIGHT},
        {DIR_UP, KEY_UP},
        {DIR_DOWN, KEY_DOWN},
    };

    for (int j = 0; j < sizeof(keys_dirs) / sizeof(keys_dirs[0]); ++j) {
        if (IsKeyPressed(keys_dirs[j].key)) {
            *dir = keys_dirs[j].dir;
            break;
        }
    } 
}

static void input() {
    // Закончилась-ли анимация?
    if (main_view.state != MVS_READY)
        return;

    enum Direction dir = {0};

    //mouse_swipe_cell(&dir);
    keyboard_swipe_cell(&dir);

    // TODO: куда лучше переместить проверку так, что-бы была возможность
    // запускать автоматические тесты, без создания новой плитки каждый ход
    //if (main_view.dir == DIR_NONE)
        //modelview_put(&main_view);

    modelview_input(&main_view, dir);
    //modelview_save_state2file(&main_view);
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
#if defined(PLATFORM_WEB)
    pos.y = 1.0 * fontsize;
#else
    pos.y = 1.0 * fontsize;
#endif
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

static void update() {
    camera_process();

    if (IsKeyPressed(KEY_P)) {
        is_paused = !is_paused;
    }

    undosys_push(undo_system, (struct UndoState) {
        .r = main_view.r,
        .timers = (struct TimerMan*[]) {
            main_view.timers_slides,
            main_view.timers_effects,
        },
        .timers_num = 2,
        .udata = &main_view,
        .sz = sizeof(main_view),
    });

    timerman_pause(main_view.timers_slides, is_paused);
    timerman_pause(main_view.timers_effects, is_paused);

    if (IsKeyPressed(KEY_R)) {
        modelview_shutdown(&main_view);
        modelview_init(&main_view, main_view_setup);
        modelview_put(&main_view);
    }

    hotkey_process(&hk);
    console_check_editor_mode();

    BeginDrawing();
    BeginMode2D(camera);

    if (main_view.state != MVS_GAMEOVER)
        input();
    modelview_draw(&main_view);

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

    console_update();

    undosys_window(undo_system);

    // восстановление состояния
    if (undosys_get(undo_system)) {
        struct UndoState *st = undosys_get(undo_system);
        assert(sizeof(main_view) == st->sz);
        memcpy(&main_view, st->udata, sizeof(main_view));
        main_view.r = st->r;
        main_view.timers_slides = st->timers[0];
        main_view.timers_effects = st->timers[1];
    }

    EndMode2D();

    modelview_draw_gui(&main_view);

    EndDrawing();
}



int main(void) {

    main_view_setup.color_theme = color_theme_light,

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

    //test_modelviews_one();
    test_modelviews_multiple();

    modelview_init(&main_view, main_view_setup);
    modelview_put(&main_view);

    hotkey_init(&hk);
    console_init(&hk, &(struct ConsoleSetup) {
        .on_enable = NULL,
        .on_disable = NULL,
        .udata = NULL,
        .color_cursor = BLUE,
        .color_text = BLACK,
        .fnt_path = "assets/djv.ttf",
        //.fnt_size = 50,
        .fnt_size = 20,
    });
    console_immediate_buffer_enable(true);

    undo_system = undosys_new(2048);

    //view_test = printing_test();
    const int target_fps = 90;
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update, NULL, target_fps, 1);
#else
    SetTargetFPS(target_fps); 
    while (!WindowShouldClose()) {
        update();
    }
#endif

    modelview_shutdown(&main_view);

    rlImGuiShutdown();

    CloseWindow();

    undosys_free(undo_system);

    hotkey_shutdown(&hk);
    sc_shutdown();
    logger_shutdown();

    return 0;
}
