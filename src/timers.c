#include "timers.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct TimerMan {
    struct Timer        *timers;
    int                 timers_cap, timers_size;
    bool                paused;
};

struct TimerMan *timerman_new(int cap) {
    struct TimerMan *tm = calloc(1, sizeof(*tm));
    assert(tm);
    assert(cap > 0);
    assert(cap < 2048 && "too many timers, 2048 is ceil");
    tm->timers = calloc(cap, sizeof(tm->timers[0]));
    tm->timers_cap = cap;
    return tm;
}

void timerman_free(struct TimerMan *tm) {
    assert(tm);

    for (int i = 0; i < tm->timers_size; ++i) {
        if (tm->timers[i].data) {
            free(tm->timers[i].data);
            tm->timers[i].data = NULL;
        }
    }
    if (tm->timers) {
        free(tm->timers);
        tm->timers = NULL;
    }

    free(tm);
}

bool timerman_add(struct TimerMan *tm, struct TimerDef td) {
    assert(tm);
    if (tm->timers_size + 1 >= tm->timers_cap) 
        return false;
    if (!td.on_update)
        trace("timerman_add: timer without on_update callback\n");
    struct Timer *tmr = &tm->timers[tm->timers_size++];
    static size_t id = 0;
    tmr->id = id++;
    tmr->start_time = GetTime();
    tmr->duration = td.duration;;

    if (td.sz) {
        tmr->sz = td.sz;
        tmr->data = malloc(td.sz);
        assert(tmr->data);
        memmove(tmr->data, td.udata, td.sz);
    } else
        tmr->data = td.udata;

    tmr->on_update = td.on_update;
    tmr->on_stop = td.on_stop;
    return true;
}

/*
int timerman_remove_expired(struct TimerMan *tm) {
    assert(tm);
    // Хранилище для неистекщих таймеров
    struct Timer tmp[tm->timers_cap];

    memset(tmp, 0, sizeof(tmp));
    int tmp_size = 0;
    for (int i = 0; i < tm->timers_size; i++) {
        if (tm->timers[i].expired) {
            if (tm->timers[i].data) {
                free(tm->timers[i].data);
                tm->timers[i].data = NULL;
            }
        } else {
            tmp[tmp_size++] = tm->timers[i];
        }
    }
    memmove(tm->timers, tmp, sizeof(tmp[0]) * tmp_size);
    tm->timers_size = tmp_size;
    return tm->timers_size;
}
*/

static void timer_shutdown(struct Timer *timer) {
    if (timer->on_stop) 
        timer->on_stop(timer);
    if (timer->data && timer->sz) {
        free(timer->data);
        timer->data = NULL;
    }
}

int timerman_update(struct TimerMan *tm) {
    struct Timer tmp[tm->timers_cap];

    memset(tmp, 0, sizeof(tmp));
    int tmp_size = 0;

    for (int i = 0; i < tm->timers_size; i++) {
        struct Timer *timer = &tm->timers[i];
        if (timer->duration < 0) continue;

        double now = GetTime();
        timer->amount = (now - timer->start_time) / timer->duration;

        // Сделать установку паузы и снятие с нее

        if (now - timer->start_time > timer->duration) {
            //timer->expired = true;
            timer_shutdown(timer);
        } else {
            if (timer->on_update) {
                if (timer->on_update(timer)) {
                    timer_shutdown(timer);
                } else
                    // оставить таймер в менеджере
                    tmp[tmp_size++] = tm->timers[i];
            }
        };
    }

    memmove(tm->timers, tmp, sizeof(tmp[0]) * tmp_size);
    tm->timers_size = tmp_size;

    return tm->timers_size;
}

void timerman_pause(struct TimerMan *tm, bool is_paused) {
    tm->paused = is_paused;
}

void timerman_window(struct TimerMan *tm) {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("timers", &open, flags);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_ScrollX |
        ImGuiTableFlags_BordersV | 
        ImGuiTableFlags_Resizable | 
        ImGuiTableFlags_Reorderable | 
        ImGuiTableFlags_Hideable;

    ImVec2 outer_size = {0., 0.}; // Размер окошка таблицы
    if (igBeginTable("timers", 8, table_flags, outer_size, 0.)) {
        ImGuiTableColumnFlags column_flags = 0;

        igTableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort, 0., 0);
        igTableSetupColumn("id", ImGuiTableColumnFlags_DefaultSort, 0., 1);
        igTableSetupColumn("start time", column_flags, 0., 2);
        igTableSetupColumn("duration time", column_flags, 0., 3);
        igTableSetupColumn("amount", column_flags, 0., 4);
        //igTableSetupColumn("expired", column_flags, 0., 5);
        igTableSetupColumn("data", column_flags, 0., 5);
        igTableSetupColumn("on_update cb", column_flags, 0., 6);
        igTableSetupColumn("on_stop cb", column_flags, 0., 7);
        igTableHeadersRow();

        for (int row = 0; row < tm->timers_size; ++row) {
            char line[64] = {0};
            struct Timer * tmr = &tm->timers[row];

            igTableNextRow(0, 0);

            igTableSetColumnIndex(0);
            sprintf(line, "%d", row);
            igText(line);

            igTableSetColumnIndex(1);
            sprintf(line, "%ld", tmr->id);
            igText(line);

            igTableSetColumnIndex(2);
            sprintf(line, "%f", tmr->start_time);
            igText(line);

            igTableSetColumnIndex(3);
            sprintf(line, "%f", tmr->duration);
            igText(line);

            igTableSetColumnIndex(4);
            sprintf(line, "%f", tmr->amount);
            igText(line);

            //igTableSetColumnIndex(4);
            //sprintf(line, "%s", tmr->expired ? "true" : "false");
            //igText(line);

            igTableSetColumnIndex(5);
            sprintf(line, "%p", tmr->data);
            igText(line);

            igTableSetColumnIndex(6);
            sprintf(line, "%p", tmr->on_update);
            igText(line);

            igTableSetColumnIndex(7);
            sprintf(line, "%p", tmr->on_stop);
            igText(line);
            // */
        }

        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);

        igEndTable();
    }
    igEnd();
}

void timerman_clear(struct TimerMan *tm) {
    assert(tm);
    tm->timers_size = 0;
}

/*
void timerman_clear_infinite(struct TimerMan *tm) {
    assert(tm);

    struct Timer tmp[tm->timers_cap];
    memset(tmp, 0, sizeof(tmp));
    int tmp_num = 0;

    for (int i = 0; i < tm->timers_size; ++i) {
        if (tm->timers[i].duration >= 0)
            tmp[tmp_num++] = tm->timers[i];
    }

    if (tmp_num > 0) {
        memcpy(tm->timers, tmp, sizeof(struct Timer) * tmp_num);
    }
    tm->timers_size = tmp_num;
}
*/

int timerman_num(struct TimerMan *tm, int *infinite_num) {
    assert(tm);
    if (infinite_num) {
        *infinite_num = 0;
        for (int i = 0; i < tm->timers_size; ++i)
            *infinite_num += tm->timers[i].duration < 0;
    }
    return tm->timers_size;
}

void timerman_each(
    struct TimerMan *tm, 
    enum TimerManAction (*iter)(struct Timer *tmr, void*),
    void *udata
) {
    assert(tm);
    assert(iter);
    if (!iter) return;

    struct Timer tmp[tm->timers_cap];
    memset(tmp, 0, sizeof(tmp));
    int tmp_num = 0;

    for (int i = 0; i < tm->timers_size; ++i) {
        switch (iter(&tm->timers[i], udata)) {
            case TMA_NEXT: 
                tmp[tmp_num++] = tm->timers[i];
                break;
            case TMA_BREAK:
                tmp[tmp_num++] = tm->timers[i];
                goto copy;
            case TMA_REMOVE_NEXT: break;
            case TMA_REMOVE_BREAK: 
                goto copy;
            default:
                trace("timerman_each: bad value in enum TimeManAction\n");
                exit(EXIT_FAILURE);
        }
    }

copy:

    if (tmp_num > 0) {
        memcpy(tm->timers, tmp, sizeof(struct Timer) * tmp_num);
    }
    tm->timers_size = tmp_num;
}
