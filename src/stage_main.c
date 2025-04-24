// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_main.h"

#include "common.h"
#include "koh_inotifier.h"
#include "modelview.h"
#include "koh_stages.h"
#include "raylib_wrap.h"

#ifdef KOH_RLWR
#include "rlwr.h"
#endif

#define NO_LUA 

typedef struct Stage_Main {
    Stage    parent;
    bool     is_paused;
    Camera2D camera;
} Stage_Main;

static struct ModelView main_view;

#ifdef NO_LUA
static lua_State *l = NULL;
#endif

static const char *init_lua = "assets/init.lua";
static char error[256] = {};
static void load_init_lua();

/*static bool is_paused = false;*/

// {{{ l_***
int l_field_size_get(lua_State *l) {
    lua_pushnumber(l, main_view.field_size);
    return 1;
}

int l_quad_width_get(lua_State *l) {
    lua_pushnumber(l, main_view.quad_width);
    return 1;
}

int l_scores_get(lua_State *l) {
    lua_pushnumber(l, main_view.scores);
    return 1;
}

int l_pos_get(lua_State *l) {
    lua_pushnumber(l, main_view.pos.x);
    lua_pushnumber(l, main_view.pos.y);
    return 2;
}

int l_state_get(lua_State *l) {
    lua_pushstring(l, modelview_state2str(main_view.state));
    return 1;
}
// }}}

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

// Загрузка Луа машины
static void load_init_lua() {
#ifdef KOH_RLWR
    if (rlwr) {
        rlwr_free(rlwr);
        rlwr = NULL;
        l = NULL;
    }
#endif

    trace("load_init:\n");

#ifdef RLWR
    rlwr = rlwr_new();
    l = rlwr_state(rlwr);
#else
    l = luaL_newstate();
#endif

    luaL_openlibs(l);

    int res = luaopen_raylib(l);
    printf("load_init_lua: luaopen_raylib = %d\n", res);

    lua_register(l, "pos_get", l_pos_get);
    lua_register(l, "state_get", l_state_get);
    lua_register(l, "scores_get", l_scores_get);
    lua_register(l, "quad_width_get", l_quad_width_get);
    lua_register(l, "field_size_get", l_field_size_get);

    if (luaL_dofile(l, init_lua) != LUA_OK) {
        strncpy(error, lua_tostring(l, -1), sizeof(error) - 1);
        trace("main: error in '%s' '%s'\n", init_lua, lua_tostring(l, -1));
    } else {
        strcpy(error, "");
    }

    if (main_view.lua_after_load)
        main_view.lua_after_load(&main_view, l);
}

void hotreload(const char *fname, void *ud) {
    trace("hotreload:\n");

    load_init_lua();
    inotifier_watch(init_lua, hotreload, NULL);
}

static void stage_main_init(Stage_Main *st) {
    trace("stage_main_new:\n");
    st->camera.zoom = 1.f;

    modelview_setup.cam = &st->camera;
    modelview_setup.on_init_lua = load_init_lua;

    modelview_init(&main_view, modelview_setup);
    inotifier_watch(init_lua, hotreload, NULL);
    modelview_put(&main_view);

}

static void stage_main_update(Stage_Main *st) {
    /*trace("stage_main_update:\n");*/

    if (IsKeyPressed(KEY_P)) {
        st->is_paused = !st->is_paused;
    }

    camera_process(&st->camera);

    // Если нужно, то установить паузу
    modelview_pause_set(&main_view, st->is_paused);
}

static void stage_main_gui(Stage_Main *st) {
    /*trace("stage_main_gui:\n");*/

    bool open = true;
    igShowAboutWindow(&open);
    modelview_draw_gui(&main_view);
}

static void stage_main_draw(Stage_Main *st) {
    BeginDrawing();
    BeginMode2D(st->camera);
    ClearBackground(RAYWHITE);

    static bool is_ok = false;
    const char *pcall_err = NULL;

    pcall_err = L_pcall(l, "update", &is_ok);
    if (!is_ok)
        strncpy(error, pcall_err, sizeof(error));

    L_pcall(l, "draw_bottom", &is_ok);

    if (main_view.state != MVS_GAMEOVER) {
        input(&main_view);
    }

    modelview_draw(&main_view);

    if (strlen(error))
        DrawText(error, 0, 0, 70, BLACK);

    pcall_err = L_pcall(l, "draw_top", &is_ok);
    if (!is_ok)
        strncpy(error, pcall_err, sizeof(error));

    EndMode2D();
}

static void stage_main_shutdown(Stage_Main *st) {
    trace("stage_main_shutdown:\n");

#ifdef KOH_RLWR
    rlwr_free(rlwr);
#else
    lua_close(l);
#endif

    modelview_shutdown(&main_view);
}

static void stage_main_enter(Stage_Main *st) {
    trace("stage_main_enter:\n");
}

static void stage_main_leave(Stage_Main *st) {
    trace("stage_main_leave:\n");
}

Stage *stage_main_new(HotkeyStorage *hk_store) {
    Stage_Main *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_main_init;
    st->parent.enter = (Stage_callback)stage_main_enter;
    st->parent.leave = (Stage_callback)stage_main_leave;

    st->parent.update = (Stage_callback)stage_main_update;
    st->parent.draw = (Stage_callback)stage_main_draw;
    st->parent.gui = (Stage_callback)stage_main_gui;
    st->parent.shutdown = (Stage_callback)stage_main_shutdown;
    return (Stage*)st;
}
