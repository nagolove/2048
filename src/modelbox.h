#pragma once

#include "timers.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

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
    CA_NONE,
};

struct Cell {
    int value;
    //void (*on_move)(struct Cell *cell, int fromj, int fromi, int toj, int toi);
    int from_j, from_i, to_j, to_i;
    enum CellAction action;
};

enum ModelViewState {
    MVS_ANIMATION,
    MVS_READY,
};

// x:y
typedef struct Cell Field[FIELD_SIZE][FIELD_SIZE];

typedef struct ModelBox ModelBox;
typedef struct ModelView ModelView;

typedef void (*ModelBoxUpdate)(
    struct ModelBox *mb, enum Direction dir, struct ModelView *mv
);

/*
    Текущеее состояние поля. Без анимации и эффектов. Подходит для машинного 
    обучения тк работает быстро и использует небольшое количество памяти.
 */
struct ModelBox {
    int                 scores;
    Field               field;
    enum ModelBoxState  state;
    enum Direction      last_dir;
    ModelBoxUpdate      update;
    void                (*start)(struct ModelBox *mb, struct ModelView *mv);
    bool                dropped;    //Флаг деинициализации структуры
};

/*
    Отображение поля. Все, что связано с анимацией.
 */
struct ModelView {
    // Таймеры для анимации плиток
    struct TimerMan     *timers;

    // Очередь действий
    struct Cell         queue[FIELD_SIZE * FIELD_SIZE];
    int                 queue_cap, queue_size;

    // Статичные плитки для рисования
    struct Cell         fixed[FIELD_SIZE * FIELD_SIZE];
    int                 fixed_cap, fixed_size;

    struct Cell         sorted[FIELD_SIZE * FIELD_SIZE];

    Vector2             pos;
    enum ModelViewState state;
    bool                dropped;    //Флаг деинициализации структуры
    void    (*draw)(struct ModelView *mv, struct ModelBox *mb);
};

void modelbox_init(struct ModelBox *mb);
void modelview_init(
    struct ModelView *mv, const Vector2 *pos, struct ModelBox *mb
);
void modelbox_shutdown(struct ModelBox *mb);
void modelview_shutdown(struct ModelView *mv);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();
