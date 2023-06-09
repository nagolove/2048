// vim: fdm=marker

// TODO: Отказаться от блокировки ввода что-бы можно было играть на скорости выше скорости анимации, то есть до 60 герц.

#include "modelbox.h"
#include "timers.h"
#include <stddef.h>

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

#define TMR_BLOCK_TIME  5
#define TMR_PUT_TIME    0.3
//#define TMR_BLOCK_TIME  .1

struct CellArr {
    struct Cell arr[32];
    int num;
};

struct TimerData {
    struct CellArr      slides;
    struct ModelView    *mv;
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

/*
static const struct DrawOpts dflt_draw_opts = {
    .offset_coef = {
        1.,
        1.,
    },
    .fontsize = 90,
    .custom_color = false,
};
// */

static const int quad_width = 128 + 64;

static int global_queue_size = 0;
static int global_queue_maxsize = 0;
static struct Cell *global_queue = 0;

static int global_arr_size = 0;
static int global_arr_maxsize = 0;
static struct CellArr *global_arr = 0;

static Color colors[] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    BLUE, 
    GRAY,
};

static void tmr_update_tile(struct Timer *t);
static void tmr_update_put(struct Timer *t);

static int find_max(struct ModelBox *mb) {
    int max = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (mb->field[i][j].value > max)
                max = mb->field[i][j].value;

    return max;
}

static bool is_over(struct ModelBox *mb) {
    int num = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (mb->field[i][j].value > 0)
                num++;

    return FIELD_SIZE * FIELD_SIZE == num;
}

static void put(struct ModelBox *mb, struct ModelView *mv) {
    assert(mb);
    int x = rand() % FIELD_SIZE;
    int y = rand() % FIELD_SIZE;

    while (mb->field[x][y].value != 0) {
        x = rand() % FIELD_SIZE;
        y = rand() % FIELD_SIZE;
    }

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9)
        mb->field[x][y].value = 2;
    else 
        mb->field[x][y].value = 4;

    struct TimerDataPut timer_data = {
        .x = x,
        .y = y,
        .value = v,
        .mv = mv,
    };
    timerman_add(mv->timers, (struct TimerDef) {
        .duration = TMR_PUT_TIME,
        .sz = sizeof(timer_data),
        .udata = &timer_data,
        .update = tmr_update_put,
    });
}

// Складывает значения плиток в определенном направлении.
// Все действия попадают в очередь действий.
static void sum(
        struct ModelBox *mb,
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int i, int j,
        bool *moved,
        struct ModelView *mv
) {
    // {{{
    if (i < 0 || i >= FIELD_SIZE) {
        return;
    }
    if (j < 0 || j >= FIELD_SIZE) {
        return;
    }

    switch (dir) {
        case DIR_UP: {
            if (i > 0 && field_copy[j][i - 1].value == field_copy[j][i].value) {
                
                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i - 1;
                    cur->to_j = j;
                    cur->action = CA_SUM;
                    cur->value = field_copy[j][i].value * 2;
                }

                field_copy[j][i - 1].value = field_copy[j][i].value * 2;
                field_copy[j][i - 1].action = CA_SUM;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_DOWN: {
            if (i + 1 < FIELD_SIZE && 
                field_copy[j][i + 1].value == field_copy[j][i].value) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i + 1;
                    cur->to_j = j;
                    cur->action = CA_SUM;
                    cur->value = field_copy[j][i].value * 2;
                }

                field_copy[j][i + 1].value = field_copy[j][i].value * 2;
                field_copy[j][i + 1].action = CA_SUM;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_LEFT: {
            if (j > 0 && field_copy[j - 1][i].value == field_copy[j][i].value) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i;
                    cur->to_j = j - 1;
                    cur->action = CA_SUM;
                    cur->value = field_copy[j][i].value * 2;
                }

                field_copy[j - 1][i].value = field_copy[j][i].value * 2;
                field_copy[j - 1][i].action = CA_SUM;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_RIGHT: {
            if (j + 1 < FIELD_SIZE && 
                field_copy[j + 1][i].value == field_copy[j][i].value) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i;
                    cur->to_j = j + 1;
                    cur->action = CA_SUM;
                    cur->value = field_copy[j][i].value * 2;
                }

                field_copy[j + 1][i].value = field_copy[j][i].value * 2;
                field_copy[j + 1][i].action = CA_SUM;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        default:
            break;
        }
    }
    // }}}
}

// Двигает плитки в определенном направлении.
// Все действия попадают в очередь действий.
static void move(
        struct ModelBox *mb,
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int i, int j,
        bool *moved,
        struct ModelView *mv
) {
    // {{{
    //if (i < 0 || i >= FIELD_SIZE || j < 0 || j >= FIELD_SIZE) {
        //return;
    //}

    switch (dir) {
        case DIR_UP: {
            if (i > 0 && field_copy[j][i - 1].value == 0) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->value = field_copy[j][i].value;
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i - 1;
                    cur->to_j = j;
                    cur->action = CA_MOVE;
                }

                field_copy[j][i - 1] = field_copy[j][i];
                field_copy[j][i].action = CA_MOVE;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_DOWN: {
            if (i + 1 < FIELD_SIZE && field_copy[j][i + 1].value == 0) {


                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->value = field_copy[j][i].value;
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i + 1;
                    cur->to_j = j;
                    cur->action = CA_MOVE;
                }

                field_copy[j][i + 1] = field_copy[j][i];
                field_copy[j][i].action = CA_MOVE;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        }
        case DIR_LEFT: {
            if (j > 0 && field_copy[j - 1][i].value == 0) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->value = field_copy[j][i].value;
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i;
                    cur->to_j = j - 1;
                    cur->action = CA_MOVE;
                }

                field_copy[j - 1][i] = field_copy[j][i];
                field_copy[j][i].action = CA_MOVE;
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("moved horizontal left\n");
            }
            break;
        }
        case DIR_RIGHT: {
            if (j + 1 < FIELD_SIZE && field_copy[j + 1][i].value == 0) {

                if (mv) {
                    struct Cell *cur = &mv->queue[mv->queue_size++];
                    cur->value = field_copy[j][i].value;
                    cur->from_i = i;
                    cur->from_j = j;
                    cur->to_i = i;
                    cur->to_j = j + 1;
                    cur->action = CA_MOVE;
                }

                field_copy[j + 1][i] = field_copy[j][i];
                field_copy[j][i].action = CA_MOVE;
                field_copy[j][i].value = 0;
                *moved = true;
            }
            break;
        default:
            break;
        }
    }
    // }}}
}


static void update(
    struct ModelBox *mb, enum Direction dir, struct ModelView *mv
) {
    assert(mb);
    if (mb->state == MBS_GAMEOVER)
        return;

    mb->last_dir = dir;
    mv->queue_size = 0;

    const int field_size_bytes = sizeof(mb->field[0][0]) * 
                                 FIELD_SIZE * FIELD_SIZE;
    struct Cell field_copy[FIELD_SIZE][FIELD_SIZE] = {0};
    memmove(field_copy, mb->field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    for (int i = 0; i < FIELD_SIZE; i++) 
        for (int j = 0; j < FIELD_SIZE; j++) 
            field_copy[j][i].action = CA_NONE;

    do {
        moved = false;

        for (int i = 0; i < FIELD_SIZE; i++) {
            for (int j = 0; j < FIELD_SIZE; j++) {
                if (field_copy[j][i].value == 0) continue;
                move(mb, dir, field_copy, i, j, &moved, mv);
            }
        }

        for (int i = 0; i < FIELD_SIZE; i++) {
            for (int j = 0; j < FIELD_SIZE; j++) {
                if (field_copy[j][i].value == 0) continue;
                sum(mb, dir, field_copy, i, j, &moved, mv);
            }
        }

        iter++;
        int iter_limit = FIELD_SIZE * FIELD_SIZE;
        if (iter > iter_limit) {
            trace("update: iter_limit was reached\n");
            exit(EXIT_FAILURE);
        }
    } while (moved);

    /*
    Как выделить те плитки, что должны рисовать, но не передвигаются?
     */

    for (int i = 0; i < FIELD_SIZE; ++i) {
        for (int j = 0; j < FIELD_SIZE; ++j) {
            if (mb->field[j][i].value == field_copy[j][i].value) {
                struct Cell *cur = &mv->queue[mv->queue_size++];
                /*struct Cell *cur = &mv->fixed[mv->fixed_size++];*/
                cur->action = CA_NONE;
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i;
                cur->to_j = j;
                cur->value = mb->field[j][i].value;
            }
        }
    }
    memmove(mb->field, field_copy, field_size_bytes);

    if (!is_over(mb))
        put(mb, mv);
    else
        mb->state = MBS_GAMEOVER;

    if (find_max(mb) == WIN_VALUE)
        mb->state = MBS_WIN;
}

static void start(struct ModelBox *mb, struct ModelView *mv) {
    put(mb, mv);
}

void modelbox_init(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    mb->last_dir = DIR_NONE;
    mb->update = update;
    mb->start = start;
    mb->state = MBS_PROCESS;
    mb->dropped = false;
}

static int cmp(const void *pa, const void *pb) {
    const int *a = pa, *b = pb;
    return *a < *b;
}

static void sort_numbers(struct ModelView *mv, struct ModelBox *mb) {
    struct Cell tmp[FIELD_SIZE * FIELD_SIZE] = {0};
    int idx = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            tmp[idx++].value = mb->field[j][i].value;

    qsort(tmp, FIELD_SIZE * FIELD_SIZE, sizeof(mb->field[0][0]), cmp);
    memmove(mv->sorted, tmp, sizeof(mv->sorted));
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
    /*printf("colors_num %d\n", colors_num);*/
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
    //const int field_width = FIELD_SIZE * quad_width;

    assert(cell);
    //if (!cell) return;

    // Создать таймер для каждого движения в очереди
    // Обработать события всех таймеров
    // Удалить закончившиеся таймеры.
    //
    // Таймеры для фиксированнного значения fps, длятся duration секунд.
    if (cell->value == 0)
        return;

    char msg[64] = {0};
    //sprintf(msg, "%d [%s]", cell->value, action2str(cell->action));
    sprintf(msg, "%d", cell->value);
    int textw = 0;

    assert(opts.amount >= 0 && opts.amount <= 1.);
    Vector2 base_pos;

    if (cell->action == CA_NONE) {
        base_pos = (Vector2) {
            cell->from_j, cell->from_i,
        };
    } else if (cell->action == CA_MOVE) {
        base_pos = (Vector2) {
            Lerp(cell->from_j, cell->to_j, opts.amount),
            Lerp(cell->from_i, cell->to_i, opts.amount),
        };
    } else {
        base_pos = (Vector2) {
            Lerp(cell->from_j, cell->to_j, opts.amount),
            Lerp(cell->from_i, cell->to_i, opts.amount),
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

static void draw_numbers(struct ModelView *mv, Field field) {
    assert(mv);
    assert(field);
    int fontsize = 90;
    float spacing = 2.;
    //const int field_width = FIELD_SIZE * quad_width;
    Vector2 start = mv->pos;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field[j][i].value == 0)
                continue;

            char msg[64] = {0};
            sprintf(msg, "%d", field[j][i].value);
            int textw = 0;

            do {
                Font f = GetFontDefault();
                textw = MeasureTextEx(f, msg, fontsize--, spacing).x;
                /*printf("fontsize %d\n", fontsize);*/
            } while (textw > quad_width);

            Vector2 pos = start;
            pos.x += j * quad_width + (quad_width - textw) / 2.;
            pos.y += i * quad_width + (quad_width - fontsize) / 2.;
            Color color = get_color(mv, field[j][i].value);
            DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);

            /*
            color = BLACK;
            sprintf(msg, "[%d, %d]", j, i);
            DrawTextEx(GetFontDefault(), msg, pos, fontsize / 3., 0, color);
            */
        }
    }
}

static const struct DrawOpts special_draw_opts = {
    //.color = BLUE,
    .custom_color = false,
    .fontsize = 40,
    //.offset_coef = { 0., 0., },
    .offset_coef = { 1., 1., },
};

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
        timer_data->mv->fixed[timer_data->mv->fixed_size++] = *cell;
    } else {
        opts.fontsize = 90;
        cell_draw(timer_data->mv, cell, opts);
    }
}

static void tmr_update_put(struct Timer *t) {
    struct TimerDataPut *timer_data = t->data;
    struct ModelView *mv = timer_data->mv;
    //int index = floor(t->amount * timer_data->slides.num);
    struct Cell cell = {
        .action = CA_MOVE,
        .from_j = timer_data->x,
        .from_i = timer_data->y,
        .to_j = timer_data->x,
        .to_i = timer_data->y,
        .value = timer_data->value,
    };
    //trace("tmr_update: udata %p\n", udata);
    struct DrawOpts opts = special_draw_opts;
    opts.amount = t->amount;
    opts.fontsize = 90;
    opts.anim_alpha = true;
    cell_draw(mv, &cell, opts);
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

static const char *cell2str(const struct Cell cell) {
    static char buf[128] = {0};
    sprintf(buf, "from (%d, %d) to (%d, %d), value %d, action %s", 
        cell.from_j,
        cell.from_i,
        cell.to_j,
        cell.to_i,
        cell.value,
        action2str(cell.action)
    );
    return buf;
}

// Из массива со слайдами делает массив массивов(цепочек) слайдов
static void divide_slides(
    struct Cell *queue, int queue_size, struct CellArr *out_arr, int *out_num
) {
    assert(queue);
    assert(out_arr);
    assert(out_num);

    *out_num = 0;

    for (int j = 0; j < queue_size; ++j) {
        struct Cell cur = queue[j];

        int found = -1;
        struct CellArr *ca = NULL;
        for (int k = 0; k < *out_num; ++k) {
            ca = &out_arr[k];
            struct Cell *top = NULL;
            if (ca->num > 0) {
                top = &ca->arr[ca->num - 1];
                if (cur.from_j == top->to_j && cur.from_i == top->to_i) {
                    trace("divide_slides: found\n");
                    found = k;
                    break;
                }
            }
        }

        if (found != -1) {
            ca->arr[ca->num++] = cur;
        } else {
            struct CellArr *last = &out_arr[(*out_num)++];
            last->arr[last->num++] = cur;
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
    rlImGuiBegin(false);
    movements_window();
    paths_window();
    timerman_window(mv->timers);
    bool open = false;
    igShowDemoWindow(&open);
    rlImGuiEnd();
}
// */

static void fill_global_queue(struct ModelView *mv) {
    if (global_queue_size + 1 == global_queue_maxsize) {
        global_queue_size = 0;
    }
    for (int row = 0; row < mv->queue_size; ++row) {
        global_queue[global_queue_size++] = mv->queue[row];
    }
    if (mv->queue_size > 0)
        global_queue[global_queue_size++].action = CA_NONE;
}

static void fill_global_arr(struct CellArr *arr, int arr_num) {
    if (global_arr_size + 1 == global_arr_maxsize) {
        global_arr_size = 0;
    }
    for (int i = 0; i < arr_num; ++i) {
        global_arr[global_arr_size++] = arr[i];
    }
    if (arr_num > 0)
        global_arr[global_arr_size++].num = -1;
}


static void model_draw(struct ModelView *mv, struct ModelBox *mb) {
    assert(mv);
    assert(mb);

    draw_field(mv);

    struct CellArr arr[32] = {0}; 
    int arr_num = 0;
    
    if (mv->queue_size && mv->state == MVS_READY) {
        divide_slides(mv->queue, mv->queue_size, arr, &arr_num);

        for (int i = 0; i < arr_num; i++) {
            trace("draw: mb->queue[%d] = %s\n", i, cell2str(mv->queue[i]));
            struct TimerData timer_data = {
                .slides = arr[i],
                .mv = mv,
            };
            /*
    Когда один таймер закончился, а другой еще нет - перестает рисоваться
    картинка того таймера, который закончился.
    Если таймер закончил выполнение, то его картинка должна продолжать
    рисоваться.
             */
            timerman_add(mv->timers, (struct TimerDef) {
                //.duration = TMR_BLOCK_TIME,
                .duration = 2. * arr[i].num,
                .udata = &timer_data, 
                .sz = sizeof(timer_data),
                .update = tmr_update_tile,
            });
        }
    }

    /*
    for (int i = 0; i < mv->fixed_size; ++i) {
        struct TimerData timer_data = {
            .slides = {
                .num = 1.,
                .arr[0] = mv->fixed[i],
            },
            .mv = mv,
        };
        timer_add((struct TimerDef) {
            .duration = TMR_BLOCK_TIME,
            .mv = mv, 
            .udata = &timer_data, 
            .sz = sizeof(timer_data),
            .update = tmr_update_tile,
        });
    }
    */


    int timersnum = timerman_update(mv->timers);

    mv->state = timersnum ? MVS_ANIMATION : MVS_READY;

    for (int j = 0; j < mv->fixed_size; ++j) {
        //cell_draw(mv, &mv->fixed[j], dflt_draw_opts);
    }
    // */

    if (mv->state == MVS_READY) {
        sort_numbers(mv, mb);
        draw_numbers(mv, mb->field);
    }


    fill_global_queue(mv);
    fill_global_arr(arr, arr_num);

    gui(mv);
}

void modelview_init(
    struct ModelView *mv, const Vector2 *pos, struct ModelBox *mb
) {
    assert(mv);
    assert(mb);
    //memset(mv, 0, sizeof(*mv));
    /*copy_field(mv->field_prev, mb->field);*/
    const int field_width = FIELD_SIZE * quad_width;
    if (!pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *pos;
    mv->timers = timerman_new();
    mv->state = MVS_READY;
    mv->draw = model_draw;
    mv->dropped = false;
    mv->queue_cap = FIELD_SIZE * FIELD_SIZE;
    mv->queue_size = 0;
}

void modelbox_shutdown(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    mb->dropped = true;
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);
    memset(mv, 0, sizeof(*mv));
    if (!mv->dropped)
        timerman_free(mv->timers);
    mv->dropped = true;
}

void model_global_init() {
    global_queue_maxsize = 100000;
    global_queue = malloc(sizeof(global_queue[0]) * global_queue_maxsize);
    assert(global_queue);

    global_arr_maxsize = 100000;
    global_arr_size = 0;
    global_arr = malloc(sizeof(global_arr[0]) * global_arr_maxsize);
    assert(global_arr);
}

void model_global_shutdown() {
    free(global_queue);
    free(global_arr);
}
