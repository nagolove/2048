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
#include "stage_test2.h"
#include "koh_stages.h"
#include "koh_stage_sprite_loader.h"
#include "koh_stage_sprite_loader2.h"
#include "stage_main.h"
#include "koh_common.h"
#include "modelview.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#if defined(PLATFORM_WEB)
int screen_width = 1920;
int screen_height = 1080;
#else
int screen_width = 1920 * 2;
int screen_height = 1080 * 2;
#endif

static StagesStore *ss;
static bool draw_gui = true;

static void update() {
    inotifier_update();

    if (IsKeyPressed(KEY_GRAVE)) {
        draw_gui = !draw_gui;
    }

    BeginDrawing();

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
    // TODO: Сократить код инициализации систем

    em_setup_screen_size(&screen_width, &screen_height);

    koh_hashers_init();
    srand(time(NULL));

    printf(
        "main: screen_width %d, screen_height %d\n",
        screen_width, screen_height
    );
    printf(
        "main: GetScreenWidth() %d, GetScreenHeight() %d\n",
        GetScreenWidth(), GetScreenHeight()
    );

    InitWindow(screen_width, screen_height, "koh-2048");

    SetWindowMonitor(1);

    const int target_fps = 90;

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
            .ranges = (ImWchar[]){
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x0400, 0x044F, // Cyrillic
                // XXX: symbols not displayed
                // media buttons like record/play etc. Used in dotool_gui()
                0x23CF, 0x23F5, 
                0,
            },
    });

#ifdef __wasm__
    const char *imgui_state = local_storage_load("imgui");
    igLoadIniSettingsFromMemory(imgui_state, strlen(imgui_state));
#endif

    //rlImGuiSetup();

    logger_init();
    koh_common_init();
    inotifier_init();

    fnt_vector_init_freetype();

    /*
    for (int j = 0; j < 10; j++) {
        //Font f = load_font_unicode("assets/jetbrains_mono.ttf", 72);

        koh_term_color_set(KOH_TERM_GREEN);
        printf("main: j %d\n", j);
        koh_term_color_reset();

        ModelView mv = {};
        Camera2D cam = {};
        cam.zoom = 1.f;
        modelview_init(&mv, (Setup) {
            .on_init_lua = NULL,
            .tmr_put_time = 0.1,
            .tmr_block_time = 0.1,
            .use_bonus = false,
            .use_fnt_vector = false,
            .field_size = 7,
            .cam = &cam,
            .auto_put = true,
            .win_value = 64,
        });
        modelview_shutdown(&mv);
    }
    printf("exit(1);\n");
    exit(1);
    // */

    ss = stage_new(NULL);
    stage_add(ss, stage_main_new(NULL), "main");
    stage_add(ss, stage_test_new(NULL), "test");
    stage_add(ss, stage_test2_new(NULL), "test2");
    stage_add(ss, stage_sprite_loader_new(NULL), "sprite_loader");
    stage_add(ss, stage_sprite_loader_new2(NULL), "sprite_loader2");
    stage_init(ss);

    //stage_active_set(ss, "main");
    stage_active_set(ss, "test");

    SetExitKey(KEY_NULL);

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

    stage_shutdown(ss);
    stage_free(ss);

    inotifier_shutdown();
    fnt_vector_shutdown_freetype();

    koh_common_shutdown();

    rlImGuiShutdown();
    CloseWindow();

    logger_shutdown();

    return 0;
}
