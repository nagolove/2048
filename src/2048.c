#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_fnt_vector.h"
#include "koh_hashers.h"
#include "koh_inotifier.h"
#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "stage_test.h"
#include "koh_stages.h"
#include "koh_stage_sprite_loader.h"
#include "koh_stage_sprite_loader2.h"
#include "stage_main.h"
#include "koh_common.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if defined(PLATFORM_WEB)
static const int screen_width = 1920;
static const int screen_height = 1080;
#else
static const int screen_width = 1920 * 2;
static const int screen_height = 1080 * 2;
#endif

static StagesStore *ss;
static bool draw_gui = true;

static void update() {
    inotifier_update();

    if (IsKeyPressed(KEY_GRAVE)) {
        draw_gui = !draw_gui;
    }

    stage_active_update(ss);

    ////////////////////// IMGUI
    rlImGuiBegin();

    if (draw_gui) {
        stage_active_gui_render(ss);
        stages_gui_window(ss);
        bool open = false;
        igShowDemoWindow(&open);
    }

    rlImGuiEnd();
    ////////////////////// IMGUI

    EndDrawing();
}


int main(void) {

    koh_hashers_init();
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "2048");
    SetWindowMonitor(1);

    const int target_fps = 90;

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
    });

#ifdef __wasm__
    const char *imgui_state = local_storage_load("imgui");
    igLoadIniSettingsFromMemory(imgui_state, strlen(imgui_state));
#endif

    //rlImGuiSetup();

    logger_init();
    inotifier_init();

    // TODO: Тесты сломаны
    // TODO: Сделать несколько исполняемых файлов - для тестов и основной
    //test_modelviews_multiple();

    fnt_vector_init_freetype();

    ss = stage_new(NULL);
    stage_add(ss, stage_main_new(NULL), "main");
    stage_add(ss, stage_test_new(NULL), "test");
    stage_add(ss, stage_sprite_loader_new(NULL), "sprite_loader");
    stage_add(ss, stage_sprite_loader_new2(NULL), "sprite_loader2");
    stage_init(ss);
    stage_active_set(ss, "main");

    //view_test = printing_test();
#if defined(PLATFORM_WEB)
    trace("main: PLATFORM_WEB\n");
    emscripten_set_main_loop_arg(update, NULL, target_fps, 1);
#else
    SetTargetFPS(target_fps); 
    while (!WindowShouldClose()) {
        update();
    }
#endif

#ifdef __wasm__
    imgui_state = igSaveIniSettingsToMemory(NULL);
    local_storage_save("imgui", imgui_state);
#endif

    stage_free(ss);

    inotifier_shutdown();
    fnt_vector_shutdown_freetype();

    rlImGuiShutdown();
    CloseWindow();

    logger_shutdown();

    return 0;
}
