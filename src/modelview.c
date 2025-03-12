// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "test_suite.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

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

// {{{ debug shit zone
#define GLOBAL_CELLS_CAP    100000

//static struct Cell global_cells[GLOBAL_CELLS_CAP] = {0};
/*static int global_cells_num = 0;*/
/*static struct Cell cell_zero = {0};*/

// XXX: зачем нужна переменная last_state?
static int *last_state = NULL;
static bool is_draw_grid = true;

//static struct ecs_circ_buf  ecs_buf = {0};
static struct ModelTest     model_checker = {0};

// }}} end of debug shit zone

typedef struct TimerData {
    ModelView   *mv;
    e_id        e;
} TimerData;

const struct ColorTheme color_theme_dark = {
                            .background = BLACK,
                            .foreground = RAYWHITE,
                        },
                        color_theme_light = {
                            .background = RAYWHITE,
                            .foreground = BLACK,
                        };

typedef struct DrawOpts {
    int                 fontsize;
    Vector2             caption_offset_coef; // 1. is default value
    bool                custom_color;
    Color               color;
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

static StrBuf str_repr_buf_cell(void *payload, e_id e) {
    StrBuf buf = strbuf_init(NULL);

    struct Cell *cell = payload;
    strbuf_add(&buf, "{ ");
    strbuf_addf(&buf, "x = %d,", cell->x);
    strbuf_addf(&buf, "y = %d,", cell->x);
    strbuf_addf(&buf, "value = %d,", cell->value);
    strbuf_addf(&buf, "dropped = %s,", cell->dropped ? "true" : "false");
    strbuf_add(&buf, "}");

    return buf;
}

e_cp_type cmp_cell = {
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
    .str_repr_buf = str_repr_buf_cell,
};

e_cp_type cmp_bonus = {
    .cp_sizeof = sizeof(struct Bonus),
    .name = "bonus",
    .initial_cap = 1000,
};
// */

e_cp_type cmp_effect = {
    .cp_sizeof = sizeof(struct Effect),
    .name = "effect",
    .initial_cap = 1000,
};

// Тег бомбы
e_cp_type cmp_bomb_exp = {
    .cp_sizeof = sizeof(char),
    .name = "bomb explosion tag",
    .initial_cap = 1000,
};

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

const char *dir2str(enum Direction dir) {
    static char buf[32] = {0};
    switch (dir) {
        case DIR_LEFT: sprintf(buf, "%s", "LEFT"); break;
        case DIR_RIGHT: sprintf(buf, "%s", "RIGHT"); break;
        case DIR_DOWN: sprintf(buf, "%s", "DOWN"); break;
        case DIR_UP: sprintf(buf, "%s", "UP"); break;
        case DIR_NONE: sprintf(buf, "%s", "NONE"); break;
    }
    return buf;
};

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

// Получить ячейку и сущность на данной позиции
Cell *modelview_get_cell(ModelView *mv, int x, int y, e_id *en) {
    assert(mv);
    assert(mv->r);

    if (en) *en = e_null;

    // TODO: Избавится от полного перебора
    for (e_view v = e_view_create_single(mv->r, cmp_cell);
        e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c && c->x == x && c->y == y) {
            if (en) {
                assert(e_valid(mv->r, e_view_entity(&v)));
                *en = e_view_entity(&v);
            }
            return c;
        }
    }
    return NULL;
    // */

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

static Cell *create_cell(ModelView *mv, int x, int y, e_id *_en) {
    assert(mv);
    assert(mv->r);

    if (get_cell_count(mv) >= mv->field_size * mv->field_size) {
        trace("create_cell: cells were reached limit of %d\n",
              mv->field_size * mv->field_size);
        abort();
        return NULL;
    }

    e_id en = e_create(mv->r);
    struct Cell *cell = e_emplace(mv->r, en, cmp_cell);
    memset(cell, 0, sizeof(*cell));
    assert(cell);
    cell->x = x;
    cell->y = y;
    cell->value = 0;

    struct Effect *ef = e_emplace(mv->r, en, cmp_effect);
    memset(ef, 0, sizeof(*ef));
    ef->anim_movement = false;
    ef->phase = rand() % 1024;

    if (_en)
        *_en = en;
    return cell;
}

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    if (!e_valid(mv->r, timer_data->e)) {
        return;
    }

    struct Cell *cell = e_get(mv->r, timer_data->e, cmp_cell);
    assert(cell);

    struct Effect *ef = e_get(mv->r, timer_data->e, cmp_effect);
    assert(ef);

    ef->anim_movement = false;
    ef->anim_alpha = AM_NONE;
    ef->anim_size = false;
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

void modelview_put_manual(struct ModelView *mv, int x, int y, int value) {
    struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
    if (!cell) {
        cell = create_cell(mv, x, y, NULL);
    }
    cell->x = x;
    cell->y = y;
    cell->value = value;
}

static bool tmr_pulsation_update(struct Timer *t) {
    TimerData *td = t->data;
    ecs_t *r = td->mv->r;
    e_id e = td->e;
    /*trace("tmr_pulsation_update:\n");*/

    if (!e_valid(r, e))
        return true;

    Effect *ef = e_get(r, e, cmp_effect);
    if (!ef)
        return false;

    return false;
}

static void tmr_pulsation_stop(Timer *t) {
    TimerData *td = t->data;
    ecs_t *r = td->mv->r;
    e_id e = td->e;

    if (!e_valid(r, e))
        return;

    /*trace("tmr_pulsation_stop:\n");*/
}

// TODO: Добавить анимацию.
// XXX: Анимацию чего?
static void modelview_put_cell(struct ModelView *mv, int x, int y) {
    assert(mv);
    assert(x >= 0);
    assert(x < mv->field_size);
    assert(y >= 0);
    assert(y < mv->field_size);
    e_id cell_en = e_null;
    struct Cell *cell = create_cell(mv, x, y, &cell_en);

    assert(cell);

    float v = (float)rand() / (float)RAND_MAX;

    if (mv->strong) {
        if (v >= 0. && v < 0.9) {
            cell->value = 1;
        } else {
            cell->value = 3;
        }
    } else {
        if (v >= 0. && v < 0.9) {
            cell->value = 2;
        } else {
            cell->value = 4;
        }
    }

    assert(cell->value >= 0);
    cell->x = x;
    cell->y = y;
    //cell->anima = true;
    cell->dropped = false;

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    assert(ef);
    ef->anim_alpha = AM_FORWARD;

    modeltest_put(&model_checker, cell->x, cell->y, cell->value);

    struct TimerData td = {
        .mv = mv,
        .e = cell_en,
    };

    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_put_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &td,
    });
    timerman_add(mv->timers_effects, (TimerDef) {
        .data = &td,
        .duration = 1.,
        .on_stop = tmr_pulsation_stop,
        .on_update = tmr_pulsation_update,
        .sz = sizeof(struct TimerData),
    });

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

static void modelview_put_bonus(ModelView *mv, int x, int y, BonusType type) {
    assert(mv);
    assert(x >= 0);
    assert(x < mv->field_size);
    assert(y >= 0);
    assert(y < mv->field_size);

    //if (bomb_find_num(mv) >= 2) 
        //return;

    e_id cell_en = e_null;

    Cell *cell = NULL;

    /*
    // XXX: Удаляет в текущей ячейке содержимое
    cell = modelview_get_cell(mv, x, y, &cell_en);
    //modelview_get_cell(mv, x, y, &cell_en);
    // Клетка уже есть созданная. Уничтожить
    if (cell_en.id != e_null.id) {
        e_destroy(mv->r, cell_en);
    }
    */

    cell = create_cell(mv, x, y, &cell_en);

    assert(cell);

    /*
    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9) {
        cell->value = 2;
    } else {
        cell->value = 4;
    }
    */

    assert(cell->value >= 0);
    cell->x = x;
    cell->y = y;
    cell->dropped = false;

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    assert(ef);
    memset(ef, 0, sizeof(*ef));
    ef->anim_alpha = AM_FORWARD;

    struct Bonus *bonus = e_emplace(mv->r, cell_en, cmp_bonus);
    assert(bonus);
    memset(bonus, 0, sizeof(*bonus));
    bonus->moves_cnt = bomb_moves_limit;
    bonus->type = type;
    bonus->bomb_color = koh_maybe();

    bonus->aura_rot = rand() % 360;
    bonus->aura_phase = rand() % 1024;

    /*modeltest_put(&model_checker, cell->x, cell->y, cell->value);*/

    TimerData td = {
        .mv = mv,
        .e = cell_en,
    };
    timerman_add(mv->timers_slides, (TimerDef) {
        .duration = mv->tmr_put_time,
        .sz = sizeof(td),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &td,
    });

}

// INFO: Добавлять бомбу, которая может уничтожить самыый большой элемент

// INFO: Добавлять второй элемент только тогда когда и только туда, где он
// может быть уничтожен. Успех зависит от игрока - ошибется он или примет
// верное решение

// XXX:Количество цифр должно зависеть от совбодной площади
// TODO: Чем более крупная фишка есть на поле, тем более крупная может выпадать
// ячейка, но до определенного предела.
void modelview_put(struct ModelView *mv) {
   
    assert(mv);
    /*field_update(mv);*/

    for (int i = 0; i < mv->put_num; i++) {
        int x = rand() % mv->field_size;
        int y = rand() % mv->field_size;
        // Максимальное количество проб клетки на возможность заполнения
        int j = mv->field_size * mv->field_size * 10; 
        Cell *cell = modelview_get_cell(mv, x, y, NULL);

        while (cell) {
            x = rand() % mv->field_size;
            y = rand() % mv->field_size;
            cell = modelview_get_cell(mv, x, y, NULL);

            j--;
            if (j <= 0) {
                // спасение бегством от бесконечного цикла
                return;
            }
        }

        if (mv->use_bonus) {
            if (mv->next_bomb) {
                modelview_put_bonus(mv, x, y, BT_BOMB);
                mv->next_bomb = false;
            } else {
                if ((double)rand() / (double)RAND_MAX > chance_bomb)
                    modelview_put_cell(mv, x, y);
                else
                    modelview_put_bonus(mv, x, y, BT_BOMB);
            }
        } else
            modelview_put_cell(mv, x, y);
    }
}

static bool cell_in_bounds(struct ModelView *mv, struct Cell *cell) {
    assert(mv);
    assert(cell);
    if (cell->x + mv->dx >= mv->field_size)
        return false;
    if (cell->x + mv->dx < 0)
        return false;

    if (cell->y + mv->dy >= mv->field_size)
        return false;
    if (cell->y + mv->dy < 0)
        return false;
    return true;
}

static bool move(
    struct ModelView *mv, e_id cell_en, int x, int y, bool *touched
) {
    bool has_move = false;

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);
    if (!cell)
        return false;
    //assert(cell);

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    if (!ef)
        return false;
    /*assert(ef);*/

    //if (cell->touched)
        //return has_move;

    if (ef->anim_movement)
        return has_move;

    if (cell->dropped)
        return has_move;

    struct Cell *neighbour = modelview_get_cell(
        mv, x + mv->dx, y + mv->dy, NULL
    );
    if (neighbour) 
        return has_move;

    if (!cell_in_bounds(mv, cell))
        return has_move;

    has_move = true;

    assert(cell_en.id != e_null.id);

    cell->x += mv->dx;
    cell->y += mv->dy;
    ef->anim_movement = true;

    // Обновление счетчика движения
    // TODO: Показывать счетчик движений. 
    // Когда достигается 0, то происходит взрыв.
    // TODO: Сделать слияние бомб одного типа
    Bonus *bonus = e_get(mv->r, cell_en, cmp_bonus);
    if (bonus) {
        bonus->moves_cnt--;
    }

    *touched = true;

    /*trace("try_move: move_call_counter %d\n", move_call_counter);*/

    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &(struct TimerData) {
            .mv = mv,
            .e = cell_en,
        },
    });

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
    Bonus *b = e_get(mv->r, cell_en, cmp_bonus);

    if (b && b->type == BT_BOMB) {
        return false;
    }

    //assert(cell);
    if (!cell)
        return false;

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    if (!ef)
        return false;
    /*assert(ef);*/

    if (cell->dropped) 
        return has_sum;

    e_id neighbour_en = e_null;

    struct Cell *neighbour = modelview_get_cell(
        mv, x + mv->dx, y + mv->dy, &neighbour_en
    );

    if (!neighbour) 
        return has_sum;
    if (neighbour->dropped) 
        return has_sum;

    // Если соседом оказался бонус - выход
    Bonus *neighbour_bonus = e_get(mv->r, neighbour_en, cmp_bonus);
    if (neighbour_bonus)
        return has_sum;

    struct Effect *neighbour_ef = e_get(mv->r, neighbour_en, cmp_effect);

    assert(cell_en.id != e_null.id);
    assert(neighbour_en.id != e_null.id);

    if (cell->value != neighbour->value)
        return has_sum;

    has_sum = true;
    *touched = true;
    neighbour->value += cell->value;
    mv->scores += cell->value;

    assert(e_valid(mv->r, cell_en));

    // cell_en уничтожается
    cell->dropped = true;
    ef->anim_alpha = AM_BACKWARD;
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .data = &(struct TimerData) {
            .mv = mv,
            .e = cell_en,
        },
    });

    // neighbour_en увеличивается 
    neighbour_ef->anim_size = true;
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .data = &(struct TimerData) {
            .mv = mv,
            .e = neighbour_en,
        },
    });

    return has_sum;
}

typedef bool (*Action)(
    struct ModelView *mv, e_id cell_en, int x, int y, bool *touched
);

static bool do_action(struct ModelView *mv, Action action) {
    assert(mv);
    assert(action);

    bool has_action = false;
    bool touched = false;

    /*
    do {
        touched = false;
        for (int y = 0; y < mv->field_size; ++y) 
            for (int x = 0; x < mv->field_size; ++x) {
                e_id cell_en = e_null;
                Cell *cell = modelview_get_cell(mv, x, y, &cell_en);
                if (!cell) continue;
                if (action(mv, cell_en, x, y, &touched))
                    has_action = true;
        }
    } while (touched);
    */

    do {
        touched = false;
        for (int y = 0; y < mv->field_size; ++y) 
            for (int x = 0; x < mv->field_size; ++x) {
                e_id cell_en = e_null;
                Cell *cell = modelview_get_cell(mv, x, y, &cell_en);
                if (!cell) continue;

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
            struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
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
        DrawLineEx(tmp, end, thick, mv->color_theme.foreground);
        tmp.x += mv->quad_width;
    }

    tmp = start;
    for (int u = 0; u <= mv->field_size; u++) {
        Vector2 end = tmp;
        end.x += field_width;
        DrawLineEx(tmp, end, thick, mv->color_theme.foreground);
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

/*
static Vector2 decrease_font_size_colored(
    struct ModelView *mv, const struct ColoredText *texts, int texts_num,
    int *fontsize, Vector2 text_bound
) {
    Vector2 textsize = {};
    do {
        textsize = MeasureTextEx(
            mv->font, msg, (*fontsize)--, mv->font_spacing
        );
    } while (textsize.x > text_bound.x || textsize.y > text_bound.y);
    return textsize;
}
*/

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

static int calc_font_size_anim(
    struct ModelView *mv, e_id en, struct DrawOpts opts
) {
    int fontsize = opts.fontsize;
    struct Effect *ef = e_get(mv->r, en, cmp_effect);
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

static Color calc_alpha(
    struct ModelView *mv, e_id en, Color color, struct DrawOpts opts
) {
    assert(mv);
    struct Effect *ef = e_get(mv->r, en, cmp_effect);
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

    struct Cell *cell = e_get(mv->r, en, cmp_cell);
    assert(cell);
    struct Effect *ef = e_get(mv->r, en, cmp_effect);

    Vector2 base_pos = { cell->x, cell->y, };
    if (!ef)
        return base_pos;
    /*assert(ef);*/

    if (ef->anim_movement) {
        base_pos = (Vector2) {
            //Lerp(cell->x, cell->x + mv->dx, opts.amount),
            //Lerp(cell->y, cell->y + mv->dy, opts.amount),
            Lerp(cell->x - mv->dx, cell->x, opts.amount),
            Lerp(cell->y - mv->dy, cell->y, opts.amount),
        };
    }

    return base_pos;
}

static void bomb_draw(
    ModelView *mv, e_id cell_en, Vector2 base_pos, DrawOpts opts
) {

    Texture2D tex = {};
    Bonus *b = e_get(mv->r, cell_en, cmp_bonus);

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

static void bonus_draw(
    ModelView *mv, e_id cell_en, Vector2 base_pos, DrawOpts opts
) {
    Bonus *b = e_get(mv->r, cell_en, cmp_bonus);
    if (b->type == BT_BOMB) {
        bomb_draw(mv, cell_en, base_pos, opts);
    }
}

static void cell_draw(
    ModelView *mv, e_id cell_en, Vector2 base_pos, DrawOpts opts
) {
    Cell *cell = e_get(mv->r, cell_en, cmp_cell);

    // HACK: Не рисовать если на сущности висит тэг взрыва бомбы
    //*
    char *bomb_tag = e_get(mv->r, cell_en, cmp_bomb_exp);
    if (bomb_tag)
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

    Bonus *bonus = e_get(mv->r, cell_en, cmp_bonus);
    Vector2 base_pos = calc_base_pos(mv, cell_en, opts);

    char *bomb_tag = e_get(mv->r, cell_en, cmp_bomb_exp);
    if (bomb_tag) {
        exp_draw(mv, cell_en, base_pos, opts);
    } else {
        if (bonus) {
            bonus_draw(mv, cell_en, base_pos, opts);
        } else {
            cell_draw(mv, cell_en, base_pos, opts);
        }
    }

}

static void draw_numbers(ModelView *mv) {
    assert(mv);
    for (int y = 0; y < mv->field_size; y++) {
        for (int x = 0; x < mv->field_size; x++) {
            e_id en = e_null;
            Cell *cell = modelview_get_cell(mv, x, y, &en);

            if (!cell) continue;
            if (cell->dropped) continue;
            if (cell->value == 0) continue;

            assert(en.id != e_null.id);
            Effect *ef = e_get(mv->r, en, cmp_effect);
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
*/

static void print_node_cell(struct Cell *c) {
    assert(c);
    igText("pos     %d, %d", c->x, c->y);
    //igText("to      %d, %d", c->to_x, c->to_y);
    //igText("act     %s", action2str(c->action));
    igText("val     %d", c->value);
    igText("dropped %s", c->dropped ? "t" : "f");
}

static void print_node_effect(struct Effect *ef) {
    assert(ef);
    igText("anim_movement   %s", ef->anim_movement ? "t" : "f");
    igText("anim_size %s", ef->anim_size ? "t" : "f");
    igText("anim_alpha %d", ef->anim_alpha);
}

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

static void options_window(struct ModelView *mv) {
    // {{{
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

    if (IsKeyPressed(KEY_R) || igButton("restart", (ImVec2) {0, 0})) {
        modelview_shutdown(mv);
        struct Setup setup = {
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
            .color_theme = theme_light ? color_theme_light : color_theme_dark,
            .on_init_lua = mv->on_init_lua,
        };
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

    entities_window(mv);
    e_gui_buf(mv->r);

    /*
    timerman_window((struct TimerMan* []) { 
            mv->timers_slides,
            mv->timers_effects,
    }, 2);
    */

    timerman_window_gui(mv->timers_slides);
    timerman_window_gui(mv->timers_effects);

    options_window(mv);
    //ecs_window(mv);
    // }}}
}

char *modelview_state2str(enum ModelViewState state) {
    static char buf[32] = {0};
    switch (state) {
        case MVS_ANIMATION: strcpy(buf, "animation"); break;
        case MVS_READY: strcpy(buf, "ready"); break;
        case MVS_WIN: strcpy(buf, "win"); break;
        case MVS_GAMEOVER: strcpy(buf, "gameover"); break;
    }
    return buf;
}

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
static void tmr_bomb_stop(Timer *t) {
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

    e_id e = e_null;
    Cell *c = modelview_get_cell(mv, x, y, &e);


    if (e.id == e_null.id)
        e = e_create(mv->r);

    if (!c) {
        Cell *c_new = e_emplace(mv->r, e, cmp_cell);
        assert(c_new);
        c_new->x = x;
        c_new->y = y;
    } else {
        e_2destroy[mv->e_2destroy_num++] = e;
    }

    /*char *bomb_tag = e_emplace(mv->r, e, cmp_bomb_exp);*/
    char *bomb_tag = e_get(mv->r, e, cmp_bomb_exp);

    if (!bomb_tag)
        bomb_tag = e_emplace(mv->r, e, cmp_bomb_exp);

    if (bomb_tag) {
        *bomb_tag = 1;

        TimerData td = { .mv = mv, .e = e, };
        timerman_add(mv->timers_effects, (TimerDef) {
            .data = &td,
            .duration = tmr_bomb_duration,
            .on_stop = tmr_bomb_stop,
            .on_update = tmr_bomb_update,
            .sz = sizeof(td),
        });
    }
}

// Удалить все клетки на пересечении x и y, кроме данной клетки
void modelview_cross_remove2(ModelView *mv, e_id e) {
    assert(mv->field_size > 0);

    Cell *c = e_get(mv->r, e, cmp_cell);
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
// TODO: Взрыв бомбы от другой бомбы
// TODO: Условие выпадения второй бомбы(для последовательной детонации)
// Вторая бомба должна редко должна когда есть открытая первая бомба.
static void bomb_scan(ModelView *mv) {
    assert(mv);

    if (!mv->can_scan_bombs)
        return;

    e_view v = e_view_create_single(mv->r, cmp_bonus);
    for (; e_view_valid(&v); e_view_next(&v)) {
        Bonus *b = e_view_get(&v, cmp_bonus);
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

    int i = 0;
    for (e_view v = e_view_create_single(mv->r, cmp_cell);
        e_view_valid(&v); e_view_next(&v)) {
        Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        i++;
        mv->field[c->y * mv->field_size + c->x] = e_view_entity(&v);
    }

    /*trace("field_update: cmp_cell num %d\n", i);*/
}

bool modelview_draw(ModelView *mv) {
    /*trace("modelview_draw:\n");*/
    assert(mv);

    bool dir_none = false;

    modeltest_update(&model_checker, mv->dir);

    timerman_update(mv->timers_effects);
    int timersnum = timerman_update(mv->timers_slides);
    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    static enum ModelViewState prev_state = 0;
    if (mv->state != prev_state) {
        prev_state = mv->state;
    }

    field_update(mv);

    if (mv->state == MVS_READY) {

        bool has_bomb_exp = false;
        // Поиск взрывающейся бомбы
        e_view v = e_view_create_single(mv->r, cmp_bomb_exp);
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
            mv->prev_dir = mv->dir;
            mv->dir = DIR_NONE;
            dir_none = true;
            if (mv->auto_put)
                modelview_put(mv);
        }
    }

    field_update(mv);

    if (is_over(mv)) {
        mv->state = MVS_GAMEOVER;
    }

    if (find_max(mv) >= mv->win_value) {
        trace("modelview_draw: win state, win_value %d\n", mv->win_value);
        mv->state = MVS_WIN;
    }


    sort_numbers(mv);
    ClearBackground(mv->color_theme.background);
    if (mv->draw_field)
        draw_grid(mv);
    draw_numbers(mv);

    return dir_none;
}

void modelview_lua_after_load(struct ModelView *mv, lua_State *l) {
    trace("modelview_lua_after_load:\n");
    mv->l = l;

    lua_pushboolean(l, is_draw_grid);
    lua_setglobal(l, "is_draw_grid");
}

void modelview_init(ModelView *mv, Setup setup) {
    assert(mv);
    /*memset(mv, 0, sizeof(*mv));*/

    assert(setup.field_size > 1);
    last_state = NULL;

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
    Resource *reslist = &mv->reslist;
    mv->tex_bomb_black = res_tex_load(reslist, "assets/bomb_black.png");
    mv->tex_bomb_red = res_tex_load(reslist, "assets/bomb_red.png");
    mv->tex_aura = res_tex_load(reslist, "assets/aura.png");
    mv->tex_bomb = res_tex_load(reslist, "assets/bomb.png");

    /*mv->tex_ex_num = 4;*/
    mv->tex_ex_num = 3;
    mv->tex_ex = calloc(mv->tex_ex_num, sizeof(mv->tex_ex[0]));

    /*mv->tex_ex[0] = res_tex_load(reslist, "assets/explosion_01.jpg");*/
    mv->tex_ex_index = 0;
    mv->tex_ex[0] = res_tex_load(reslist, "assets/explosion_02.png");
    mv->tex_ex[1] = res_tex_load(reslist, "assets/explosion_03.png");
    mv->tex_ex[2] = res_tex_load(reslist, "assets/explosion_04.png");

    /*mv->tex_bomb_black = LoadTexture("assets/bomb_black.png");*/
    /*mv->tex_bomb_red = LoadTexture("assets/bomb_red.png");*/
    /*mv->tex_aura = LoadTexture("assets/aura.png");*/

    /*SetTraceLogLevel(LOG_ERROR);*/

    const int cells_num = mv->field_size * mv->field_size;

    const int gap = 300;
    mv->quad_width = (GetScreenHeight() - gap) / setup.field_size;
    assert(mv->quad_width > 0);

    const int field_width = mv->field_size * mv->quad_width;
    //if (!setup.pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    /*} else */
        /*mv->pos = *setup.pos;*/

    mv->timers_slides = timerman_new(
        mv->field_size * mv->field_size * 2, "slides"
    );
    mv->timers_effects = timerman_new(
        mv->field_size * mv->field_size * 2, "effects"
    );

    mv->state = MVS_READY;
    mv->dropped = false;

    mv->win_value = setup.win_value;

    mv->r = e_new(NULL);
    e_register(mv->r, &cmp_bonus);
    e_register(mv->r, &cmp_cell);
    e_register(mv->r, &cmp_effect);
    e_register(mv->r, &cmp_bomb_exp);

    mv->camera = setup.cam;
    mv->use_gui = setup.use_gui;
    mv->auto_put = setup.auto_put;
    mv->use_bonus = setup.use_bonus;
    mv->font_spacing = 2.;
    mv->color_theme = setup.color_theme;

    // XXX: Утечка из-за глобальной переменной?
    //ecs_circ_buf_init(&ecs_buf, 2048);

    assert(setup.tmr_block_time > 0.);
    assert(setup.tmr_put_time > 0.);
    mv->tmr_block_time = setup.tmr_block_time;
    mv->tmr_put_time = setup.tmr_put_time;

    modeltest_init(&model_checker, mv->field_size);
    /*
    FILE *file = fopen("state.txt", "w");
    if (file)
        fclose(file);
    */
    mv->font = load_font_unicode(
        "assets/jetbrains_mono.ttf", dflt_draw_opts.fontsize
    );
    mv->sorted = calloc(cells_num, sizeof(mv->sorted[0]));

    /*global_cells_num = 0;*/
    mv->font_vector = fnt_vector_new("assets/djv.ttf", &(FntVectorOpts) {
        .line_thick = 10.f,
    });

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
        trace("modeltest_init:\n");
        mv->on_init_lua();
    }
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);

    res_unload_all(&mv->reslist, true);

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

    if (last_state) {
        free(last_state);
        last_state = NULL;
    }

    //memset(mv, 0, sizeof(*mv));
    if (mv->dropped)
        return;

    if (mv->timers_effects) {
        timerman_free(mv->timers_effects);
        mv->timers_effects = NULL;
    }

    if (mv->timers_slides) {
        timerman_free(mv->timers_slides);
        mv->timers_slides = NULL;
    }

    UnloadFont(mv->font);

    if (mv->r) {
        e_free(mv->r);
        mv->r = NULL;
    }

    colored_text_shutdown();

    if (mv->sorted) {
        free(mv->sorted);
        mv->sorted = NULL;
    }
    modeltest_shutdown(&model_checker);
    mv->dropped = true;
}

void modelview_draw_gui(struct ModelView *mv) {
    assert(mv);

#if !defined(PLATFORM_WEB)
    if (mv->use_gui) 
        gui(mv);
#endif

}

