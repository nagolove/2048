#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_fnt_vector.h"
#include "koh_hashers.h"
#include "koh_inotifier.h"
#include "koh_logger.h"
#include "koh_lua.h"
#include "koh_lua.h"
#include "koh_timerman.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "modelview.h"
#include "raylib.h"
#include "raymath.h"
#include "rlwr.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#include "test_suite.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static struct ModelView    main_view;

#if defined(PLATFORM_WEB)
static const int screen_width = 1920;
static const int screen_height = 1080;
#else
static const int screen_width = 1920 * 2;
static const int screen_height = 1080 * 2;
#endif

static Camera2D camera = { 0 };
static bool draw_gui = true;

static lua_State *l = NULL;
// Создает Луа состояние с raylib окружением.
static rlwr_t *rlwr = NULL;
static const char *init_lua = "assets/init.lua";
static char error[256] = {};

static struct Setup main_view_setup = {
    .pos = NULL,
    .use_bonus = false,
    .cam = &camera,
    .field_size = 6,
    .tmr_block_time = 0.05,
    .tmr_put_time = 0.05,
    .use_gui = true,
    .auto_put = true,
    .win_value = 2048,
};

float maxf(float a, float b) {
    return a > b ? a : b;
}

int l_state_get(lua_State *l) {
    lua_pushstring(l, modelview_state2str(main_view.state));
    return 1;
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

static void load_init() {
    trace("load_init:\n");
    rlwr = rlwr_new();
    l = rlwr_state(rlwr);
    luaL_openlibs(l);

    if (luaL_dofile(l, init_lua) != LUA_OK) {
        strncpy(error, lua_tostring(l, -1), sizeof(error) - 1);
        trace("main: error in '%s' '%s'\n", init_lua, lua_tostring(l, -1));
    } else {
        strcpy(error, "");

        lua_register(l, "state_get", l_state_get);
    }
}

void hotreload(const char *fname, void *ud) {
    trace("hotreload:\n");

    if (rlwr) {
        rlwr_free(rlwr);
        rlwr = NULL;
        l = NULL;
    }

    load_init();
    inotifier_watch(init_lua, hotreload, NULL);
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
    inotifier_update();
    camera_process();

    if (IsKeyPressed(KEY_GRAVE)) {
        draw_gui = !draw_gui;
    }

    if (IsKeyPressed(KEY_P)) {
        is_paused = !is_paused;
    }

    timerman_pause(main_view.timers_slides, is_paused);
    timerman_pause(main_view.timers_effects, is_paused);

    if (IsKeyPressed(KEY_R)) {
        modelview_shutdown(&main_view);
        modelview_init(&main_view, main_view_setup);
        modelview_put(&main_view);
    }

    BeginDrawing();
    BeginMode2D(camera);

    trace(
        "update: main_view.state '%s'\n",
        modelview_state2str(main_view.state)
    );

    if (main_view.state != MVS_GAMEOVER)
        input();
    modelview_draw(&main_view);

    draw_scores();

    /*
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
    */

    static bool is_ok = false;
    const char *pcall_err = L_call(l, "update", &is_ok);
    if (!is_ok)
        strncpy(error, pcall_err, sizeof(error));

    if (strlen(error))
        DrawText(error, 0, 0, 70, BLACK);

    EndMode2D();

    if (draw_gui)
        modelview_draw_gui(&main_view);

    EndDrawing();
}



int main(void) {

    main_view_setup.color_theme = color_theme_light,

    koh_hashers_init();
    camera.zoom = 1.0f;
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "2048");
    SetWindowMonitor(1);

    const int target_fps = 90;

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
    });

    logger_init();

    // TODO: Тесты сломаны
    // TODO: Сделать несколько исполняемых файлов - для тестов и основной
    //test_modelviews_multiple();

    fnt_vector_init_freetype();

    modelview_init(&main_view, main_view_setup);

    load_init();
    inotifier_watch(init_lua, hotreload, NULL);

    modelview_put(&main_view);

    //view_test = printing_test();
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update, NULL, target_fps, 1);
#else
    SetTargetFPS(target_fps); 
    while (!WindowShouldClose()) {
        update();
    }
#endif

    rlwr_free(rlwr);
    fnt_vector_shutdown_freetype();
    modelview_shutdown(&main_view);

    rlImGuiShutdown();
    CloseWindow();

    logger_shutdown();

    return 0;
}
