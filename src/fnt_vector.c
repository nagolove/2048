// vim: fdm=marker
#include "fnt_vector.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "koh_logger.h"
#include "koh_common.h"
#include "koh_table.h"
#include "utf8proc.h"

typedef struct Char {
    FT_Face face;
} Char;

struct FntVector {
    Vector2 cursor;
    HTable  *char2vector;
};

static int move_to(const FT_Vector* to, void* user);
static int line_to(const FT_Vector* to, void* user);
static int conic_to(const FT_Vector* control, const FT_Vector* to, void* user);
static int cubic_to(
    const FT_Vector* control1, const FT_Vector* control2, 
    const FT_Vector* to, void* user
);

static FT_Library library = NULL;

static void char_render(FntVector *fv, Char *c) {
    assert(fv);
    assert(c);

    FT_Outline_Funcs outline_funcs;
    outline_funcs.move_to = move_to;
    outline_funcs.line_to = line_to;
    outline_funcs.conic_to = conic_to;
    outline_funcs.cubic_to = cubic_to;
    outline_funcs.shift = 0;
    outline_funcs.delta = 0;

    FT_Outline outline = c->face->glyph->outline;
    FT_Outline_Decompose(&outline, &outline_funcs, fv);
}

static Char char_load(FntVector *fv, const char *ttf_file, int char_code) {
    Char c = {};

    if (FT_New_Face(library, ttf_file, 0, &c.face)) {
        trace("Error loading font\n");
        koh_trap();
    }

    // Устанавливаем размер шрифта
    FT_Set_Pixel_Sizes(c.face, 0, 48);

    // Загружаем глиф символа char_code
    if (FT_Load_Char(c.face, char_code, FT_LOAD_NO_BITMAP)) {
        trace("char_load: error loading glyph\n");
    }

    // Очистка ресурсов
    /*FT_Done_Face(c.face);*/

    htable_add(fv->char2vector, &char_code, sizeof(char_code), &c, sizeof(c));

    return c;
}

void fnt_vector_shutdown() {
    if (library) {
        FT_Done_FreeType(library);
        library = NULL;
    }
}

void fnt_vector_init() {
    // Инициализация FreeType
    if (!library && FT_Init_FreeType(&library)) {
        trace("Error initializing FreeType\n");
        koh_trap();
    }
}

void fnt_vector_free(FntVector *fv) {
    assert(fv);

    if (fv->char2vector) {
        htable_free(fv->char2vector);
    }
}

void on_remove_char(
    const void *key, int key_len, void *value, int value_len, void *userdata
) {
    trace("on_remove_char:\n");
    const Char *c = key;

    FT_Done_Face(c->face);
}

FntVector *fnt_vector_new(const char *ttf_file) {
    FntVector fv = {
        .char2vector = htable_new(&(HTableSetup) {
            /*.f_on_remove = on_remove_char,*/
        }),
    };

    if (!library) {
        fnt_vector_init();
    }

    const int ascii_last = 256, ascii_first = 32;
    // Константы из таблиц Юникода
    const int cyrillic_last = 0x450, cyrillic_first = 0x410;
    const int pseudo_gr_last = 0x25FF, pseudo_gr_first = 0x2500;
    const int arrows_last = 0x21FF, arrows_first = 0x2190;

    // Загрузка базовых и долнительных символов
    for (int i = ascii_first; i < ascii_last; i++) {
        char_load(&fv, ttf_file, i);
    }
    for (int i = cyrillic_first; i < cyrillic_last; i++) {
        char_load(&fv, ttf_file, i);
    }
    for (int i = pseudo_gr_first; i < pseudo_gr_last; i++) {
        char_load(&fv, ttf_file, i);
    }
    for (int i = arrows_first; i < arrows_last; i++) {
        char_load(&fv, ttf_file, i);
    }

    return 0;
}

// Проверка для аргумента utf8proc_iterate()
_Static_assert(
    sizeof(utf8proc_int32_t) == sizeof(int), "only for x86_x64"
);

void fnt_vector_draw(FntVector *fv, const char *text, Vector2 pos) {
    assert(fv);
    assert(fv->char2vector);

    const uint8_t *ptr = (const uint8_t *)text;
    int codepoint;
    utf8proc_ssize_t bytes;

    while (*ptr) {
        bytes = utf8proc_iterate(ptr, -1, &codepoint);
        if (bytes < 0) {
            trace("fnt_vector_draw: Invalid UTF-8 encoding detected.\n");
            break;
        }

        printf(
            "fnt_vector_draw: Codepoint: U+%04X (%d)\n",
            codepoint, codepoint
        );

        Char *c = htable_get(
            fv->char2vector, &codepoint, sizeof(codepoint), NULL
        );

        if (c) char_render(fv, c);

        ptr += bytes;
    }

}

static int move_to(const FT_Vector* to, void* user) {
    //std::cout << "Move to: (" << to->x / 64.0 << ", " << to->y / 64.0 << ")\n";
    trace("move_to:\n");
    FntVector *fv = user;
    fv->cursor.x = to->x;
    fv->cursor.y = to->y;
    return 0;
}

static int line_to(const FT_Vector* to, void* user) {
    /*std::cout << "Line to: (" << to->x / 64.0 << ", " << to->y / 64.0 << ")\n";*/
    trace("line_to:\n");
    FntVector *fv = user;
    DrawLine(fv->cursor.x, fv->cursor.y, to->x, to->y, RED);
    return 0;
}

static int conic_to(
    const FT_Vector* control, const FT_Vector* to, void* user
) {
    /*std::cout << "Quadratic curve to: (" << control->x / 64.0 << ", " << control->y / 64.0 << ") -> (" << to->x / 64.0 << ", " << to->y / 64.0 << ")\n";*/
    trace("conic_to:\n");

    FntVector *fv = user;
    const float thick = 5.;
    DrawSplineSegmentBezierQuadratic(
        fv->cursor,
        (Vector2) { control->x, control->y },
        (Vector2) { to->x, to->y },
        thick,
        BLUE
    );

    return 0;
}

static int cubic_to(
    const FT_Vector* control1, const FT_Vector* control2,
    const FT_Vector* to, void* user
) {

    FntVector *fv = user;
    const float thick = 5.;
    DrawSplineSegmentBezierCubic(
            fv->cursor,
            (Vector2) { control1->x, control1->y },
            (Vector2) { control2->x, control2->y },
            (Vector2) { to->x, to->y },
            thick,
            BLUE
    );

    /*std::cout << "Cubic curve to: (" << control1->x / 64.0 << ", " << control1->y / 64.0 << ") -> ("*/
    /*<< control2->x / 64.0 << ", " << control2->y / 64.0 << ") -> ("*/
    /*<< to->x / 64.0 << ", " << to->y / 64.0 << ")\n";*/
    trace("cubic_to:\n");
    return 0;
}
