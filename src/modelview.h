#pragma once

#include "ecs_circ_buf.h"
#include "koh_ecs.h"
#include "raylib.h"
#include "timers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    bool            dropped;        // ячейка подлежит удалению 
    int             x, y, value;
};

enum BonusType {
    BT_DOUBLE = 0, 
};

struct Bonus {
    enum BonusType  type;
    Color           border_color;
};

struct Effect {
    enum AlphaMode  anim_alpha;
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

struct ColorTheme {
    Color   background, foreground;
};

/*
    Отображение поля. Все, что связано с анимацией.
 */
struct ModelView {
    ecs_t               *r;
    Camera2D            *camera;
    int                 scores;
    int                 dx, dy, field_size;
    float               tmr_put_time,   // появление плитки в секундах
                        tmr_block_time; // движение плитки в секундах
    enum Direction      dir, prev_dir;
    bool                has_sum, has_move;
    bool                use_gui,        // рисовать imgui
                        auto_put;       // класть новую плитку на след. ход

    int                 quad_width;
    Font                font;

    struct TimerMan     *timers_slides,     // Таймеры анимации плиток
                        *timers_effects;    // Таймеры эффектов

    struct Cell         *sorted;
    int                 sorted_num;

    Vector2             pos;
    enum ModelViewState state;
    bool                dropped;    //Флаг деинициализации структуры
    bool                use_bonus;
    float               font_spacing;
    struct ColorTheme   color_theme;
    void                *test_payload;
};

extern const struct ColorTheme color_theme_dark, color_theme_light;

struct Setup {
    Vector2             *pos;
    Camera2D            *cam;
    int                 field_size;
    float               tmr_put_time, tmr_block_time;
    bool                use_gui, auto_put;
    bool                use_bonus;
    struct ColorTheme   color_theme;
};

void modelview_init(struct ModelView *mv, struct Setup setup);
void modelview_put_manual(struct ModelView *mv, int x, int y, int value);
//void modelview_put_cell(struct ModelView *mv, struct Cell cell);
void modelview_put(struct ModelView *mv);
void modelview_shutdown(struct ModelView *mv);
void modelview_save_state2file(struct ModelView *mv);
bool modelview_draw(struct ModelView *mv);
void modelview_draw_gui(struct ModelView *mv);
void modelview_input(struct ModelView *mv, enum Direction dir);
struct Cell *modelview_get_cell(
    struct ModelView *mv, int x, int y, e_id *en
);
char *modelview_state2str(enum ModelViewState state);
void modelview_field_print(struct ModelView *mv);
void modelview_field_print_s(struct ModelView *mv, char *str, size_t str_sz);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();

extern bool _use_field_printing;
void _field_print(struct ModelView *mv, char **msg);

void _modelview_field_print_s(
    ecs_t *r, int field_size, char *str, size_t str_sz
);
void _modelview_field_print(ecs_t *r, int field_size);
struct Cell *modelview_find_by_value(ecs_t *r, int value);
