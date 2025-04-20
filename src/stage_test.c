// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_test.h"

#include "koh_lua.h"
#include "koh_common.h"
#include "common.h"
#include "modelview.h"
#include "koh_stages.h"

static struct ModelView test_view;

// Размер поля
#define T_FIELD_SIZE 6
// Размер массива строк описывающих тест 
#define T_X_SIZE \
    T_FIELD_SIZE * T_FIELD_SIZE * 2 + 1 + 1

typedef struct TestSet {
    // Был ли произведен ввод?
    bool input_done, assert_done;
    // Индекс в массиве x, где находится курсор считывающий команды в данный
    // момент
    int idx;
    const char *description;
    // для imgui
    bool selected;
    // Входные данные
    char *x[T_X_SIZE];
} TestSet;

// XXX: Можно-ли сделать так, что все одинаковые подряд цифры сложатся?
// {{{
static TestSet sets[] = {

    // {{{
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
        .description = "проба, нулевой",
    },
    // }}}

    // {{{
    { 
        .x = {
            // state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            // user input
            "right", 
            // assert state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            NULL,
        },
        .description = "первый, пустой",
    },
    // }}}

    { 
        .x = {
            // state
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            // user input
            "down", 
            // assert state
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", "16", " ", " ",
            " ", " ", " ", "32", " ", " ",
            NULL,
        },
        .description = "второй",
    },

    // TODO: Сделать возможной последовательность из нескольких движений 
    // для одного начального состояния.
    { 
        .x = {
            // state
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            // user input
            "down", 
            // assert state
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", "32", "16","16", " ", " ",
            " ", "8",  "4", "32", " ", " ",
            NULL,
        },
        .description = "третий",
        .selected = true,
    },

    { 
        // {{{
        .x = {
            // XXX: Как можно было-бы сделать тест загружаемым из файла?

            // XXX: Использовать Lua или нет?
            // + Загрузка одним вызовом
            // + Поддержка сложной структуры
            // - Не так удобно редактировать

            // XXX: Убрать все тесты из кода и перенести в файлы?
            // Удобно редактировать длинные последовательности

            /*
           
            --[[
            хранить индекс массива i
            перечисление текущего состояние
            enum State {
                STATE,
                INPUT,
                ASSERT
            };

            --]]

return {
    state = {
        " ", "8", "4", "8", " ", " ",
        " ", "8", "4", "8", " ", " ",
        " ", "4", "2", "8", " ", " ",
        " ", "4", "2", "8", " ", " ",
        " ", "8", "4", "8", " ", " ",
        " ", "8", "4", "8", " ", " ",
    },
    user_input = "down",
    assert_state = {
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", "32", "16","16", " ", " ",
    " ", "8",  "4", "32", " ", " ",
    },

    --[[
XXX: Проблема с начальным состоянием и последующими. Все последующие состояния
являются проверочными
    --]]

    // state
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", "32", "16","16", " ", " ",
    " ", "8",  "4", "32", " ", " ",
    // user input
    "right", 
    // assert state
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", " ",
    " ", " ",  " ", " ",  " ", "64",
    " ", " ",  " ", "8",  "4", "32",
}

             */

            // state
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            // user input
            "down", 
            // assert state
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", "32", "16","16", " ", " ",
            " ", "8",  "4", "32", " ", " ",

            /*
            // state
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", "32", "16","16", " ", " ",
            " ", "8",  "4", "32", " ", " ",
            // user input
            "right", 
            // assert state
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", "64",
            " ", " ",  " ", "8",  "4", "32",
            */

            NULL,
        },
        // }}}
        .description = "длинный",
        .selected = true,
    },

};
// }}}

const static int sets_num = sizeof(sets) / sizeof(sets[0]);
static int set_current = 0;

typedef enum TestState {
    T_INITIAL,
    T_INPUT,
    T_ASSERT,
    T_FINISHED,
} TestState;

typedef struct Test {
    lua_State *l;
              // индекс в таблице теста
    int       index,
              // количество элементов в таблице
              len;
    enum      TestState state;
} Test;

/* XXX: Что мне нужно для Луа реализации?
    Общая структура struct Test
    Индекс в таблице t_01.lua
    Lua машина
 */

static void test_init(Test *t, const char *fname) {
    assert(t);
    trace("test_init:\n");
    memset(t, 0, sizeof(*t));
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

    trace("test_init: [%s]\n", L_stack_dump(t->l));

    lua_pushvalue(t->l, -1);

    // debug stuff
    lua_setglobal(t->l, "T");
    l_inline(t->l,
        "package.path = './?.lua;' .. package.path\n"
        "inspect = require 'inspect'\n"
        "print(inspect(T))\n"
    );
    // debuf stuff

    if (lua_type(t->l, -1) != LUA_TTABLE) {
        trace(
            "test_init: stack top element is not a table [%s]\n",
            L_stack_dump(t->l)
        );
        lua_settop(t->l, 0);
    } else {
        t->len = lua_rawlen(t->l, -1);
        trace("test_init: len %d\n", t->len);

        for (int i = 1; i <= t->len; i++) {
            int type = lua_rawgeti(t->l, -1, i);
            printf("type %s\n", lua_typename(t->l, type));
            lua_pop(t->l, 1);
        }
    }

    trace("test_init: [%s]\n", L_stack_dump(t->l));
    t->index = 1;
    trace("test_init: index %d, len %d\n", t->index, t->len);
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

static void test_input(Test *t) {
    /*trace("test_input:\n");*/
}

static void test_next_state(Test *t) {
    assert(t);
    // trace("test_next_state: index %d, len %d\n", t->index, t->len);

    if (t->state != T_FINISHED && t->index == t->len) {
        printf("test_next_state: finished\n");
        t->state = T_FINISHED;
        return;
    }

    if (t->state == T_INITIAL) {
        trace("test_next_state: before printing\n");
        // TODO: Заполнить поле 

        printf("lua_rawgeti 1 [%s]\n", L_stack_dump(t->l));
        lua_rawgeti(t->l, -1, t->index);

        int type = LUA_TNONE;

        // XXX: Считать 'size' и пропустить эту таблицу
        type = lua_getfield(t->l, -1, "size");
        if (type == LUA_TNUMBER) {
            printf("test_next_state: [%s]\n", L_stack_dump(t->l));
            printf("test_next_state: size %f\n", lua_tonumber(t->l, -1));
            // скинуть число и таблицу
            lua_pop(t->l, 2);
            printf("test_next_state: [%s]\n", L_stack_dump(t->l));
           
            printf("test_next_state: index %d\n", t->index);
            lua_rawgeti(t->l, -1, t->index);
        }

        printf("lua_rawgeti 2 [%s]\n", L_stack_dump(t->l));

        type = lua_type(t->l, -1);

        if (type == LUA_TSTRING) {
            printf("test_next_state: string type\n");
        } else if (type == LUA_TTABLE) {
            printf("test_next_state: table type\n");

            printf("[%s]\n", L_stack_dump(t->l));
            lua_pushvalue(t->l, -1);
            lua_setglobal(t->l, "TMP");
            printf("[%s]\n", L_stack_dump(t->l));

            l_inline(t->l, 
                "print('inspect', inspect)\n"
                "print('TMP', inspect(TMP))"
            );

        } else {
            printf(
                "test_next_state: unknown type '%s'\n",
                lua_typename(t->l, type)
            );
        }

        t->state = T_INPUT;
    }
}

static void t_set(ModelView *mv, TestSet *ts) {
    // {{{
    assert(mv);
    assert(mv->field_size == T_FIELD_SIZE);
    assert(ts);

    int i = 0;
    while (ts->x[i++]);
    assert(i == T_X_SIZE);

    ts->input_done = false;
    ts->idx = 0;

    int *idx = &ts->idx;
    assert(*idx >= 0);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            int val = -1;
            sscanf(ts->x[*idx], "%d", &val);
            /*trace("t_set: val %d\n", val);*/
            if (val != -1)
                modelview_put_cell(mv, x, y, val);
            (*idx)++;
        }
    }
    // }}}
}

__attribute__((unused))
static void t_assert(ModelView *mv, TestSet *ts) {
    // {{{
    assert(mv);
    assert(ts);

    // Анимация должна закончиться
    if (mv->state != MVS_READY)
        return;

    // Проверка производится только после ввода
    if (!ts->input_done) {
        return;
    }

    // Проверка производится только один раз
    if (ts->assert_done)
        return;

    trace("t_assert: '%s'\n", ts->description);
    trace("t_assert: idx %d\n", ts->idx);
    //trace("t_assert: x '%s'\n", ts->x[ts->idx]);

    // Напечатать стартовое поле
    int other_idx = 0;

    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("initial\n");

    koh_term_color_set(KOH_TERM_BLUE);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            const char *val = ts->x[other_idx];
            if (strcmp(val, " ") == 0)
                val = ".";
            //printf("%s ", val);
            printf("%-4s ", val);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Поле напечатано

    printf("direction '%s'\n", ts->x[other_idx]);

    // Напечатать поле
    other_idx = ts->idx;

    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("should be\n");

    koh_term_color_set(KOH_TERM_BLUE);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            const char *val = ts->x[other_idx];
            if (strcmp(val, " ") == 0)
                val = ".";
            /*printf("%s ", val);*/
            printf("%-4s ", val);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Поле напечатано

    // TODO: Напечатать текущее состояние
    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("state is\n");

    other_idx = ts->idx;
    koh_term_color_set(KOH_TERM_YELLOW);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            Cell *cell = modelview_search_cell(mv, x, y);
            char buf[16] = {};
            if (!cell) {
                sprintf(buf, "%s", ". ");
            } else {
                sprintf(buf, "%d ", cell->value);
            }
            printf("%-4s ", buf);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Состояние напечатано

    ts->assert_done = true;

    int *idx = &ts->idx;
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            int val = -1;
            sscanf(ts->x[*idx], "%d", &val);

            /*trace("t_assert: val %d\n", val);*/

            if (val != -1) {
                //modelview_put_cell(mv, x, y, val);
                Cell *cell = modelview_search_cell(mv, x, y);
                if (!cell) {
                    koh_term_color_set(KOH_TERM_RED);
                    trace("t_assert: no cell at [%d, %d]\n", x, y);
                    koh_term_color_reset();

                    /*exit(EXIT_FAILURE);*/
                    return;
                }

                if (cell->value != val) {
                    trace(
                        "t_assert: cell->value == %d, "
                        "but should be equal %d\n",
                        cell->value,
                        val
                    );

                    /*exit(EXIT_FAILURE);*/
                    return;
                }
            }

            (*idx)++;
        }
    }

    koh_term_color_set(KOH_TERM_GREEN);
    trace("t_assert: passed\n");
    koh_term_color_reset();

    // }}}
}

static void t_input(ModelView *mv, TestSet *ts) {
    // {{{
    // Закончилась-ли анимация?
    if (mv->state != MVS_READY)
        return;

    if (!ts->input_done) {
        const char *dirstr = ts->x[ts->idx++];
        trace("t_input: applying direction '%s'\n", dirstr);
        enum Direction dir = str2direction(dirstr);
        modelview_input(mv, dir);
        ts->input_done = true;
    }
    // }}}
}

typedef struct Stage_Test {
    Stage               parent;
    Camera2D            camera;
    bool                is_paused;
    Test                t;
} Stage_Test;

// Установить тест из массива sets по индексу set_index
static void reinit(int set_index) {

    for (int j = 0; j < sets_num; j++) {
        sets[j].input_done = false;
        sets[j].assert_done = false;
        sets[j].idx = 0;
    }

    modelview_shutdown(&test_view);
    modelview_init(&test_view, modelview_setup);
    // Отключение автоматического создания фишек на следующем ходе
    test_view.auto_put = false;
    set_current = set_index;

    ModelView *mv = &test_view;
    t_set(mv, &sets[set_current]);
}

static void stage_test_init(Stage_Test *st) {
    trace("stage_test_init: T_X_SIZE %d \n", (int)T_X_SIZE);
    st->camera.zoom = 1.f;

    modelview_setup.cam = &st->camera;
    //main_view_setup.on_init_lua = load_init_lua;

    reinit(1);
    test_init(&st->t, "t_01.lua");
}

static void stage_test_update(Stage_Test *st) {
    /*trace("stage_test_update:\n");*/
    modelview_pause_set(&test_view, st->is_paused);
    t_input(&test_view, &sets[set_current]);
    test_input(&st->t);
}

static void t_gui(Stage_Test *st) {
    // {{{
    bool open = true;
    
    igBegin("t_gui", &open, 0);

    if (igBeginListBox("tests", (ImVec2){})) {
        for (int i = 0; i < sets_num; i++) {
            const char *label = sets[i].description;
            bool *selected = &sets[i].selected;
            igSelectable_BoolPtr(label, selected, 0, (ImVec2){});
            if (*selected) {
                for (int j = 0; j < sets_num; j++) {
                    if (j != i)
                        sets[j].selected = false;
                }
            }
        }
        igEndListBox();
    }

    if (igButton("run", (ImVec2){})) {
        for (int j = 0; j < sets_num; j++) {
            if (sets[j].selected) {
                reinit(j);
            }
        }
    }

    igEnd();
    // }}}
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
    t_gui(st);
}

static void stage_test_draw(Stage_Test *st) {
    BeginDrawing();
    BeginMode2D(st->camera);
    ClearBackground(RAYWHITE);

    if (test_view.state != MVS_GAMEOVER) {
        input(&test_view);
    }

    int timersnum = modelview_draw(&test_view);
    //trace("stage_test_draw: timersnum %d\n", timersnum);

    // TODO: Стоит-ли делать окошко с imgui для выбора теста?

    /*
    if (!timersnum)
        t_assert(&test_view, &sets[set_current]);
        */

    if (!timersnum)
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
