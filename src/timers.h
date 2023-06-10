#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct TimerMan TimerMan;

struct Timer {
    struct TimerMan *tm; // Заполняется автоматически при вызове update()
    double          start_time; // GetTime()
    double          duration;   // in seconds
    double          amount;     // 0..1
    bool            expired;
    size_t          id;
    void            *data;      // всегда динамически выделяемая память
    void            (*update)(struct Timer *tmr);
};

struct TimerDef {
    void *udata;    // источник для копирования данных
    size_t sz;      // размер копируемых данных
    double duration;
    void (*update)(struct Timer*);
};

struct TimerMan *timerman_new();
void timerman_free(struct TimerMan *tm);

void timerman_add(struct TimerMan *tm, struct TimerDef td);
int timerman_update(struct TimerMan *tm);
void timerman_pause(struct TimerMan *tm, bool is_paused);
void timerman_window(struct TimerMan *tm);
void timerman_clear(struct TimerMan *tm);
// Удаляет закончившиеся таймеры
int timerman_remove_expired(struct TimerMan *tm);
// Возвращает количество всех(конечных и бесконечных) таймеров. 
// infinite_num возвращает количество бесконечных таймеров
int timerman_num(struct TimerMan *tm, int *infinite_num);
// Удалить бесконечные таймеры
void timerman_clear_infinite(struct TimerMan *tm);
