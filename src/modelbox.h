#pragma once

#include "raylib.h"
#include <stdbool.h>

#define FIELD_SIZE  4
#define WIN_VALUE   2048

enum Direction {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
    DIR_NONE,
};

enum ModelBoxState {
    MBS_PROCESS,
    MBS_WIN,
    MBS_GAMEOVER,
};

enum CellAction {
    CA_MOVE,
    CA_SUM,
};

struct Cell {
    int value;
    //void (*on_move)(struct Cell *cell, int fromj, int fromi, int toj, int toi);
    int from_j, from_i, to_j, to_i;
    enum CellAction action;
};

struct Timer {
    double  start_time; // GetTime()
    double  duration;   // in seconds
    double  amount;     // 0..1
    bool    expired;
    void    *data;
};

enum ModelViewState {
    MVS_ANIMATION,
    MVS_READY,
};

typedef struct Cell Field[FIELD_SIZE][FIELD_SIZE];

struct ModelBox {
    // x:y
    int  scores;
    Field               field;
    struct Cell         queue[FIELD_SIZE * FIELD_SIZE];
    int                 queue_cap, queue_size;
    enum ModelBoxState  state;
    enum Direction      last_dir;
    void (*update)(struct ModelBox *mb, enum Direction dir);
    bool                dropped;
};

struct ModelView {
    struct Timer        timers[FIELD_SIZE * FIELD_SIZE * 2];
    int                 timers_cap, timers_size;

    struct Cell         queue[FIELD_SIZE * FIELD_SIZE];
    int                 queue_cap, queue_size;

    struct Cell         *expired_cells[FIELD_SIZE * FIELD_SIZE];
    int                 expired_cells_num, expired_cells_cap;
    struct Cell         sorted[FIELD_SIZE * FIELD_SIZE];
    Vector2 pos;
    enum ModelViewState state;
    Field               field_prev;
    double              tmr_block;
    void    (*draw)(struct ModelView *mv, struct ModelBox *mb);
    bool                dropped;
};

void modelbox_init(struct ModelBox *mb);
void modelview_init(
    struct ModelView *mv, const Vector2 *pos, struct ModelBox *mb
);
void modelbox_shutdown(struct ModelBox *mb);
void modelview_shutdown(struct ModelView *mv);
