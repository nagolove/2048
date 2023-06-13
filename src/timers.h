#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct TimerMan TimerMan;

struct Timer {
    struct TimerMan *tm; // Заполняется автоматически при вызове update()
    double          start_time; // GetTime()
    double          duration;   // in seconds
    double          amount;     // 0..1
    //bool            expired;
    size_t          id, sz;
    void            *data;      // всегда динамически выделяемая память
    // возвращает истину для удаления таймера
    bool            (*on_update)(struct Timer *tmr); 
    void            (*on_stop)(struct Timer *tmr);
};

struct TimerDef {
    void    *udata;    // источник для копирования данных
    size_t  sz;      // размер копируемых данных
    double  duration;
    // возвращает истину для удаления таймера
    bool    (*on_update)(struct Timer *tmr);
    void    (*on_stop)(struct Timer *tmr);
};

enum TimerManAction {
    TMA_NEXT,
    TMA_BREAK,
    TMA_REMOVE_NEXT,
    TMA_REMOVE_BREAK,
};

struct TimerMan *timerman_new(int cap);
void timerman_free(struct TimerMan *tm);

bool timerman_add(struct TimerMan *tm, struct TimerDef td);
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
//void timerman_clear_infinite(struct TimerMan *tm);
void timerman_each(
    struct TimerMan *tm, 
    enum TimerManAction (*iter)(struct Timer *tmr, void*),
    void *udata
);
