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

enum AlphaMode {
    AM_NONE,
    AM_FORWARD,
    AM_BACKWARD,
};

struct Cell {
    enum AlphaMode  anim_alpha;
    bool            dropped;        // ячейка подлежит удалению 
    int             x, y, value;
    bool            anim_movement,  // ячейка в движении
                    anim_size;      // ячейка меняет размер
};

enum ModelViewState {
    MVS_ANIMATION,
    MVS_READY,
    MVS_WIN,
    MVS_GAMEOVER,
};

typedef struct ModelView ModelView;

/*
    Отображение поля. Все, что связано с анимацией.
 */
struct ModelView {
    de_ecs              *r;
    Camera2D            *camera;
    int                 scores;
    int                 dx, dy, field_size;
    float               tmr_put_time, tmr_block_time;
    enum Direction      dir;
    bool                has_sum, has_move;
    bool                use_gui;
    // Таймеры для анимации плиток
    struct TimerMan     *timers;

    struct Cell         *sorted;
    int                 sorted_num;

    Vector2             pos;
    enum ModelViewState state;
    bool                dropped;    //Флаг деинициализации структуры
};

struct Setup {
    Vector2     *pos;
    Camera2D    *cam;
    int         field_size;
    float       tmr_put_time, tmr_block_time;
};

void modelview_init(struct ModelView *mv, struct Setup setup);
void modelview_put_manual(struct ModelView *mv, int x, int y, int value);
void modelview_put_cell(struct ModelView *mv, struct Cell cell);
void modelview_put(struct ModelView *mv);
void modelview_shutdown(struct ModelView *mv);
void modelview_save_state2file(struct ModelView *mv);
bool modelview_draw(struct ModelView *mv);
void modelview_input(struct ModelView *mv, enum Direction dir);
struct Cell *modelview_get_cell(
    struct ModelView *mv, int x, int y, de_entity *en
);
char *modelview_state2str(enum ModelViewState state);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();
