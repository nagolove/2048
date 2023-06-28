// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
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

struct ecs_circ_buf {
    de_ecs  **ecs;
    int     num, cap;
    int     i, j, cur;
};

static struct ecs_circ_buf  ecs_buf = {0};
static struct de_ecs        *ecs_tmp = NULL;
static struct ModelTest     model_checker = {0};

static void ecs_circ_buf_init(struct ecs_circ_buf *b, int cap) {
    assert(cap > 0);
    assert(b);
    b->cap = cap;
    b->ecs = calloc(b->cap, sizeof(b->ecs[0]));
    b->num = 0;
    b->cur = 0;
}

static void ecs_circ_buf_shutdown(struct ecs_circ_buf *b) {
    assert(b);
    for (int j = 0; j < b->num; ++j) {
        if (b->ecs[j]) {
            de_ecs_destroy(b->ecs[j]);
            b->ecs[j] = NULL;
        }
    }
    if (b->ecs) {
        free(b->ecs);
        b->ecs = NULL;
    }
}

static void ecs_circ_buf_push(struct ecs_circ_buf *b, de_ecs *r) {
    if (b->ecs[b->i]) {
        de_ecs_destroy(b->ecs[b->i]);
    }
    b->ecs[b->i] = de_ecs_clone(r);
    b->i = (b->i + 1) % b->cap;
    b->num++;
    if (b->num >= b->cap)
        b->num = b->cap;
}

static de_ecs *ecs_circ_buf_prev(struct ecs_circ_buf *b) {
    assert(b);
    de_ecs *ret = NULL;
    if (b->cur > 0) {
        if (b->ecs[b->cur - 1]) {
            ret = b->ecs[b->cur - 1];
            b->cur--;
        }
    }
    return ret;
}

static de_ecs *ecs_circ_buf_next(struct ecs_circ_buf *b) {
    assert(b);
    de_ecs *ret = NULL;
    if (b->cur < b->cap) {
        if (b->ecs[b->cur + 1]) {
            ret = b->ecs[b->cur + 1];
            b->cur++;
        }
    }
    return ret;
}

struct TimerData {
    struct ModelView    *mv;
    de_entity           cell;
};

enum AlphaMode {
    AM_NONE,
    AM_FORWARD,
    AM_BACKWARD,
};

struct DrawOpts {
    int                 fontsize;
    Vector2             offset_coef; // 1. is default value
    bool                custom_color;
    Color               color;
    double              amount;
    bool                anim_size;
    enum    AlphaMode   anim_alpha;
};

static const struct DrawOpts dflt_draw_opts = {
    .offset_coef = {
        1.,
        1.,
    },
    .fontsize = 140,
    .custom_color = false,
    .anim_alpha = AM_NONE,
    .anim_size = false,
    .amount = 0.,
    .color = BLACK,
};

static const de_cp_type cmp_cell = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
};

#if defined(PLATFORM_WEB)
static const int quad_width = 128 + 32;
#else
static const int quad_width = 128 + 64 + 32;
#endif

static Color colors[] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    BLUE, 
    GRAY,
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
    struct ModelView *mv, struct Cell *cell, struct DrawOpts opts
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

static struct Cell *get_cell(
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

static void fill_guards(struct Cell *cell) {
    assert(cell);
    int8_t v = rand() % 127;
    memset(cell->guard1, v, sizeof(cell->guard1));
    memset(cell->guard2, v, sizeof(cell->guard2));
}

static void check_guards(struct Cell *cell) {
    assert(cell);
    for (int i = 0; i < sizeof(cell->guard1); ++i) {
        assert(cell->guard1[i] == cell->guard2[i]);
    }
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
    fill_guards(cell);
    cell->x = x;
    cell->y = y;
    cell->value = -1;
    check_guards(cell);
    //trace("create_cell: en %ld at (%d, %d)\n", en, cell->from_x, cell->from_y);
    if (_en)
        *_en = en;
    return cell;
}

static void tmr_cell_draw_stop_drop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    assert(de_valid(mv->r, timer_data->cell));
    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);
    assert(cell);

    cell->anima = false;
    cell->anim_size = false;
    //cell->dropped = true;
}

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    assert(de_valid(mv->r, timer_data->cell));
    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);
    assert(cell);

    cell->anima = false;
    cell->anim_size = false;
}

// какой эффект рисует функция и для какой плитки?
static bool tmr_cell_draw_drop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    struct DrawOpts opts = dflt_draw_opts;

    assert(de_valid(mv->r, timer_data->cell));

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    opts.amount = t->amount;
    opts.color = MAGENTA;
    opts.anim_alpha = AM_BACKWARD;
    cell_draw(timer_data->mv, cell, opts);
    return false;
}

// какой эффект рисует функция и для какой плитки?
static bool tmr_cell_draw_neighbour_drop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    struct DrawOpts opts = dflt_draw_opts;

    assert(de_valid(mv->r, timer_data->cell));

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    opts.anim_size = true;
    opts.amount = t->amount;
    cell_draw(timer_data->mv, cell, opts);
    return false;
}

static bool tmr_cell_draw_put(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    struct DrawOpts opts = dflt_draw_opts;

    assert(de_valid(mv->r, timer_data->cell));

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    opts.amount = t->amount;
    opts.anim_alpha = AM_FORWARD;
    cell_draw(timer_data->mv, cell, opts);
    return false;
}

static bool tmr_cell_draw(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    struct DrawOpts opts = dflt_draw_opts;

    assert(de_valid(mv->r, timer_data->cell));

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    if (!cell) return false;

    opts.amount = t->amount;
    cell_draw(timer_data->mv, cell, opts);
    return false;
}

void modelview_put_manual(struct ModelView *mv, int x, int y, int value) {
    struct Cell *cell = get_cell(mv, x, y, NULL);
    if (!cell) {
        cell = create_cell(mv, x, y, NULL);
    }
    cell->x = x;
    cell->y = y;
    cell->value = value;
}

void modelview_put(struct ModelView *mv) {
    assert(mv);
    int x = rand() % mv->field_size;
    int y = rand() % mv->field_size;

    struct Cell *cell = get_cell(mv, x, y, NULL);
    while (cell) {
        x = rand() % mv->field_size;
        y = rand() % mv->field_size;
        cell = get_cell(mv, x, y, NULL);
    }

    assert(!cell);
    de_entity cell_en = de_null;
    cell = create_cell(mv, x, y, &cell_en);

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
    cell->anima = true;
    cell->dropped = false;

    modeltest_put(&model_checker, cell->x, cell->y, cell->value);

    struct TimerData td = {
        .mv = mv,
        .cell = cell_en,
    };
    timerman_add(mv->timers, (struct TimerDef) {
        .duration = mv->tmr_put_time,
        .sz = sizeof(struct TimerData),
        .on_update = tmr_cell_draw_put,
        .on_stop = tmr_cell_draw_stop,
        .udata = &td,
    });
}

static void global_cell_push(struct Cell *cell) {
    assert(cell);
    global_cells[global_cells_num++] = *cell;
}

static bool move(struct ModelView *mv) {
    /*int cells_num = de_typeof_num(mv->r, cmp_cell);*/

    for (int x = 0; x < mv->field_size; ++x)
        for (int y = 0; y < mv->field_size; ++y) {
            de_entity cell_en = de_null;
            struct Cell *cell = get_cell(mv, x, y, &cell_en);
            if (cell)
                cell->touched = false;
    }

    bool has_move = false;

    /*for (int i = 0; i <= cells_num; ++i) {*/
    bool touched = false;
    do {
        touched = false;
        for (int x = 0; x < mv->field_size; ++x)
            for (int y = 0; y < mv->field_size; ++y) {
                de_entity cell_en = de_null;
                struct Cell *cell = get_cell(mv, x, y, &cell_en);
                if (!cell) continue;

                if (cell->touched)
                    continue;

                if (cell->dropped)
                    continue;

                struct Cell *neighbour = get_cell(
                    mv, x + mv->dx, y + mv->dy, NULL
                );
                if (neighbour) continue;

                if (cell->x + mv->dx >= mv->field_size)
                    continue;
                if (cell->x + mv->dx < 0)
                    continue;

                if (cell->y + mv->dy >= mv->field_size)
                    continue;
                if (cell->y + mv->dy < 0)
                    continue;

                cell->anima = true;
                has_move = true;

                assert(cell_en != de_null);
                struct TimerData td = {
                    .mv = mv,
                    .cell = cell_en,
                };

                cell->x += mv->dx;
                cell->y += mv->dy;
                cell->touched = true;
                touched = true;

                timerman_add(mv->timers, (struct TimerDef) {
                    .duration = mv->tmr_block_time,
                    .sz = sizeof(struct TimerData),
                    .on_update = tmr_cell_draw,
                    .on_stop = tmr_cell_draw_stop,
                    .udata = &td,
                });
            }
    } while (touched);

    return has_move;
}

static bool try_sum(
    struct ModelView *mv, struct Cell *cell, de_entity cell_en, int x, int y
) {
    bool has_sum = false;
    assert(mv);
    assert(cell);
    assert(cell_en != de_null);

    if (cell->dropped) 
        return has_sum;

    de_entity neighbour_en = de_null;
    struct Cell *neighbour = get_cell(
        mv, x + mv->dx, y + mv->dy, &neighbour_en
    );
    if (!neighbour) 
        return has_sum;
    if (neighbour->dropped) 
        return has_sum;

    assert(cell_en != de_null);
    assert(neighbour_en != de_null);

    if (cell->value != neighbour->value)
        return has_sum;

    has_sum = true;
    neighbour->value += cell->value;
    mv->scores += cell->value;

    /*koh_screenshot_incremental();*/

    assert(de_valid(mv->r, cell_en));
    if (!de_valid(mv->r, cell_en))
        return has_sum;

    cell->dropped = true;
    cell->anima = true;
    cell->anim_size = true;

    struct TimerData td = {
        .mv = mv,
        .cell = cell_en,
    };
    timerman_add(mv->timers, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw_drop,
        .sz = sizeof(struct TimerData),
        .udata = &td,
    });

    neighbour->anima = true;
    td.cell = neighbour_en;

    timerman_add(mv->timers, (struct TimerDef) {
        .duration = mv->tmr_block_time,
        //.on_stop = tmr_cell_draw_stop_drop,
        .on_stop = tmr_cell_draw_stop,
        .on_update = tmr_cell_draw_neighbour_drop,
        .sz = sizeof(struct TimerData),
        .udata = &td,
    });

    return has_sum;
}

static bool sum(struct ModelView *mv) {
    bool has_sum = false;
    for (int x = 0; x < mv->field_size; ++x)
        for (int y = 0; y < mv->field_size; ++y) {
            de_entity cell_en = de_null;
            struct Cell *cell = get_cell(mv, x, y, &cell_en);
            if (!cell) continue;
            // XXX: применение ||
            has_sum = has_sum || try_sum(mv, cell, cell_en, x, y);
        }
    return has_sum;
}

static void update(struct ModelView *mv, enum Direction dir) {
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
    struct Cell tmp[mv->field_size * mv->field_size];
    memset(tmp, 0, sizeof(tmp));
    int idx = 0;
    for (int y = 0; y < mv->field_size; y++)
        for (int x = 0; x < mv->field_size; x++) {
            struct Cell *cell = get_cell(mv, x, y, NULL);
            if (cell)
                tmp[idx++].value = cell->value;
        }

    //trace("sort_numbers: idx %d\n", idx);
    qsort(tmp, idx, sizeof(tmp[0]), cmp);
    memmove(mv->sorted, tmp, sizeof(tmp[0]) * idx);
}

static void draw_field(struct ModelView *mv) {
    const int field_width = mv->field_size * quad_width;
    Vector2 start = mv->pos;
    const float thick = 8.;

    Vector2 tmp = start;
    for (int u = 0; u <= mv->field_size; u++) {
        Vector2 end = tmp;
        end.y += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.x += quad_width;
    }

    tmp = start;
    for (int u = 0; u <= mv->field_size; u++) {
        Vector2 end = tmp;
        end.x += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.y += quad_width;
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

static void cell_draw(
    struct ModelView *mv, struct Cell *cell, struct DrawOpts opts
) {
    assert(mv);
    assert(cell);

    int fontsize = opts.fontsize;
    float spacing = 2.;

    char msg[64] = {0};
    sprintf(msg, "%d", cell->value);
    int textw = 0;

    assert(opts.amount >= 0 && opts.amount <= 1.);
    Vector2 base_pos;

    if (cell->anima) {
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

    if (opts.anim_size) {
        const float font_scale = 6.;
        if (opts.amount <= 0.5)
            fontsize = Lerp(fontsize, fontsize * font_scale, opts.amount);
        else
            fontsize = Lerp(fontsize * font_scale, fontsize, opts.amount);
    }

    do {
        Font f = GetFontDefault();
        textw = MeasureTextEx(f, msg, fontsize--, spacing).x;
    } while (textw > quad_width);

    Vector2 pos = mv->pos;

    Vector2 offset = {
        opts.offset_coef.x * (quad_width - textw) / 2.,
        opts.offset_coef.x * (quad_width - fontsize) / 2.,
    };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, quad_width), offset);
    pos = Vector2Add(pos, disp);

    Color color = opts.custom_color ? opts.color : get_color(mv, cell->value);
    switch (opts.anim_alpha) {
        case AM_FORWARD:
            color.a = Lerp(10., 255., opts.amount);
            break;
        case AM_BACKWARD:
            color.a = Lerp(255., 10., opts.amount);
            break;
        default:
            break;
    }
    DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);
}

static void draw_numbers(struct ModelView *mv) {
    assert(mv);
    for (int y = 0; y < mv->field_size; y++) {
        for (int x = 0; x < mv->field_size; x++) {
            struct Cell *cell = get_cell(mv, x, y, NULL);
            if (!cell)
                continue;

            if (!cell->anima)
                cell_draw(mv, cell, dflt_draw_opts);
        }
    }
}

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
    if (igBeginTable("movements", 4, table_flags, outer_size, 0.)) {

        igTableSetupColumn("fr", 0, 0, 0);
        igTableSetupColumn("to", 0, 0, 1);
        igTableSetupColumn("val", 0, 0, 2);
        //igTableSetupColumn("act", 0, 0, 3);
        igTableSetupColumn("anima", 0, 0, 3);
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
                igTableSetColumnIndex(3);
                igText("-");
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
                igTableSetColumnIndex(3);
                igText("%s", global_cells[row].anima ? "true" : "false");

            }
        }
        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);
        igEndTable();
    }
    igEnd();
}

static void ecs_window(struct ModelView *mv) {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("ecs", &open, flags);

    if (igButton("|<<", (ImVec2) {0})) {
    }
    igSameLine(0, 10);
    if (igButton("<<", (ImVec2) {0})) {
        mv->r = ecs_circ_buf_prev(&ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton("restore", (ImVec2) {0})) {
        if (ecs_tmp)
            mv->r = ecs_tmp;
    }
    igSameLine(0, 10);
    if (igButton(">>", (ImVec2) {0})) {
        mv->r = ecs_circ_buf_next(&ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton(">>|", (ImVec2) {0})) {
    }
    igText("captured %d systems", ecs_buf.num);
    igText("cur %d system", ecs_buf.cur);

    igEnd();
}

static void removed_entities_window() {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("removed entities", &open, flags);

    for (int i = 0; i < global_cells_num; i++) {
            struct Cell *c = &global_cells[i];
            igSetNextItemOpen(false, ImGuiCond_Once);
            if (igTreeNode_Ptr((void*)(uintptr_t)i, "i %d", i)) {
                //igText("en      %ld", de_view_entity(&v));
                igText("p       %d, %d", c->x, c->y);
                //igText("to      %d, %d", c->to_x, c->to_y);
                //igText("act     %s", action2str(c->action));
                igText("val     %d", c->value);
                igText("anima   %s", c->anima ? "true" : "false");
                igText("anim_sz %s", c->anim_size ? "true" : "false");
                igText("dropped %s", c->dropped ? "true" : "false");
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
            if (c) {
                igSetNextItemOpen(true, ImGuiCond_Once);
                if (igTreeNode_Ptr((void*)(uintptr_t)idx, "en %d", idx)) {
                    igText("en      %ld", de_view_entity(&v));
                    igText("p       %d, %d", c->x, c->y);
                    //igText("to      %d, %d", c->to_x, c->to_y);
                    //igText("act     %s", action2str(c->action));
                    igText("val     %d", c->value);
                    igText("anima   %s", c->anima ? "true" : "false");
                    igText("anim_sz %s", c->anim_size ? "true" : "false");
                    igTreePop();
                }
                idx++;
            } else {
                printf("bad entity: without cmp_cell component\n");
                exit(EXIT_FAILURE);
            }
        }
    }

_exit:
    igEnd();
}

static void options_window(struct ModelView *mv) {
    bool wnd_open = true;
    igBegin("options_window", &wnd_open, 0);
    igEnd();
}

static void gui(struct ModelView *mv) {
    rlImGuiBegin(false, mv->camera);
    movements_window();
    removed_entities_window();
    //paths_window();
    entities_window(mv);
    timerman_window(mv->timers);
    options_window(mv);
    ecs_window(mv);
    bool open = false;
    igShowDemoWindow(&open);
    rlImGuiEnd();
}

/*
static char *state2str(enum ModelViewState state) {
    static char buf[32] = {0};
    switch (state) {
        case MVS_ANIMATION: strcpy(buf, "animation"); break;
        case MVS_READY: strcpy(buf, "ready"); break;
        case MVS_WIN: strcpy(buf, "win"); break;
        case MVS_GAMEOVER: strcpy(buf, "gameover"); break;
    }
    return buf;
}
*/

static void destroy_dropped(struct ModelView *mv) {
    ecs_circ_buf_push(&ecs_buf, mv->r);
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        assert(de_valid(mv->r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c->dropped) {
            check_guards(c);
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

void anima_clear(struct ModelView *mv) {
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        if (de_valid(mv->r, de_view_entity(&v))) {
            struct Cell *c = de_view_get_safe(&v, cmp_cell);
            assert(c);
            if (c) c->anima = false;
        }
    }
}

/*
static void check_model(struct ModelView *mv, struct ModelTest *mb) {
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        if (de_valid(mv->r, de_view_entity(&v))) {
            struct Cell *c = de_view_get_safe(&v, cmp_cell);
            assert(c);
            if (mb->field[c->x][c->y].value != c->value) {
                abort();
            }
        }
    }
}
*/

static void model_draw(struct ModelView *mv) {
    assert(mv);

#if !defined(PLATFORM_WEB)
    gui(mv);
#endif

    if (!mv->r)
        return;

    modeltest_update(&model_checker, mv->dir);
    /*check_model(mv, &model_checker);*/

    draw_field(mv);

//_anim:
    timerman_update(mv->timers);
    int infinite_num = 0;
    int timersnum = timerman_num(mv->timers, &infinite_num);
    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    if (mv->state == MVS_READY) {
        // TODO: удаляет неправильно
        destroy_dropped(mv);
        anima_clear(mv);
        if (mv->dx || mv->dy) {

            mv->has_sum = sum(mv);
            //if (mv->has_sum) goto _anim;
            mv->has_move = move(mv);
            //if (mv->has_move) goto _anim;


            if (!mv->has_move && !mv->has_sum) {
                mv->dx = 0;
                mv->dy = 0;
                mv->dir = DIR_NONE;
                modelview_put(mv);
            }
        }
    }

    sort_numbers(mv);
    draw_numbers(mv);

    if (is_over(mv)) {
        mv->state = MVS_GAMEOVER;
    }

    if (find_max(mv) == WIN_VALUE) {
        mv->state = MVS_WIN;
    }
}

void modelview_init(struct ModelView *mv, const struct Setup setup) {
    assert(mv);
    mv->field_size = setup.field_size;
    const int cells_num = mv->field_size * mv->field_size;
    mv->sorted = calloc(cells_num, sizeof(mv->sorted[0]));
    const int field_width = mv->field_size * quad_width;
    if (!setup.pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *setup.pos;
    mv->timers = timerman_new(mv->field_size * mv->field_size * 2);
    mv->state = MVS_READY;
    mv->draw = model_draw;
    mv->dropped = false;
    mv->update = update;
    mv->r = de_ecs_make();
    mv->camera = setup.cam;
    ecs_circ_buf_init(&ecs_buf, 2048);

    mv->tmr_block_time = 0.3;
    mv->tmr_put_time = 0.3;

    modeltest_init(&model_checker, mv->field_size);

    FILE *file = fopen("state.txt", "w");
    if (file)
        fclose(file);
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);
    memset(mv, 0, sizeof(*mv));
    if (!mv->dropped) {
        if (mv->timers) {
            timerman_free(mv->timers);
            mv->timers = NULL;
        }
        if (mv->r) {
            de_ecs_destroy(mv->r);
            mv->r = NULL;
        }
    }
    if (mv->sorted) {
        free(mv->sorted);
        mv->sorted = NULL;
    }
    ecs_circ_buf_shutdown(&ecs_buf);
    modeltest_shutdown(&model_checker);
    mv->dropped = true;
}

void modelview_put_cell(struct ModelView *mv, struct Cell cell) {
    assert(mv);
    struct Cell *new_cell = get_cell(mv, cell.x, cell.y, NULL);
    if (!new_cell) {
        new_cell = create_cell(mv, cell.x, cell.y, NULL);
    }
    memset(new_cell, 0, sizeof(*new_cell));
    new_cell->x = cell.x;
    new_cell->y = cell.y;
    new_cell->value = cell.value;
}
