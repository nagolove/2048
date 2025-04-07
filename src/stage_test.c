// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_test.h"

#include "common.h"
#include "modelview.h"
#include "koh_stages.h"

/*
OK - Установить определенные значения поля

Создать данные того, каким должно быть поле после хода или серии ходов
Только для поля 6*6

Произвести искуственный ввод
Проверить состояние
Повторить
*/

#define T_FIELD_SIZE 6
#define T_X_SIZE \
    T_FIELD_SIZE * T_FIELD_SIZE * 2 + 1 + 1

typedef struct TestSet {
    // Входные данные
    char *x[T_X_SIZE];
    // Был ли произведен ввод?
    bool input_done;
    // Индекс в массиве x, где находится курсор считывающий команды в данный
    // момент
    int idx;
} TestSet;

// XXX: Можно-ли сделать так, что все одинаковые подряд цифры сложатся?

static TestSet sets[] = {
    { 
        .x = {

            // state
            " ", " ", " ", " ", " ", " ",
            "2", " ", " ", "4", "4", "4",
            "2", " ", " ", " ", " ", " ",
            " ", " ", "8", " ", " ", " ",
            " ", " ", "8", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            // user input
            "right", 
            // assert state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", "2", "8", "4",
            " ", " ", " ", " ", " ", "2",
            " ", " ", " ", " ", " ", "8",
            " ", " ", " ", " ", " ", "8",
            " ", " ", " ", " ", " ", " ",

            NULL,

        },
        .input_done = false,
        .idx = 0,
    },
};
static int set_current = 0;

static void t_set(ModelView *mv, TestSet *ts) {
    assert(mv);
    assert(mv->field_size == T_FIELD_SIZE);
    assert(ts);

    int *idx = &ts->idx;
    assert(*idx >= 0);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            int val = -1;
            sscanf(ts->x[*idx], "%d", &val);
            trace("t_set: val %d\n", val);
            if (val != -1)
                modelview_put_cell(mv, x, y, val);
            (*idx)++;
        }
    }
}

static void t_assert(ModelView *mv, TestSet *ts) {
    assert(mv);
    assert(ts);

    if (mv->state != MVS_READY)
        return;

    int *idx = &ts->idx;
    if (ts->input_done) {
        char *elem = NULL;
        do {
            elem = ts->x[(*idx)++];
            trace("t_assert: '%s'\n", elem);
        } while (elem);
    }
}

static void t_input(ModelView *mv, TestSet *ts) {
    // Закончилась-ли анимация?
    if (mv->state != MVS_READY)
        return;

    if (!ts->input_done) {
        enum Direction dir = str2direction(ts->x[ts->idx++]);
        modelview_input(mv, dir);
        ts->input_done = true;
    }
}

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

    // Начальный ход
    /*modelview_put(&test_view);*/

    ModelView *mv = &test_view;
    t_set(mv, &sets[set_current]);
}

static void stage_test_update(Stage_Test *st) {
    /*trace("stage_test_update:\n");*/
    modelview_pause_set(&test_view, st->is_paused);
    t_input(&test_view, &sets[set_current]);
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
