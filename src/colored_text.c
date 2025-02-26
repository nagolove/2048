#include "colored_text.h"

#include "koh_timerman.h"
#include "modelview.h"
#include <assert.h>
#include "koh_routine.h"
#include "koh_logger.h"

static TimerMan *tm = NULL;

void colored_text_init(int field_size) {
    tm = timerman_new(field_size * field_size, "colored_text");
}

void colored_text_shutdown() {
    if (tm) {
        timerman_free(tm);
        tm = NULL;
    }
}

int colored_text_pickup_size(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts
) {
    assert(text);
    assert(text_num >= 0);
    assert(opts.font_bitmap);
    Vector2 text_bound = text->text_bound;
    assert(text_bound.x >= 0);
    assert(text_bound.y >= 0);

    int fontsize = 2;

    if (opts.use_fnt_vector) {
    } else {

        //Font font = *opts.font_bitmap;
        Vector2 textsize = {};

        do {
            text->base_font_size = fontsize;
            textsize = colored_text_measure(text, text_num, opts);
            fontsize *= 2;
        } while (textsize.x < text_bound.x);
        //trace("colored_text_pickup_size: fontsize %d\n", fontsize);

        do {
            text->base_font_size = fontsize;
            textsize = colored_text_measure(text, text_num, opts);
            fontsize--;
        } while (textsize.x > text_bound.x || textsize.y > text_bound.y);
    }

    return fontsize;
}

Vector2 colored_text_measure(
    struct ColoredText *text, int text_num, ColoredTextOpts opts
) {
    Font f = *opts.font_bitmap;
    Vector2 textsize = {};
    if (!opts.use_fnt_vector) {
        for (int i = 0; i < text_num; ++i) {
            const char *t = text[i].text;
            float scale = text[i].scale;
            Vector2 m = MeasureTextEx(f, t, text->base_font_size * scale, 0.);
            textsize.x += m.x;
            if (m.y > textsize.y)
                textsize.y = m.y;
        }
    } else {
    }

    return textsize;
}

// Как и что будет меняться для пульсации?
void colored_text_print(
    struct ColoredText *text, int text_num,
    ColoredTextOpts opts,
    ecs_t *r, e_id cell_en
) {
    assert(text);
    assert(text->base_font_size > 0);

    Font f = *opts.font_bitmap;
    Vector2 pos = text->pos;
    int base_font_size = text->base_font_size;
    if (!opts.use_fnt_vector) {
        Vector2 measures[text_num];
        // считать размеры каждого куска теста при печати
        for (int i = 0; i < text_num; ++i) {
            assert(text[i].scale > -100.);
            assert(text[i].scale < 100.);
            const char *t = text[i].text;
            float scale = text[i].scale;
            float base_font_size = text->base_font_size;
            measures[i] = MeasureTextEx(f, t, base_font_size * scale, 0.);
        }

        float max_height = 0;
        for (int j = 0; j < text_num; j++) {
            if (measures[j].y > max_height)
                max_height = measures[j].y;
        }

        //float pulsation_scale = (2. * M_PI - sin(GetTime()));
        // 0.9 .. 1.1
        float phase = 0.f;

        if (e_valid(r, cell_en)) {
            Effect *ef = e_get(r, cell_en, cmp_effect);

            if (ef) {
                phase = ef->phase;
            }
        }

        float pulsation_scale = 1.0 + 0.05 * sin(phase + GetTime());

        // Зачем нужен text_num??
        for (int i = 0; i < text_num; ++i) {
            int font_size = base_font_size * text[i].scale * pulsation_scale;
            // Ширина и высота текста
            Vector2 m = measures[i];
            Vector2 disp = {0, (max_height - m.y) / 2.};
            pos = Vector2Add(pos, disp);

            /*
            Vector2 puls_disp =  { 
                -m.x * pulsation_scale / 8.,
                -m.y * pulsation_scale / 8. 
            };
            trace(
                "colored_text_print: puls_disp %s\n",
                Vector2_tostr(puls_disp)
            );
            pos = Vector2Add(pos, puls_disp);
            */

            DrawTextEx(
                *opts.font_bitmap, text[i].text, 
                pos, font_size, 0., text[i].color
            );
            pos.x += m.x;
        }
    }

}

