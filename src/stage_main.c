// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_main.h"

#include "koh_inotifier.h"
#include "modelview.h"
#include "koh_stages.h"
#include "rlwr.h"

typedef struct Stage_Main {
    Stage               parent;
    bool                is_paused;
} Stage_Main;

static struct ModelView main_view;
static Camera2D camera = { 0 };
static lua_State *l = NULL;
static rlwr_t *rlwr = NULL;
static const char *init_lua = "assets/init.lua";
static char error[256] = {};
static void load_init_lua();
static struct Setup main_view_setup = {
    .use_bonus = false,
    .cam = &camera,
    .field_size = 6,
    .tmr_block_time = 0.05,
    .tmr_put_time = 0.05,
    .use_gui = true,
    .auto_put = true,
    .win_value = 2048,
    .on_init_lua = load_init_lua,
};
static bool is_paused = false;


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

// Загрузка Луа машины
static void load_init_lua() {
    if (rlwr) {
        rlwr_free(rlwr);
        rlwr = NULL;
        l = NULL;
    }

    trace("load_init:\n");
    rlwr = rlwr_new();
    l = rlwr_state(rlwr);
    luaL_openlibs(l);

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
        .x = (GetScreenWidth() - width) / 2.,
        .y = (GetScreenWidth() - fontsize) / 2.,
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

//////////////////////////////////////////////

static void stage_main_init(Stage_Main *st) {
    trace("stage_main_new:\n");
    //st->cam.zoom = 1.f;
    camera.zoom = 1.f;

    main_view_setup.color_theme = color_theme_light,
    modelview_init(&main_view, main_view_setup);
    inotifier_watch(init_lua, hotreload, NULL);
    modelview_put(&main_view);

}

static void stage_main_update(Stage_Main *st) {
    /*trace("stage_main_update:\n");*/

    if (IsKeyPressed(KEY_P)) {
        is_paused = !is_paused;
    }

    if (IsKeyPressed(KEY_R)) {
        modelview_shutdown(&main_view);
        modelview_init(&main_view, main_view_setup);
        main_view.on_init_lua = load_init_lua;
        modelview_put(&main_view);
    }

    camera_process();
    timerman_pause(main_view.timers_slides, is_paused);
    timerman_pause(main_view.timers_effects, is_paused);
}

static void stage_main_gui(Stage_Main *st) {
    /*trace("stage_main_gui:\n");*/

    /*
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    bool wnd_open = true;
    igBegin("main", &wnd_open, wnd_flags);
    igEnd();
    */

    modelview_draw_gui(&main_view);
}

static void stage_main_draw(Stage_Main *st) {
    BeginDrawing();
    BeginMode2D(camera);
    ClearBackground(RAYWHITE);

    static bool is_ok = false;
    const char *pcall_err = NULL;

    pcall_err = L_pcall(l, "update", &is_ok);
    if (!is_ok)
        strncpy(error, pcall_err, sizeof(error));

    L_pcall(l, "draw_bottom", &is_ok);

    if (main_view.state != MVS_GAMEOVER)
        input();
    modelview_draw(&main_view);

    draw_scores();

    if (strlen(error))
        DrawText(error, 0, 0, 70, BLACK);

    pcall_err = L_pcall(l, "draw_top", &is_ok);
    if (!is_ok)
        strncpy(error, pcall_err, sizeof(error));

    EndMode2D();

}

static void stage_main_shutdown(Stage_Main *st) {
    trace("stage_main_shutdown:\n");

    rlwr_free(rlwr);
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
