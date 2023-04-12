#include "modelbox.h"

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

static const int quad_width = 128 + 64;

static Color colors[] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    BLUE, 
    GRAY,
};

static void copy_field(
    struct Cell dest[FIELD_SIZE][FIELD_SIZE], 
    struct Cell source[FIELD_SIZE][FIELD_SIZE]
) {
    memmove(dest, source, sizeof(dest[0][0]) * FIELD_SIZE * FIELD_SIZE);
}

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

    //printf("is_over: %d\n", FIELD_SIZE * FIELD_SIZE == num);
    return FIELD_SIZE * FIELD_SIZE == num;
}

static void put(struct ModelBox *mb) {
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
}

/*
static void sum_sub(
    enum Direction dir,
    struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
    int i, int j,
    int newi, int newj,
    bool *moved,
    struct ModelBox *mb
) {
    if (i > 0 && field_copy[newj][newi].value == field_copy[j][i].value) {
        field_copy[newj][newi].value = field_copy[j][i].value * 2;
        mb->scores += field_copy[j][i].value;
        field_copy[j][i].value = 0;
        *moved = true;
        //printf("summarized vertical up\n");
    }
}
*/

static void sum(
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int i, int j,
        bool *moved,
        struct ModelBox *mb
) {
    switch (dir) {
        case DIR_UP: {
            if (i > 0 && field_copy[j][i - 1].value == field_copy[j][i].value) {
                
                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i - 1;
                cur->to_j = j;
                cur->action = CA_SUM;

                field_copy[j][i - 1].value = field_copy[j][i].value * 2;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("summarized vertical up\n");
            }
            break;
        }
        case DIR_DOWN: {
            if (i + 1 < FIELD_SIZE && 
                field_copy[j][i + 1].value == field_copy[j][i].value) {

                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i + 1;
                cur->to_j = j;
                cur->action = CA_SUM;

                field_copy[j][i + 1].value = field_copy[j][i].value * 2;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("summarized vertical down\n");
            }
            break;
        }
        case DIR_LEFT: {
            if (j > 0 && field_copy[j - 1][i].value == field_copy[j][i].value) {


                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i;
                cur->to_j = j - 1;
                cur->action = CA_SUM;

                field_copy[j - 1][i].value = field_copy[j][i].value * 2;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("summarized horizontal left\n");
            }
            break;
        }
        case DIR_RIGHT: {
            if (j + 1 < FIELD_SIZE &&
                field_copy[j + 1][i].value == field_copy[j][i].value) {

                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i - 1;
                cur->to_j = j + 1;
                cur->action = CA_SUM;

                field_copy[j + 1][i].value = field_copy[j][i].value * 2;
                mb->scores += field_copy[j][i].value;
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("summarized horizontal right\n");
            }
            break;
        default:
            break;
        }
    }
}

static void move(
        struct ModelBox *mb,
        enum Direction dir,
        struct Cell field_copy[FIELD_SIZE][FIELD_SIZE],
        int i, int j,
        bool *moved
) {
    switch (dir) {
        case DIR_UP: {
            if (i > 0 && field_copy[j][i - 1].value == 0) {

                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->value = field_copy[j][i].value;
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i - 1;
                cur->to_j = j;
                cur->action = CA_MOVE;

                field_copy[j][i - 1] = field_copy[j][i];
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("moved vertical up\n");
            }
            break;
        }
        case DIR_DOWN: {
            if (i + 1 < FIELD_SIZE && field_copy[j][i + 1].value == 0) {


                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->value = field_copy[j][i].value;
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i + 1;
                cur->to_j = j;
                cur->action = CA_MOVE;

                field_copy[j][i + 1] = field_copy[j][i];
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("moved vertical down\n");
            }
            break;
        }
        case DIR_LEFT: {
            if (j > 0 && field_copy[j - 1][i].value == 0) {

                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->value = field_copy[j][i].value;
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i;
                cur->to_j = j - 1;
                cur->action = CA_MOVE;

                field_copy[j - 1][i] = field_copy[j][i];
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("moved horizontal left\n");
            }
            break;
        }
        case DIR_RIGHT: {
            if (j + 1 < FIELD_SIZE && field_copy[j + 1][i].value == 0) {


                struct Cell *cur = &mb->queue[mb->queue_size++];
                cur->value = field_copy[j][i].value;
                cur->from_i = i;
                cur->from_j = j;
                cur->to_i = i;
                cur->to_j = j + 1;
                cur->action = CA_MOVE;

                field_copy[j + 1][i] = field_copy[j][i];
                field_copy[j][i].value = 0;
                *moved = true;
                //printf("moved horizontal right\n");
            }
            break;
        default:
            break;
        }
    }
}


static void update(struct ModelBox *mb, enum Direction dir) {
    assert(mb);
    if (mb->state == MBS_GAMEOVER)
        return;

    mb->last_dir = dir;
    mb->queue_size = 0;

    const int field_size_bytes = sizeof(mb->field[0][0]) * FIELD_SIZE * FIELD_SIZE;
    struct Cell field_copy[FIELD_SIZE][FIELD_SIZE] = {0};
    memmove(field_copy, mb->field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int i = 0; i < FIELD_SIZE; i++) {
            for (int j = 0; j < FIELD_SIZE; j++) {
                if (field_copy[j][i].value == 0) continue;
                move(mb, dir, field_copy, i, j, &moved);
            }
        }

        for (int i = 0; i < FIELD_SIZE; i++) {
            for (int j = 0; j < FIELD_SIZE; j++) {
                if (field_copy[j][i].value == 0) 
                    continue;
                sum(dir, field_copy, i, j, &moved, mb);
            }
        }

        iter++;
        //printf("iter %d\n", iter);
    } while (moved);

    memmove(mb->field, field_copy, field_size_bytes);

    if (!is_over(mb))
        put(mb);
    else
        mb->state = MBS_GAMEOVER;

    if (find_max(mb) == WIN_VALUE)
        mb->state = MBS_WIN;
}

void modelbox_init(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(*mb));
    put(mb);
    mb->last_dir = DIR_NONE;
    mb->update = update;
    mb->state = MBS_PROCESS;
    mb->queue_cap = FIELD_SIZE * FIELD_SIZE;
    mb->queue_size = 0;
    mb->dropped = true;
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
    const float thick = 3.;

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

static Color get_color(struct ModelView *mv, int value) {
    int colors_num = sizeof(colors) / sizeof(colors[0]);
    /*printf("colors_num %d\n", colors_num);*/
    for (int k = 0; k < colors_num; k++) {
        if (value == mv->sorted[k].value) {
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
        default:
            printf("bad value in action2str()\n");
            exit(EXIT_FAILURE);
    }
    return str;
}

struct DrawOpts {
    int     fontsize;
    Vector2 offset_coef; // 1. is default value
    bool    custom_color;
    Color   color;
    double  amount;
};

static const struct DrawOpts dflt_draw_opts = {
    .offset_coef = {
        1.,
        1.,
    },
    .fontsize = 90,
    .custom_color = false,
};

static void cell_draw(
    struct ModelView *mv, struct Cell *cell, struct DrawOpts opts
) {
    assert(mv);
    int fontsize = opts.fontsize;
    float spacing = 2.;
    //const int field_width = FIELD_SIZE * quad_width;
    Vector2 start = mv->pos;

    /*
    trace("cell_draw: value %d\n", cell->value);
    trace(
        "cell_draw: from (%d, %d) to (%d, %d)\n", 
        cell->from_j, cell->from_i,
        cell->to_j, cell->to_i
    );
    trace("\n");
    */

    assert(cell);
    //if (!cell) return;

    // Создать таймер для каждого движения в очереди
    // Обработать события всех таймеров
    // Удалить закончившиеся таймеры.
    //
    // Таймеры для фиксированнного значения fps, длятся duration секунд.

    char msg[64] = {0};
    //sprintf(msg, "%d [%s]", cell->value, action2str(cell->action));
    sprintf(msg, "%d", cell->value);
    int textw = 0;

    do {
        Font f = GetFontDefault();
        textw = MeasureTextEx(f, msg, fontsize--, spacing).x;
        /*printf("fontsize %d\n", fontsize);*/
    } while (textw > quad_width);

    Vector2 pos = start;
    // TODO: Как сделать offset_coef рабочим в диапазоне -1..1
    // Значение по умолчанию - 0
    // -1 - смещение влево
    /*
    
    // 1 - смещение вправо
    Vector2 offset = {
        (quad_width - textw) / 2.,
        opts.offset_coef.x * (quad_width - fontsize) / 2.,
    };
    pos.x += cell->to_j * quad_width + offset.x;
    pos.y += cell->to_i * quad_width + offset.y;

    */

    Vector2 offset = {
        opts.offset_coef.x * (quad_width - textw) / 2.,
        opts.offset_coef.x * (quad_width - fontsize) / 2.,
    };

    assert(opts.amount >= 0 && opts.amount <= 1.);
    Vector2 base_pos = {
        Lerp(cell->from_j, cell->to_j, opts.amount),
        Lerp(cell->from_i, cell->to_i, opts.amount),
    };

    trace("cell_draw: opts.amount %f\n", opts.amount);
    trace("cell_draw: cell %p, base_pos %s\n", cell, Vector2_tostr(base_pos));

    /*
    pos.x += base_pos * quad_width + offset.x;
    pos.y += base_pos * quad_width + offset.y;
    */

    Vector2 disp = Vector2Add(Vector2Scale(base_pos, quad_width), offset);
    pos = Vector2Add(pos, disp);

    Color color = opts.custom_color ? opts.color : get_color(mv, cell->value);
    //Color color = DARKBLUE;
    DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);

    // text_draw_in_rect((Rectangle), ALIGN_LEFT, pos, font, fontsize);
}

static void draw_cell(struct ModelView *mv, struct ModelBox *mb, int index) {
    assert(mv);
    assert(mb);
    int fontsize = 90 / 2.;
    float spacing = 2.;
    //const int field_width = FIELD_SIZE * quad_width;
    Vector2 start = mv->pos;

    assert(index >= 0);
    assert(index < mb->queue_cap);

    struct Cell *cell = &mb->queue[index];
    //if (cell->action != CA_SUM)
        //return;

    trace("draw_cell: value %d\n", cell->value);
    trace("draw_cell: from_j %d, from_i %d\n", cell->from_j, cell->from_j);
    trace("draw_cell: to_j %d, to_i %d\n", cell->to_j, cell->to_j);
    trace("\n");

    assert(cell);
    //if (!cell) return;

    // Создать таймер для каждого движения в очереди
    // Обработать события всех таймеров
    // Удалить закончившиеся таймеры.
    //
    // Таймеры для фиксированнного значения fps, длятся duration секунд.

    char msg[64] = {0};
    sprintf(msg, "%d [%s]", cell->value, action2str(cell->action));
    int textw = 0;

    do {
        Font f = GetFontDefault();
        textw = MeasureTextEx(f, msg, fontsize--, spacing).x;
        /*printf("fontsize %d\n", fontsize);*/
    } while (textw > quad_width);

    Vector2 pos = start;
    pos.x += cell->to_j * quad_width + (quad_width - textw) / 2.;
    pos.y += cell->to_i * quad_width + (quad_width - fontsize) / 2.;
    //Color color = get_color(mv, cell->value);
    Color color = DARKBLUE;
    DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, color);
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
        }
    }
}

#define TMR_BLOCK_TIME  .5

static void timer_add(struct ModelView *mv, void *udata, size_t sz) {
    trace(
        "timer_add: timer_size %d, timers_cap %d\n",
        mv->timers_size, 
        mv->timers_cap
    );
    assert(mv->timers_size + 1 < mv->timers_cap);
    struct Timer *tmr = &mv->timers[mv->timers_size++];
    tmr->start_time = GetTime();
    tmr->duration = TMR_BLOCK_TIME;
    tmr->expired = false;
    tmr->data = malloc(sz);
    memmove(tmr->data, udata, sz);
}

static const struct DrawOpts special_draw_opts = {
    .color = BLUE,
    .custom_color = true,
    .fontsize = 40,
    .offset_coef = { 0., 0., },
};

static void tmr_update(struct Timer *t, void *udata) {
    struct Cell *cell = t->data;
    struct ModelView *mv = udata;
    /*trace("tmr_update: udata %p\n", udata);*/
    struct DrawOpts opts = special_draw_opts;
    opts.amount = t->amount;
    cell_draw(mv, cell, opts);
}

static void timers_remove_expired(
    struct ModelView *mv,
    struct Cell **expired_cells,
    int *expired_cells_num
) {
    assert(expired_cells_num);
    assert(expired_cells);

    //*expired_cells_num = 0;

    struct Timer tmp[mv->timers_cap];
    memset(tmp, 0, sizeof(tmp));
    int tmp_size = 0;
    for (int i = 0; i < mv->timers_size; i++) {
        if (!mv->timers[i].expired) {
            tmp[tmp_size++] = mv->timers[i];
        } else {
            if (mv->timers[i].data) {
                //free(mv->timers[i].data);
                struct Cell *cell = (struct Cell*)mv->timers[i].data;
                expired_cells[(*expired_cells_num)++] = cell;
            }
        }
    }
    memmove(mv->timers, tmp, sizeof(tmp[0]) * tmp_size);
    mv->timers_size = tmp_size;
}

static void timers_update(struct ModelView *mv) {
    for (int i = 0; i < mv->timers_size; i++) {
        double now = GetTime();
        struct Timer *timer = &mv->timers[i];
        timer->amount = (now - timer->start_time) / timer->duration;
        if (now - timer->start_time >= timer->duration) {
            timer->expired = true;
        } else {
            trace(
                "timers_update: timer %p, amount %f\n", timer, timer->amount
            );
            tmr_update(timer, mv);
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

static void draw(struct ModelView *mv, struct ModelBox *mb) {
    assert(mv);
    assert(mb);
    sort_numbers(mv, mb);
    draw_field(mv);

    /*
    if (mv->state == MVS_READY) {
        mv->tmr_block = GetTime();
        mv->state = MVS_ANIMATION;
    } else if (mv->state == MVS_ANIMATION) {
        double now = GetTime();
        if (now - mv->tmr_block >= TMR_BLOCK_TIME) {
            mv->state = MVS_READY;
        } else {
            DrawCircle(GetScreenWidth() / 2., GetScreenHeight() / 2., 40, BLUE);
        }
    }
    */

    draw_numbers(mv, mb->field);

    console_write("timers num: %d\n", mv->timers_size);
    console_write("last dir: %s\n", dir2str(mb->last_dir));

    //trace("draw: mb->queue_size %d\n", mb->queue_size);
    // TODO: таймеры запускаются параллельно, а не последовательно
    /*
    for (int i = 0; i < mb->queue_size; i++) {
        timer_add(mv, &mb->queue[i], sizeof(mb->queue[0]));
    }
    timers_update(mv);

    struct Cell *tmp_cells[FIELD_SIZE * FIELD_SIZE] = {0};
    int tmp_cells_num = 0;
    timers_remove_expired(mv, tmp_cells, &tmp_cells_num);

    if (mv->timers_size == 0)
        mv->state = MVS_READY;
    else mv->state = MVS_ANIMATION;

    //trace("draw: mv->timers_size %d\n", mv->timers_size);

    // Копии ячеек уже стоящих на местах не попадают в финальный массив для
    // отображения
    for (int i = 0; i < tmp_cells_num; i++) {
        struct Cell *tmp_cell = tmp_cells[i];
        bool found = false;
        for (int k = 0; k < mv->expired_cells_num; k++) {
            struct Cell *expired_cell = mv->expired_cells[k];
            if (tmp_cell->from_i == expired_cell->from_i &&
                tmp_cell->to_i == expired_cell->to_i &&
                tmp_cell->from_j == expired_cell->from_j &&
                tmp_cell->to_j == expired_cell->to_j) {
                found = true;
                break;
            }
        }
        if (!found) {
            mv->expired_cells[mv->expired_cells_num++] = tmp_cell;
        }
    }

    // Рисовать фигуры оставшиеся после просроченных таймеров
    for (int j = 0; j < mv->expired_cells_num; j++) {
        cell_draw(mv, mv->expired_cells[j], dflt_draw_opts);
    }
    // */

}

void modelview_init(
    struct ModelView *mv, const Vector2 *pos, struct ModelBox *mb
) {
    assert(mv);
    assert(mb);
    //memset(mv, 0, sizeof(*mv));
    copy_field(mv->field_prev, mb->field);
    const int field_width = FIELD_SIZE * quad_width;
    if (!pos) {
        mv->pos = (Vector2){
            .x = (GetScreenWidth() - field_width) / 2.,
            .y = (GetScreenHeight() - field_width) / 2.,
        };
    } else 
        mv->pos = *pos;
    mv->timers_cap = FIELD_SIZE * FIELD_SIZE * 2;
    mv->timers_size = 0;
    mv->state = MVS_READY;
    mv->draw = draw;
    mv->tmr_block = GetTime();
    mv->expired_cells_cap = FIELD_SIZE * FIELD_SIZE;
    mv->dropped = false;
}

void modelbox_shutdown(struct ModelBox *mb) {
    assert(mb);
    memset(mb, 0, sizeof(mb));
    mb->dropped = true;
}

void modelview_shutdown(struct ModelView *mv) {
    assert(mv);
    memset(mv, 0, sizeof(*mv));
    mv->dropped = true;
}


