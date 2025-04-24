// vim: fdm=marker
#pragma once

#include "lua.h"
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
#include "koh_resource.h"

#define WIN_VALUE   2048

// {{{
typedef enum Direction {
    DIR_NONE,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_UP,
} Direction;

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

enum BombColor {
    BC_BLACK,
    BC_RED,
};

typedef struct Position {
    int x, y; 
} Position;

typedef struct Cell {
          // ячейка подлежит удалению 
    bool  dropped;        
          // клетка не рисуется при значении равном 0
    int   value;

          // Для пульсации шрифта 
          // коэффициент масштаба
    float coef, 
          // начальная фаза колебания
          phase;
} Cell;

typedef struct Transition {
    enum AlphaMode  anim_alpha;
    bool            anim_movement,  // ячейка в движении
                    anim_size;      // ячейка меняет размер
} Transition;

typedef struct Bomb {
    enum BombColor  bomb_color;
                    // угол поворота огонька фитиля
    float           aura_rot,
                    // фаза для поворота огонька фитиля
                    aura_phase;
                    // количество оставшихся ходов, до 0
    int             moves_cnt;
} Bomb;

typedef struct Explosition {
    int i;
} Explosition;

/*
Возможные сочетания компонентов
Position + Cell + Transition
Position + Transition + Bomb
*/

typedef enum ModelViewState {
    // Есть активные анимации
    MVS_ANIMATION,
    // Готовность к вводу
    MVS_READY,
    MVS_WIN,
    MVS_GAMEOVER,
} ModelViewState;

// }}}

typedef struct History History;

// Отображение поля. Все, что связано с анимацией.
typedef struct ModelView {
    // Показывает, что сцена инициализирована
    bool                inited;

    // Массив field_size * field_size - временное хранения сущностей для
    // удаления
    e_id                *e_2destroy;
    int                 e_2destroy_num;

    lua_State           *l;
    Resource            reslist;


                        // бомба
    Texture2D           tex_bomb,
                        // черная бомба
                        tex_bomb_black,
                        // красная бомба
                        tex_bomb_red,
                        // горение фитиля бомбы
                        tex_aura;

    // Текстуры взрывов
    Texture2D           *tex_ex;
                        // общее количество тектстур
    int                 tex_ex_num, 
                        // индекс активной текстуры
                        tex_ex_index;

    ecs_t               *r;
    Camera2D            *camera;
    int                 scores;
    int                 dx, dy, field_size;
    float               tmr_put_time,   // появление плитки в секундах
                        tmr_block_time; // движение плитки в секундах
    Direction           dir;
    bool                has_sum, has_move;
    bool                use_gui,        // рисовать imgui
                        auto_put,       // класть новую плитку на след. ход
                        draw_field;

    int                 quad_width;

    ColoredTextOpts     text_opts;
    Font                font;
    FntVector           *font_vector;

                        // Таймеры анимации
    TimerMan            *timers;     

    Cell                *sorted;
    int                 sorted_num;

                        // XXX: Позиция чего?
    Vector2             pos;
    ModelViewState      state;
    bool                dropped;    //Флаг деинициализации структуры
                        // использовать какие-нибудь бонусы или только цифрв
    bool                use_bonus,
                        // можно сканировать на бомбы
                        can_scan_bombs,
                        // следующая фигура - бомба
                        next_bomb,
                        // выпадают 1 или 3
                        strong;
    float               font_spacing,
                        // XXX: Толщина чего?
                        thick;
    /*struct ColorTheme   color_theme;*/
    void                *test_payload;
                        // значение на клетке для победы
    int                 win_value,
                        // количество фишек для создания на одном ходу
                        put_num;
    void                (*on_init_lua)();
    void                (*lua_after_load)(struct ModelView *mv, lua_State *l);

    // [y * field_size + x] - сущности с прикрепленными ячейками
    e_id                *field;
    History             *history;
} ModelView;

extern const struct ColorTheme color_theme_dark, color_theme_light;

typedef struct Setup {
    int                 win_value;
    Camera2D            *cam;
    int                 field_size;
    float               tmr_put_time, tmr_block_time;
    bool                use_gui, auto_put;
    bool                use_bonus, use_fnt_vector;
    void                (*on_init_lua)();
} Setup;

void modelview_init(ModelView *mv, Setup setup);
// Для внутреннего использования и для тестирования
void modelview_put_cell(struct ModelView *mv, int x, int y, int value);
// Ход игры, создает фишки по внутренним условиям
void modelview_put(ModelView *mv);
void modelview_shutdown(ModelView *mv);
void modelview_save_state2file(ModelView *mv);
int modelview_draw(ModelView *mv);
void modelview_draw_gui(ModelView *mv);
void modelview_input(ModelView *mv, enum Direction dir);
/*Cell *modelview_get_cell(ModelView *mv, int x, int y, e_id *en);*/
char *modelview_state2str(enum ModelViewState state);
void modelview_pause_set(ModelView *mv, bool is_paused);

extern e_cp_type cmp_cell, cmp_bomb, cmp_position, cmp_transition;

// "right" -> DIR_RIGHT etc.
Direction str2direction(const char *str);
Cell *modelview_search_cell(ModelView *mv, int x, int y);
e_id modelview_search_entity(ModelView *mv, int x, int y);

extern e_cp_type cmp_position, cmp_cell, cmp_bomb, cmp_transition, cmp_exp;
extern const char *dir2str[];
