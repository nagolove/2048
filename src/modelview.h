#pragma once

#include "koh_destral_ecs.h"
#include "timers.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIN_VALUE   2048

enum Direction {
    DIR_NONE,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
};

enum CellAction {
    CA_NONE,
    CA_MOVE,
    CA_SUM,
};

struct Cell {
    int8_t          guard1[5];
    bool            dropped;    // подлежит удалению 
    int             value;
    bool            anim_size;  // анимировать размер шрифта
    int             x, y;
    bool            anima;      // флаг нахождения в таймере
    bool            touched;
    int8_t          guard2[5];
};

enum ModelViewState {
    MVS_ANIMATION,
    MVS_READY,
    MVS_WIN,
    MVS_GAMEOVER,
};

typedef struct ModelView ModelView;

typedef void (*ModelBoxUpdate)(struct ModelView *mb, enum Direction dir);

/*
    Отображение поля. Все, что связано с анимацией.
 */
struct ModelView {
    de_ecs              *r;
    Camera2D            *camera;
    int                 scores;
    ModelBoxUpdate      update;
    int                 dx, dy, field_size;
    float               tmr_put_time, tmr_block_time;
    enum Direction      dir;
    bool                has_sum, has_move;

    // Таймеры для анимации плиток
    struct TimerMan     *timers;

    struct Cell         *sorted;
    int                 sorted_num;

    Vector2             pos;
    enum ModelViewState state;
    bool                dropped;    //Флаг деинициализации структуры
    void                (*draw)(struct ModelView *mv);
};

struct Setup {
    Vector2  *pos;
    Camera2D *cam;
    int      field_size;
};

//void modelbox_init(struct ModelBox *mb);
void modelview_init(struct ModelView *mv, struct Setup setup);
//void modelbox_shutdown(struct ModelBox *mb);
void modelview_put_manual(struct ModelView *mv, int x, int y, int value);
void modelview_put_cell(struct ModelView *mv, struct Cell cell);
void modelview_put(struct ModelView *mv);
void modelview_shutdown(struct ModelView *mv);
void modelview_save_state2file(struct ModelView *mv);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();