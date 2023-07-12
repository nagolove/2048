#include "timers.h"
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

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
    double              pause_time; // время начала паузы
    char                name[128];
};

struct TimerMan *timerman_new(int cap, const char *name) {
    struct TimerMan *tm = calloc(1, sizeof(*tm));
    assert(tm);
    assert(cap > 0);
    assert(cap < 2048 && "too many timers, 2048 is ceil");
    tm->timers = calloc(cap, sizeof(tm->timers[0]));
    tm->timers_cap = cap;
    strncpy(tm->name, name, sizeof(tm->name));
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
    tmr->start_time = tmr->last_now = GetTime();
    assert(td.duration > 0);
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

static void timer_shutdown(struct Timer *timer) {
    if (timer->on_stop) 
        timer->on_stop(timer);
    if (timer->data && timer->sz) {
        free(timer->data);
        timer->data = NULL;
    }
}

int timerman_update(struct TimerMan *tm) {
    assert(tm);

    struct Timer tmp[tm->timers_cap];

    memset(tmp, 0, sizeof(tmp));
    int tmp_size = 0;

    for (int i = 0; i < tm->timers_size; i++) {
        struct Timer *timer = &tm->timers[i];
        if (timer->duration < 0) continue;

        double now = tm->paused ? timer->last_now : GetTime();
        timer->amount = (now - timer->start_time) / timer->duration;
        timer->last_now = now;

        // Сделать установку паузы и снятие с нее

        if (now - timer->start_time > timer->duration) {
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
    if (is_paused) {
        if (!tm->paused) {
            // enable -> disable
            trace("timerman_pause: enable -> disable\n");
            tm->pause_time = GetTime();
            for (int i = 0; i < tm->timers_size; ++i) {
                tm->timers[i].last_now = tm->pause_time;
            }
            tm->paused = is_paused;
        }
    } else {
        if (tm->paused) {
            // disable -> enable
            double shift = GetTime() - tm->pause_time;
            trace("timerman_pause: disable -> enable, shift %f\n", shift);
            for (int i = 0; i < tm->timers_size; ++i) {
                tm->timers[i].start_time += shift;
            }
            tm->paused = is_paused;
        }
    }
}

static void table_draw(struct TimerMan *tm) {
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
}

void timerman_window(struct TimerMan **tm, int tm_num) {
    assert(tm);
    assert(tm_num >= 0);

    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("timers", &open, flags);

    int node_idx = 1;
    igSetNextItemOpen(true, ImGuiCond_Once);

    for (int i = 0; i < tm_num; ++i) {
        if (!tm[i]) continue;

        if (igTreeNode_Ptr((void*)(uintptr_t)node_idx++, "%s", tm[i]->name)) {
            table_draw(tm[i]);
            igTreePop();
        }
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

struct TimerMan *timerman_clone(struct TimerMan *tm) {
    assert(tm);
    struct TimerMan *ret = calloc(1, sizeof(*tm));
    assert(ret);
    *ret = *tm;
    ret->timers = calloc(ret->timers_cap, sizeof(ret->timers[0]));
    assert(ret->timers);
    size_t sz = sizeof(tm->timers[0]) * tm->timers_size;
    memcpy(ret->timers, tm->timers, sz);
    return ret;
}
