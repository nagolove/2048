#pragma once

#include "raylib.h"
#include "raymath.h"
#include "koh_fnt_vector.h"
#include "koh_ecs.h"

typedef struct ColoredTextOpts {
    Font      *font_bitmap;
    FntVector *font_vector;
    bool      use_fnt_vector;
} ColoredTextOpts;

typedef struct ColoredText {
    const char  *text;
    float       scale; // 1. or 2. or 0.5
    Color       color;
    Vector2     text_bound;
    int         base_font_size;
    Vector2     pos;
} ColoredText;

int colored_text_pickup_size(
    ColoredText *text, int text_num, ColoredTextOpts opts
);

void colored_text_print(
    ColoredText *text, int text_num, ColoredTextOpts opts,
    ecs_t *r, e_id cell_en
);

Vector2 colored_text_measure(
    ColoredText *text, int text_num, ColoredTextOpts opts
);

void colored_text_init(int field_size);
void colored_text_shutdown();

