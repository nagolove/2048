#include <raylib.h>

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "genann.h"
#include "modelbox.h"

const int screen_width = 1920;
const int screen_height = 1080;
const int quad_width = 128 + 64;

// x:y
int field[field_size][field_size] = {0};
int scores = 0;
bool gameover = false;

int sorted[field_size * field_size] = {0};
Color colors[5] = {
    RED,
    ORANGE,
    GOLD,
    GREEN,
    GRAY,
};

int cmp(const void *pa, const void *pb) {
    const int *a = pa, *b = pb;
    return *a < *b;
}

void sort_numbers() {
    int tmp[field_size * field_size] = {0};
    int idx = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++)
            tmp[idx++] = field[j][i];

    qsort(tmp, field_size * field_size, sizeof(field[0][0]), cmp);
    memmove(sorted, tmp, sizeof(sorted));
}

bool is_over() {
    int num = 0;
    for (int i = 0; i < field_size; i++)
        for (int j = 0; j < field_size; j++)
            if (field[i][j] > 0)
                num++;

    printf("is_over: %d\n", field_size * field_size == num);
    return field_size * field_size == num;
}

void put() {
    int x = rand() % field_size;
    int y = rand() % field_size;

    while (field[x][y] != 0) {
        x = rand() % field_size;
        y = rand() % field_size;
    }

    float v = (float)rand() / (float)RAND_MAX;
    if (v >= 0. && v < 0.9)
        field[x][y] = 2;
    else 
        field[x][y] = 4;
}

void swipe_horizontal(bool left) {
    printf("swipe_horizontal: left %d\n", (int)left);
    const int field_size_bytes = sizeof(field[0][0]) * field_size * field_size;
    int field_copy[field_size][field_size] = {0};
    memmove(field_copy, field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] != 0) {
                    switch (left) {
                        case true: {
                            if (j > 0 && field_copy[j - 1][i] == 0) {
                                field_copy[j - 1][i] = field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("moved horizontal\n");
                            }
                            break;
                        }
                        case false: {
                            if (j + 1 < field_size && field_copy[j + 1][i] == 0) {
                                field_copy[j + 1][i] = field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("moved horizontal\n");
                            }
                            break;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] != 0) {
                    switch (left) {
                        case true: {
                            if (j > 0 && field_copy[j - 1][i] == field_copy[j][i]) {
                                field_copy[j - 1][i] = field_copy[j][i] * 2;
                                scores += field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("summarized horizontal left\n");
                            }
                            break;
                        }
                        case false: {
                            if (j + 1 < field_size && field_copy[j + 1][i] == field_copy[j][i]) {
                                field_copy[j + 1][i] = field_copy[j][i] * 2;
                                scores += field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("summarized horizontal right\n");
                            }
                            break;
                        }
                    }
                }
            }
        }

        iter++;
        printf("iter %d\n", iter);
    } while (moved);

    memmove(field, field_copy, field_size_bytes);
}

void swipe_vertical(bool up) {
    printf("swipe_vertical: up %d\n", (int)up);
    const int field_size_bytes = sizeof(field[0][0]) * field_size * field_size;
    int field_copy[field_size][field_size] = {0};
    memmove(field_copy, field, field_size_bytes);

    bool moved = false;
    int iter = 0;

    do {
        moved = false;

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] != 0) {
                    switch (up) {
                        case true: {
                            if (i > 0 && field_copy[j][i - 1] == 0) {
                                field_copy[j][i - 1] = field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("moved vertical up\n");
                            }
                            break;
                        }
                        case false: {
                            if (i + 1 < field_size && field_copy[j][i + 1] == 0) {
                                field_copy[j][i + 1] = field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("moved vertical down\n");
                            }
                            break;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < field_size; i++) {
            for (int j = 0; j < field_size; j++) {
                if (field_copy[j][i] != 0) {
                    switch (up) {
                        case true: {
                            if (i > 0 && field_copy[j][i - 1] == field_copy[j][i]) {
                                field_copy[j][i - 1] = field_copy[j][i] * 2;
                                scores += field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("summarized vertical up\n");
                            }
                            break;
                        }
                        case false: {
                            if (i + 1 < field_size && field_copy[j][i + 1] == field_copy[j][i]) {
                                field_copy[j][i + 1] = field_copy[j][i] * 2;
                                scores += field_copy[j][i];
                                field_copy[j][i] = 0;
                                moved = true;
                                printf("summarized vertical down\n");
                            }
                            break;
                        }
                    }
                }
            }
        }

        iter++;
        printf("iter %d\n", iter);
    } while (moved);

    memmove(field, field_copy, field_size_bytes);
}

Color get_color(int value) {
    int colors_num = sizeof(colors) / sizeof(colors[0]);
    /*printf("colors_num %d\n", colors_num);*/
    for (int k = 0; k < colors_num; k++) {
        if (value == sorted[k]) {
            return colors[k];
        }
    }
    return colors[colors_num - 1];
}

void draw_numbers() {
    int fontsize = 90;
    float spacing = 2.;
    const int field_width = field_size * quad_width;
    Vector2 start = {
        .x = (screen_width - field_width) / 2.,
        .y = (screen_height - field_width) / 2.,
    };

    for (int i = 0; i < field_size; i++) {
        for (int j = 0; j < field_size; j++) {
            if (field[j][i] == 0)
                continue;

            char msg[64] = {0};
            sprintf(msg, "%d", field[j][i]);
            int textw = 0;

            do {
                textw = MeasureTextEx(GetFontDefault(), msg, fontsize--, spacing).x;
                /*printf("fontsize %d\n", fontsize);*/
            } while (textw > quad_width);

            Vector2 pos = start;
            pos.x += j * quad_width + (quad_width - textw) / 2.;
            pos.y += i * quad_width + (quad_width - fontsize) / 2.;
            DrawTextEx(GetFontDefault(), msg, pos, fontsize, 0, get_color(field[j][i]));
        }
    }
}

void draw_field() {
    const int field_width = field_size * quad_width;
    Vector2 start = {
        .x = (screen_width - field_width) / 2.,
        .y = (screen_height - field_width) / 2.,
    };
    const float thick = 3.;

    Vector2 tmp = start;
    for (int u = 0; u <= field_size; u++) {
        Vector2 end = tmp;
        end.y += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.x += quad_width;
    }

    tmp = start;
    for (int u = 0; u <= field_size; u++) {
        Vector2 end = tmp;
        end.x += field_width;
        DrawLineEx(tmp, end, thick, BLACK);
        tmp.y += quad_width;
    }
}

void check_over() {
    if (!is_over())
        put();
    else
        gameover = true;
}

void input() {
    if (IsKeyPressed(KEY_LEFT)) {
        swipe_horizontal(true);
        check_over();
    } else if (IsKeyPressed(KEY_RIGHT)) {
        swipe_horizontal(false);
        check_over();
    } else if (IsKeyPressed(KEY_UP)) {
        swipe_vertical(true);
    } else if (IsKeyPressed(KEY_DOWN)) {
        swipe_vertical(false);
    }
}

void draw_over() {
    const char *msg = "GAMEOVER";
    const int fontsize = 190;
    float width = MeasureText(msg, fontsize);
    Vector2 pos = {
        .x = (screen_width - width) / 2.,
        .y = (screen_height - fontsize) / 2.,
    };
    DrawText(msg, pos.x, pos.y, fontsize, GREEN);
}

void reset() {
    gameover = false;
    memset(field, 0, sizeof(field[0][0]) * field_size * field_size);
    scores = 0;
}

void draw_scores() {
    char msg[64] = {0};
    const int fontsize = 70;
    sprintf(msg, "scores: %d", scores);
    Vector2 pos = {
        .x = (screen_width - MeasureText(msg, fontsize)) / 2.,
        .y = 1.2 * fontsize,
    };
    DrawText(msg, pos.x, pos.y, fontsize, BLUE);
}

void update() {
    if (!gameover)
        input();
    else if (IsKeyPressed(KEY_SPACE)) {
        reset();
    }

    sort_numbers();
    BeginDrawing();
    ClearBackground(RAYWHITE);
    draw_field();
    draw_numbers();
    draw_scores();
    if (gameover)
        draw_over();
    EndDrawing();
}

int main(void) {

    InitWindow(screen_width, screen_height, "2048");
    SetTargetFPS(60);
    srand(time(NULL));

    put();
    while (!WindowShouldClose()) {
        update();
    }

    CloseWindow();

    return 0;
}
