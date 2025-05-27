// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "test_suite.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

// {{{
#include "koh_hotkey.h"
#include "koh_common.h"
#include "koh_lua.h"
#include "cimgui.h"
#include "cimgui_impl.h"
#include "colored_text.h"
#include "koh_common.h"
#include "koh_ecs.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "modeltest.h"
#include "modelview.h"
#include "raylib.h"
#include "raymath.h"
#include "koh_timerman.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "koh_strbuf.h"
#include "koh_lua.h"
#include "common.h"
#include "koh_render.h"
// }}}

/*
 INFO: Добавлять бомбу, которая может уничтожить самыый большой элемент
 INFO: Добавлять второй элемент только тогда когда и только туда, где он
 может быть уничтожен. Успех зависит от игрока - ошибется он или примет
 верное решение
 XXX:Количество цифр должно зависеть от совбодной площади
 TODO: Чем более крупная фишка есть на поле, тем более крупная может выпадать
 ячейка, но до определенного предела.
 TODO: Взрыв бомбы от другой бомбы
 TODO: Условие выпадения второй бомбы(для последовательной детонации)
 Вторая бомба должна редко должна когда есть открытая первая бомба.
 TODO: Ползунок сложности. Сложность возрастает нелинейно. 
 Появляются преграды, добавлются бомбы.
 На легком и среднем уровне есть подсказки
 TODO: Отключаемость анимации подложки
 TODO: Делать подсказку наиболее продуктивного хода
 TODO: Непроходимые блоки
 TODO: Создавать бомбы после пары движений без сложения
 TODO: Опциональная подпись с количеством движений бомбы
 Один раунд - с делением от 2048 до 2
*/

// TODO: Перенести в структуру ModelView
static bool is_draw_grid = true;

typedef struct TimerData {
    ModelView   *mv;
    e_id        e;
} TimerData;

typedef struct DrawOpts {
    int                 fontsize;
    Vector2             caption_offset_coef; // 1. is default value
    bool                custom_color;
    Color               color;
    // XXX: За что отвечает?
    double              amount;             // 0 .. 1.
} DrawOpts;

static void field_update(ModelView *mv);
static void tile_draw(ModelView *mv, e_id cell_en, DrawOpts opts);

static const int alpha_min = 10, alpha_max = 255;
static float chance_bomb = 0.1;
static float tmr_bomb_duration = 2.f;
static int bomb_moves_limit = 9;

static const struct DrawOpts dflt_draw_opts = {
    .caption_offset_coef = { 1., 1., },
    .fontsize            = 500,
    .custom_color        = false,
    .amount              = 0.,
    /*.color               = BLACK,*/
};

static StrBuf str_repr_buf_position(void *payload, e_id e) {
    StrBuf buf = strbuf_init(NULL);
    Position *pos = payload;
    strbuf_add(&buf, "{ ");
    strbuf_addf(&buf, "x = %d, ", pos->x);
    strbuf_addf(&buf, "y = %d, ", pos->x);
    strbuf_add(&buf, "}");
    return buf;
}

// {{{ components

e_cp_type cmp_position = {
    .cp_sizeof = sizeof(struct Position),
    .name = "position",
    .initial_cap = 1000,
    .str_repr_buf = str_repr_buf_position,
};

e_cp_type cmp_cell = {
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
};

e_cp_type cmp_bomb = {
    .cp_sizeof = sizeof(struct Bomb),
    .name = "bomb",
    .initial_cap = 1000,
};

e_cp_type cmp_transition = {
    .cp_sizeof = sizeof(struct Transition),
    .name = "transition",
    .initial_cap = 1000,
};

e_cp_type cmp_exp = {
    .cp_sizeof = sizeof(struct Explosition),
    .name = "explosition",
    .initial_cap = 1000,
};

// }}}

static Color colors[] = {
    RED,
    /*{ 230, 41, 55, 205 }, //RED*/
    ORANGE, // плохо виден на фоне прозрачного красного?
    GOLD,// плохо виден на фоне прозрачного красного?
    GREEN,
    BLUE, 

    /*GRAY,*/
    { 100, 150, 150, 255 }   // Gray
};

// ассоциативный массив для получения строки по значению перечисления
const char *dir2str[] = {
    [DIR_NONE] = "none",
    [DIR_LEFT] = "left",
    [DIR_RIGHT] = "right",
    [DIR_DOWN] = "down",
    [DIR_UP] = "up",
};

// History_** {{{

typedef struct History {
    lua_State *l;
    StrBuf    lines;
    int       *field;
    FILE      *f;
    ModelView *mv;
    bool      first_run;
} History;

static History *history_new(ModelView *mv, bool devnull) {
    assert(mv);
    History *h = calloc(sizeof(*h), 1);
    h->lines = strbuf_init(NULL);
    h->mv = mv;
    h->l = luaL_newstate();
    luaL_openlibs(h->l);

    int err = luaL_dostring(h->l, "package.path = './?.lua;' .. package.path");
    if (err != LUA_OK) {
        trace(
            "history_new: could not change package.path with '%s'\n",
            lua_tostring(h->l, -1)
        );
        lua_pop(h->l, -1);
    }

    trace("history_new: [%s]\n", L_stack_dump(h->l));

    lua_newtable(h->l);  // создаём новую таблицу на стеке
    lua_setglobal(h->l, "LINES");  

    trace("history_new: [%s]\n", L_stack_dump(h->l));

    const char *fname = "/dev/null";
    if (!devnull)
        fname = koh_uniq_fname_str("modelview_history_", ".lua");
    trace("history_new: fname %s\n", fname);

    h->f = fopen(fname, "w");
    h->first_run = true;

    /*
    if (h->f) {
        fprintf(h->f, "return {\n");
    }
    */

    fflush(h->f);
    h->field = calloc(mv->field_size * mv->field_size, sizeof(h->field[0]));
    return h;
}

static void history_free(History *h) {
    trace("history_free:\n");

    /*
    if (h->f) {
        fprintf(h->f, "\n}");
        fflush(h->f);
        fclose(h->f);
        h->f = NULL;
    }
    */

    strbuf_shutdown(&h->lines);

    if (h->l) {
        lua_close(h->l);
        h->l = NULL;
    }

    if (h->field) {
        free(h->field);
        h->field = NULL;
    }

    free(h);
}

// XXX: Можно ли сделать писатель истории более универсальным?
// Стоит ли делать его универсальным?
static void history_write(History *h) {
    FILE *f = h->f;
    ModelView *mv = h->mv;

    assert(f);

    int field[h->mv->field_size * h->mv->field_size], i = 0;
    memset(field, 0, sizeof(field));

    // Записать текущее состояние в массив int*
    for (int y = 0; y < mv->field_size; y++) {
        for (int x = 0; x < mv->field_size; x++) {
            Cell *c = modelview_search_cell(mv, x, y);
            int val = -1;
            if (c) {
                val = c->value;
            }
            field[i++] = val;
        }
    }

    /*
    printf("field: ");
    for (int j = 0; j < mv->field_size * mv->field_size; j++) {
        printf("%d ", field[j]);
    }
    printf("\n");
    */

    memmove(h->field, field, sizeof(field));

    char buf_line[2048] = {}, *buf_line_ptr = buf_line;

    buf_line_ptr += sprintf(buf_line_ptr, "return { ");
    for (int y = 0; y < mv->field_size; y++) {
        buf_line_ptr += sprintf(buf_line_ptr, "{ ");
        for (int x = 0; x < mv->field_size; x++) {
            Cell *c = modelview_search_cell(mv, x, y);
            int val = -1;
            if (c) {
                val = c->value;
            }
            buf_line_ptr += sprintf(buf_line_ptr, "%d, ", val);
        }
        buf_line_ptr += sprintf(buf_line_ptr, "}, ");
    }
    sprintf(buf_line_ptr, "}");

    // Писать в файл построчно
    fprintf(f, "%s\n", buf_line);
    fflush(f);

    // Далее идет проверка на равенство строк описывающих состония. 
    // Есть новое состояние или оно является повтором прошлого?
    // Первую строку всегда добавляю
    if (h->lines.num == 0) {
        strbuf_add(&h->lines, buf_line);
    } else {
        char *last = strbuf_last(&h->lines);
        assert(last);
        // NOTE: Сравнение по строкам оказалось проще чем бинарное
        if (strcmp(last, buf_line))
            strbuf_add(&h->lines, buf_line);
    }

    // Добавить в таблицу LINES строку
    lua_getglobal(h->l, "LINES");
    int type = lua_type(h->l, -1);
    if (type == LUA_TTABLE) {
        lua_pushinteger(h->l, h->lines.num);
        lua_pushstring(h->l, buf_line);
        lua_settable(h->l, -3);  
        lua_pop(h->l, 1);
    } else {
        trace(
            "history_write: could not get 'LINES' with '%s'\n",
            lua_tostring(h->l, -1)
        );
        lua_pop(h->l, 1);
    }

}

void history_gui(History *h) {
    assert(h);

    // XXX: Как обозначить конечно положение поле, после запуска теста?
    if (igButton("update", (ImVec2){})) {
    }

    if (igBeginListBox("states", (ImVec2){})) {

        for (int i = 0; i < h->lines.num; i++) {
            igSelectable_Bool(h->lines.s[i], false, 0, (ImVec2){});
        }

        igEndListBox();
    }
}

// }}}

// GridAnim {{{

typedef struct GridAnimQuad {
          // коэффициент масштаба
    float scale, 
          // фаза колебания
          phase;
} GridAnimQuad;

typedef struct GridAnim {
    Color        color;
    float        i;
    GridAnimQuad *anim;
} GridAnim;

static GridAnim *gridanim_new(const ModelView *mv) {
    GridAnim *ga = calloc(1, sizeof(*ga));

    const int field_size = mv->field_size;
    ga->anim = calloc(field_size * field_size, sizeof(ga->anim[0]));

    for (int y = 0; y < field_size; y++) {
        for (int x = 0; x < field_size; x++) {
            ga->anim[y * field_size + x].phase = rand() / (float)RAND_MAX;
            ga->anim[y * field_size + x].scale = 10 + rand() % 10;
            /*printf("scale %f\n", ga->anim[y * field_size + x].scale);*/
        }
    }

    ga->color = GRAY;
    ga->color.a = 40;
    ga->i = 0;

    return ga;
}

static void gridanim_free(GridAnim *ga) {
    if (ga->anim) {
        free(ga->anim);
        ga->anim = NULL;
    }

    free(ga);
}

static void gridanim_draw(GridAnim *ga, ModelView *mv) {
    const int field_size = mv->field_size;
    /*trace("gridanim_draw: field_size %d\n", field_size);*/

    Vector2 pos = mv->pos;
    GridAnimQuad *anim = ga->anim;

    const float roundness = 0.3f,
                segments = 20;

    for (int x = 0; x < field_size; x++) {
        for (int y = 0; y < field_size; y++) {
            float phase = anim[y * field_size + x].phase;
            float scale = anim[y * field_size + x].scale;
            float space = scale * (0.8 * M_PI - sinf(ga->i + phase));

            //printf("phase %f\n", phase);

            Rectangle r = {
                .x = pos.x + x * mv->quad_width + space,
                .y = pos.y + y * mv->quad_width + space,
                .width = mv->quad_width - space * 2.,
                .height = mv->quad_width - space * 2.,
            };

            /*printf("%s\n\n", rect2str(r));*/

            DrawRectangleRounded(r, roundness, segments, ga->color);
        }
    }

    /*printf("\n\n\n");*/

    ga->i += 0.01; 
}

// }}}

// {{{ automation

static void automation_select(ModelView *mv) {
    if (!IsKeyDown(KEY_LEFT_CONTROL)) {
        return;
    }

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
        if (IsKeyPressed(KEY_P)) {
            printf("automation_select: play\n");
        } else if (IsKeyPressed(KEY_R)) {
            printf("automation_select: record\n");
        }
    } else {
        printf("automation_select: KEY_LEFT_CONTROL\n");
        for (int i = 0; i <= 9; i++) {
            int key = KEY_ZERO + i;
            if (IsKeyPressed(key)) {
                printf("automation_select: key '%s'\n", koh_key2str[key]);
                mv->automation_current = i;
                break;
            }
        }
    }
}

static void automation_load(ModelView *mv) {
    int num = sizeof(mv->automation) / sizeof(mv->automation[0]);
    for (int i = 0; i < num; ++i) {
        char fname[128] = {};
        sprintf(fname, "automation_0%d.txt", i);
        FILE *f = fopen(fname, "r");
        if (f) {
            mv->automation[i] = LoadAutomationEventList(fname);
            fclose(f);
        }
    }
}


static void automation_unload(ModelView *mv) {
    int num = sizeof(mv->automation) / sizeof(mv->automation[0]);
    for (int i = 0; i < num; ++i) {
        if (mv->automation_loaded[i]) {
            UnloadAutomationEventList(mv->automation[i]);
            mv->automation_loaded[i] = false;
        }
    }
}

static void automation_draw(ModelView *mv) {
    if (mv->automation_current == 0) {
        return;
    }

    const int fnt_sz = 100;
    char buf[128] = {};

    int x = 0, y = 0;

    sprintf(buf, "automation: selected [%d]", mv->automation_current);
    DrawText(buf, x, y += fnt_sz, fnt_sz, RED);

    sprintf(buf, "CTRL+SHIFT+p - play");
    DrawText(buf, x, y += fnt_sz, fnt_sz, RED);

    sprintf(buf, "CTRL+SHIFT+r - record start/stop");
    DrawText(buf, x, y, fnt_sz, RED);

    /*
    sprintf(buf, "loaded");
    DrawText(buf, x, y, fnt_sz, RED);
    */

}

// }}}

static int find_max(struct ModelView *mv) {
    int max = 0;

    for (e_view v = e_view_create_single(mv->r, cmp_cell);
         e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c && c->value > max)
            max = c->value;
    }

    return max;
}

static bool is_over(struct ModelView *mv) {
    int num = 0;

    for (e_view v = e_view_create_single(mv->r, cmp_cell); 
         e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c) num += c->value > 0;
    }

    return mv->field_size * mv->field_size == num;
}

// Получить ячейку и сущность на данной позиции или вернуть e_null
e_id modelview_search_entity(ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);

    // TODO: Избавится от полного перебора
    for (e_view v = e_view_create_single(mv->r, cmp_position);
        e_view_valid(&v); e_view_next(&v)) {
        Position *p = e_view_get(&v, cmp_position);
        if (p->x == x && p->y == y) {
            return e_view_entity(&v);
        }
    }
    return e_null;

    /*
    // XXX: Не работает field_update() и такое кэширование
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= mv->field_size) x = mv->field_size - 1;
    if (y >= mv->field_size) y = mv->field_size - 1;

    e_id e = mv->field[y * mv->field_size + x];
    if (e_valid(mv->r, e)) {
        if (en) *en = e;
        return e_get(mv->r, e, cmp_cell);
    } else
        return NULL;
     //   */
}

Cell *modelview_search_cell(ModelView *mv, int x, int y) {
    e_id e = modelview_search_entity(mv, x, y);
    if (e.id == e_null.id) 
        return NULL;
    return e_get(mv->r, e, cmp_cell);
}

static int get_cell_count(ModelView *mv) {
    assert(mv);
    assert(mv->r);
    int num = 0;
    e_view v = e_view_create_single(mv->r, cmp_cell);
    for (; e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        assert(e_valid(mv->r, e_view_entity(&v)));
        num += c ? 1 : 0;
    }
    return num;
}

// Создает сущность с позицией и клеткой
static e_id create_cell(ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);

    if (get_cell_count(mv) >= mv->field_size * mv->field_size) {
        trace("create_cell: cells were reached limit of %d\n",
              mv->field_size * mv->field_size);
        abort();
        return e_null;
    }

    ecs_t *r = mv->r;
    e_id en = e_create(r);

    Position *position = e_emplace(r, en, cmp_position);
    position->x = x;
    position->y = y;

    Transition *tr = e_emplace(r, en, cmp_transition);
    tr->anim_movement = false;

    Cell *c = e_emplace(r, en, cmp_cell);
    c->value = 0;
    c->phase = rand() % 1024;

    return en;
}

static bool tmr_cell_draw_stop(Timer *t) {
    TimerData *timer_data = t->data;
    ModelView *mv = timer_data->mv;

    if (!e_valid(mv->r, timer_data->e)) {
        return false;
    }

    /*struct Cell *cell = e_get(mv->r, timer_data->e, cmp_cell);*/
    /*assert(cell);*/

    struct Transition *tr = e_get(mv->r, timer_data->e, cmp_transition);
    /*assert(tr);*/
    if (tr) {
        tr->anim_movement = false;
        tr->anim_alpha = AM_NONE;
        tr->anim_size = false;
    }

    return false;
}

// какой эффект рисует функция и для какой плитки?
static bool tmr_cell_draw(struct Timer *t) {
    TimerData *timer_data = t->data;
    ModelView *mv = timer_data->mv;
    DrawOpts opts = dflt_draw_opts;

    if (!e_valid(mv->r, timer_data->e)) {
        koh_term_color_set(KOH_TERM_YELLOW);
        trace("tmr_cell_draw: invalid cell draw timer\n");
        koh_term_color_reset();
        return true;
    }

    opts.amount = t->amount;
    tile_draw(timer_data->mv, timer_data->e, opts);
    return false;
}

// XXX: Пульсация чего?
static bool tmr_pulsation_update(struct Timer *t) {
    TimerData *td = t->data;
    ecs_t *r = td->mv->r;
    e_id e = td->e;
    /*trace("tmr_pulsation_update:\n");*/

    if (!e_valid(r, e))
        return true;

    Transition *ef = e_get(r, e, cmp_transition);
    if (!ef)
        return false;

    return false;
}

static bool tmr_pulsation_stop(Timer *t) {
    TimerData *td = t->data;
    ecs_t *r = td->mv->r;
    e_id e = td->e;

    if (!e_valid(r, e))
        return false;

    /*trace("tmr_pulsation_stop:\n");*/
    return false;
}

// TODO: Добавить анимацию при создании клетки. 
// Как цифры появляются из центра ячейки.
void modelview_put_cell(struct ModelView *mv, int x, int y, int value) {
    assert(value > 0);
    assert(mv);
    assert(x >= 0);
    assert(x < mv->field_size);
    assert(y >= 0);
    assert(y < mv->field_size);

    e_id cell_en = create_cell(mv, x, y);
    Cell *cell = e_get(mv->r, cell_en, cmp_cell);

    cell->value = value;
    cell->dropped = false;

    assert(cell->value >= 0);

    Position *pos = e_get(mv->r, cell_en, cmp_position);
    pos->x = x;
    pos->y = y;

    struct Transition *ef = e_get(mv->r, cell_en, cmp_transition);
    assert(ef);
    ef->anim_alpha = AM_FORWARD;

    /*modeltest_put(&model_checker, cell->x, cell->y, cell->value);*/

    struct TimerData td = {
        .mv = mv,
        .e = cell_en,
    };

    static int timer_draw_cnt = 0,
               timer_pulsation_cnt = 0;
    TimerDef timer_draw = {
        // XXX: Почему не используется tmr_block_time ??
        .duration = mv->tmr_put_time,
        .sz = sizeof(td),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &td,
    }, timer_pulsation = {
        .data = &td,
        .duration = mv->tmr_put_time,
        .on_stop = tmr_pulsation_stop,
        .on_update = tmr_pulsation_update,
        .sz = sizeof(td),
    };

    // Задать имена
    sprintf(timer_draw.uniq_name, "timer_draw[%d]", timer_draw_cnt++);
    sprintf(
        timer_pulsation.uniq_name,
        "timer_pulsation[%d]",
        timer_pulsation_cnt++
    );

    /*
    printf(
        "modelview_put_cell: timer_draw %s\n",
        timer2str(timer_draw)
    );
    printf(
        "modelview_put_cell: timer_pulsation %s\n",
        timer2str(timer_pulsation)
    );
    */

    int id;
    id = timerman_add(mv->timers, timer_draw);
    assert(id != -1);
    id = timerman_add(mv->timers, timer_pulsation);
    assert(id != -1);
}

// Возвращает количество бомб на поле
/*
static int bomb_find_num(ModelView *mv) {
    int num = 0;
    e_view v = e_view_create_single(mv->r, cmp_bonus);
    for (; e_view_valid(&v); e_view_next(&v)) {
        Bonus *b = e_view_get(&v, cmp_bonus);
        if (b->type == BT_BOMB) 
            num++;
    }
    return num;
}
*/

static void bomb_put(ModelView *mv, int x, int y) {
    assert(mv);
    assert(x >= 0);
    assert(x < mv->field_size);
    assert(y >= 0);
    assert(y < mv->field_size);

    //if (bomb_find_num(mv) >= 2) 
        //return;

    /*
    // XXX: Удаляет в текущей ячейке содержимое
    cell = modelview_get_cell(mv, x, y, &cell_en);
    //modelview_get_cell(mv, x, y, &cell_en);
    // Клетка уже есть созданная. Уничтожить
    if (cell_en.id != e_null.id) {
        e_destroy(mv->r, cell_en);
    }
    */

    ecs_t *r = mv->r;
    e_id en = e_create(r);

    Position *position = e_emplace(r, en, cmp_position);
    position->x = x;
    position->y = y;

    struct Bomb *bomb = e_emplace(mv->r, en, cmp_bomb);
    assert(bomb);

    /*
    // XXX: Как будет работать анимация альфа канала на бомбе при ее движении?
    Transition *tr = e_emplace(r, en, cmp_transition);
    tr->anim_alpha = AM_FORWARD;
    */

    assert(bomb);
    bomb->moves_cnt = bomb_moves_limit;
    bomb->bomb_color = koh_maybe();
    bomb->aura_rot = rand() % 360;
    bomb->aura_phase = rand() % 1024;

    /*modeltest_put(&model_checker, cell->x, cell->y, cell->value);*/

    TimerData tdata = {
        .mv = mv,
        .e = en,
    };
    TimerDef tdef =  {
        .duration = mv->tmr_put_time,
        .sz = sizeof(tdata),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &tdata,
    };
    static int tmr_cell_draw_cnt = 0;
    sprintf(tdef.uniq_name, "tmr_cell_draw[%d]", tmr_cell_draw_cnt++);

    timer_id_t id = timerman_add(mv->timers, tdef);
    assert(id != -1);
}

static int gen_value(ModelView *mv) {
    int value = -1;
    float v = (float)rand() / (float)RAND_MAX;
    if (mv->strong) {
        if (v >= 0. && v < 0.9) {
            value = 1;
        } else {
            value = 3;
        }
    } else {
        if (v >= 0. && v < 0.9) {
            value = 2;
        } else {
            value = 4;
        }
    }
    return value;
}

// Разместить новую клетку или бомбу
void modelview_put(ModelView *mv) {
   
    assert(mv);
    /*field_update(mv);*/

    for (int i = 0; i < mv->put_num; i++) {
        int x = rand() % mv->field_size;
        int y = rand() % mv->field_size;
        // Максимальное количество проб клетки на возможность заполнения
        // XXX: Не работает, не удается заполнить все поле клетками
        int j = mv->field_size * mv->field_size * 30; 

        e_id e = modelview_search_entity(mv, x, y);

        while (e.id != e_null.id) {
            x = rand() % mv->field_size;
            y = rand() % mv->field_size;
            e = modelview_search_entity(mv, x, y);

            j--;
            if (j <= 0) {
                trace("modelview_put: too much iterations\n");
                // спасение бегством от бесконечного цикла
                return;
            }
        }

        int value = gen_value(mv);
        if (mv->use_bonus) {
            if (mv->next_bomb) {
                bomb_put(mv, x, y);
                mv->next_bomb = false;
            } else {
                if ((double)rand() / (double)RAND_MAX > chance_bomb)
                    modelview_put_cell(mv, x, y, value);
                else
                    bomb_put(mv, x, y);
            }
        } else {
            modelview_put_cell(mv, x, y, value);
        }

    }

}

static bool entity_in_bounds(ModelView *mv, e_id e) {
    assert(mv);
    assert(e.id != e_null.id);

    Position *pos = e_get(mv->r, e, cmp_position);
    assert(pos);

    if (pos->x + mv->dx >= mv->field_size)
        return false;
    if (pos->x + mv->dx < 0)
        return false;
    if (pos->y + mv->dy >= mv->field_size)
        return false;
    if (pos->y + mv->dy < 0)
        return false;

    return true;
}

// Двигает клетку cell_en на позицию x, y
// XXX: Зачем нужен touched флаг?
static bool move(ModelView *mv, e_id cell_en, int x, int y, bool *touched) {
    ecs_t *r = mv->r;
    bool has_move = false;

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);
    if (!cell)
        return false;

    Position *pos = e_get(mv->r, cell_en, cmp_position);
    assert(pos);

    struct Transition *tr = e_get(mv->r, cell_en, cmp_transition);
    if (!tr)
        return false;
    /*assert(ef);*/

    //if (cell->touched)
        //return has_move;

    if (tr->anim_movement)
        return has_move;

    if (cell->dropped)
        return has_move;

    e_id neighbour_e = modelview_search_entity(mv, x + mv->dx, y + mv->dy);

    /*
    if (neighbour_e.id == e_null.id)
        return has_move;
        */

    Cell *neighbour_cell = e_get(r, neighbour_e, cmp_cell);

    if (neighbour_cell) 
        return has_move;

    if (!entity_in_bounds(mv, cell_en))
        return has_move;

    has_move = true;

    assert(cell_en.id != e_null.id);

    pos->x += mv->dx;
    pos->y += mv->dy;
    tr->anim_movement = true;

    // Обновление счетчика движения
    // TODO: Показывать счетчик движений. 
    // Когда достигается 0, то происходит взрыв.
    // TODO: Сделать слияние бомб одного типа
    Bomb *bomb = e_get(mv->r, cell_en, cmp_bomb);
    if (bomb) {
        bomb->moves_cnt--;
    }

    *touched = true;

    /*trace("try_move: move_call_counter %d\n", move_call_counter);*/

    static int tmr_cell_draw_cnt = 0;

    int id = timerman_add(mv->timers, timer_def((TimerDef) {
        .duration = mv->tmr_block_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &(struct TimerData) {
            .mv = mv,
            .e = cell_en,
        },
    }, "tmr_cell_draw[%d]", tmr_cell_draw_cnt++));
    assert(id != -1);

    return has_move;
}

static bool sum(
    struct ModelView *mv, e_id cell_en, 
    int x, int y, bool *touched
) {
    assert(mv);
    assert(cell_en.id != e_null.id);

    bool has_sum = false;

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);
    /*assert(cell);*/

    /*Position *pos = e_get(mv->r, cell_en, cmp_position);*/
    Bomb *b = e_get(mv->r, cell_en, cmp_bomb);

    if (b) {
        return false;
    }

    //assert(cell);
    if (!cell)
        return false;

    struct Transition *ef = e_get(mv->r, cell_en, cmp_transition);
    if (!ef)
        return false;
    /*assert(ef);*/

    if (cell->dropped) 
        return has_sum;

    e_id neighbour_en = modelview_search_entity(mv, x + mv->dx, y + mv->dy);

    if (neighbour_en.id == e_null.id) 
        return has_sum;

    Cell *neighbour_cell = e_get(mv->r, neighbour_en, cmp_cell);
    if (!neighbour_cell)
        return has_sum;

    /*if (cell && neighbour_cell->dropped) */
    if (neighbour_cell && neighbour_cell->dropped) 
        return has_sum;

    // Если соседом оказался бонус - выход
    Bomb *neighbour_bonus = e_get(mv->r, neighbour_en, cmp_bomb);
    if (neighbour_bonus)
        return has_sum;

    Transition *neighbour_ef = e_get(mv->r, neighbour_en, cmp_transition);

    assert(cell_en.id != e_null.id);
    assert(neighbour_en.id != e_null.id);

    if (cell && neighbour_cell && cell->value != neighbour_cell->value)
        return has_sum;

    has_sum = true;
    *touched = true;
    neighbour_cell->value += cell->value;
    mv->scores += cell->value;

    assert(e_valid(mv->r, cell_en));

    static int tmr_cell_draw_cnt = 0;

    // cell_en уничтожается
    cell->dropped = true;
    ef->anim_alpha = AM_BACKWARD;
    int id;

    // XXX: Не создается таймер так как используется тоже самое имя.
    id = timerman_add(mv->timers, timer_def((TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .data = &(struct TimerData) {
            .mv = mv,
            .e = cell_en,
        },
    }, "tmr_cell_draw[%d]", ++tmr_cell_draw_cnt));
    assert(id != -1);

    // neighbour_en увеличивается 
    neighbour_ef->anim_size = true;
    id = timerman_add(mv->timers, timer_def((TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .data = &(struct TimerData) {
            .mv = mv,
            .e = neighbour_en,
        },
    }, "tmr_cell_draw[%d]", ++tmr_cell_draw_cnt));
    assert(id != -1);

    return has_sum;
}

typedef bool (*Action)(ModelView *mv, e_id cell, int x, int y, bool *touched);

static bool do_action(struct ModelView *mv, Action action) {
    assert(mv);
    assert(action);

    bool has_action = false;
    bool touched = false;

    do {
        touched = false;
        for (int y = 0; y < mv->field_size; ++y) 
            for (int x = 0; x < mv->field_size; ++x) {
                e_id cell_en = modelview_search_entity(mv, x, y);

                if (cell_en.id == e_null.id)
                    continue;

                Cell *cell = e_get(mv->r, cell_en, cmp_cell);
                if (!cell) 
                    continue;

                if (action(mv, cell_en, x, y, &touched))
                    has_action = true;
        }
    } while (touched);

    return has_action;
}

void modelview_input(struct ModelView *mv, enum Direction dir) {
    assert(mv);
    mv->dir = dir;
    switch (dir) {
        case DIR_UP: mv->dy = -1; break;
        case DIR_DOWN: mv->dy = 1; break;
        case DIR_LEFT: mv->dx = -1; break;
        case DIR_RIGHT: mv->dx = 1; break;
        default: break;
    }
}

static int cmp(const void *pa, const void *pb) {
    const struct Cell *a = pa, *b = pb;
    return a->value < b->value;
}

static void sort_numbers(struct ModelView *mv) {
    size_t num = mv->field_size * mv->field_size;
    struct Cell tmp[num];
    memset(tmp, 0, sizeof(tmp));
    int idx = 0; // Количество клеток
    for (int y = 0; y < mv->field_size; y++)
        for (int x = 0; x < mv->field_size; x++) {
            e_id e = modelview_search_entity(mv, x, y);
            Cell *cell = e_get(mv->r, e, cmp_cell);
            if (cell)
                tmp[idx++].value = cell->value;
        }

    //trace("sort_numbers: idx %d\n", idx);
    qsort(tmp, idx, sizeof(tmp[0]), cmp);

    {
        char buf[20 /* value len */ * num ] = {}, *pbuf = buf;
        for (int i = 0; i < num; i++) {
            pbuf += sprintf(pbuf, "%d ", tmp[i].value);
        }
        //trace("sort_numbers: %s\n", buf);
    }

    // TODO: Выкинуть дубликаты
    /*int dup = tmp[0].value;*/
    if (idx > 1) {
        // XXX: Не работает
        /*
        for (int j = 1; j < idx; j++) {
            while (dup == tmp[j].value && j < idx) {
                j++;
            }
            dup = tmp[j].value;
        }
        */
    }
    // XXX:Заполнить остатки массива нулями?

    {
        char buf[20 /* value len */ * num ] = {}, *pbuf = buf;
        for (int i = 0; i < num; i++) {
            pbuf += sprintf(pbuf, "%d ", tmp[i].value);
        }
        /*trace("sort_numbers: without dups %s\n", buf);*/
    }

    memmove(mv->sorted, tmp, sizeof(tmp[0]) * idx);
}

static void draw_grid(struct ModelView *mv) {
    assert(mv);

    const int field_width = mv->field_size * mv->quad_width;
    Vector2 start = mv->pos;
    const float thick = 8.;

    Vector2 tmp = start;
    for (int u = 0; u <= mv->field_size; u++) {
        Vector2 end = tmp;
        end.y += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.x += mv->quad_width;
    }

    tmp = start;
    for (int u = 0; u <= mv->field_size; u++) {
        Vector2 end = tmp;
        end.x += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.y += mv->quad_width;
    }
}

static Color get_color(ModelView *mv, int cell_value) {
    int colors_num = sizeof(colors) / sizeof(colors[0]);
    for (int k = 0; k < colors_num; k++) {
        if (cell_value == mv->sorted[k].value) {
            return colors[k];
        }
    }
    // Возврат серого цвета
    return colors[colors_num - 1];
}

// Уменьшает размер шрифта до тех пор, пока данный текст msg не влезет по 
// ширине в text_bound
static Vector2 decrease_font_size(
    struct ModelView *mv, const char *msg, int *fontsize, Vector2 text_bound
) {
    Vector2 textsize = {};
    do {
        textsize = MeasureTextEx(
            mv->font, msg, (*fontsize)--, mv->font_spacing
        );
    } while (textsize.x > text_bound.x || textsize.y > text_bound.y);
    return textsize;
}

static int calc_font_size_anim(ModelView *mv, e_id en, DrawOpts opts) {
    int fontsize = opts.fontsize;
    Transition *ef = e_get(mv->r, en, cmp_transition);
    assert(ef);
    if (ef->anim_size) {
        const float font_scale = 6.;
        if (opts.amount <= 0.5)
            fontsize = Lerp(fontsize, fontsize * font_scale, opts.amount);
        else
            fontsize = Lerp(fontsize * font_scale, fontsize, opts.amount);
    }
    return fontsize;
}

static Color calc_alpha(ModelView *mv, e_id en, Color color, DrawOpts opts) {
    assert(mv);
    Transition *ef = e_get(mv->r, en, cmp_transition);
    assert(ef);
    switch (ef->anim_alpha) {
        case AM_FORWARD:
            color.a = Lerp(alpha_min, alpha_max, opts.amount);
            break;
        case AM_BACKWARD:
            color.a = Lerp(alpha_max, alpha_min, opts.amount);
            break;
        default:
            break;
    }
    return color;
}

// Возвращает индексы от 0 до mv->field_size возможно с дробной интерполяцией
static Vector2 calc_base_pos(ModelView *mv, e_id en, DrawOpts opts) {

    Position *pos = e_get(mv->r, en, cmp_position);
    assert(pos);

    Transition *ef = e_get(mv->r, en, cmp_transition);
    /*assert(ef);*/

    Vector2 base_pos = { pos->x, pos->y, };
    if (!ef)
        return base_pos;
    /*assert(ef);*/

    if (ef->anim_movement) {
        base_pos = (Vector2) {
            Lerp(pos->x - mv->dx, pos->x, opts.amount),
            Lerp(pos->y - mv->dy, pos->y, opts.amount),
        };
    }

    return base_pos;
}

static void bomb_draw(
    ModelView *mv, e_id cell_en, Vector2 base_pos, DrawOpts opts
) {

    Texture2D tex = {};
    Bomb *b = e_get(mv->r, cell_en, cmp_bomb);

    if (b->bomb_color == BC_BLACK) {
        tex = mv->tex_bomb_black;
    } else if (b->bomb_color == BC_RED) {
        tex = mv->tex_bomb_red;
    }

    Vector2 text_bound = { .x = mv->quad_width, .y = mv->quad_width };
    Vector2 _offset = {
    opts.caption_offset_coef.x * (mv->quad_width - text_bound.x) / 2.,
    opts.caption_offset_coef.y * (mv->quad_width - text_bound.y) / 2.,
    };
    Vector2 tmp = Vector2Scale(base_pos, mv->quad_width);
    Vector2 _disp = Vector2Add(tmp, _offset);
    Vector2 _pos = Vector2Add(mv->pos, _disp);

    float thick = 40.; // Отступ внутрь квадрата
    Rectangle src = { 0., 0., tex.width, tex.height, },
          dst_bomb = {
            .x = _pos.x + thick * 1.,
            .y = _pos.y + thick * 1.,
            .width = mv->quad_width - thick * 2.,
            .height = mv->quad_width - thick * 2.,
          }; 

    char color_str[32] = {};
    switch (b->bomb_color) {
        case BC_BLACK: {
            sprintf(color_str, "black"); 
            break;
        }
        case BC_RED: {
            sprintf(color_str, "red");
            break;
        }
    }

    DrawTexturePro(tex, src, dst_bomb, Vector2Zero(), 0., WHITE);

    src = (Rectangle){ 0., 0., mv->tex_aura.width, mv->tex_aura.height, };
    Vector2 disp = {
        .x = 234,
        .y = 20,
    };

    double t = sin(GetTime() + b->aura_phase) * 10.f;
    Rectangle dst_aura = (Rectangle){
        .x = _pos.x + thick * 1. + disp.x,
        .y = _pos.y + thick * 1. + disp.y,
        .width = mv->quad_width - thick * 2. + t,
        .height = mv->quad_width - thick * 2. + t,
    }; 

    Vector2 origin = { 
        dst_aura.width / 2.,
        dst_aura.height / 2.,
    };

    DrawTexturePro(mv->tex_aura, src, dst_aura, origin, b->aura_rot, WHITE);

    char buf[32] = {};
    const int fnt_size = 32;
    sprintf(buf, "%d", b->moves_cnt);
    DrawText(buf, dst_bomb.x, dst_bomb.y, fnt_size, BLACK);
}

static void cell_draw(
    ModelView *mv, e_id cell_en, Vector2 base_pos, DrawOpts opts
) {
    Cell *cell = e_get(mv->r, cell_en, cmp_cell);

    // Если бомба, то выхожу
    Bomb *b = e_get(mv->r, cell_en, cmp_bomb);
    if (b)
        return;
    // */

    if (cell->dropped || cell->value == 0)
        return;

    char cell_text[64] = {0};
    sprintf(cell_text, "%d", cell->value);

    int fntsize = calc_font_size_anim(mv, cell_en, opts);
    // XXX: Почему не используются все цвета из colors?
    /*Color color = opts.custom_color ? opts.color : get_color(mv, cell->value);*/
    Color color = get_color(mv, cell->value);

    color = calc_alpha(mv, cell_en, color, opts);

    Vector2 text_bound = { .x = mv->quad_width, .y = mv->quad_width },
            textsize = decrease_font_size(mv, cell_text, &fntsize, text_bound);

    ColoredText text_cell[] = {
        { 
            .text = cell_text,
            .scale = 1.,
            .color = color,
        },
    };

    ColoredText *texts = text_cell;
    int texts_num = sizeof(text_cell) / sizeof(text_cell[0]);

    assert(texts);

    texts->text_bound = text_bound;
    fntsize = colored_text_pickup_size(
        texts, texts_num,
        mv->text_opts
    );

    Vector2 offset = {
        opts.caption_offset_coef.x * (mv->quad_width - textsize.x) / 2.,
        opts.caption_offset_coef.y * (mv->quad_width - fntsize) / 2.,
    };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, mv->quad_width), offset);
    Vector2 pos = Vector2Add(mv->pos, disp);

    //colored_text_print(texts, texts_num, pos, mv->text_opts, fntsize);
    texts->pos = pos;
    texts->base_font_size = fntsize;
    // Найти место создания клетки
    colored_text_print(texts, texts_num, mv->text_opts, mv->r, cell_en);
}

static void exp_draw(
    ModelView *mv, e_id e_cell, Vector2 base_pos, DrawOpts opts
) {
    /*struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);*/
    //trace("exp_draw:\n");

    Vector2 offset = { };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, mv->quad_width), offset);
    Vector2 pos = Vector2Add(mv->pos, disp);

    // XXX: С текстурами что-то непонятное происходит.
    Texture2D tex = mv->tex_ex[mv->tex_ex_index];

    /*const float scale = 0.99f;*/
    const float scale = 1.1 - (opts.amount + 0.01);

    trace(
        "exp_draw: cell_en %s, opts.amount %f\n",
        e_id2str(e_cell), opts.amount
    );
    //*/

    /*trace("exp_draw: opts.amount %f\n", opts.amount);*/

    /*assert(0.01 < scale);*/
    /*assert(1. >= scale);*/

    float new_width = mv->quad_width * scale,
          new_height = mv->quad_width * scale;

    Rectangle src = {
        .x = 0, .y = 0,
        .width = tex.width,
        .height = tex.height,
    }, dst = { 
        // Точка в центре спрайта. 
        // При scale == 1. - целиком рисуется
        // При scale == 0.1 - целиком рисуется, но сильно уменьшенная картинка
        .x = pos.x + (mv->quad_width - new_width) / 2.,
        .y = pos.y + (mv->quad_width - new_height) / 2.,
        .width = mv->quad_width * scale,
        .height = mv->quad_width * scale,
    };

    /*DrawTexturePro(tex, src, dst, Vector2Zero(), 0.f, WHITE);*/
    /*DrawRectangle(dst.x, dst.y, mv->quad_width, mv->quad_width, RED);*/
    DrawTexturePro(tex, src, dst, Vector2Zero(), 0.f, WHITE);
}

// Рисовать компоненты данной сущности
static void tile_draw(ModelView *mv, e_id cell_en, DrawOpts opts) {
    assert(mv);

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);
    if (!cell)
        return;

    Vector2 base_pos = calc_base_pos(mv, cell_en, opts);

    Explosition *exp = e_get(mv->r, cell_en, cmp_exp);
    if (exp) {
        exp_draw(mv, cell_en, base_pos, opts);
    } else {
        Bomb *bomb = e_get(mv->r, cell_en, cmp_bomb);
        if (bomb) {
            bomb_draw(mv, cell_en, base_pos, opts);
        } else {
            cell_draw(mv, cell_en, base_pos, opts);
        }
    }

}

static void draw_numbers(ModelView *mv) {
    assert(mv);

    // TODO: Переписать через итерацию всех cmp_cell ??

    for (int y = 0; y < mv->field_size; y++) {
        for (int x = 0; x < mv->field_size; x++) {
            e_id en = modelview_search_entity(mv, x, y);
            Cell *cell = e_get(mv->r, en, cmp_cell);

            if (!cell) continue;
            if (cell->dropped) continue;
            if (cell->value == 0) continue;

            assert(en.id != e_null.id);
            Transition *ef = e_get(mv->r, en, cmp_transition);
            if (!ef) continue;
            /*assert(ef);*/

            bool can_draw = !ef->anim_movement && 
                            !ef->anim_size && ef->anim_alpha == AM_NONE;
            if (can_draw)
                tile_draw(mv, en, dflt_draw_opts);
        }
    }
}

/*
// {{{
static void movements_window() {
    // {{{
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("movements", &open, flags);

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_BordersV | 
        ImGuiTableFlags_Resizable | 
        ImGuiTableFlags_Reorderable | 
        ImGuiTableFlags_Hideable;

    ImVec2 outer_size = {0., 0.};
    if (igBeginTable("movements", 3, table_flags, outer_size, 0.)) {

        igTableSetupColumn("fr", 0, 0, 0);
        igTableSetupColumn("to", 0, 0, 1);
        igTableSetupColumn("val", 0, 0, 2);
        //igTableSetupColumn("act", 0, 0, 3);
        //igTableSetupColumn("anima", 0, 0, 3);
        igTableHeadersRow();

        for (int row = 0; row < global_cells_num; ++row) {
            igTableNextRow(0, 0);

            if (!memcmp(&global_cells[row], 
                       &cell_zero,
                       sizeof(cell_zero))) {

                igTableSetColumnIndex(0);
                igText("-");
                igTableSetColumnIndex(1);
                igText("-");
                igTableSetColumnIndex(2);
                igText("-");
                //igTableSetColumnIndex(3);
                //igText("-");
                //igTableSetColumnIndex(4);
                //igText("-");

            } else {

                igTableSetColumnIndex(0);
                igText("%d, %d", 
                        global_cells[row].x, global_cells[row].y);
                igTableSetColumnIndex(1);
                igText("%d, %d", 
                        global_cells[row].x, global_cells[row].y);
                igTableSetColumnIndex(2);
                igText("%d", global_cells[row].value);
                //igTableSetColumnIndex(3);
                //igText("%s", action2str(global_cells[row].action));
                //igTableSetColumnIndex(3);
                //igText("%s", global_cells[row].anima ? "true" : "false");

            }
        }
        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);
        igEndTable();
    }
    igEnd();
    // }}}
}
// }}}
*/

/*
static void print_node_cell(struct Cell *c) {
    assert(c);
    igText("pos     %d, %d", c->x, c->y);
    //igText("to      %d, %d", c->to_x, c->to_y);
    //igText("act     %s", action2str(c->action));
    igText("val     %d", c->value);
    igText("dropped %s", c->dropped ? "t" : "f");
}
*/

/*
static void print_node_effect(struct Effect *ef) {
    assert(ef);
    igText("anim_movement   %s", ef->anim_movement ? "t" : "f");
    igText("anim_size %s", ef->anim_size ? "t" : "f");
    igText("anim_alpha %d", ef->anim_alpha);
}
*/

/*
static void removed_entities_window() {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("removed entities", &open, flags);

    for (int i = 0; i < global_cells_num; i++) {
            struct Cell *c = &global_cells[i];
            igSetNextItemOpen(false, ImGuiCond_Once);
            if (igTreeNode_Ptr((void*)(uintptr_t)i, "i %d", i)) {
                print_node_cell(c);
                igTreePop();
            }
    }

    igEnd();
}
*/

/*
static void entities_window(struct ModelView *mv) {
    // {{{
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("entities", &open, flags);

    if (!mv->r) goto _exit;

    int idx = 0;
    e_view v = e_view_create_single(mv->r, cmp_cell);
    for (; e_view_valid(&v); e_view_next(&v)) {
        if (e_valid(mv->r, e_view_entity(&v))) {
            struct Cell *c = e_view_get(&v, cmp_cell);
            struct Effect *ef = e_view_get(&v, cmp_effect);

            igSetNextItemOpen(true, ImGuiCond_Once);

            if (igTreeNode_Ptr((void*)(uintptr_t)idx, "i %d", idx)) {
                igText("en      %ld", e_view_entity(&v));
                if (c)
                    print_node_cell(c);
                if (ef)
                    print_node_effect(ef);

                idx++;
                igTreePop();
            }
        } else {
            printf("bad entity: without cmp_cell component\n");
            exit(EXIT_FAILURE);
        }
    }

_exit:
    igEnd();
    // }}}
}
*/

static void options_window(struct ModelView *mv) {
    // {{{
    const ImVec2 z = {0, 0};

    assert(mv);
    bool wnd_open = true;
    ImGuiWindowFlags wnd_flag = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("options_window", &wnd_open, wnd_flag);

    static int field_size = 0;

    if (!field_size)
        field_size = mv->field_size;

    igSliderInt("field size", &field_size, 4, 20, "%d", 0);

    static int put_num = 1;
    igSliderInt("put cells num", &put_num, 1, mv->field_size, "%d", 0);

    igSliderInt("bomb_moves_limit", &bomb_moves_limit, 1, 20, "%d", 0);

    igSliderFloat("tmr_bomb_duration", &tmr_bomb_duration, 0.01, 2., "%f", 0);
    igSliderFloat("tmr_block_time", &mv->tmr_block_time, 0.01, 1., "%f", 0);
    igSliderFloat("tmr_put_time", &mv->tmr_put_time, 0.01, 1., "%f", 0);

    igSliderFloat("bomb chance", &chance_bomb, 0.1, 0.9, "%f", 0);

    static bool use_bonus = true;
    igCheckbox("use bonuses", &use_bonus);

    static bool strong = false;
    igCheckbox("strong(1 + 3)", &strong);

    igCheckbox("draw animated grid background", &is_draw_grid);

    // INFO: Во избежании постронних эффектов - переменную не удалять
    static bool next_bomb;
    next_bomb = mv->next_bomb;
    igCheckbox("next bomb", &next_bomb);

    static bool use_fnt_vector = false;
    igCheckbox("[experimental] use vector font drawing", &use_fnt_vector);

    static bool draw_field = true;
    igCheckbox("draw field", &draw_field);

    static bool theme_light = true;
    if (igRadioButton_Bool("color theme: light", theme_light)) {
        theme_light = true;
    } else if (igRadioButton_Bool("color theme: dark", !theme_light)) {
        theme_light = false;
    }

    static int win_value = 2048;
    igSliderInt("points for win", &win_value, 0, 2048 * 4, "%d", 0);

    static int tex_ex_index = 0;
    igSliderInt(
        "explosion texture index", &tex_ex_index, 0,
        mv->tex_ex_num - 1, "%d", 0
    );

    // {{{
    // XXX: Как сделать установку состояния постоянной?
    static ModelViewState state;

    if (igButton("gameover", z)) {
        state = MVS_GAMEOVER;
    } else
        state = mv->state;

    igSameLine(0., 10.f);

    if (igButton("win", z)) {
        state = MVS_GAMEOVER;
    } else
        state = mv->state;

    if (igButton("reset camera", z)) {
        mv->camera->offset = Vector2Zero();
        mv->camera->rotation = 0.f;
        mv->camera->zoom = 1.f;
    }

    // }}}

    mv->state = state;

    if (igButton("restart", z)) {
        modelview_shutdown(mv);

        struct Setup setup = {};

        // XXX: Коряво, при нажатии 'restart' обнуляется время таймеров
        // Возможная причина - ??
        if (mv->inited) {
            setup = (Setup) {
                .win_value = win_value,
                .auto_put = true,
                .cam = mv->camera,
                .field_size = field_size,
                .tmr_block_time = mv->tmr_block_time,
                .tmr_put_time = mv->tmr_put_time,
                /*.pos = &mv->pos,*/
                .use_gui = mv->use_gui,
                .use_bonus = use_bonus,
                .use_fnt_vector = use_fnt_vector,
                /*.color_theme = theme_light ? color_theme_light : color_theme_dark,*/
                .on_init_lua = mv->on_init_lua,
            };
        } else {
            setup = (Setup) {
                .win_value = win_value,
                .auto_put = true,
                .cam = NULL,
                .field_size = field_size,
                .tmr_block_time = modelview_setup.tmr_block_time,
                .tmr_put_time = modelview_setup.tmr_put_time,
                .use_gui = modelview_setup.use_gui,
                .use_bonus = use_bonus,
                .use_fnt_vector = use_fnt_vector,
                .on_init_lua = NULL,
            };
        }

        modelview_init(mv, setup);

        // XXX: Опасный код так как параметры проходят мимо конструктора
        mv->put_num = put_num;
        mv->draw_field = draw_field;
        mv->next_bomb = next_bomb;
        mv->strong = strong;
        mv->tex_ex_index = tex_ex_index;

        modelview_put(mv);
    }

    igEnd();
    // }}}
}

static void gui(struct ModelView *mv) {
    // {{{

    //if (mv->camera)
        //trace("gui: %s\n", camera2str(*mv->camera));

    //rlImGuiBegin(false, NULL);
    
    /*movements_window();*/
    /*removed_entities_window();*/

    //paths_window();

    /*entities_window(mv);*/
    e_gui_buf(mv->r);

    /*
    timerman_window((struct TimerMan* []) { 
            mv->timers_slides,
            mv->timers_effects,
    }, 2);
    */

    timerman_window_gui(mv->timers);

    options_window(mv);
    //ecs_window(mv);
    // }}}
}

const char *modelview_state2str[] = {
    [MVS_ANIMATION] = "MVS_ANIMATION",
    [MVS_READY] = "MVS_READY",
    [MVS_WIN] = "MVS_WIN",
    [MVS_GAMEOVER] = "MVS_GAMEOVER",
};

static void destroy_dropped(struct ModelView *mv) {
    e_id destroy_arr[mv->field_size * mv->field_size];
    int destroy_num = 0;
    
    e_view v = e_view_create_single(mv->r, cmp_cell);
    for (; e_view_valid(&v); e_view_next(&v)) {
        assert(e_valid(mv->r, e_view_entity(&v)));
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c->dropped) {

            int destroy_arr_num = sizeof(destroy_arr) / sizeof(destroy_arr[0]);
            assert(destroy_num + 1 < destroy_arr_num);
            destroy_arr[destroy_num++] = e_view_entity(&v);
        }
    }

    if (mv->test_payload && ((TestPayload*)mv->test_payload)->do_trap) {
        printf("test_payload trap\n");
        koh_trap();
    }

    for (int j = 0; j < destroy_num; ++j) {
        e_id e = destroy_arr[j];
        e_destroy(mv->r, e);
    }
}

// Происходит при остановке таймера бомбы
static bool tmr_bomb_stop(Timer *t) {
    TimerData *td = t->data;
    ecs_t *r = td->mv->r;
    ModelView *mv = td->mv;
    trace("tmr_bomb_stop:\n");

    // Уничтожить все найденные в cross_remove() сущности для данного типа
    // бомбочки
    for (int i = 0; i < mv->e_2destroy_num; i++) {
        e_destroy(mv->r, mv->e_2destroy[i]);
    }

    // Сброс состояния массива
    memset(mv->e_2destroy, 0, sizeof(mv->e_2destroy[0]) * mv->e_2destroy_num);
    mv->e_2destroy_num = 0;

    // Удалить компонент взрыва
    if (e_valid(r, td->e)) {
        /*e_remove(r, td->e, cmp_bomb_exp);*/
        e_destroy(r, td->e);
    }

    mv->can_scan_bombs = true;
    return false;
}

static bool tmr_bomb_update(Timer *t) {
    /*trace("tmr_bomb_update:\n");*/

    TimerData *timer_data = t->data;
    /*ModelView *mv = timer_data->mv;*/
    DrawOpts opts = dflt_draw_opts;

    /*
    if (!e_valid(mv->r, timer_data->e)) {
        koh_term_color_set(KOH_TERM_YELLOW);
        trace("tmr_bomb_update: invalid cell draw timer\n");
        koh_term_color_reset();
        return true;
    }
    */

    /*trace("tmr_bomb_update: t->amount %f\n", t->amount);*/

    opts.amount = t->amount;

    trace(
        "tmr_bomb_update: cell_en %s, opts.amount %f\n",
        e_id2str(timer_data->e), opts.amount
    );

    tile_draw(timer_data->mv, timer_data->e, opts);

    return false;
}

static void make_ex(ModelView *mv, int x, int y) {
    e_id *e_2destroy = mv->e_2destroy;

    e_id e = modelview_search_entity(mv, x, y);
    Cell *c = e_get(mv->r, e, cmp_cell);

    if (e.id == e_null.id)
        e = e_create(mv->r);

    if (!c) {
        // Создание позиции для пустых клеток, что-бы отображать в них
        // взрыв
        Position *pos = e_emplace(mv->r, e, cmp_position);
        assert(pos);
        pos->x = x;
        pos->y = y;
    } else {
        e_2destroy[mv->e_2destroy_num++] = e;
    }

    /*char *bomb_tag = e_emplace(mv->r, e, cmp_bomb_exp);*/
    Explosition *exp = e_get(mv->r, e, cmp_exp);

    if (!exp)
        exp = e_emplace(mv->r, e, cmp_exp);

    if (exp) {
        exp->i = 1;
        
        static int tmr_bomb_update_cnt = 0;
        TimerData td = { .mv = mv, .e = e, };
        int id = timerman_add(mv->timers, timer_def((TimerDef) {
            .data = &td,
            .duration = tmr_bomb_duration,
            .on_stop = tmr_bomb_stop,
            .on_update = tmr_bomb_update,
            .sz = sizeof(td),
        }, "tmr_bomb_update[%d]", tmr_bomb_update_cnt++));
        assert(id != -1);
    }
}

// Удалить все клетки на пересечении x и y, кроме данной клетки
void modelview_cross_remove2(ModelView *mv, e_id e) {
    assert(mv->field_size > 0);

    Position *c = e_get(mv->r, e, cmp_position);
    int x = c->x, y = c->y;
    mv->e_2destroy_num = 0;

    for (int i = 0; i < mv->field_size; i++) {
        make_ex(mv, i, y);
    }
    for (int i = 0; i < mv->field_size; i++) {
        make_ex(mv, x, i);
    }

}

/*
static void modelview_call_cross_remove(ModelView *mv, int x, int y) {
    // Вызов луа функции с двумя аргументами
    lua_State *l = mv->l;
    const char *func_name = "cross_remove";
    lua_getglobal(l, func_name);
    const char *top = lua_tostring(l, -1);
    if (top && strcmp(top, func_name) == 0) {
        lua_pushnumber(l, x);
        lua_pushnumber(l, y);
        if (lua_pcall(l, 2, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(l, -1);
            trace("modelview_call_cross_remove: lua error '%s'\n", err);
            lua_pop(l, 1);
        }
    }
    // XXX: правильно ли идет работа со стеком?
}
*/

// Найти бомбы превысившие лимит передвижений
static void bomb_scan(ModelView *mv) {
    assert(mv);

    if (!mv->can_scan_bombs)
        return;

    e_view v = e_view_create_single(mv->r, cmp_bomb);
    for (; e_view_valid(&v); e_view_next(&v)) {
        Bomb *b = e_view_get(&v, cmp_bomb);
        // Нашел бомбу которую нужно взрывать
        if (b->moves_cnt < 1) {
            e_id e = e_view_entity(&v);

            /*
            Cell *c = e_get(mv->r, e, cmp_cell);
            trace("bomb_scan: bomb at %d, %d\n", c->x, c->y);
            // */

            mv->can_scan_bombs = false;
            /*modelview_call_cross_remove(mv, c->x, c->y);*/
            modelview_cross_remove2(mv, e);
        }
    }
}

// Обновить значения сущностей в поле для быстрого получения по [x, y]
static void field_update(ModelView *mv) {

    for (int i = 0; i < mv->field_size * mv->field_size; i++) {
        mv->field[i] = e_null;
    }

    /*int i = 0;*/
    for (e_view v = e_view_create_single(mv->r, cmp_cell);
        e_view_valid(&v); e_view_next(&v)) {
        /*Cell *c = e_view_get(&v, cmp_cell);*/
        Position *c = e_get(mv->r, e_view_entity(&v), cmp_position);
        assert(c);
        /*i++;*/
        mv->field[c->y * mv->field_size + c->x] = e_view_entity(&v);
    }

    /*trace("field_update: cmp_cell num %d\n", i);*/
}

Vector2 place_center(const char *text, int fontsize) {
    float width = MeasureText(text, fontsize);
    return (Vector2) {
        .x = (GetScreenWidth() - width) / 2.,
        .y = (GetScreenWidth() - fontsize) / 2.,
    };
}

static void draw_scores(ModelView *mv) {
    char msg[64] = {0};
    const int fontsize = 70;
    sprintf(msg, "scores: %d", mv->scores);
    Vector2 pos = place_center(msg, fontsize);
    DrawText(msg, pos.x, pos.y, fontsize, BLUE);
}

// {{{ GameOverAnim

#include "box2d/id.h"
#include "box2d/types.h"
#include "box2d/box2d.h"
#include "box2d/math_functions.h"
#include "koh_b2.h"

struct GameOverAnim {
    RenderTexture2D rt;
    float       y, x;
    Color       color;
    b2WorldId   world;
    b2BodyId    b_caption, b_borders[4];
    b2DebugDraw ddraw;
    b2ShapeId   shape;
    bool        is_debug_draw;
};

typedef enum GameOverAnim_Categories {
    GameOverAnim_Caption = 0b01,
    GameOverAnim_Border =  0b10,
} GameOverAnim_Categories;

//static const char *gameover_str = "GAMEOVER";
static const char *gameover_str = "You loose";
static const int gameover_fnt_size = 400;
static const int fnt_spacing = 10;
static const float space = 100.f;

static void make_segments(GameOverAnim *ga) {
    // Сегменты-сенсоры ограничивающие полет таблички
    b2Segment segments[] = {
        {
            .point1 = { -space, -space, },
            .point2 = { GetScreenWidth() + space, -space, },
        },
        {
            .point1 = { GetScreenWidth() + space, -space, },
            .point2 = { GetScreenWidth() + space, GetScreenHeight() + space, },
        },
        {
            .point1 = { GetScreenWidth() + space, GetScreenHeight() + space, },
            .point2 = { -space, GetScreenHeight() + space, },
        },
        {
            .point1 = { -space, GetScreenHeight() + space, },
            .point2 = { -space, -space},
        },
    };

    for (int j = 0; j < 4; j++) {
        b2BodyDef bdef = b2DefaultBodyDef();
        b2ShapeDef sdef = b2DefaultShapeDef();
        //sdef.userData = &ga->vel_transformations[j];
        //sdef.filter.categoryBits = GameOverAnim_Border;
        //sdef.filter.maskBits = GameOverAnim_Caption;
        sdef.isSensor = true;
        bdef.type = b2_staticBody;
        ga->b_borders[j] = b2CreateBody(ga->world, &bdef);
        b2CreateSegmentShape(ga->b_borders[j], &sdef, &segments[j]);
    }
}

static void make_caption(GameOverAnim *ga) {
    Vector2 m = MeasureTextEx(
        GetFontDefault(), gameover_str, gameover_fnt_size, fnt_spacing
    );
    printf("make_caption: m %s\n", Vector2_tostr(m));
    ga->rt = LoadRenderTexture(m.x, m.y);
    printf(
        "gameover_new: rt %dx%d\n",
        ga->rt.texture.width,
        ga->rt.texture.height
    );

    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_dynamicBody;
    bdef.position = (b2Vec2) {
        .x = (GetScreenWidth() - m.x) / 2. /* HACK: */ + 200. ,
        .y = (GetScreenHeight() - m.y) / 2.,
    };
    ga->b_caption = b2CreateBody(ga->world, &bdef);

    b2ShapeDef shapedef = b2DefaultShapeDef();
    //shapedef.filter.categoryBits = GameOverAnim_Caption;
    //shapedef.filter.maskBits = GameOverAnim_Border;

    const float half_w = m.x / 2., half_h = m.y / 2.;
    b2Polygon box = b2MakeBox(half_w, half_h);
    ga->shape = b2CreatePolygonShape(ga->b_caption, &shapedef, &box);


    b2MassData massdata = b2ComputePolygonMass(&box, 1.f);
    printf("gameover_new: massdata %s\n", b2MassData_tostr(massdata));
    b2Body_SetMassData(ga->b_caption, massdata);

    float mass = b2Body_GetMass(ga->b_caption);
    printf("gameover_new: mass %f\n", mass);
}

// TODO: 
// Тело двигать приложением силы или импульса. 
// Добавить легкое вращение.
// Сделать границы экрана.
static GameOverAnim *gameover_new() {
    printf("gameover_new:\n");

    GameOverAnim *ga = calloc(1, sizeof(*ga));
    ga->is_debug_draw = true;
    ga->y = ga->x = 0;
    ga->color = BLACK;

    b2WorldDef def = b2DefaultWorldDef();
    def.gravity.x = 0.f;
    def.gravity.y = 0.f;
    ga->world = b2CreateWorld(&def);

    make_segments(ga);
    make_caption(ga);

    ga->ddraw = b2_world_dbg_draw_create2();

    float r1 = 0.5 + rand() / (float)RAND_MAX,
          r2 = 0.5 + rand() / (float)RAND_MAX;
    b2Vec2 imp = {1.000000e+08 * r1, -1.000000e+08 * r2};
    b2Body_ApplyLinearImpulseToCenter(ga->b_caption, imp, true);
    return ga;
}

static void gameover_gui(GameOverAnim *ga) {
    bool open = true;
    igBegin("gameover", &open, ImGuiWindowFlags_AlwaysAutoResize);

    ImVec2 z = {};
    static float force[2] = { 0.f, 0.f },
                 point[2] = { 0.f, 0.f };

    const float force_max = 10E7 * 2.f;
    igSliderFloat2("force", force, -force_max, force_max, "%f", 0);
    igSameLine(0., 10.f);
    if (igSmallButton("copy")) {
        char buf[64] = {};
        sprintf(buf, "{%e, %e}", force[0], force[1]);
        SetClipboardText(buf);
    }

    igSliderFloat2("point", point, -force_max, force_max, "%f", 0);
    /*
    if (igSmallButton("copy")) {
        char buf[64] = {};
        sprintf(buf, "{%f, %f}", force[0], force[1]);
        SetClipboardText(buf);
    }
    */

    igCheckbox("is_debug_draw", &ga->is_debug_draw);

    if (igButton("reset velocities", z)) {
        b2Body_SetAngularVelocity(ga->b_caption, 0.f);
        b2Body_SetLinearVelocity(ga->b_caption, b2Vec2_zero);
    }

    if (igButton("force to center", z)) {
        b2Vec2 t = { force[0], force[1] };
        b2Body_ApplyForceToCenter(ga->b_caption, t, true);
    }
    igSameLine(0., 10.f);
    if (igButton("force to point", z)) {
        b2Vec2 t_f = { force[0], force[1] },
               t_p = { point[0], point[1] };
        b2Body_ApplyForce(ga->b_caption, t_f, t_p, true);
    }

    if (igButton("impulse to center", z)) {
        b2Vec2 t = { force[0], force[1] };
        b2Body_ApplyLinearImpulseToCenter(ga->b_caption, t, true);
    }
    igSameLine(0., 10.f);
    if (igButton("impulse to point", z)) {
        b2Vec2 t_f = { force[0], force[1] },
               t_p = { point[0], point[1] };
        b2Body_ApplyLinearImpulse(ga->b_caption, t_f, t_p, true);
    }

    igEnd();
}

static void gameover_draw(GameOverAnim *ga) {
    // XXX: Использоть GetFrameTime()?
    const float timestep = 1. / GetFPS();
    b2WorldId world = ga->world;

    b2World_Step(world, timestep, 6);
    if (ga->is_debug_draw)
        b2World_Draw(world, &ga->ddraw);

    b2ContactEvents cevents = b2World_GetContactEvents(ga->world);
    for (int i = 0; i < cevents.beginCount; i++) {
        //sevents.beginEvents[i].sensorShapeId;
        //b2Shape_GetContactData();
        b2ContactBeginTouchEvent e = cevents.beginEvents[i];
        for (int j = 0; j < e.manifold.pointCount; j++) {
            Vector2 point = b2Vec2_to_Vector2(e.manifold.points[j].point);
            const float radius = 70.0f;
            DrawCircleV(point, radius, RED);
        }
    }

    b2SensorEvents sevents = b2World_GetSensorEvents(ga->world);
    for (int i = 0; i < sevents.beginCount; i++) {
        //sevents.beginEvents[i].sensorShapeId;
        //b2Shape_GetContactData();

        //printf("gameover_draw: sensor event i %d\n", i);

        /*
        b2ShapeId sensor = sevents.beginEvents[i].sensorShapeId;
        */
        b2ShapeId visitor = sevents.beginEvents[i].visitorShapeId;

        /*
        void *ud = b2Shape_GetUserData(sensor);
        assert(ud);
        */

        // Матрица поворота на четверть окружности
        b2Mat22 mat_90deg_rot = {
            .cx = { 0.0f,  1.0f },
            .cy = {-1.0f,  0.0f }
        };

        b2BodyId bid = b2Shape_GetBody(visitor);
        b2Vec2 vel = b2Body_GetLinearVelocity(bid);
        //printf("gameover_draw: vel %s\n", b2Vec2_to_str(vel));
        b2Vec2 new_vel = b2MulMV(mat_90deg_rot, vel);

        new_vel = b2MulSV(10E7, new_vel);
        //printf("gameover_draw: new_vel %s\n", b2Vec2_to_str(new_vel));
        // XXX: Импульс слишком незначителен
        b2Body_ApplyLinearImpulseToCenter(bid, new_vel, true);
    }

    /*printf("gameover_draw:\n");*/
    /*int fnt_size = 600;*/

    //Texture2D t = ga->rt.texture;
    Color c = ga->color;
    static int colord = 1;

    b2Polygon p = b2Shape_GetPolygon(ga->shape);
    RenderTexOpts r_opts = {
        .texture = ga->rt.texture,
        .tint = WHITE,
        // XXX: Поможет ли смещение вершин от перевернутой текстуры?
        .vertex_disp = 1,
    };

    b2Transform tr = b2Body_GetTransform(ga->b_caption);

    for (int i = 0; i < p.count; i++) {
        b2Vec2 p_l = p.vertices[i];
        b2Vec2 p_w = b2TransformPoint(tr, p_l);
        Vector2 v = b2Vec2_to_Vector2(p_w);
        r_opts.verts[i] = v;
    }

    set_uv1_inv_y(r_opts.uv);
    /*printf("gameover_draw: r_opts.uv %s\n", Vector2_arr_tostr(r_opts.uv, 4));*/

    render_v4_with_tex2(&r_opts);

    BeginTextureMode(ga->rt);
    ClearBackground(BLANK);
    DrawTextEx(
        GetFontDefault(),
        gameover_str,
        (Vector2) {0., 0.,}, 
        gameover_fnt_size,
        fnt_spacing,
        c
    );
    EndTextureMode();

    /*
    DrawTexturePro(
        t, (Rectangle) { 0., 0., t.width, -t.height },
        (Rectangle) { ga->x, ga->y, t.width, t.height },
        Vector2Zero(),
        0.f,
        WHITE
    );
    */

    ga->color.r += colord;
    ga->color.g += colord;
    ga->color.b += colord;

    if (ga->color.r >= 100)
        colord = -colord;
    if (ga->color.g >= 100)
        colord = -colord;
    if (ga->color.b >= 100)
        colord = -colord;

    ga->y += 10;
    if (ga->y > GetScreenHeight()) {
        ga->y = -gameover_fnt_size;
    }

    ga->x += rand() % 10;
    if (ga->x > GetScreenWidth()) {
        ga->x = 0;
    }
}

static void gameover_free(GameOverAnim *ga) {
    assert(ga);
    UnloadRenderTexture(ga->rt);
    b2DestroyWorld(ga->world);
    free(ga);
}

// }}}

int modelview_draw(ModelView *mv) {
    // {{{
    assert(mv);

    if (!mv->inited)
        return 0;

    automation_select(mv);

    gridanim_draw(mv->ga, mv);

    int timersnum = timerman_update(mv->timers);
    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    field_update(mv);

    history_write(mv->history);

    if (mv->state == MVS_READY) {

        bool has_bomb_exp = false;

        // Поиск взрывающейся бомбы
        e_view v = e_view_create_single(mv->r, cmp_exp);
        for (; e_view_valid(&v); e_view_next(&v)) {
            has_bomb_exp = true;
            break;
        }

        if (has_bomb_exp) {
            mv->dx = mv->dy = 0;
        }

        if (mv->dx || mv->dy) {
            destroy_dropped(mv);

            field_update(mv);

            if (!has_bomb_exp) {
                mv->has_move = do_action(mv, move);
                field_update(mv);
                mv->has_sum = do_action(mv, sum);
                field_update(mv);
            }

        }

        field_update(mv);

        if (mv->has_move) {
            bomb_scan(mv);
        }

        field_update(mv);

        if (!mv->has_sum && !mv->has_move && (mv->dx || mv->dy)) {
            mv->dx = 0;
            mv->dy = 0;
            mv->dir = DIR_NONE;
            if (mv->auto_put)
                modelview_put(mv);
        }
    }

    field_update(mv);

    if (is_over(mv)) {
        mv->state = MVS_GAMEOVER;
    }

    if (find_max(mv) >= mv->win_value) {
        /*trace("modelview_draw: win state, win_value %d\n", mv->win_value);*/
        mv->state = MVS_WIN;
    }

    sort_numbers(mv);
    ClearBackground(RAYWHITE);
    if (mv->draw_field)
        draw_grid(mv);
    draw_numbers(mv);

    draw_scores(mv);

    // XXX: идет привязка к fps
    if (mv->state == MVS_GAMEOVER) {
        gameover_draw(mv->go);
        /*return 0;*/
    }

    automation_draw(mv);

    timersnum = timerman_num(mv->timers, NULL);
    return timersnum;
    // }}}
}

void modelview_lua_after_load(struct ModelView *mv, lua_State *l) {
    trace("modelview_lua_after_load:\n");
    mv->l = l;

    lua_pushboolean(l, is_draw_grid);
    lua_setglobal(l, "is_draw_grid");
}

void modelview_init(ModelView *mv, Setup setup) {
    // {{{
    assert(mv);
    assert(setup.field_size > 1);

    mv->can_scan_bombs = true;

    mv->put_num = 1;
    mv->draw_field = true;
    mv->field_size = setup.field_size;

    int num = mv->field_size * mv->field_size;

    mv->field = calloc(num, sizeof(mv->field[0]));
    
    trace("modeltest_init: e_2destroy size %d\n", num);
    mv->e_2destroy = calloc(num, sizeof(mv->e_2destroy[0]));
    mv->e_2destroy_num = 0;

    /*SetTraceLogLevel(LOG_ALL);*/
    //Resource *reslist = &mv->reslist;
    ResList *rl = mv->reslist = reslist_new();

    mv->tex_bomb_black = reslist_load_texture(rl, "assets/bomb_black.png");
    mv->tex_bomb_red = reslist_load_texture(rl, "assets/bomb_red.png");
    mv->tex_aura = reslist_load_texture(rl, "assets/aura.png");
    mv->tex_bomb = reslist_load_texture(rl, "assets/bomb.png");

    mv->tex_ex_num = 3;
    mv->tex_ex = calloc(mv->tex_ex_num, sizeof(mv->tex_ex[0]));

    mv->tex_ex_index = 0;
    mv->tex_ex[0] = reslist_load_texture(rl, "assets/explosion_02.png");
    mv->tex_ex[1] = reslist_load_texture(rl, "assets/explosion_03.png");
    mv->tex_ex[2] = reslist_load_texture(rl, "assets/explosion_04.png");
    mv->tex_ex[3] = reslist_load_texture(rl, "assets/explosion_01.jpg");

    /*mv->font = reslist_load_font_unicode(*/
    mv->font = reslist_load_font(
        rl, "assets/jetbrains_mono.ttf", dflt_draw_opts.fontsize
    );

    /*
    mv->tex_bomb_black = res_tex_load(reslist, "assets/bomb_black.png");
    mv->tex_bomb_red = res_tex_load(reslist, "assets/bomb_red.png");
    mv->tex_aura = res_tex_load(reslist, "assets/aura.png");
    mv->tex_bomb = res_tex_load(reslist, "assets/bomb.png");

    mv->tex_ex_num = 3;
    mv->tex_ex = calloc(mv->tex_ex_num, sizeof(mv->tex_ex[0]));

    mv->tex_ex_index = 0;
    mv->tex_ex[0] = res_tex_load(reslist, "assets/explosion_02.png");
    mv->tex_ex[1] = res_tex_load(reslist, "assets/explosion_03.png");
    mv->tex_ex[2] = res_tex_load(reslist, "assets/explosion_04.png");
    mv->tex_ex[3] = res_tex_load(reslist, "assets/explosion_01.jpg");
    */

    /*SetTraceLogLevel(LOG_ERROR);*/

    const int gap = 300;
    mv->quad_width = (GetScreenHeight() - gap) / setup.field_size;
    if (mv->quad_width <= 0) {
        printf("modeltest_init: quad_width %d\n", mv->quad_width);
        exit(EXIT_FAILURE);
    }

    const int field_width = mv->field_size * mv->quad_width;
    mv->pos = (Vector2){
        .x = (GetScreenWidth() - field_width) / 2.,
        .y = (GetScreenHeight() - field_width) / 2.,
    };
    mv->timers = timerman_new(mv->field_size * mv->field_size * 3, "timers");
    mv->state = MVS_READY;
    mv->win_value = setup.win_value;

    mv->r = e_new(NULL);
    e_register(mv->r, &cmp_bomb);
    e_register(mv->r, &cmp_position);
    e_register(mv->r, &cmp_cell);
    e_register(mv->r, &cmp_transition);
    e_register(mv->r, &cmp_exp);

    mv->camera = setup.cam;
    assert(mv->camera);
    mv->use_gui = setup.use_gui;
    mv->auto_put = setup.auto_put;
    mv->use_bonus = setup.use_bonus;
    mv->font_spacing = 2.;

    if (setup.tmr_block_time > 0.) {
        const float dflt = 0.04;
        setup.tmr_block_time = dflt;
        printf(
            "modelview_init: setp.tmr_block_time %f default value\n",
            setup.tmr_block_time
        );
    }

    if (setup.tmr_put_time > 0.) {
        const float dflt = 0.04;
        setup.tmr_put_time = dflt;
        printf(
            "modelview_init: setp.tmr_put_time %f default value\n",
            setup.tmr_put_time
        );
    }

    mv->tmr_block_time = setup.tmr_block_time;
    mv->tmr_put_time = setup.tmr_put_time;

    const int cells_num = mv->field_size * mv->field_size;
    mv->sorted = calloc(cells_num, sizeof(mv->sorted[0]));

    mv->font_vector = fnt_vector_new(
        "assets/djv.ttf", &(FntVectorOpts) { .line_thick = 10.f, }
    );

    mv->text_opts = (ColoredTextOpts) {
        .font_bitmap = &mv->font,
        .font_vector = mv->font_vector,
        .use_fnt_vector = false,
    };

    field_update(mv);

    mv->on_init_lua = setup.on_init_lua;
    colored_text_init(mv->field_size);

    mv->lua_after_load = modelview_lua_after_load;

    if (mv->on_init_lua) {
        //trace("modeltest_init:\n");
        mv->on_init_lua();
    }

    mv->history = history_new(mv, true);
    mv->ga = gridanim_new(mv);
    mv->go = gameover_new();
    mv->inited = true;

    automation_load(mv);

    // }}}
}

void modelview_shutdown(struct ModelView *mv) {
    // {{{
    assert(mv);
    automation_unload(mv);

    if (mv->go) {
        gameover_free(mv->go);
        mv->go = NULL;
    }

    if (mv->ga) {
        gridanim_free(mv->ga);
        mv->ga = NULL;
    }

    if (mv->history) {
        history_free(mv->history);
        mv->history = NULL;
    }

    if (mv->reslist) {
        // FIXME: падает 
        /*reslist_free(mv->reslist);*/
        mv->reslist = NULL;
    }

    if (mv->field) {
        free(mv->field);
        mv->field = NULL;
    }

    if (mv->e_2destroy) {
        free(mv->e_2destroy);
        mv->e_2destroy = NULL;
    }

    if (mv->tex_ex) {
        free(mv->tex_ex);
        mv->tex_ex = NULL;
    }

    if (mv->font_vector) {
        fnt_vector_free(mv->font_vector);
        mv->font_vector = NULL;
    }
   
    // */

    if (mv->timers) {
        timerman_free(mv->timers);
        mv->timers = NULL;
    }

    if (mv->r) {
        e_free(mv->r);
        mv->r = NULL;
    }

    colored_text_shutdown();

    if (mv->sorted) {
        free(mv->sorted);
        mv->sorted = NULL;
    }

    mv->inited = false;
    memset(mv, 0, sizeof(*mv));
    // }}}
}

void modelview_draw_gui(struct ModelView *mv) {
    assert(mv);

    if (mv->use_gui) {
        if (mv->state == MVS_GAMEOVER)
            gameover_gui(mv->go);

        gui(mv);
        history_gui(mv->history);
    }
}

void modelview_pause_set(ModelView *mv, bool is_paused) {
    assert(mv);
    timerman_pause(mv->timers, is_paused);
}

Direction str2direction(const char *str) {
    char buf[64] = {};
    strncpy(buf, str, sizeof(buf));

    for (int i = 0; i < strlen(buf); i++) {
        buf[i] = tolower(buf[i]);
    }

    if (strcmp(buf, "left") == 0)
        return DIR_LEFT;
    else if (strcmp(buf, "right") == 0)
        return DIR_RIGHT;
    else if (strcmp(buf, "up") == 0)
        return DIR_UP;
    else if (strcmp(buf, "down") == 0)
        return DIR_DOWN;

    return DIR_NONE;
}
