// vim: set colorcolumn=85
// vim: fdm=marker
#include "test.h"

#include "modelview.h"

// XXX: Можно-ли сделать так, что все одинаковые подряд цифры сложатся?
// {{{
/*
static TestSet sets[] = {

    // {{{
    { 
        .x = {
            // state
            " ", " ", " ", " ", " ", " ",
            "2", " ", " ", "4", "4", "4",
            "2", " ", " ", " ", " ", " ",
            " ", " ", "8", " ", " ", " ",
            " ", " ", "8", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            // user input
            "right", 
            // assert state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", "2", "8", "4",
            " ", " ", " ", " ", " ", "2",
            " ", " ", " ", " ", " ", "8",
            " ", " ", " ", " ", " ", "8",
            " ", " ", " ", " ", " ", " ",
            NULL,
        },
        .description = "проба, нулевой",
    },
    // }}}

    // {{{
    { 
        .x = {
            // state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            // user input
            "right", 
            // assert state
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            " ", " ", " ", " ", " ", " ",
            NULL,
        },
        .description = "первый, пустой",
    },
    // }}}

    { 
        .x = {
            // state
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            " ", " ", " ", "8", " ", " ",
            // user input
            "down", 
            // assert state
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", " ",  " ", " ",
            " ", " ", " ", "16", " ", " ",
            " ", " ", " ", "32", " ", " ",
            NULL,
        },
        .description = "второй",
    },

    // TODO: Сделать возможной последовательность из нескольких движений 
    // для одного начального состояния.
    { 
        .x = {
            // state
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "4", "2", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            " ", "8", "4", "8", " ", " ",
            // user input
            "down", 
            // assert state
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", " ",  " ", " ",  " ", " ",
            " ", "32", "16","16", " ", " ",
            " ", "8",  "4", "32", " ", " ",
            NULL,
        },
        .description = "третий",
        .selected = true,
    },


};
*/
// }}}

/*
const static int sets_num = sizeof(sets) / sizeof(sets[0]);
static int set_current = 0;

__attribute__((unused))
static void t_set(ModelView *mv, TestSet *ts) {
    // {{{
    assert(mv);
    assert(mv->field_size == T_FIELD_SIZE);
    assert(ts);

    int i = 0;
    while (ts->x[i++]);
    assert(i == T_X_SIZE);

    ts->input_done = false;
    ts->idx = 0;

    int *idx = &ts->idx;
    assert(*idx >= 0);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            int val = -1;
            sscanf(ts->x[*idx], "%d", &val);
            if (val != -1)
                modelview_put_cell(mv, x, y, val);
            (*idx)++;
        }
    }
    // }}}
}

__attribute__((unused))
static void t_assert(ModelView *mv, TestSet *ts) {
    // {{{
    assert(mv);
    assert(ts);

    // Анимация должна закончиться
    if (mv->state != MVS_READY)
        return;

    // Проверка производится только после ввода
    if (!ts->input_done) {
        return;
    }

    // Проверка производится только один раз
    if (ts->assert_done)
        return;

    trace("t_assert: '%s'\n", ts->description);
    trace("t_assert: idx %d\n", ts->idx);
    //trace("t_assert: x '%s'\n", ts->x[ts->idx]);

    // Напечатать стартовое поле
    int other_idx = 0;

    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("initial\n");

    koh_term_color_set(KOH_TERM_BLUE);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            const char *val = ts->x[other_idx];
            if (strcmp(val, " ") == 0)
                val = ".";
            //printf("%s ", val);
            printf("%-4s ", val);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Поле напечатано

    printf("direction '%s'\n", ts->x[other_idx]);

    // Напечатать поле
    other_idx = ts->idx;

    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("should be\n");

    koh_term_color_set(KOH_TERM_BLUE);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            const char *val = ts->x[other_idx];
            if (strcmp(val, " ") == 0)
                val = ".";
            printf("%-4s ", val);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Поле напечатано

    // TODO: Напечатать текущее состояние
    printf("\n");
    koh_term_color_set(KOH_TERM_MAGENTA);
    printf("state is\n");

    other_idx = ts->idx;
    koh_term_color_set(KOH_TERM_YELLOW);
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            Cell *cell = modelview_search_cell(mv, x, y);
            char buf[16] = {};
            if (!cell) {
                sprintf(buf, "%s", ". ");
            } else {
                sprintf(buf, "%d ", cell->value);
            }
            printf("%-4s ", buf);
            other_idx++;
        }
        printf("\n");
    }
    printf("\n");
    koh_term_color_reset();
    // Состояние напечатано

    ts->assert_done = true;

    int *idx = &ts->idx;
    for (int y = 0; y < T_FIELD_SIZE; y++) {
        for (int x = 0; x < T_FIELD_SIZE; x++) {
            int val = -1;
            sscanf(ts->x[*idx], "%d", &val);

            if (val != -1) {
                //modelview_put_cell(mv, x, y, val);
                Cell *cell = modelview_search_cell(mv, x, y);
                if (!cell) {
                    koh_term_color_set(KOH_TERM_RED);
                    trace("t_assert: no cell at [%d, %d]\n", x, y);
                    koh_term_color_reset();

                    return;
                }

                if (cell->value != val) {
                    trace(
                        "t_assert: cell->value == %d, "
                        "but should be equal %d\n",
                        cell->value,
                        val
                    );

                    return;
                }
            }

            (*idx)++;
        }
    }

    koh_term_color_set(KOH_TERM_GREEN);
    trace("t_assert: passed\n");
    koh_term_color_reset();

    // }}}
}

__attribute__((unused))
static void t_input(ModelView *mv, TestSet *ts) {
    // {{{
    // Закончилась-ли анимация?
    if (mv->state != MVS_READY)
        return;

    if (!ts->input_done) {
        const char *dirstr = ts->x[ts->idx++];
        trace("t_input: applying direction '%s'\n", dirstr);
        enum Direction dir = str2direction(dirstr);
        modelview_input(mv, dir);
        ts->input_done = true;
    }
    // }}}
}

__attribute__((unused))
static void t_gui(Stage_Test *st) {
    // {{{
    bool open = true;
    
    igBegin("t_gui", &open, 0);

    if (igBeginListBox("tests", (ImVec2){})) {
        for (int i = 0; i < sets_num; i++) {
            const char *label = sets[i].description;
            bool *selected = &sets[i].selected;
            igSelectable_BoolPtr(label, selected, 0, (ImVec2){});
            if (*selected) {
                for (int j = 0; j < sets_num; j++) {
                    if (j != i)
                        sets[j].selected = false;
                }
            }
        }
        igEndListBox();
    }

    if (igButton("run", (ImVec2){})) {
        for (int j = 0; j < sets_num; j++) {
            if (sets[j].selected) {
                reinit(j);
            }
        }
    }

    igEnd();
    // }}}
}
*/
