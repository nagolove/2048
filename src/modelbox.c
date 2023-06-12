// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "modelbox.h"
#include "koh_destral_ecs.h"
#include "timers.h"
#include <stddef.h>
#include <stdint.h>

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

struct CellArr {
    struct Cell arr[32];
    int num;
};

struct TimerData {
    struct ModelView    *mv;
    struct Cell         *cell;
};

struct TimerDataPut {
    int                 x, y, value;
    struct ModelView    *mv;
};

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
    .fontsize = 90,
    .custom_color = false,
};
// */

static const de_cp_type cmp_cell = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
};

static const int quad_width = 128 + 64;

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
        if (c) num += c->value > 0;
    }

    return FIELD_SIZE * FIELD_SIZE == num;
}

static struct Cell *get_cell_to(struct ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        if (c && c->to_x == x && c->to_y == y) {
            return c;
        }
    }
    return NULL;
}

static struct Cell *get_cell_from(struct ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        if (c && c->from_x == x && c->from_y == y) {
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
        num += c ? 1 : 0;
    }
    return num;
}


static struct Cell *create_cell(struct ModelView *mv, int x, int y) {
    assert(mv);
    assert(mv->r);

    if (get_cell_count(mv) >= FIELD_SIZE * FIELD_SIZE)
        return NULL;

    de_entity en = de_create(mv->r);
    struct Cell *cell = de_emplace(mv->r, en, cmp_cell);
    assert(cell);
    cell->from_x = x;
    cell->from_y = y;
    cell->action = CA_NONE;
    cell->to_x = x;
    cell->to_y = y;
    cell->value = -1;
    cell->moving = false;
    trace("create_cell: (%d, %d)\n", cell->from_x, cell->from_y);
    return cell;
}

static void put(struct ModelView *mv) {
    assert(mv);
    int x = rand() % FIELD_SIZE;
    int y = rand() % FIELD_SIZE;

    struct Cell *cell = get_cell_from(mv, x, y);
    while (cell && cell->value != 0) {
        x = rand() % FIELD_SIZE;
        y = rand() % FIELD_SIZE;
        cell = get_cell_from(mv, x, y);
    }

    if (!cell)
        cell = create_cell(mv, x, y);

    assert(cell);

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9) {
        cell->value = 2;
    } else {
        cell->value = 4;
    }

    cell->action = CA_NONE;
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

static void tmr_cell_draw_stop(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    struct Cell *cell = timer_data->cell;
    cell->moving = false;
    cell->action = CA_NONE;
    cell->from_x = cell->to_x;
    cell->from_y = cell->to_y;
    //cell_draw(timer_data->mv, timer_data->cell, opts);
}


static void tmr_cell_draw(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    //struct DrawOpts opts = special_draw_opts;
    struct DrawOpts opts = dflt_draw_opts;
    opts.amount = t->amount;
    opts.fontsize = 90;
    opts.color = MAGENTA;
    cell_draw(timer_data->mv, timer_data->cell, opts);
}

static void update(struct ModelView *mv, enum Direction dir) {
    assert(mv);
    if (mv->state == MVS_GAMEOVER)
        return;

    if (mv->state == MVS_ANIMATION)
        return;

    if (!is_over(mv)) {
        if (IsKeyDown(KEY_P))
            put(mv);
    } else
        mv->state = MVS_GAMEOVER;

    if (find_max(mv) == WIN_VALUE)
        mv->state = MVS_WIN;

    int dx = 0, dy = 0;
    switch (dir) {
        case DIR_UP: dy = -1; break;
        case DIR_DOWN: dy = 1; break;
        case DIR_LEFT: dx = -1; break;
        case DIR_RIGHT: dx = 1; break;
        default: break;
    }

    for (int i = 0; i <= de_typeof_num(mv->r, cmp_cell); ++i) {
        for (int x = 0; x < FIELD_SIZE; ++x)
            for (int y = 0; y < FIELD_SIZE; ++y) {
                struct Cell *cell = get_cell_from(mv, x, y);
                if (!cell) continue;

                struct Cell *neighbour = get_cell_to(mv, x + dx, y + dy);
                if (neighbour) continue;

                if (cell->moving)
                    continue;

                cell->to_x += dx;

                if (cell->to_x >= FIELD_SIZE)
                    cell->to_x = FIELD_SIZE - 1;
                if (cell->to_x < 0)
                    cell->to_x = 0;

                cell->to_y += dy;

                if (cell->to_y >= FIELD_SIZE)
                    cell->to_y = FIELD_SIZE - 1;
                if (cell->to_y < 0)
                    cell->to_y = 0;


                cell->action = CA_MOVE;
                cell->moving = true;
                timerman_add(mv->timers, (struct TimerDef) {
                    .duration = TMR_BLOCK_TIME,
                    .sz = sizeof(struct TimerData),
                    .on_update = tmr_cell_draw,
                    .on_stop = tmr_cell_draw_stop,
                    .udata = &(struct TimerData) {
                        .mv = mv,
                        .cell = cell,
                }, });

            }
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
            struct Cell *cell = get_cell_from(mv, x, y);
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
    const float thick = 7.;

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

static const char *action2str(enum CellAction action) {
    static char str[64] = {0};
    memset(str, 0, sizeof(str));
    switch (action) {
        case CA_SUM: {
            strcpy(str, "sum");
        break;
        }
        case CA_MOVE: {
            strcpy(str, "move");
        break;
        }
        case CA_NONE: {
            strcpy(str, "none");
        break;
        }
        default:
            printf("bad value in action2str()\n");
            exit(EXIT_FAILURE);
    }
    return str;
}

static void cell_draw(
    struct ModelView *mv, struct Cell *cell, struct DrawOpts opts
) {
    assert(mv);
    int fontsize = opts.fontsize;
    float spacing = 2.;

    assert(cell);
    //if (!cell) return;

    // Создать таймер для каждого движения в очереди
    // Обработать события всех таймеров
    // Удалить закончившиеся таймеры.
 
    // Таймеры для фиксированнного значения fps, длятся duration секунд.
    //if (cell->value == 0)
        //return;

    char msg[64] = {0};
    sprintf(msg, "%d", cell->value);
    int textw = 0;

    assert(opts.amount >= 0 && opts.amount <= 1.);
    Vector2 base_pos;

    if (cell->action == CA_NONE) {
        base_pos = (Vector2) {
            cell->from_x, cell->from_y,
        };
    } else if (cell->action == CA_MOVE) {
        base_pos = (Vector2) {
            Lerp(cell->from_x, cell->to_x, opts.amount),
            Lerp(cell->from_y, cell->to_y, opts.amount),
        };
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

    //trace("cell_draw: opts.amount %f\n", opts.amount);
    //trace("cell_draw: cell %p, base_pos %s\n", cell, Vector2_tostr(base_pos));

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
            struct Cell *cell = get_cell_from(mv, x, y);
            if (!cell)
                continue;

            if (cell->value == 0)
                continue;

            if (!cell->moving)
                cell_draw(mv, cell, dflt_draw_opts);
        }
    }
}

/*
static void tmr_update_tile(struct Timer *t) {
    struct TimerData *timer_data = t->data;
    // slides: [ ], [ ], [ ], [ ]
    int index = floor(t->amount * timer_data->slides.num);
    struct Cell *cell = &timer_data->slides.arr[index];
    //trace("tmr_update: udata %p\n", udata);
    struct DrawOpts opts = special_draw_opts;
    opts.amount = t->amount;

    if (t->amount > 0.998) {
        trace("tmr_update_tile: amount was reached 0.998\n");
        // приехали, рисовать плитку статически
        //timer_data->mv->fixed[timer_data->mv->fixed_size++] = *cell;
        //timerman_add(
    } else {
        opts.fontsize = 90;
        cell_draw(timer_data->mv, cell, opts);
    }
}
*/

/*
static void tmr_update_put(struct Timer *t) {
    struct TimerDataPut *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    //int index = floor(t->amount * timer_data->slides.num);
    struct Cell cell = {
        .action = CA_MOVE,
        .from_x = timer_data->x,
        .from_y = timer_data->y,
        .to_x = timer_data->x,
        .to_y = timer_data->y,
        .value = timer_data->value,
    };
    //trace("tmr_update: udata %p\n", udata);
    struct DrawOpts opts = special_draw_opts;
    opts.amount = t->amount;
    opts.fontsize = 90;
    opts.anim_alpha = true;
    cell_draw(mv, &cell, opts);
}
*/

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
static const char *cell2str(const struct Cell cell) {
    static char buf[128] = {0};
    sprintf(buf, "from (%d, %d) to (%d, %d), value %d, action %s", 
        cell.from_x,
        cell.from_y,
        cell.to_x,
        cell.to_y,
        cell.value,
        action2str(cell.action)
    );
    return buf;
}
*/

/*
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
    if (igBeginTable("movements", 1, table_flags, outer_size, 0.)) {
        for (int row = 0; row < global_queue_size; ++row) {
            igTableNextRow(0, 0);
            igTableSetColumnIndex(0);
            char *action = NULL;
            if (global_queue[row].action == CA_SUM)
                action = "SUM";
            else if (global_queue[row].action == CA_MOVE)
                action = "MOVE";
            else if (global_queue[row].action == CA_NONE) {
                igText("---------------------------");
                continue;
            }
            igText(
                    "[%d, %d] -> [%d, %d] %s", 
                    global_queue[row].from_j,
                    global_queue[row].from_i,
                    global_queue[row].to_j,
                    global_queue[row].to_i,
                    action
                  );
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
        struct Cell *c = de_view_get_safe(&v, cmp_cell);
        if (c) {
            igSetNextItemOpen(false, ImGuiCond_Once);
            if (igTreeNode_Ptr((void*)(intptr_t)idx, "en %d", idx)) {
                igText("fr %d, %d", c->from_x, c->from_y);
                igText("to %d, %d", c->to_x, c->to_y);
                igText("act %s", action2str(c->action));
                igText("val %d", c->value);
                igTreePop();
            }
            idx++;
        } else {
            printf("bad entity: without cmp_cell component\n");
            exit(EXIT_FAILURE);
        }
    }

    igEnd();
}

/*
static void paths_window() {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("paths", &open, flags);

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_BordersV | 
        ImGuiTableFlags_Resizable | 
        ImGuiTableFlags_Reorderable | 
        ImGuiTableFlags_Hideable;

    ImVec2 outer_size = {0., 0.}; // Размер окошка таблицы
    if (igBeginTable("paths", 1, table_flags, outer_size, 0.)) {
        for (int row = 0; row < global_arr_size; ++row) {
            igTableNextRow(0, 0);
            igTableSetColumnIndex(0);

            struct Cell *cells = global_arr[row].arr;
            char line[512] = {0};
            if (global_arr[row].num == -1)
                igText(">>>>>>>>>");
            else {
                for (int j = 0; j < global_arr[row].num; j++) {
                    char desc[64] = {0};
                    sprintf(
                        desc, "[%d, %d] -> [%d, %d];",
                        cells[j].from_j,
                        cells[j].from_i,
                        cells[j].to_j,
                        cells[j].to_i
                    );
                    strcat(line, desc);
                }
                igText(line);
            }
        }
        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);
        igEndTable();
    }
    igEnd();
}
// */

static void gui(struct ModelView *mv) {
    rlImGuiBegin(false, mv->camera);
    //movements_window();
    //paths_window();
    entities_window(mv);
    timerman_window(mv->timers);
    bool open = false;
    igShowDemoWindow(&open);
    rlImGuiEnd();
}

static void model_draw(struct ModelView *mv) {
    assert(mv);

    draw_field(mv);

    /*
    for (de_view v = de_create_view(mv->r, 1, (de_cp_type[1]) { 
        cmp_cell }); de_view_valid(&v); de_view_next(&v)) {
        struct Cell *cell = de_view_get_safe(&v, cmp_cell);
        if (cell) {
            trace("model_draw: cell_draw\n");
            cell_draw(mv, cell, dflt_draw_opts);
        }
    }
    */

    timerman_update(mv->timers);
    int infinite_num = 0;
    timerman_remove_expired(mv->timers);
    int timersnum = timerman_num(mv->timers, &infinite_num);

    if (timersnum - infinite_num == 0 && infinite_num != 0)
        timerman_clear_infinite(mv->timers);

    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    if (timersnum - infinite_num == 0) {
        // animate
        for (int x = 0; x < FIELD_SIZE; ++x)
            for (int y = 0; y < FIELD_SIZE; ++y) {
                struct Cell *cell = get_cell_from(mv, x, y);
                if (!cell)
                    continue;

                if (cell->from_x != cell->to_x || 
                        cell->from_y != cell->to_y) {

                }
            }
    }

    if (mv->state == MVS_READY) {
        sort_numbers(mv);
    }

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
