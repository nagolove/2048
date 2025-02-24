// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "test_suite.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

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

static struct Cell global_cells[GLOBAL_CELLS_CAP] = {0};
static int global_cells_num = 0;
static struct Cell cell_zero = {0};

// XXX: зачем нужна переменная last_state?
static int *last_state = NULL;

//static struct ecs_circ_buf  ecs_buf = {0};
static struct ModelTest     model_checker = {0};

// }}} end of debug shit zone

struct TimerData {
    ModelView   *mv;
    e_id        cell;
};

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

static void cell_draw(ModelView *mv, e_id cell_en, DrawOpts opts);

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

static e_cp_type cmp_cell = {
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
    .str_repr_buf = str_repr_buf_cell,
};

static e_cp_type cmp_bonus = {
    .cp_sizeof = sizeof(struct Bonus),
    .name = "bonus",
    .initial_cap = 1000,
};
// */

static e_cp_type cmp_effect = {
    .cp_sizeof = sizeof(struct Effect),
    .name = "effect",
    .initial_cap = 1000,
};

static Color colors[] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    BLUE, 
    GRAY,
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

struct Cell *modelview_get_cell(
    struct ModelView *mv, int x, int y, e_id *en
) {
    assert(mv);
    assert(mv->r);

    if (en) *en = e_null;

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
}

static int get_cell_count(struct ModelView *mv) {
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

static struct Cell *create_cell(
    struct ModelView *mv, int x, int y, e_id *_en
) {
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
    assert(cell);
    cell->x = x;
    cell->y = y;
    cell->value = -1;

    struct Effect *ef = e_emplace(mv->r, en, cmp_effect);
    ef->anim_movement = false;

    if (_en)
        *_en = en;
    return cell;
}

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    assert(e_valid(mv->r, timer_data->cell));
    struct Cell *cell = e_get(mv->r, timer_data->cell, cmp_cell);
    assert(cell);

    struct Effect *ef = e_get(mv->r, timer_data->cell, cmp_effect);
    assert(ef);

    ef->anim_movement = false;
    ef->anim_alpha = AM_NONE;
    ef->anim_size = false;
}

// какой эффект рисует функция и для какой плитки?
static bool tmr_cell_draw(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    struct DrawOpts opts = dflt_draw_opts;

    assert(e_valid(mv->r, timer_data->cell));

    opts.amount = t->amount;
    cell_draw(timer_data->mv, timer_data->cell, opts);
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
    if (v >= 0. && v < 0.9) {
        cell->value = 2;
    } else {
        cell->value = 4;
    }

    assert(cell->value > 0);
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
        .cell = cell_en,
    };
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_put_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &td,
    });
}

static void modelview_put_bonus(
    struct ModelView *mv, int x, int y, enum BonusType type
) {
    assert(mv);
    assert(x >= 0);
    assert(x < mv->field_size);
    assert(y >= 0);
    assert(y < mv->field_size);
    e_id cell_en = e_null;
    struct Cell *cell = create_cell(mv, x, y, &cell_en);

    assert(cell);

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9) {
        cell->value = 2;
    } else {
        cell->value = 4;
    }

    assert(cell->value > 0);
    cell->x = x;
    cell->y = y;
    cell->dropped = false;

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    assert(ef);
    ef->anim_alpha = AM_FORWARD;

    struct Bonus *bonus = e_emplace(mv->r, cell_en, cmp_bonus);
    assert(bonus);
    bonus->type = type;

    /*modeltest_put(&model_checker, cell->x, cell->y, cell->value);*/

    struct TimerData td = {
        .mv = mv,
        .cell = cell_en,
    };
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_put_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &td,
    });

}


// XXX:Количество цифр должно зависеть от совбодной площади
// TODO: Чем более крупная фишка есть на поле, тем более крупная может выпадать
// ячейка, но до определенного предела.
void modelview_put(struct ModelView *mv) {
    
    assert(mv);

    for (int i = 0; i < mv->put_num; i++) {
        int x = rand() % mv->field_size;
        int y = rand() % mv->field_size;
        // Максимальное количество проб клетки на возможность заполнения
        int j = mv->field_size * mv->field_size * 10; 
        struct Cell *cell = modelview_get_cell(mv, x, y, NULL);

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
            if ((double)rand() / (double)RAND_MAX > 0.5)
                modelview_put_cell(mv, x, y);
            else
                modelview_put_bonus(mv, x, y, BT_DOUBLE);
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

    /*
    printf(
        "cell_en id %lu, ord %u, ver %u\n",
        cell_en.id, 
        cell_en.ord,
        cell_en.ver
    );
    */

    /*
    printf(
        "move: de_has(cell_en,  cmp_cell) %s\n",
        e_has(mv->r, cell_en,  cmp_cell) ? "true" : "false"
    );
    printf(
        "move: de_has(cell_en,  cmp_effect) %s\n",
        e_has(mv->r, cell_en,  cmp_effect) ? "true" : "false"
    );
    printf(
        "move: de_has(cell_en,  cmp_bonus) %s\n",
        e_has(mv->r, cell_en,  cmp_bonus) ? "true" : "false"
    );
    */

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);
    if (!cell)
        return false;
    //assert(cell);

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    assert(ef);

    //struct Bonus *bonus = e_get(mv->r, cell_en, cmp_bonus);

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

    *touched = true;

    /*trace("try_move: move_call_counter %d\n", move_call_counter);*/

    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .data = &(struct TimerData) {
            .mv = mv,
            .cell = cell_en,
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
    //assert(cell);
    if (!cell)
        return false;

    struct Effect *ef = e_get(mv->r, cell_en, cmp_effect);
    assert(ef);

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

    struct Effect *neighbour_ef = e_get(mv->r, neighbour_en, cmp_effect);

    assert(cell_en.id != e_null.id);
    assert(neighbour_en.id != e_null.id);

    if (cell->value != neighbour->value)
        return has_sum;

    has_sum = true;
    *touched = true;
    neighbour->value += cell->value;
    mv->scores += cell->value;

    /*koh_screenshot_incremental();*/
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
            .cell = cell_en,
        },
    });

    // neighbour_en увеличивается 
    neighbour_ef->anim_size = true;
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        //.on_stop = tmr_cell_draw_stop_drop,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .data = &(struct TimerData) {
            .mv = mv,
            .cell = neighbour_en,
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

    /*
    const char *action_name = action == sum ? "sum" : "move";
    printf("do_action: %s\n", action_name);
    */

    bool has_action = false;
    bool touched = false;

    /*modelview_field_print(mv);*/

    do {
    //for (int i = 0; i < 3; ++i) {
        touched = false;
        for (int y = 0; y < mv->field_size; ++y) 
            for (int x = 0; x < mv->field_size; ++x) {
                e_id cell_en = e_null;
                struct Cell *cell = modelview_get_cell(mv, x, y, &cell_en);
                if (!cell) continue;
                if (action(mv, cell_en, x, y, &touched))
                    has_action = true;
        }
    //}
    } while (touched);

    /*modelview_field_print(mv);*/

    /*
    trace(
        "move: timerman_num %d, move_call_counter %d\n",
        timerman_num(mv->timers_slides, NULL),
        move_call_counter++
    );
    */

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
        default: 
            //mv->dx = 0;
            //mv->dy = 0;
            break;
    }
    //trace("modelview_input: dir %s\n", dir2str(dir));
}

static int cmp(const void *pa, const void *pb) {
    const struct Cell *a = pa, *b = pb;
    return a->value < b->value;
}

static void sort_numbers(struct ModelView *mv) {
    size_t num = mv->field_size * mv->field_size;
    struct Cell tmp[num];
    memset(tmp, 0, sizeof(tmp));
    int idx = 0;
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

        /*trace("sort_numbers: %s\n", buf);*/
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

static Color get_color(struct ModelView *mv, int cell_value) {
    int colors_num = sizeof(colors) / sizeof(colors[0]);
    for (int k = 0; k < colors_num; k++) {
        if (cell_value == mv->sorted[k].value) {
            return colors[k];
        }
    }
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
            color.a = Lerp(10., 255., opts.amount);
            break;
        case AM_BACKWARD:
            color.a = Lerp(255., 10., opts.amount);
            break;
        default:
            break;
    }
    return color;
}

// XXX: Что возвращает функция?
static Vector2 calc_base_pos(ModelView *mv, e_id en, DrawOpts opts) {
    Vector2 base_pos;

    struct Cell *cell = e_get(mv->r, en, cmp_cell);
    assert(cell);
    struct Effect *ef = e_get(mv->r, en, cmp_effect);
    assert(ef);

    if (ef->anim_movement) {
        base_pos = (Vector2) {
            //Lerp(cell->x, cell->x + mv->dx, opts.amount),
            //Lerp(cell->y, cell->y + mv->dy, opts.amount),
            Lerp(cell->x - mv->dx, cell->x, opts.amount),
            Lerp(cell->y - mv->dy, cell->y, opts.amount),
        };
    } else {
        base_pos = (Vector2) {
            cell->x, cell->y,
        };
    }

    return base_pos;
}

static void cell_draw(
    struct ModelView *mv, e_id cell_en, struct DrawOpts opts
) {
    assert(mv);

    struct Cell *cell = e_get(mv->r, cell_en, cmp_cell);

    if (!cell)
        return;

    // TODO: падает при проверке наличия компоненты в сущности(если ни одной
    // сущности такого типа не создано?)
    struct Bonus *bonus = e_get(mv->r, cell_en, cmp_bonus);

    char cell_text[64] = {0};
    if (!bonus)
        sprintf(cell_text, "%d", cell->value);
    else if (bonus->type == BT_DOUBLE)
        sprintf(cell_text, "x2");
    else {
        trace("cell_draw: bad bonus type %d\n", bonus->type);
        abort();
    }

    /*
    // XXX: Зачем нужен данный вывод к консоль?
    if (opts.amount <= 0.) {
        trace("cell_draw: \033[1;31m!\033[0m opts.amount %f\n", opts.amount);
        opts.amount = 0.;
    }

    // XXX: Зачем нужен данный вывод к консоль?
    if (opts.amount >= 1.) {
        trace("cell_draw: \033[1;31m!\033[0m opts.amount %f\n", opts.amount);
        opts.amount = 1.;
    }
    */

    Vector2 base_pos = calc_base_pos(mv, cell_en, opts);
    int fontsize = calc_font_size_anim(mv, cell_en, opts);
    // XXX: Почему не используются все цвета из colors?
    Color color = opts.custom_color ? opts.color : get_color(mv, cell->value);

    color = calc_alpha(mv, cell_en, color, opts);

    const float thick = 8.;

    Vector2 text_bound;
    if (!bonus) {
        text_bound.x = mv->quad_width;
        text_bound.y = mv->quad_width;
    } else {
        text_bound.x = mv->quad_width - thick * 6.;
        text_bound.y = mv->quad_width - thick * 6.;
    }

    Vector2 textsize = decrease_font_size(
        mv, cell_text, &fontsize, text_bound
    );

    struct ColoredText text_cell[] = {
        { 
            .text = cell_text,
            .scale = 1.,
            .color = color,
        },
    };

    Color multiplier_color = mv->color_theme.foreground;
    multiplier_color. a = color.a;

    struct ColoredText text_double_bonus[] =  {
        { 
            .text = "x",
            .scale = 0.5,
            .color = multiplier_color,
        },
        { 
            .text = "2",
            .scale = 1.,
            .color = color,
        },
    };

    struct ColoredText *texts;
    int texts_num;
    if (!bonus) {
        texts = text_cell;
        texts_num = sizeof(text_cell) / sizeof(text_cell[0]);
    } else {
        texts = text_double_bonus;
        texts_num = sizeof(text_double_bonus) / sizeof(text_double_bonus[0]);
    }

    assert(texts);

    fontsize = colored_text_pickup_size(
        texts, texts_num,
        mv->text_opts, text_bound
    );

    Vector2 offset = {
        opts.caption_offset_coef.x * (mv->quad_width - textsize.x) / 2.,
        opts.caption_offset_coef.y * (mv->quad_width - fontsize) / 2.,
    };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, mv->quad_width), offset);
    Vector2 pos = Vector2Add(mv->pos, disp);

    colored_text_print(texts, texts_num, pos, mv->text_opts, fontsize);
    //DrawTextEx(mv->font, msg, pos, fontsize, 0, color);

    if (bonus) {
        Vector2 _offset = {
            opts.caption_offset_coef.x * (mv->quad_width - text_bound.x) / 2.,
            opts.caption_offset_coef.y * (mv->quad_width - text_bound.y) / 2.,
        };
        Vector2 tmp = Vector2Scale(base_pos, mv->quad_width);
        Vector2 _disp = Vector2Add(tmp, _offset);
        Vector2 _pos = Vector2Add(mv->pos, _disp);

        //float cur_thick = (-1. + sin(GetTime())) * 1.5 + 0.5;
        const float cur_thick = thick;
        Rectangle rect = {
            .x = _pos.x + cur_thick * 0.,
            .y = _pos.y + cur_thick * 0.,
            .width = mv->quad_width - cur_thick * 6.,
            .height = mv->quad_width - cur_thick * 6.,
        };

        //const float roundness = 0.5;
        //const int segments = 10;
        //Color color = BLACK;
        //DrawRectangleRoundedLines(rect, roundness, segments, thick, color);
       
        Color border_color = BLUE;
        border_color.a = 128. + (0. + sin(GetTime())) * 128.;
        DrawRectangleLinesEx(rect, thick, border_color);
    }

    //DrawText("PRIVET", 100, 100, 200, BLUE);
    //DrawText("PRIVET", 100, 600, 40, GREEN);
}

static void draw_numbers(struct ModelView *mv) {
    assert(mv);
    for (int y = 0; y < mv->field_size; y++) {
        for (int x = 0; x < mv->field_size; x++) {
            e_id en = e_null;
            struct Cell *cell = modelview_get_cell(mv, x, y, &en);
            if (!cell) continue;
            assert(en.id != e_null.id);
            struct Effect *ef = e_get(mv->r, en, cmp_effect);
            assert(ef);

            bool can_draw = cell && !ef->anim_movement && 
                            !ef->anim_size && ef->anim_alpha == AM_NONE;
            if (can_draw)
                cell_draw(mv, en, dflt_draw_opts);
        }
    }
}

static void movements_window() {
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
}

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

static void entities_window(struct ModelView *mv) {
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
}

static void options_window(struct ModelView *mv) {
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

    igSliderFloat("tmr_block_time", &mv->tmr_block_time, 0.01, 1., "%f", 0);
    igSliderFloat("tmr_put_time", &mv->tmr_put_time, 0.01, 1., "%f", 0);

    static bool use_bonus = false;
    igCheckbox("use bonuses", &use_bonus);

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

    if (igButton("restart", (ImVec2) {0, 0})) {
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
        };
        modelview_init(mv, setup);

        // XXX: Опасный код так как параметры проходят мимо конструктора
        mv->put_num = put_num;
        mv->draw_field = draw_field;

        modelview_put(mv);
    }

    igEnd();
}

static void gui(struct ModelView *mv) {
    //rlImGuiBegin(false, mv->camera);
    rlImGuiBegin();

    //if (mv->camera)
        //trace("gui: %s\n", camera2str(*mv->camera));

    //rlImGuiBegin(false, NULL);
    movements_window();
    removed_entities_window();
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
    bool open = false;
    igShowDemoWindow(&open);
    rlImGuiEnd();
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

bool modelview_draw(ModelView *mv) {
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

    if (mv->state == MVS_READY) {
        if (mv->dx || mv->dy) {
            destroy_dropped(mv);

            mv->has_move = do_action(mv, move);
            mv->has_sum = do_action(mv, sum);
        }

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

void modelview_init(struct ModelView *mv, Setup setup) {
    assert(mv);
    memset(mv, 0, sizeof(*mv));

    assert(setup.field_size > 1);
    last_state = NULL;

    mv->put_num = 1;
    mv->draw_field = true;
    mv->field_size = setup.field_size;
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

    global_cells_num = 0;
    mv->font_vector = fnt_vector_new("assets/djv.ttf", &(FntVectorOpts) {
        .line_thick = 10.f,
    });

    mv->text_opts = (ColoredTextOpts) {
        .font_bitmap = &mv->font,
        .font_vector = mv->font_vector,
        .use_fnt_vector = false,
    };
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);

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

void modelview_field_print(struct ModelView *mv) {
    char buf[1024 * 8] = {};
    modelview_field_print_s(mv, buf, sizeof(buf));
    printf("%s\n", buf);
}

void _modelview_field_print(ecs_t *r, int field_size) {
    assert(r);
    char buf[1024 * 8] = {};
    _modelview_field_print_s(r, field_size, buf, sizeof(buf));
    printf("%s\n", buf);
}

void modelview_field_print_s(struct ModelView *mv, char *str, size_t str_sz) {
    assert(mv);
    _modelview_field_print_s(mv->r, mv->field_size, str, str_sz);
}

// XXX: Что делает функция?
void _modelview_field_print_s(
    ecs_t *r, int field_size, char *str, size_t str_sz
) {
    assert(r);

    int cells_num = field_size * field_size;
    assert(cells_num > 0);
    int field[cells_num];
    memset(field, 0, sizeof(field));
    bool dropped[cells_num];
    memset(dropped, 0, sizeof(dropped));

    e_view v = e_view_create_single(r, cmp_cell); 
    for (; e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c) {
            field[c->y * field_size + c->x] = c->value;
            dropped[c->y * field_size + c->x] = c->dropped;
        }
    }

    // TODO: Проверка достаточности длины буфера str
    str += sprintf(str, "\n");
    for (int y = 0; y < field_size; ++y) {
        for (int x = 0; x < field_size; ++x) {
            int cell_value = field[field_size * y + x];
            bool is_dropped = dropped[field_size * y + x];
            if (cell_value) {

                /*
                char digits_buf_in[12] = {};
                char digits_buf_out[12] = {};
                sprintf(digits_buf_in, "%.3d", cell_value);
                for (int i = 0; i < strlen(digits_buf_in); i++) {
                    char digit[6] = {};
                    sprintf(digit, "%c", digits_buf_in[i]);
                    strcat(digits_buf_out, digit);
                    //strcat(digits_buf_out, u8"\u0303");
                    if (is_dropped) {
                        //koh_trap();
                        strcat(digits_buf_out, u8"\u035A");
                    }
                }
                */

                //sprinf("[%s] ", digits_buf_out);
                str += sprintf(str, "[%c%.3d] ", is_dropped ? 'x' : ' ', cell_value);
                //if (is_dropped)
                    //koh_trap();
            } else {
                str += sprintf(str, "[----] ");
            }
        }
        str += sprintf(str, "\n");
    }
    str += sprintf(str, "\n");
}

static bool iter_entity(ecs_t *r, e_id e, void *udata) {
    printf("%lu ", e.id);
    return false;
}

struct Cell *modelview_find_by_value(ecs_t *r, int value) {
    for (e_view v = e_view_create_single(r, cmp_cell);
        e_view_valid(&v); e_view_next(&v)) {
        struct Cell *c = e_view_get(&v, cmp_cell);
        assert(c);
        if (c && c->value == value) {
            return c;
        }
    }

    return NULL;
}

void modelview_each_entity(ecs_t *r) {
    printf("modelview_each_entity:\n");
    e_each(r, iter_entity, NULL);
    printf("\n");
}
