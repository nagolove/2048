#include "colored_text.h"

#include <assert.h>
#include "koh_logger.h"

int colored_text_pickup_size(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts, Vector2 text_bound 
) {
    assert(text);
    assert(text_num >= 0);
    assert(opts.font_bitmap);
    assert(text_bound.x >= 0);
    assert(text_bound.y >= 0);

    int fontsize = 2;

    if (opts.use_fnt_vector) {
    } else {

        //Font font = *opts.font_bitmap;
        Vector2 textsize = {};

        do {
            textsize = colored_text_measure(text, text_num, opts, fontsize);
            fontsize *= 2;
        } while (textsize.x < text_bound.x);
        //trace("colored_text_pickup_size: fontsize %d\n", fontsize);

        do {
            textsize = colored_text_measure(text, text_num, opts, fontsize);
            fontsize--;
        } while (textsize.x > text_bound.x || textsize.y > text_bound.y);
    }

    return fontsize;
}

Vector2 colored_text_measure(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts, int base_size 
) {

    Vector2 textsize = {};
    if (!opts.use_fnt_vector) {

        for (int i = 0; i < text_num; ++i) {
            Vector2 m = MeasureTextEx(
                *opts.font_bitmap, text[i].text, base_size * text[i].scale, 0.
            );
            textsize.x += m.x;
            if (m.y > textsize.y)
                textsize.y = m.y;
        }
    } else {
    }

    return textsize;
}

void colored_text_print(
    struct ColoredText *text, int text_num,
    Vector2 pos, ColoredTextOpts opts, int base_size 
) {
    assert(text);
    assert(base_size > 0);

    if (!opts.use_fnt_vector) {
        Vector2 measures[text_num];
        // считать размеры каждого куска теста при печати
        for (int i = 0; i < text_num; ++i) {
            assert(text[i].scale > -100.);
            assert(text[i].scale < 100.);
            measures[i] = MeasureTextEx(
                *opts.font_bitmap, text[i].text, base_size * text[i].scale, 0.
            );
        }

        float max_height = 0;
        for (int j = 0; j < text_num; j++) {
            if (measures[j].y > max_height)
                max_height = measures[j].y;
        }

        for (int i = 0; i < text_num; ++i) {
            int font_size = base_size * text[i].scale;
            Vector2 m = measures[i];
            //Vector2 disp = {0, -(font_size - m.y) / 2.};
            Vector2 disp = {0, (max_height - m.y) / 2.};
            pos = Vector2Add(pos, disp);
            DrawTextEx(
                *opts.font_bitmap, text[i].text, 
                pos, font_size,
                0., text[i].color
            );
            pos.x += m.x;
        }
    }

}

