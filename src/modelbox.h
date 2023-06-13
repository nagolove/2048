#pragma once

#include "koh_destral_ecs.h"
#include "timers.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

#define FIELD_SIZE  7
#define WIN_VALUE   2048

enum Direction {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
    DIR_NONE,
};

enum CellAction {
    CA_NONE,
    CA_MOVE,
    CA_SUM,
};

struct Cell {
    int             value;
    int             from_x, from_y, to_x, to_y;
    bool            moving;
    //enum CellAction action;
};

enum ModelViewState {
    MVS_ANIMATION,
    MVS_READY,
    MVS_WIN,
    MVS_GAMEOVER,
};

// x:y
typedef struct Cell Field[FIELD_SIZE][FIELD_SIZE];

typedef struct ModelBox ModelBox;
typedef struct ModelView ModelView;

typedef void (*ModelBoxUpdate)(struct ModelView *mb, enum Direction dir);

/*
    Текущеее состояние поля. Без анимации и эффектов. Подходит для машинного 
    обучения тк работает быстро и использует небольшое количество памяти.
 */
struct ModelBox {
    enum Direction      last_dir;
    ModelBoxUpdate      update;
    void                (*start)(struct ModelBox *mb, struct ModelView *mv);
    bool                dropped;    //Флаг деинициализации структуры
};

/*
    Отображение поля. Все, что связано с анимацией.
 */
struct ModelView {
    de_ecs              *r;
    Camera2D            *camera;
    //Field               field;
    int                 scores;
    ModelBoxUpdate      update;
    int                 dx, dy;
    bool                has_sum, has_move;

    // Таймеры для анимации плиток
    struct TimerMan     *timers;

    /*

    // Очередь действий
    struct Cell         queue[FIELD_SIZE * FIELD_SIZE];
    int                 queue_cap, queue_size;

    // Статичные плитки для рисования
    struct Cell         fixed[FIELD_SIZE * FIELD_SIZE];
    int                 fixed_cap, fixed_size;

    */
    struct Cell         sorted[FIELD_SIZE * FIELD_SIZE];

    Vector2             pos;
    enum ModelViewState state;
    bool                dropped;    //Флаг деинициализации структуры
    void    (*draw)(struct ModelView *mv);
};

//void modelbox_init(struct ModelBox *mb);
void modelview_init(struct ModelView *mv, const Vector2 *pos, Camera2D *cam);
//void modelbox_shutdown(struct ModelBox *mb);
void modelview_shutdown(struct ModelView *mv);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();
