// vim: fdm=marker
#pragma once

#include "ecs_circ_buf.h"
#include "koh_ecs.h"
#include "raylib.h"
#include "koh_timerman.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "koh_fnt_vector.h"
#include "colored_text.h"

#define WIN_VALUE   2048

// {{{
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

typedef struct Cell {
                    // ячейка подлежит удалению 
    bool            dropped;        
    int             x, y, value;
} Cell;

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

struct ColorTheme {
    Color   background, foreground;
};

// }}}

/*
    Отображение поля. Все, что связано с анимацией.
 */
typedef struct ModelView {
    ecs_t               *r;
    Camera2D            *camera;
    int                 scores;
    int                 dx, dy, field_size;
    float               tmr_put_time,   // появление плитки в секундах
                        tmr_block_time; // движение плитки в секундах
    enum Direction      dir, prev_dir;
    bool                has_sum, has_move;
    bool                use_gui,        // рисовать imgui
                        auto_put,       // класть новую плитку на след. ход
                        draw_field;

    int                 quad_width;

    ColoredTextOpts     text_opts;
    Font                font;
    FntVector           *font_vector;

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
                        // значение на клетке для победы
    int                 win_value,
                        // количество фишек для создания на одном ходу
                        put_num;
} ModelView;

extern const struct ColorTheme color_theme_dark, color_theme_light;

typedef struct Setup {
    int                 win_value;
    /*Vector2             *pos;*/
    Camera2D            *cam;
    int                 field_size;
    float               tmr_put_time, tmr_block_time;
    bool                use_gui, auto_put;
    bool                use_bonus, use_fnt_vector;
    struct ColorTheme   color_theme;
} Setup;

void modelview_init(ModelView *mv, Setup setup);
void modelview_put_manual(ModelView *mv, int x, int y, int value);
//void modelview_put_cell(ModelView *mv, Cell cell);
void modelview_put(ModelView *mv);
void modelview_shutdown(ModelView *mv);
void modelview_save_state2file(ModelView *mv);
bool modelview_draw(ModelView *mv);
void modelview_draw_gui(ModelView *mv);
void modelview_input(ModelView *mv, enum Direction dir);
Cell *modelview_get_cell(
    ModelView *mv, int x, int y, e_id *en
);
char *modelview_state2str(enum ModelViewState state);
void modelview_field_print(ModelView *mv);
void modelview_field_print_s(ModelView *mv, char *str, size_t str_sz);

void model_global_init();
void model_global_shutdown();

void test_divide_slides();

void _modelview_field_print_s(
    ecs_t *r, int field_size, char *str, size_t str_sz
);
void _modelview_field_print(ecs_t *r, int field_size);
Cell *modelview_find_by_value(ecs_t *r, int value);
