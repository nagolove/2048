// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "colored_text.h"
#include "ecs_circ_buf.h"
#include "koh_common.h"
#include "koh_console.h"
#include "koh_destral_ecs.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "modeltest.h"
#include "modelview.h"
#include "raylib.h"
#include "raymath.h"
#include "timers.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GLOBAL_CELLS_CAP    100000

static struct Cell global_cells[GLOBAL_CELLS_CAP] = {0};
static int global_cells_num = 0;
static struct Cell cell_zero = {0};

//static struct ecs_circ_buf  ecs_buf = {0};
static struct ModelTest     model_checker = {0};

struct TimerData {
    struct ModelView    *mv;
    de_entity           cell;
};

const struct ColorTheme color_theme_dark = {
                            .background = BLACK,
                            .foreground = RAYWHITE,
                        },
                        color_theme_light = {
                            .background = RAYWHITE,
                            .foreground = BLACK,
                        };

struct DrawOpts {
    int                 fontsize;
    Vector2             caption_offset_coef; // 1. is default value
    bool                custom_color;
    Color               color;
    double              amount;             // 0 .. 1.
};

static const struct DrawOpts dflt_draw_opts = {
    .caption_offset_coef = { 1., 1., },
    .fontsize            = 500,
    .custom_color        = false,
    .amount              = 0.,
    /*.color               = BLACK,*/
};

static const de_cp_type cmp_cell = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
};

static const de_cp_type cmp_bonus = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct Bonus),
    .name = "bonus",
    .initial_cap = 1000,
};
// */

static const de_cp_type cmp_effect = {
    .cp_id = 2,
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

/*
void modelview_save_state2file(struct ModelView *mv) {
    static struct Cell prev_state[FIELD_SIZE * FIELD_SIZE] = {0};
    static int prev_state_sz = 0;

    static struct Cell state[FIELD_SIZE * FIELD_SIZE] = {0};
    static int state_sz = 0;

    state_sz = 0;
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { cmp_cell });
         de_view_valid(&v); de_view_next(&v)) {
        assert(de_valid(mv->r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        state[state_sz++] = *c;
    }

    bool equal = false;
    if (prev_state_sz == state_sz) {
        int eq_num = 0;
        for (int i = 0; i < state_sz; ++i) {
            for (int j = 0; j < prev_state_sz; j++) {
                eq_num += memcmp(
                        &state[i], &prev_state[j], sizeof(state[0])) == 0;
            }
            trace("modelview_save_state2file: eq_num %d\n", eq_num);
        }

        equal = eq_num == prev_state_sz;
    }

    memmove(prev_state, state, sizeof(state[0]) * state_sz);
    prev_state_sz = state_sz;

    trace("modelview_save_state2file: equal %s\n", equal ? "t" : "f");
    if (equal) return;

    FILE *file = fopen("state.txt", "a");
    if (!file) return;
    fprintf(file, "\n");

    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { cmp_cell });
         de_view_valid(&v); de_view_next(&v)) {
        assert(de_valid(mv->r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);

        //fprintf(file, "fr %d, %d ", c->from_x, c->from_y);
        //fprintf(file, "to %d, %d ", c->to_x, c->to_y);
        fprintf(file, "%d, %d ", c->x, c->y);
        fprintf(file, "val %d ", c->value);
        fprintf(file, "anim_size %s ", c->anim_size ? "true" : "false");
        fprintf(file, "anima %s ", c->anima ? "true" : "false");
        fprintf(file, "dropped %s ", c->dropped ? "true" : "false");
        fprintf(file, "\n");
    }

    fprintf(file, "\n");
    fprintf(file, "\n");

    fclose(file);
}
*/

static void cell_draw(
    struct ModelView *mv, de_entity cell_en, struct DrawOpts opts
);

static int find_max(struct ModelView *mv) {
    int max = 0;

    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c && c->value > max)
            max = c->value;
    }

    return max;
}

static bool is_over(struct ModelView *mv) {
    int num = 0;

    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c) num += c->value > 0;
    }

    return mv->field_size * mv->field_size == num;
}

struct Cell *modelview_get_cell(
    struct ModelView *mv, int x, int y, de_entity *en
) {
    assert(mv);
    assert(mv->r);

    if (en) *en = de_null;

    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c && c->x == x && c->y == y) {
            if (en) {
                assert(de_valid(mv->r, de_view_entity(&v)));
                *en = de_view_entity(&v);
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
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        assert(de_valid(mv->r, de_view_entity(&v)));
        num += c ? 1 : 0;
    }
    return num;
}

static struct Cell *create_cell(
    struct ModelView *mv, int x, int y, de_entity *_en
) {
    assert(mv);
    assert(mv->r);

    if (get_cell_count(mv) >= mv->field_size * mv->field_size) {
        trace("create_cell: cells were reached limit of %d\n",
              mv->field_size * mv->field_size);
        abort();
        return NULL;
    }

    de_entity en = de_create(mv->r);
    struct Cell *cell = de_emplace(mv->r, en, cmp_cell);
    assert(cell);
    cell->x = x;
    cell->y = y;
    cell->value = -1;

    struct Effect *ef = de_emplace(mv->r, en, cmp_effect);
    ef->anim_movement = false;

    if (_en)
        *_en = en;
    return cell;
}

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    assert(de_valid(mv->r, timer_data->cell));
    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);
    assert(cell);

    struct Effect *ef = de_try_get(mv->r, timer_data->cell, cmp_effect);
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

    assert(de_valid(mv->r, timer_data->cell));

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
    de_entity cell_en = de_null;
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

    struct Effect *ef = de_try_get(mv->r, cell_en, cmp_effect);
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
        .udata = &td,
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
    de_entity cell_en = de_null;
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

    struct Effect *ef = de_try_get(mv->r, cell_en, cmp_effect);
    assert(ef);
    ef->anim_alpha = AM_FORWARD;

    struct Bonus *bonus = de_emplace(mv->r, cell_en, cmp_bonus);
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
        .udata = &td,
    });

    /*
    timerman_add(mv->timers_effects, (struct TimerDef) {
        .duration = 0.5,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_bonus_border_color_update,
        .udata = &td,
    });
    */

}

/*
static bool tmr_bonus_border_color_update(struct Timer *t) {
    assert(t);
    struct TimerData *td = t->data;
    assert(td);
    assert(td->mv);
    assert(td->mv->r);
    assert(td->cell != de_null);
    struct Bonus *bonus = de_try_get(td->mv->r, td->cell, cmp_bonus);
    assert(bonus);
    bonus->border_color.a = 
    return false;
}
*/


void modelview_put(struct ModelView *mv) {
    assert(mv);
    int x = rand() % mv->field_size;
    int y = rand() % mv->field_size;

    struct Cell *cell = modelview_get_cell(mv, x, y, NULL);
    while (cell) {
        x = rand() % mv->field_size;
        y = rand() % mv->field_size;
        cell = modelview_get_cell(mv, x, y, NULL);
    }

    if (mv->use_bonus) {
        if ((double)rand() / (double)RAND_MAX > 0.5)
            modelview_put_cell(mv, x, y);
        else
            modelview_put_bonus(mv, x, y, BT_DOUBLE);
    } else
        modelview_put_cell(mv, x, y);
}

static void global_cell_push(struct Cell *cell) {
    assert(cell);
    global_cells[global_cells_num++] = *cell;
}

/*
static void clear_touched(struct ModelView *mv) {
    for (int x = 0; x < mv->field_size; ++x)
        for (int y = 0; y < mv->field_size; ++y) {
            de_entity cell_en = de_null;
            struct Cell *cell = modelview_get_cell(mv, x, y, &cell_en);
            if (cell)
                cell->touched = false;
        }
}
*/

static int move_call_counter = 0;

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
    struct ModelView *mv, de_entity cell_en, int x, int y, bool *touched
) {
    bool has_move = false;

    struct Cell *cell = de_try_get(mv->r, cell_en, cmp_cell);
    assert(cell);

    struct Effect *ef = de_try_get(mv->r, cell_en, cmp_effect);
    assert(ef);

    //struct Bonus *bonus = de_try_get(mv->r, cell_en, cmp_bonus);

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

    assert(cell_en != de_null);

    cell->x += mv->dx;
    cell->y += mv->dy;
    ef->anim_movement = true;

    *touched = true;
    trace("try_move: move_call_counter %d\n", move_call_counter);

    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw,
        .on_stop = tmr_cell_draw_stop,
        .udata = &(struct TimerData) {
            .mv = mv,
            .cell = cell_en,
        },
    });

    return has_move;
}

static bool sum(
    struct ModelView *mv, de_entity cell_en, 
    int x, int y, bool *touched
) {
    assert(mv);
    assert(cell_en != de_null);

    bool has_sum = false;

    struct Cell *cell = de_try_get(mv->r, cell_en, cmp_cell);
    assert(cell);

    struct Effect *ef = de_try_get(mv->r, cell_en, cmp_effect);
    assert(ef);

    if (cell->dropped) 
        return has_sum;

    de_entity neighbour_en = de_null;

    struct Cell *neighbour = modelview_get_cell(
        mv, x + mv->dx, y + mv->dy, &neighbour_en
    );
    if (!neighbour) 
        return has_sum;
    if (neighbour->dropped) 
        return has_sum;

    struct Effect *neighbour_ef = de_try_get(mv->r, neighbour_en, cmp_effect);

    assert(cell_en != de_null);
    assert(neighbour_en != de_null);

    if (cell->value != neighbour->value)
        return has_sum;

    has_sum = true;
    *touched = true;
    neighbour->value += cell->value;
    mv->scores += cell->value;

    /*koh_screenshot_incremental();*/
    assert(de_valid(mv->r, cell_en));

    // cell_en уничтожается
    cell->dropped = true;
    ef->anim_alpha = AM_BACKWARD;
    timerman_add(mv->timers_slides, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw,
        .sz = sizeof(struct TimerData),
        .udata = &(struct TimerData) {
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
        .udata = &(struct TimerData) {
            .mv = mv,
            .cell = neighbour_en,
        },
    });

    return has_sum;
}

typedef bool (*Action)(
    struct ModelView *mv, de_entity cell_en, int x, int y, bool *touched
);

static bool do_action(struct ModelView *mv, Action action) {
    assert(mv);
    assert(action);

    printf("do_action:\n");

    //clear_touched(mv);
    bool has_action = false;
    bool touched = false;

    do {
    //for (int i = 0; i < 3; ++i) {
        touched = false;
        for (int y = 0; y < mv->field_size; ++y) 
            for (int x = 0; x < mv->field_size; ++x) {
                de_entity cell_en = de_null;
                struct Cell *cell = modelview_get_cell(mv, x, y, &cell_en);
                if (!cell) continue;
                if (action(mv, cell_en, x, y, &touched))
                    has_action = true;
        }
    //}
    } while (touched);

    trace(
        "move: timerman_num %d, move_call_counter %d\n",
        timerman_num(mv->timers_slides, NULL),
        move_call_counter++
    );

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
    struct Cell tmp[mv->field_size * mv->field_size];
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
    memmove(mv->sorted, tmp, sizeof(tmp[0]) * idx);
}

static void draw_field(struct ModelView *mv) {
    assert(mv);
    ClearBackground(mv->color_theme.background);

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
    struct ModelView *mv, de_entity en, struct DrawOpts opts
) {
    int fontsize = opts.fontsize;
    struct Effect *ef = de_try_get(mv->r, en, cmp_effect);
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
    struct ModelView *mv, de_entity en, Color color, struct DrawOpts opts
) {
    assert(mv);
    struct Effect *ef = de_try_get(mv->r, en, cmp_effect);
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

static Vector2 calc_base_pos(
    struct ModelView *mv, de_entity en, struct DrawOpts opts
) {
    Vector2 base_pos;

    struct Cell *cell = de_try_get(mv->r, en, cmp_cell);
    assert(cell);
    struct Effect *ef = de_try_get(mv->r, en, cmp_effect);
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
    struct ModelView *mv, de_entity cell_en, struct DrawOpts opts
) {
    assert(mv);

    struct Cell *cell = de_try_get(mv->r, cell_en, cmp_cell);

    if (!cell)
        return;

    // TODO: падает при проверке наличия компоненты в сущности(если ни одной
    // сущности такого типа не создано?)
    struct Bonus *bonus = de_try_get(mv->r, cell_en, cmp_bonus);

    char msg[64] = {0};
    if (!bonus)
        sprintf(msg, "%d", cell->value);
    else if (bonus->type == BT_DOUBLE)
        sprintf(msg, "x2");
    else {
        trace("cell_draw: bad bonus type %d\n", bonus->type);
        abort();
    }

    if (opts.amount <= 0.) {
        trace("cell_draw: \033[1;31m!\033[0m opts.amount %f\n", opts.amount);
        opts.amount = 0.;
    }

    if (opts.amount >= 1.) {
        trace("cell_draw: \033[1;31m!\033[0m opts.amount %f\n", opts.amount);
        opts.amount = 1.;
    }

    Vector2 base_pos = calc_base_pos(mv, cell_en, opts);
    int fontsize = calc_font_size_anim(mv, cell_en, opts);
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

    Vector2 textsize = decrease_font_size(mv, msg, &fontsize, text_bound);

    struct ColoredText text_cell[] = {
        { 
            .text = msg,
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
        &mv->font, text_bound
    );

    Vector2 offset = {
        opts.caption_offset_coef.x * (mv->quad_width - textsize.x) / 2.,
        opts.caption_offset_coef.y * (mv->quad_width - fontsize) / 2.,
    };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, mv->quad_width), offset);
    Vector2 pos = Vector2Add(mv->pos, disp);

    colored_text_print(texts, texts_num, pos, &mv->font, fontsize);
    //DrawTextEx(mv->font, msg, pos, fontsize, 0, color);

    if (bonus) {
        Vector2 _offset = {
            opts.caption_offset_coef.x * (mv->quad_width - text_bound.x) / 2.,
            opts.caption_offset_coef.y * (mv->quad_width - text_bound.y) / 2.,
        };
        Vector2 _disp = Vector2Add(Vector2Scale(base_pos, mv->quad_width), _offset);
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
            de_entity en = de_null;
            struct Cell *cell = modelview_get_cell(mv, x, y, &en);
            if (!cell) continue;
            assert(en != de_null);
            struct Effect *ef = de_try_get(mv->r, en, cmp_effect);
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
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        if (de_valid(mv->r, de_view_entity(&v))) {
            struct Cell *c = de_view_get_safe(&v, cmp_cell);
            struct Effect *ef = de_view_get_safe(&v, cmp_effect);

            igSetNextItemOpen(true, ImGuiCond_Once);

            if (igTreeNode_Ptr((void*)(uintptr_t)idx, "i %d", idx)) {
                igText("en      %ld", de_view_entity(&v));
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
    igBegin("options_window", &wnd_open, 0);

    static int field_size = 0;

    if (!field_size)
        field_size = mv->field_size;

    igSliderInt("field size", &field_size, 3, 20, "%d", 0);

    static bool use_bonus = true;
    igCheckbox("use bonuses", &use_bonus);

    static bool theme_light = true;
    if (igRadioButton_Bool("color theme: light", theme_light)) {
        theme_light = true;
    } else if (igRadioButton_Bool("color theme: dark", !theme_light)) {
        theme_light = false;
    }

    if (igButton("restart", (ImVec2) {0, 0})) {
        modelview_shutdown(mv);
        struct Setup setup = {
            .auto_put = true,
            .cam = mv->camera,
            .field_size = field_size,
            .tmr_block_time = mv->tmr_block_time,
            .tmr_put_time = mv->tmr_put_time,
            .pos = &mv->pos,
            .use_gui = mv->use_gui,
            .use_bonus = use_bonus,
            .color_theme = theme_light ? color_theme_light : color_theme_dark,
        };
        modelview_init(mv, setup);
        modelview_put(mv);
    }

    igEnd();
}

static void gui(struct ModelView *mv) {
    //rlImGuiBegin(false, mv->camera);
    rlImGuiBegin(false, NULL);

    //if (mv->camera)
        //trace("gui: %s\n", camera2str(*mv->camera));

    //rlImGuiBegin(false, NULL);
    movements_window();
    removed_entities_window();
    //paths_window();
    entities_window(mv);

    timerman_window((struct TimerMan* []) { 
            mv->timers_slides,
            mv->timers_effects,
    }, 2);

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
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        assert(de_valid(mv->r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c->dropped) {
            trace(
                "destroy_dropped: cell val %d, (%d, %d)\n",
                c->value, c->x, c->x
            );
            global_cell_push(c);
            memset(c, 0, sizeof(*c));
            de_destroy(mv->r, de_view_entity(&v));
        }
    }
}

bool modelview_draw(struct ModelView *mv) {
    assert(mv);

    bool dir_none = false;

    modeltest_update(&model_checker, mv->dir);

    timerman_update(mv->timers_effects);
    int timersnum = timerman_update(mv->timers_slides);
    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    //destroy_dropped(mv);

    static enum ModelViewState prev_state = 0;
    if (mv->state != prev_state) {
        trace("modelview_draw: state %s\n", modelview_state2str(mv->state));
        prev_state = mv->state;
        trace("modelview_draw: dir %s\n", dir2str(mv->dir));
    }

    if (mv->state == MVS_READY) {
        if (mv->dx || mv->dy) {
            // TODO: удаляет неправильно?
            destroy_dropped(mv);

            mv->has_move = do_action(mv, move);
            mv->has_sum = do_action(mv, sum);
            //mv->has_move = do_action(mv, move);
            
            trace(
                "modelview_draw: has_move %s, has_sum %s\n",
                mv->has_move ? "t" : "f",
                mv->has_sum ? "t" : "f"
            );

            printf("modelview_draw: clear input flags\n");
        }

        if (!mv->has_sum && !mv->has_move && (mv->dx || mv->dy)) {
            mv->dx = 0;
            mv->dy = 0;
            mv->dir = DIR_NONE;
            dir_none = true;
            if (mv->auto_put)
                modelview_put(mv);
        }
    }

    if (is_over(mv)) {
        mv->state = MVS_GAMEOVER;
    }

    if (find_max(mv) == WIN_VALUE) {
        mv->state = MVS_WIN;
    }

    draw_field(mv);
    sort_numbers(mv);
    draw_numbers(mv);

    return dir_none;
}

void modelview_init(struct ModelView *mv, const struct Setup setup) {
    assert(mv);
    assert(setup.field_size > 1);
    mv->field_size = setup.field_size;
    const int cells_num = mv->field_size * mv->field_size;

    const int gap = 300;
    mv->quad_width = (GetScreenHeight() - gap) / setup.field_size;
    assert(mv->quad_width > 0);

    const int field_width = mv->field_size * mv->quad_width;
    if (!setup.pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *setup.pos;

    mv->timers_slides = timerman_new(
        mv->field_size * mv->field_size * 2, "slides"
    );
    mv->timers_effects = timerman_new(
        mv->field_size * mv->field_size * 2, "effect"
    );

    mv->state = MVS_READY;
    mv->dropped = false;
    mv->r = de_ecs_make();
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
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);
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
        de_ecs_destroy(mv->r);
        mv->r = NULL;
    }

    if (mv->sorted) {
        free(mv->sorted);
        mv->sorted = NULL;
    }
    modeltest_shutdown(&model_checker);
    mv->dropped = true;
}

/*
void modelview_put_cell(struct ModelView *mv, struct Cell cell) {
    assert(mv);

    assert(cell.x >= 0);
    assert(cell.y >= 0);
    assert(cell.x < mv->field_size);
    assert(cell.y < mv->field_size);

    struct Cell *new_cell = modelview_get_cell(mv, cell.x, cell.y, NULL);
    if (!new_cell) {
        new_cell = create_cell(mv, cell.x, cell.y, NULL);
    }
    memset(new_cell, 0, sizeof(*new_cell));
    new_cell->x = cell.x;
    new_cell->y = cell.y;
    new_cell->value = cell.value;
}
*/

void modelview_draw_gui(struct ModelView *mv) {
    assert(mv);

#if !defined(PLATFORM_WEB)
    if (mv->use_gui) 
        gui(mv);
#endif

}
