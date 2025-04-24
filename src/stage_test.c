// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_test.h"

#include "koh_lua.h"
#include "koh_common.h"
#include "common.h"
#include "lua.h"
#include "modelview.h"
#include "koh_stages.h"

static struct ModelView test_view;

typedef enum TestState {
    T_PROCESS,
    T_READY_FINISH,
    T_FINISHED,
} TestState;

typedef enum TestStatus {
    TS_UNKNOWN,
    TS_PASSED,
    TS_PARTIALLY,
    TS_FAILED,
} TestStatus;

static const char *state2str[] = {
    [T_PROCESS] = "PROCESS",
    [T_READY_FINISH] = "READY_FINISH",
    [T_FINISHED] = "FINISHED",
};

static const char *status2str[] = {
    [TS_UNKNOWN] = "UNKNOWN",
    [TS_PASSED] = "PASSED",
    [TS_PARTIALLY] = "PARTIALLY",
    [TS_FAILED] = "FAILED",
};

typedef struct Test {
    lua_State  *l;
              // индекс в таблице теста
    int        index,
              // количество элементов в таблице
              len;
    enum       TestState state;
    ModelView  *mv;
    TestStatus status;

    void       (*on_finish)(struct Test *t);
    void       *udata;
} Test;

typedef struct TestGui {
    // Флажки выбранного теста
    bool              selected[1024];
    TestStatus        stats[1024];
    char              *description[1024];
    int               *load_index;
    FilesSearchResult search;
} TestGui;

typedef struct Stage_Test {
    Stage               parent;
    int                 timersnum;
    Camera2D            camera;
    bool                is_paused;
    // Хранит один тест
    Test                t;
    // Окошко с выбором вариантов
    TestGui             tg;
} Stage_Test;

static void test_init_ex(Test *t, const char *fname, ModelView *mv) {
    assert(t);
    assert(mv);
    trace("test_init: fname '%s'\n", fname);

    memset(t, 0, sizeof(*t));

    t->mv = mv;
    t->l = luaL_newstate();
    luaL_openlibs(t->l);

    int err = luaL_dofile(t->l, fname);
    if (err != LUA_OK) {
        trace(
            "test_init: could not load '%s' with '%s'\n",
            fname, lua_tostring(t->l, -1)
        );
        lua_close(t->l);
        t->l = NULL;
    }

    lua_State *l = t->l;
    t->len = lua_rawlen(l, -1);

    printf("test_init_ex: len %d\n", t->len);

    int type;

    type = lua_type(l, -1);
    assert(type == LUA_TTABLE);

    lua_rawgeti(l, -1, 1);
    assert(type == LUA_TTABLE);

    type = lua_getfield(l, -1, "id");

    assert(type == LUA_TSTRING);
    const char *id = lua_tostring(l, -1);

    if (strcmp(id, "options") == 0) {
        t->index++;
        printf("test_init_ex: options found\n");
        // XXX: Что-то сделать
    }

    // скинуть 'id' и таблицу с опциями
    lua_pop(l, 2);

    t->index++;
    t->state = T_PROCESS;
}

static void test_shutdown(Test *t) {
    trace("test_shutdown:\n");
    assert(t);
    if (t->l) {
        lua_close(t->l);
        t->l = NULL;
    }
}

static void term_print_field(Test *t, int *field) {
    const int field_size = t->mv->field_size;
    koh_term_color_set(KOH_TERM_BLUE);
    // Печатать необходимое поле
    int index = 0;
    for (int y = 0; y < field_size; y++) {
        for (int x = 0; x < field_size; x++) {
            int v = field[index];
            if (v != -1) 
                printf("%-2d ", v);
            else
                printf(" . ");
            index++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
}

static void term_print_field_ecs(Test *t) {
    const int field_size = t->mv->field_size;
    koh_term_color_set(KOH_TERM_YELLOW);
    // печатать имеющееся поле
    for (int y = 0; y < field_size; y++) {
        for (int x = 0; x < field_size; x++) {
            Cell *cell = modelview_search_cell(t->mv, x, y);
            if (cell) {
                printf("%-2d ", cell->value);
            } else {
                printf(" . ");
            }
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
}

static void test_id_initial(Test *t) {
    assert(t);
    assert(t->mv);

    const int field_size = t->mv->field_size;
    int field[(int)pow(field_size, 2)];
    lua_State *l = t->l;
    int len = luaL_len(t->l, -1);
    assert(len == field_size * field_size);

    printf("test_id_initial: len %d\n", len);

    memset(field, 0, sizeof(field));

    int x = 0, y = 0;
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(l, -1, i);

        assert(lua_type(l, -1) == LUA_TSTRING);
        const char *cmd = lua_tostring(l, -1);
        int val = -1;

        sscanf(cmd, "%d", &val);
        if (val > 0) {
            // Установить значения клетки
            modelview_put_cell(t->mv, x, y, val);
        }
        field[i - 1] = val;
        
        x += 1;
        if (x == field_size) {
            y++;
            x = 0;
        }

        // Скинуть элемент поля
        lua_pop(l, 1);
    }

    koh_term_color_set(KOH_TERM_CYAN);
    term_print_field(t, field);
    koh_term_color_reset();
}

static void test_id_input(Test *t) {
    /*printf("test_id_input\n");*/

    lua_State *l = t->l;

    /*int len = luaL_len(l, -1);*/
    // TODO: Ввод нескольких значений. Нужно ждать конца таймеров
    int len = 1;

    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(l, -1, i);
        const char *cmd = lua_tostring(l, -1);
        printf("test_id_input: '%s'\n", cmd);
        Direction dir = str2direction(cmd);
        modelview_input(t->mv, dir);
        lua_pop(l, 1);
    }

}

static void test_id_function(Test *t) {
    printf("test_id_function\n");

    lua_State *l = t->l;
    int type = lua_rawgeti(l, -1, 1);
    assert(type == LUA_TFUNCTION);

    lua_call(l, 0, 0);
}

static void test_id_assert(Test *t) {
    printf("test_id_assert\n");

    lua_State *l = t->l;
    const int field_size = t->mv->field_size;
    int len = luaL_len(t->l, -1);
    assert(len == field_size * field_size);
    int field[(int)pow(field_size, 2)];
    memset(field, 0, sizeof(field));

    // Заполнить поле
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(l, -1, i);

        // Проверка типа
        int type = lua_type(l, -1);
        assert(type == LUA_TSTRING);

        const char *cmd = lua_tostring(l, -1);

        int val = -1;
        sscanf(cmd, "%d", &val);
        field[i - 1] = val;

        lua_pop(l, 1);
    }

    printf("field:\n");
    term_print_field(t, field);
    printf("ecs:\n");
    term_print_field_ecs(t);

    t->status = TS_PASSED;

    int index = 0;
    for (int y = 0; y < field_size; y++) {
        for (int x = 0; x < field_size; x++) {
            int v = field[index];

            Cell *cell = modelview_search_cell(t->mv, x, y);
            if (v != -1) {
                if (!cell) {
                    t->state = T_READY_FINISH;
                    t->status = TS_FAILED;
                    return;
                }
                if (cell->value != v) {
                    t->state = T_READY_FINISH;
                    t->status = TS_FAILED;
                    return;
                }
            } else {
                /*assert(cell == NULL);*/
                /*printf("test_check_assert: strange cell on [%d, %d]\n", x, y);*/
                /*t->status = TS_PARTIALLY;*/
            }

            index++;
        }
    }

    if (t->status == TS_FAILED) {
        koh_term_color_set(KOH_TERM_RED);
        printf("failed\n");
        koh_term_color_reset();
    } else {
        koh_term_color_set(KOH_TERM_GREEN);
        printf("passed\n");
        koh_term_color_reset();
    }

}

static void test_next_state_ex(Test *t) {
    assert(t);

    if (!t->l) 
        return;

    if (t->state == T_FINISHED)
        return;

    printf("test_next_state_ex: state %s\n", state2str[t->state]);

    int type;
    lua_State *l = t->l;

    if (t->index == t->len) {
        printf("test_next_state_ex: len %d, index %d\n", t->len, t->index);

        koh_term_color_set(KOH_TERM_BLUE);
        printf("TESTCASE FINISHED\n");
        koh_term_color_reset();

        t->state = T_READY_FINISH;
    }

    if (t->state == T_READY_FINISH) {
        t->state = T_FINISHED;
        if (t->on_finish)
            t->on_finish(t);
    }

    if (t->state == T_PROCESS) {
        type = lua_rawgeti(l, -1, t->index++);
        if (type != LUA_TTABLE) {

            /*
            printf("[%s]\n", L_stack_dump(l));
            lua_getglobal(l, "_G");
            L_inspect(l, -1);
            */

            printf(
                "test_next_state_ex: type is '%s'\n",
                lua_typename(l, type)
            );
            exit(EXIT_FAILURE);
        }

        type = lua_getfield(l, -1, "id");
        assert(type == LUA_TSTRING);
        const char *id = lua_tostring(l, -1);
        lua_pop(l, 1);

        printf("id '%s'\n", id);

        const struct {
            void (*foo)(Test *t);
            const char *id;
        } x[] = {
            { test_id_initial, "initial" },
            { test_id_input, "input" },
            { test_id_assert, "assert" },
            { test_id_function, "function" },
            { NULL, NULL, },
        };

        bool done = false;

        for(int i = 0; x[i].foo; i++) {
            if (strcmp(id, x[i].id) == 0) {
                // Нужная таблица лежит на вершине
                // Скидывать таблицу в функции не нужно
                int top1 = lua_gettop(l);
                x[i].foo(t);
                int top2 = lua_gettop(l);

                if (top1 != top2) {
                    printf(
                        "test_next_state_ex: top1 %d != top2 %d\n",
                        top1, top2
                    );
                    exit(EXIT_FAILURE);
                }

                done = true;
                break;
            }
        }

        if (!done) {
            koh_term_color_set(KOH_TERM_RED);
            printf("test_next_state_ex: bad id '%s'\n", id);
            koh_term_color_reset();
        }

        lua_pop(l, 1);
    }
}

static void test_gui_init(TestGui *tg) {

    FilesSearchSetup setup = {
        .path = "./",
        .deep = 0,
        .regex_pattern = "t_\\d\\dex.lua",
    };

    tg->search = koh_search_files(&setup);

    for (int i = 0; i < tg->search.num; i++) {
        lua_State *l = luaL_newstate();
        luaL_openlibs(l);
        const char *fname = tg->search.names[i];
        int err = luaL_dofile(l, fname);
        if (err != LUA_OK) {
            printf(
                "test_gui_init: error '%s' in '%s'\n",
                fname, lua_tostring(l, -1)
            );
        }

        lua_rawgeti(l, -1, 1);
        int type = lua_getfield(l, -1, "description");
        if (type == LUA_TSTRING) {
            const char *desc = lua_tostring(l, -1);
            assert(desc);
            tg->description[i] = strdup(desc);
        }

        lua_close(l);
    }
}

static void test_gui_shutdown(TestGui *tg) {
    koh_search_files_shutdown(&tg->search);
    int num = sizeof(tg->description) / sizeof(tg->description[0]);
    for (int i = 0; i < num; i++) {
        if (tg->description[i]) {
            free(tg->description[i]);
            tg->description[i] = NULL;
        }
    }
}

static void test_reinit(
    Test *t, TestGui *tg, void (*on_finish_func)(Test *tg)
) {
    const char *fname = tg->search.names[(*tg->load_index)];
    printf("on_finish_next: fname %s\n", fname);
    modelview_shutdown(&test_view);

    // TODO: Добавить сюда применение поля size
    modelview_init(&test_view, modelview_setup);
    test_view.auto_put = false;
    test_shutdown(t);

    test_init_ex(t, fname, &test_view);

    t->udata = tg;
    t->on_finish = on_finish_func;
}

static void stage_test_init(Stage_Test *st) {
    st->camera.zoom = 1.f;

    modelview_setup.cam = &st->camera;
    test_view.auto_put = false;

    st->t = (Test){};
    test_gui_init(&st->tg);
}

static void stage_test_update(Stage_Test *st) {
    if (test_view.inited)
        modelview_pause_set(&test_view, st->is_paused);
}

static void on_finish_next(Test *t) {
    printf("on_finish_next: %s\n", status2str[t->status]);

    TestGui *tg = t->udata;
    if (tg->load_index)
        tg->stats[*tg->load_index] = t->status;

    if (tg->load_index && *tg->load_index + 1 < tg->search.num) {
        (*tg->load_index)++;

        test_reinit(t, tg, on_finish_next);
    }
}

static void on_finish(Test *t) {
    printf("on_finish: %s\n", status2str[t->status]);
    TestGui *tg = t->udata;
    if (tg->load_index)
        tg->stats[*tg->load_index] = t->status;
}

static void test_gui(Test *t, TestGui *tg) {
    assert(t);
    assert(tg);

    bool wnd_open = true;
    igBegin("test_gui", &wnd_open, 0);
    const ImVec2 z = {};


    int selected_num = sizeof(tg->selected) / sizeof(tg->selected[0]);
    bool *selected = tg->selected;
    TestStatus *stats = tg->stats;

    assert(selected_num > tg->search.num);

    const ImVec4 colors[] = {
        [TS_UNKNOWN] = {0.5f, .5f, 0.5f, 1.0f},
        [TS_PASSED] = {0.0f, 1.0f, 0.0f, 1.0f},
        [TS_FAILED] = {1.0f, 0.0f, 0.0f, 1.0f},
    };

    static int load_index = -1;
    tg->load_index = &load_index;

    if (igBeginListBox("files", (ImVec2){0.f, 500.f})) {
        for (int i = 0; i < tg->search.num; ++i) {

            if (stats[i] != TS_PARTIALLY) {
                igPushStyleColor_Vec4(ImGuiCol_Text, colors[stats[i]]);
                igBullet();
                igPopStyleColor(1);
            } else {
                igPushStyleColor_Vec4(ImGuiCol_Text, colors[TS_FAILED]);
                igBullet();
                igPopStyleColor(1);
                igSameLine(0.f, 0.f);
                igPushStyleColor_Vec4(ImGuiCol_Text, colors[TS_PASSED]);
                igBullet();
                igPopStyleColor(1);
            }

            igSameLine(0., 0.);

            if (igSelectable_BoolPtr(tg->search.names[i], &selected[i], 0, z)) {
                load_index = i;
                for (int j = 0; j < tg->search.num; j++) {
                    if (i != j)
                        selected[j] = false;
                }
                break;
            }

            if (tg->description[i]) {
                igSameLine(0., 10.f);
                igText("|%s|", tg->description[i]);
            }

        }

        igEndListBox();
    }

    if (igButton("run", z) && load_index != -1) {
        const char *fname = tg->search.names[load_index];
        printf("test_gui: fname '%s'\n", fname);

        test_reinit(t, tg, on_finish);
    }
    igSameLine(0., 10.);

    // Запуск нескольких примеров
    if (igButton("run all", z)) {
        memset(selected, 0, sizeof(tg->selected));
        memset(stats, 0, sizeof(tg->stats));

        *tg->load_index = 0;

        printf("test_gui: load_index %d\n", *tg->load_index);
        test_reinit(t, tg, on_finish_next);
    }

    igSameLine(0., 10.);
    if (igButton("reset bullets", z)) {
        memset(selected, 0, sizeof(tg->selected));
        memset(stats, 0, sizeof(tg->stats));
    }

    igEnd();
}

static void stage_test_gui(Stage_Test *st) {
    modelview_draw_gui(&test_view);
    test_gui(&st->t, &st->tg);
}

static void stage_test_draw(Stage_Test *st) {
    BeginDrawing();
    BeginMode2D(st->camera);
    ClearBackground(RAYWHITE);

    if (test_view.state != MVS_GAMEOVER) {
        input(&test_view);
    }

    st->timersnum = modelview_draw(&test_view);

    if (!st->timersnum)
        test_next_state_ex(&st->t);

    EndMode2D();
}

static void stage_test_shutdown(Stage_Test *st) {
    trace("stage_test_shutdown:\n");
    modelview_shutdown(&test_view);
    test_shutdown(&st->t);
    test_gui_shutdown(&st->tg);
}

static void stage_test_enter(Stage_Test *st) {
    trace("stage_test_enter:\n");
}

static void stage_test_leave(Stage_Test *st) {
    trace("stage_test_leave:\n");
}

Stage *stage_test_new(HotkeyStorage *hk_store) {
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
