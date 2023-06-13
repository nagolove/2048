// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "modelbox.h"
#include "koh_destral_ecs.h"
#include "timers.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "koh_logger.h"
#include "koh_console.h"
#include "koh_common.h"
#include "koh_routine.h"
#include "raymath.h"
#include "cimgui.h"
#include "cimgui_impl.h"

#define TMR_BLOCK_TIME  0.5
#define TMR_PUT_TIME    0.3
//#define TMR_BLOCK_TIME  .1


#define GLOBAL_CELLS_CAP    100000

static struct Cell global_cells[GLOBAL_CELLS_CAP] = {0};
static int global_cells_num = 0;
static struct Cell cell_zero = {0};

struct TimerData {
    struct ModelView    *mv;
    de_entity           cell;
};

/*
struct TimerDataPut {
    int                 x, y, value;
    struct ModelView    *mv;
};
*/

struct DrawOpts {
    int     fontsize;
    Vector2 offset_coef; // 1. is default value
    bool    custom_color;
    Color   color;
    double  amount;
    bool    anim_alpha;
};

static const struct DrawOpts dflt_draw_opts = {
    .offset_coef = {
        1.,
        1.,
    },
    .fontsize = 140,
    .custom_color = false,
};
// */

static const de_cp_type cmp_cell = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 1000,
};

static const int quad_width = 128 + 64 + 32;

static Color colors[] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    BLUE, 
    GRAY,
};

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

    return FIELD_SIZE * FIELD_SIZE == num;
}

static struct Cell *get_cell_to(
    struct ModelView *mv, int x, int y, de_entity *en
) {
    assert(mv);
    assert(mv->r);


    if (en) *en = de_null;
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c && c->to_x == x && c->to_y == y) {
            if (en) {
                assert(de_valid(mv->r, de_view_entity(&v)));
                *en = de_view_entity(&v);
            }
            return c;
        }
    }
    return NULL;
}

static struct Cell *get_cell_from(
    struct ModelView *mv, int x, int y, de_entity *en
) {
    assert(mv);
    assert(mv->r);

    if (en) *en = de_null;

    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        assert(c);
        if (c && c->from_x == x && c->from_y == y) {
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


static struct Cell *create_cell(struct ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);

    if (get_cell_count(mv) >= FIELD_SIZE * FIELD_SIZE) {
        trace("create_cell: cells were reached limit of %d\n",
              FIELD_SIZE * FIELD_SIZE);
        abort();
        return NULL;
    }

    de_entity en = de_create(mv->r);
    struct Cell *cell = de_emplace(mv->r, en, cmp_cell);
    assert(cell);
    cell->from_x = x;
    cell->from_y = y;
    cell->to_x = x;
    cell->to_y = y;
    cell->value = -1;
    trace("create_cell: en %ld at (%d, %d)\n", en, cell->from_x, cell->from_y);
    return cell;
}

static void put(struct ModelView *mv) {
    assert(mv);
    int x = rand() % FIELD_SIZE;
    int y = rand() % FIELD_SIZE;

    /*
    struct Cell *cell = get_cell_from(mv, x, y, NULL);
    while (cell && cell->value != 0) {
        x = rand() % FIELD_SIZE;
        y = rand() % FIELD_SIZE;
        cell = get_cell_from(mv, x, y, NULL);
    }
    */

    struct Cell *cell = get_cell_from(mv, x, y, NULL);
    while (cell) {
        x = rand() % FIELD_SIZE;
        y = rand() % FIELD_SIZE;
        cell = get_cell_from(mv, x, y, NULL);
    }

    assert(!cell);
    cell = create_cell(mv, x, y);

    assert(cell);

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9) {
        cell->value = 2;
    } else {
        cell->value = 4;
    }

    //cell->action = CA_NONE;
    cell->to_x = cell->from_x;
    cell->to_y = cell->from_y;
    trace("put: val %d\n", cell->value);
}

/*
static const struct DrawOpts special_draw_opts = {
    //.color = BLUE,
    .custom_color = false,
    .fontsize = 40,
    //.offset_coef = { 0., 0., },
    .offset_coef = { 1., 1., },
};
*/

//static inline struct Cell *cell_from_timer(struct Timer *t) {
//}

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;

    assert(de_valid(mv->r, timer_data->cell));

    /*
    if (!de_valid(mv->r, timer_data->cell)) {
        trace("tmr_cell_draw_stop: invalid cell entity\n");
        return;
    }
    */

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);
    //struct Cell *cell = de_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    /*
    if (!cell) {
        trace("tmr_cell_draw_stop: invalid cell component\n");
        return;
    }
    */

    cell->moving = false;
    //cell->action = CA_NONE;
    cell->from_x = cell->to_x;
    cell->from_y = cell->to_y;
}


static bool tmr_cell_draw(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    //struct DrawOpts opts = special_draw_opts;
    struct DrawOpts opts = dflt_draw_opts;

    assert(de_valid(mv->r, timer_data->cell));
    /*
    if (!de_valid(mv->r, timer_data->cell)) {
        trace("tmr_cell_draw: invalid cell entity\n");
        return true;
    }
    */

    /*
    trace(
        "tmr_cell_draw: has cmp_cell %s\n",
        de_has(mv->r, timer_data->cell, cmp_cell) ? "true" : "false"
    );
    */

    printf(
        "tmr_cell_draw: en %u has cmp_cell %s\n",
        timer_data->cell,
        de_has(mv->r, timer_data->cell, cmp_cell) ? "true" : "false"
    );

    struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);

    assert(cell);

    if (!cell) return false;

    /*
    if (!cell) {
        trace("tmr_cell_draw: invalid cell component\n");
        de_destroy(mv->r, timer_data->cell);
        return true;
    }
    */

    opts.amount = t->amount;
    //opts.fontsize = 140;
    opts.color = MAGENTA;
    cell_draw(timer_data->mv, cell, opts);
    return false;
}

static enum TimerManAction iter_timers(struct Timer *tmr, void* udata) {
    trace("iter_timers:\n");
    struct TimerData *timer_data = tmr->data;
    //de_entity en = (de_entity)(uintptr_t)udata;
    de_entity en = *(de_entity*)udata;
    struct ModelView *mv = timer_data->mv;
    //struct DrawOpts opts = special_draw_opts;

    assert(de_valid(mv->r, en));
    assert(de_valid(mv->r, timer_data->cell));

    /*
    if (!de_valid(mv->r, timer_data->cell)) {
        trace("iter_timers: remove timer with invalid cell entity\n");
        return TMA_REMOVE_NEXT;
    }
    */

    //struct Cell *cell = de_try_get(mv->r, timer_data->cell, cmp_cell);
    //if (timer_data->cell == cell) {
    if (timer_data->cell == en) {
        // XXX: Какое лучше условие использовать?
        //return TMA_REMOVE_BREAK;
        trace("iter_timers: remove timer with id %d\n", tmr->id);
        return TMA_REMOVE_NEXT;
    }

    return TMA_NEXT;
}

static void global_cell_push(struct Cell *cell) {
    assert(cell);
    global_cells[global_cells_num++] = *cell;
}

static bool move(struct ModelView *mv) {
    int cells_num = de_typeof_num(mv->r, cmp_cell);
    bool has_move = false;
    for (int i = 0; i <= cells_num; ++i) {
        for (int x = 0; x < FIELD_SIZE; ++x)
            for (int y = 0; y < FIELD_SIZE; ++y) {
                de_entity cell_en = de_null;
                struct Cell *cell = get_cell_from(mv, x, y, &cell_en);
                if (!cell) continue;

                if (cell->moving)
                    continue;

                struct Cell *neighbour = get_cell_to(
                    mv, x + mv->dx, y + mv->dy, NULL
                );
                if (neighbour) continue;


                if (cell->to_x + mv->dx == FIELD_SIZE)
                    continue;
                if (cell->to_x + mv->dx < 0)
                    continue;

                if (cell->to_y + mv->dy == FIELD_SIZE)
                    continue;
                if (cell->to_y + mv->dy < 0)
                    continue;

                cell->to_x += mv->dx;
                cell->to_y += mv->dy;


                //cell->action = CA_MOVE;
                cell->moving = true;
                has_move = true;

                global_cell_push(cell);

                assert(cell_en != de_null);
                struct TimerData td = {
                    .mv = mv,
                    .cell = cell_en,
                };
                if (!timerman_add(mv->timers, (struct TimerDef) {
                    .duration = TMR_BLOCK_TIME,
                    .sz = sizeof(struct TimerData),
                    .on_update = tmr_cell_draw,
                    .on_stop = tmr_cell_draw_stop,
                    .udata = &td,
                })) {
                    trace("move: all timers are used\n");
                    mv->state = MVS_GAMEOVER;
                }

            }
    }

    global_cell_push(&cell_zero);
    return has_move;
}

static bool sum(struct ModelView *mv) {
    int cells_num = de_typeof_num(mv->r, cmp_cell);
    bool has_sum = false;
    for (int i = 0; i <= cells_num; ++i) {
        for (int x = 0; x < FIELD_SIZE; ++x)
            for (int y = 0; y < FIELD_SIZE; ++y) {
                de_entity cell_en = de_null;
                struct Cell *cell = get_cell_from(mv, x, y, &cell_en);
                if (!cell) continue;

                de_entity neighbour_en = de_null;
                struct Cell *neighbour = get_cell_to(
                    mv, x + mv->dx, y + mv->dy, &neighbour_en
                );
                if (!neighbour) continue;

                //if (cell->value == 0 || neighbour->value == 0) continue;

                assert(cell_en != de_null);
                assert(neighbour_en != de_null);
                if (cell->value == neighbour->value) {
                    //if (!cell->moving) {
                    if (!cell->moving) {
                        has_sum = true;
                        //cell->value = 111;
                        neighbour->value += cell->value;
                        trace(
                                "model_draw: timerman_each for (%d, %d)\n",
                                cell->from_x, cell->from_y
                             );
                        timerman_each(
                            mv->timers,
                            iter_timers,
                            &cell_en
                        );
                        memset(cell, 0, sizeof(*cell));
                        trace("model_draw: timerman_each end\n");
                        //assert(de_valid(mv->r, cell_en));
                        assert(de_valid(mv->r, cell_en));
                        if (de_valid(mv->r, cell_en)) {
                            trace("sum: de_destroy\n");
                            de_destroy(mv->r, cell_en);
                            // TODO: возможность бесконечного цикла из-за i--?
                            i--;
                        }
                    }
                }
            }
    }
    return has_sum;
}

static void update(struct ModelView *mv, enum Direction dir) {
    assert(mv);
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
    struct Cell tmp[FIELD_SIZE * FIELD_SIZE] = {0};
    int idx = 0;
    for (int y = 0; y < FIELD_SIZE; y++)
        for (int x = 0; x < FIELD_SIZE; x++) {
            struct Cell *cell = get_cell_from(mv, x, y, NULL);
            if (cell)
                tmp[idx++].value = cell->value;
        }

    //trace("sort_numbers: idx %d\n", idx);
    qsort(tmp, idx, sizeof(tmp[0]), cmp);
    memmove(mv->sorted, tmp, sizeof(tmp[0]) * idx);
}

static void draw_field(struct ModelView *mv) {
    const int field_width = FIELD_SIZE * quad_width;
    Vector2 start = mv->pos;
    const float thick = 8.;

    Vector2 tmp = start;
    for (int u = 0; u <= FIELD_SIZE; u++) {
        Vector2 end = tmp;
        end.y += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.x += quad_width;
    }

    tmp = start;
    for (int u = 0; u <= FIELD_SIZE; u++) {
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

    if (cell->moving) {
        base_pos = (Vector2) {
            Lerp(cell->from_x, cell->to_x, opts.amount),
            Lerp(cell->from_y, cell->to_y, opts.amount),
        };
    } else {
        base_pos = (Vector2) {
            cell->from_x, cell->from_y,
        };
    }
    /* sum
    } else {
        base_pos = (Vector2) {
            Lerp(cell->from_x, cell->to_x, opts.amount),
            Lerp(cell->from_y, cell->to_y, opts.amount),
        };
        const float font_scale = 4.;
        if (opts.amount <= 0.5)
            fontsize = Lerp(fontsize, fontsize * font_scale, opts.amount);
        else
            fontsize = Lerp(fontsize * 2., fontsize, opts.amount);
    }
    */


    do {
        Font f = GetFontDefault();
        textw = MeasureTextEx(f, msg, fontsize--, spacing).x;
        //printf("fontsize %d\n", fontsize);
    } while (textw > quad_width);

    Vector2 pos = mv->pos;
    // TODO: Как сделать offset_coef рабочим в диапазоне -1..1
    // Значение по умолчанию - 0, -1 - смещение влево

    Vector2 offset = {
        opts.offset_coef.x * (quad_width - textw) / 2.,
        opts.offset_coef.x * (quad_width - fontsize) / 2.,
    };

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, quad_width), offset);
    pos = Vector2Add(pos, disp);

    Color color = opts.custom_color ? opts.color : get_color(mv, cell->value);
    if (opts.anim_alpha)
        color.a = Lerp(10, 255, opts.amount);
    //Color color = DARKBLUE;
    DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);

    // text_draw_in_rect((Rectangle), ALIGN_LEFT, pos, font, fontsize);
}

static void draw_numbers(struct ModelView *mv) {
    assert(mv);
    for (int y = 0; y < FIELD_SIZE; y++) {
        for (int x = 0; x < FIELD_SIZE; x++) {
            struct Cell *cell = get_cell_from(mv, x, y, NULL);
            if (!cell)
                continue;

            if (!cell->moving)
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
        igTableSetupColumn("moving", 0, 0, 3);
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
                        global_cells[row].from_x, global_cells[row].from_y);
                igTableSetColumnIndex(1);
                igText("%d, %d", 
                        global_cells[row].to_x, global_cells[row].to_y);
                igTableSetColumnIndex(2);
                igText("%d", global_cells[row].value);
                //igTableSetColumnIndex(3);
                //igText("%s", action2str(global_cells[row].action));
                igTableSetColumnIndex(3);
                igText("%s", global_cells[row].moving ? "true" : "false");

            }
        }
        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);
        igEndTable();
    }
    igEnd();
}
// */

static void entities_window(struct ModelView *mv) {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("entities", &open, flags);

    int idx = 0;
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        if (de_valid(mv->r, de_view_entity(&v))) {
            struct Cell *c = de_view_get_safe(&v, cmp_cell);
            if (c) {
                igSetNextItemOpen(true, ImGuiCond_Once);
                if (igTreeNode_Ptr((void*)(uintptr_t)idx, "en %d", idx)) {
                    igText("en      %ld", de_view_entity(&v));
                    igText("fr      %d, %d", c->from_x, c->from_y);
                    igText("to      %d, %d", c->to_x, c->to_y);
                    //igText("act     %s", action2str(c->action));
                    igText("val     %d", c->value);
                    igText("moving  %s", c->moving ? "true" : "false");
                    igTreePop();
                }
                idx++;
            } else {
                printf("bad entity: without cmp_cell component\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    igEnd();
}

static void gui(struct ModelView *mv) {
    rlImGuiBegin(false, mv->camera);
    movements_window();
    //paths_window();
    entities_window(mv);
    timerman_window(mv->timers);
    bool open = false;
    igShowDemoWindow(&open);
    rlImGuiEnd();
}

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

static void model_draw(struct ModelView *mv) {
    assert(mv);

    if (mv->state == MVS_GAMEOVER)
        return;

    if (is_over(mv)) {
        mv->state = MVS_GAMEOVER;
        return;
    }

    if (find_max(mv) == WIN_VALUE) {
        mv->state = MVS_WIN;
        return;
    }

    if (IsKeyDown(KEY_P))
        put(mv);

    draw_field(mv);

    timerman_update(mv->timers);
    int infinite_num = 0;
    int timersnum = timerman_num(mv->timers, &infinite_num);
    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    //trace("model_draw: state %s\n", state2str(mv->state));

    if (mv->state == MVS_ANIMATION) {
    } else if (mv->state == MVS_READY) {
        // MVS_READY

        for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
                    cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
            if (de_valid(mv->r, de_view_entity(&v))) {
                struct Cell *c = de_view_get_safe(&v, cmp_cell);
                assert(c);
                if (c)
                    c->moving = false;
            }
        }

        mv->has_move = move(mv);
        mv->has_sum = sum(mv);
        //if (!mv->has_move) {
            mv->dx = 0;
            mv->dy = 0;
        //}

    }

    sort_numbers(mv);
    draw_numbers(mv);

    gui(mv);
}

void modelview_init(struct ModelView *mv, const Vector2 *pos, Camera2D *cam) {
    assert(mv);
    const int field_width = FIELD_SIZE * quad_width;
    if (!pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *pos;
    mv->timers = timerman_new(FIELD_SIZE * FIELD_SIZE * 2);
    mv->state = MVS_READY;
    mv->draw = model_draw;
    mv->dropped = false;
    mv->update = update;
    mv->r = de_ecs_make();
    mv->camera = cam;
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
    mv->dropped = true;
}
