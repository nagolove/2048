// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_test.h"

#include "common.h"
#include "modelview.h"
#include "koh_stages.h"

typedef struct Stage_Test {
    Stage               parent;
    Camera2D            camera;
    bool                is_paused;
} Stage_Test;

static struct ModelView test_view;

static void stage_test_init(Stage_Test *st) {
    trace("stage_test_new:\n");
    st->camera.zoom = 1.f;

    modelview_setup.cam = &st->camera;
    //main_view_setup.on_init_lua = load_init_lua;

    modelview_init(&test_view, modelview_setup);
    modelview_put(&test_view);
}

static void stage_test_update(Stage_Test *st) {
    /*trace("stage_test_update:\n");*/
    modelview_pause_set(&test_view, st->is_paused);
}

static void stage_test_gui(Stage_Test *st) {
    /*trace("stage_test_gui:\n");*/

    /*
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    bool wnd_open = true;
    igBegin("test", &wnd_open, wnd_flags);
    igEnd();
    */

    modelview_draw_gui(&test_view);
}

/*
Установить определенные значения поля
Создать данные того, каким должно быть поле после хода или серии ходов
Произвести искуственный ввод
Проверить состояние
Повторить
*/

static void stage_test_draw(Stage_Test *st) {
    BeginDrawing();
    BeginMode2D(st->camera);
    ClearBackground(RAYWHITE);

    if (test_view.state != MVS_GAMEOVER) {
        input(&test_view);
    }
    modelview_draw(&test_view);

    EndMode2D();
}

static void stage_test_shutdown(Stage_Test *st) {
    trace("stage_test_shutdown:\n");
    modelview_shutdown(&test_view);
}

static void stage_test_enter(Stage_Test *st) {
    trace("stage_test_enter:\n");
}

static void stage_test_leave(Stage_Test *st) {
    trace("stage_test_leave:\n");
}

Stage *stage_test_new(HotkeyStorage *hk_store) {
    //assert(hk_store);
    Stage_Test *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_test_init;
    st->parent.enter = (Stage_callback)stage_test_enter;
    st->parent.leave = (Stage_callback)stage_test_leave;

    st->parent.update = (Stage_callback)stage_test_update;
    st->parent.draw = (Stage_callback)stage_test_draw;
    st->parent.gui = (Stage_callback)stage_test_gui;
    st->parent.shutdown = (Stage_callback)stage_test_shutdown;
    return (Stage*)st;
}
