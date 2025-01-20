#pragma once

#include "raylib.h"
#include "raymath.h"
#include "fnt_vector.h"

typedef struct ColoredTextOpts {
    Font      *font_bitmap;
    FntVector *font_vector;
    bool      use_fnt_vector;
} ColoredTextOpts;

struct ColoredText {
    const char  *text;
    float       scale; // 1. or 2. or 0.5
    Color       color;
};

int colored_text_pickup_size(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts,
    Vector2 text_bound 
);

void colored_text_print(
    struct ColoredText *text, int text_num,
    Vector2 pos, 
    ColoredTextOpts opts,
    int base_size 
);

Vector2 colored_text_measure(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts,
    int base_size 
);
