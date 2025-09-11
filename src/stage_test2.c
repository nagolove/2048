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
    // установка значений
    T_INITIAL,
    // ввод пользователя
    T_INPUT,
    // проверка
    T_ASSERT,
    // все сделано
    T_FINISHED,
} TestState;

typedef enum TestStatus {
    TS_UNKNOWN,
    TS_PASSED,
    TS_PARTIALLY,
    TS_FAILED,
} TestStatus;

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

/* XXX: Что мне нужно для Луа реализации?
    Общая структура struct Test
    Индекс в таблице t_01.lua
    Lua машина
 */

static void test_init(Test *t, const char *fname, ModelView *mv) {
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

    /*trace("test_init: [%s]\n", L_stack_dump(t->l));*/

    lua_pushvalue(t->l, -1);

    // debug stuff
    lua_setglobal(t->l, "T");

    /*
    L_inline(t->l,
        "package.path = './?.lua;' .. package.path\n"
        "inspect = require 'inspect'\n"
        "print(inspect(T))\n"
    );
    */

    // debuf stuff

    if (lua_type(t->l, -1) != LUA_TTABLE) {
        trace(
            "test_init: stack top element is not a table [%s]\n",
            L_stack_dump(t->l)
        );
        lua_settop(t->l, 0);
    } else {
        t->len = lua_rawlen(t->l, -1);
        /*trace("test_init: len %d\n", t->len);*/

        /*
        for (int i = 1; i <= t->len; i++) {
            int type = lua_rawgeti(t->l, -1, i);
            printf("type %s\n", lua_typename(t->l, type));
            lua_pop(t->l, 1);
        }
        */

    }

    /*trace("test_init: [%s]\n", L_stack_dump(t->l));*/
    t->index = 1;
    /*trace("test_init: index %d, len %d\n", t->index, t->len);*/
    t->state = T_INITIAL;
}

static void test_shutdown(Test *t) {
    trace("test_shutdown:\n");
    assert(t);
    if (t->l) {
        lua_close(t->l);
        t->l = NULL;
    }
}

static bool test_check_finished(Test *t) {
    assert(t);

    if (t->state != T_FINISHED && t->index >= t->len) {

        printf("test_check_finished: finished\n");
        t->state = T_FINISHED;

        if (t->status != TS_PARTIALLY)
            t->status = TS_PASSED;

        koh_term_color_set(KOH_TERM_BLUE);
        printf("TESTCASE FINISHED\n");
        koh_term_color_reset();

        if (t->on_finish)
            t->on_finish(t);

        return true;
    }

    return false;
}

static void test_input(Test *t) {
    assert(t);

    if (!t->l)
        return;

    if (t->state != T_INPUT) {
        return;
    }

    lua_State *l = t->l;

    if (test_check_finished(t))
        return;

    /*
    printf("test_input 1: [%s]\n", L_stack_dump(l));
    */

    printf("test_input: index %d\n", t->index);

    int type = lua_type(l, -1);

    /*L_inspect(l, -1);*/

    if (type != LUA_TSTRING) {
        trace("test_input: bad type for user input command\n");
        abort();
    }

    /*
    printf("test_input: ready to process\n");
    printf("test_input 2: [%s]\n", L_stack_dump(l));
    */

    const char *cmd = lua_tostring(l, -1);
    Direction dir = str2direction(cmd);
    modelview_input(t->mv, dir);

    t->state = T_ASSERT;

    /*
    printf("test_input: cmd '%s'\n", cmd);
    printf("test_input 3: [%s]\n", L_stack_dump(l));
    */
}

static void test_check_initial(Test *t) {
    const int field_size = t->mv->field_size;

    trace("test_check_initial: before printing\n");
    // TODO: Заполнить поле 

    //printf("lua_rawgeti 1 [%s]\n", L_stack_dump(t->l));
    
    lua_rawgeti(t->l, -1, t->index);

    int type = LUA_TNONE;

    // Считать таблицу с 'size' и пропустить её
    type = lua_getfield(t->l, -1, "size");
    if (type == LUA_TNUMBER) {

        //printf("test_next_state: [%s]\n", L_stack_dump(t->l));
        //printf("test_next_state: size %f\n", lua_tonumber(t->l, -1));
        
        // скинуть число и таблицу
        lua_pop(t->l, 2);

        /*printf("test_next_state: [%s]\n", L_stack_dump(t->l));*/
        /*printf("test_next_state: index %d\n", t->index);*/

        // Перехожу к следующему элементу
        lua_rawgeti(t->l, -1, ++t->index);
    }

    /*printf("lua_rawgeti 2 [%s]\n", L_stack_dump(t->l));*/

    type = lua_type(t->l, -1);

    if (type == LUA_TSTRING) {
        printf("test_next_state: string type\n");
    } else if (type == LUA_TTABLE) {
        /*printf("test_next_state: table type\n");*/

        /*L_inspect(t->l, -1);*/
        int len = luaL_len(t->l, -1);
        assert(len == field_size * field_size);

        for (int i = 1; i <= len; i++) {
            lua_rawgeti(t->l, -1, i);

            if (lua_type(t->l, -1) != LUA_TSTRING) {
                trace(
                    "test_check_initial: "
                    "last argument should be a string, not %s\n",
                    lua_typename(t->l, lua_type(t->l, -1))
                );
            }

            const char *cmd = lua_tostring(t->l, -1);
            int val = -1;

            sscanf(cmd, "%d", &val);
            if (val != -1) {
                // Установить значения клетки
                int x = i % field_size - 1;
                int y = i / field_size;
                /*printf("%s %d %d\n", cmd, x, y);*/
                modelview_put_cell(t->mv, x, y, val);
            }

            // Скинуть элемент поля
            lua_pop(t->l, 1);
        }
        /*printf("\n");*/

        // Скинуть таблицу с элементами поля
        lua_pop(t->l, 1);

        /*L_inspect(t->l, -1);*/
        // Перехожу к следующему элементу
        lua_rawgeti(t->l, -1, ++t->index);
        /*L_inspect(t->l, -1);*/

    } else {
        printf(
            "test_check_initial: unknown type '%s'\n",
            lua_typename(t->l, type)
        );
    }

    t->state = T_INPUT;
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
            const Cell *cell = modelview_search_cell(t->mv, x, y);
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

static void test_check_assert(Test *t) {
    printf("test_check_assert: T_ASSERT\n");

    lua_State *l = t->l;
    const int field_size = t->mv->field_size;

    int type = lua_type(t->l, -1);
    assert(type == LUA_TSTRING);

    // скинуть строку с пользовательским вводом
    lua_pop(l, 1);

    type = lua_rawgeti(t->l, -1, ++t->index);
    assert(type == LUA_TTABLE);

    /*L_inspect(l, -1);*/

    int len = luaL_len(t->l, -1);
    int field[(int)pow(field_size, 2)];
    memset(field, 0, sizeof(field));

    // Заполнить поле
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(l, -1, i);

        // Проверка типа
        type = lua_type(l, -1);
        assert(type == LUA_TSTRING);

        const char *cmd = lua_tostring(l, -1);

        // Игнор первого ассерта в таблице
        if (i == 1 && strcmp(cmd, "assert") == 0) {
            i--;
            lua_pop(l, 1);
        }

        int val = -1;
        sscanf(cmd, "%d", &val);
        field[i - 1] = val;

        lua_pop(l, 1);
    }

    /*
    for (int i = 0; i < len; i++) {
        printf("%d ", field[i]);
    }
    printf("\n");
    */

    term_print_field(t, field);
    term_print_field_ecs(t);

    int index = 0;
    for (int y = 0; y < field_size; y++) {
        for (int x = 0; x < field_size; x++) {
            int v = field[index];

            const Cell *cell = modelview_search_cell(t->mv, x, y);
            if (v != -1) {
                if (!cell) {
                    t->state = T_FINISHED;
                    t->status = TS_FAILED;
                    lua_settop(l, 0);
                    return;
                }
                if (cell->value != v) {
                    t->state = T_FINISHED;
                    t->status = TS_FAILED;
                    lua_settop(l, 0);
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

    koh_term_color_set(KOH_TERM_GREEN);
    printf("passed\n");
    koh_term_color_reset();

    // скинуть таблицу с полем
    lua_pop(l, 1);

    // Перехожу к следующему элементу
    lua_rawgeti(t->l, -1, ++t->index);

    /*L_inspect(l, -1);*/

    t->state = T_INPUT;
    t->status = TS_PASSED;
}

static void test_next_state(Test *t) {
    assert(t);

    if (!t->l)
        return;

    // trace("test_next_state: index %d, len %d\n", t->index, t->len);

    if (test_check_finished(t))
        return;

    if (t->state == T_INITIAL) {
        test_check_initial(t);
    } else if (t->state == T_ASSERT) {
        test_check_assert(t);
    }
}

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

static void test_gui_init(TestGui *tg) {
    FilesSearchSetup setup = {
        .path = "./",
        .deep = 0,
        .regex_pattern = "t_\\d\\d\\.lua",
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

        /*L_inspect(l, -1);*/
        /*printf("\n\n");*/

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

static void stage_test_init(Stage_Test *st) {
    st->camera.zoom = 1.f;

    /*tracec("hello %{green} world %{reset}\n");*/

    modelview_setup.cam = &st->camera;
    //main_view_setup.on_init_lua = load_init_lua;

    /*reinit(1);*/
    test_view.auto_put = false;

    st->t = (Test){};
    /*test_init(&st->t, "t_01.lua", &test_view);*/
    test_gui_init(&st->tg);
}

static void stage_test_update(Stage_Test *st) {
    /*trace("stage_test_update:\n");*/

    if (test_view.inited)
        modelview_pause_set(&test_view, st->is_paused);

    /*t_input(&test_view, &sets[set_current]);*/

    if (st->timersnum == 0)
        test_input(&st->t);
}

static void on_finish_next(Test *t) {
    printf("on_finish_next: %s\n", status2str[t->status]);

    TestGui *tg = t->udata;
    if (tg->load_index)
        tg->stats[*tg->load_index] = t->status;

    if (tg->load_index && *tg->load_index + 1 < tg->search.num) {
        (*tg->load_index)++;

        const char *fname = tg->search.names[(*tg->load_index)];
        printf("on_finish_next: fname %s\n", fname);
        modelview_shutdown(&test_view);
        modelview_init(&test_view, modelview_setup);
        test_view.auto_put = false;
        test_shutdown(t);
        test_init(t, fname, &test_view);

        t->udata = tg;
        t->on_finish = on_finish_next;
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

        modelview_shutdown(&test_view);
        modelview_init(&test_view, modelview_setup);
        test_view.auto_put = false;
        test_shutdown(t);
        test_init(t, fname, &test_view);

        // Поместить информацию о статусе теста в статическую переменную при
        // завершении выполнения
        t->udata = tg;
        t->on_finish = on_finish;
    }
    igSameLine(0., 10.);

    // Запуск нескольких примеров
    if (igButton("run all", z)) {
        memset(selected, 0, sizeof(tg->selected));
        memset(stats, 0, sizeof(tg->stats));

        *tg->load_index = 0;

        printf("test_gui: load_index %d\n", *tg->load_index);

        const char *fname = tg->search.names[0];
        modelview_shutdown(&test_view);
        modelview_init(&test_view, modelview_setup);
        test_view.auto_put = false;
        test_shutdown(t);
        test_init(t, fname, &test_view);

        // Поместить информацию о статусе теста в статическую переменную при
        // завершении выполнения
        t->udata = tg;
        t->on_finish = on_finish_next;
    }

    igSameLine(0., 10.);
    if (igButton("reset bullets", z)) {
        memset(selected, 0, sizeof(tg->selected));
        memset(stats, 0, sizeof(tg->stats));
    }

    igEnd();
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
    /*t_gui(st);*/
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

    /*
    if (!timersnum)
        t_assert(&test_view, &sets[set_current]);
        */

    if (!st->timersnum)
        test_next_state(&st->t);

    /*
    if (IsKeyPressed(KEY_T))
        t_assert(&test_view, &sets[set_current]);
    */

    EndMode2D();
}

static void stage_test_shutdown(Stage_Test *st) {
    trace("stage_test_shutdown:\n");
    modelview_shutdown(&test_view);
    test_shutdown(&st->t);
    test_gui_shutdown(&st->tg);
}

Stage *stage_test2_new(HotkeyStorage *hk_store) {
    //assert(hk_store);
    Stage_Test *st = calloc(1, sizeof(*st));
    if (!st) {
        printf("stage_test2_new: bad allocation\n");
        koh_fatal();
    } else {
        st->parent.data = hk_store;

        st->parent.init = (Stage_callback)stage_test_init;
        //st->parent.enter = (Stage_callback)stage_test_enter;
        //st->parent.leave = (Stage_callback)stage_test_leave;

        st->parent.update = (Stage_callback)stage_test_update;
        st->parent.draw = (Stage_callback)stage_test_draw;
        st->parent.gui = (Stage_callback)stage_test_gui;
        st->parent.shutdown = (Stage_callback)stage_test_shutdown;
    }
    return (Stage*)st;
}
