// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include <stdlib.h>
#include <stdbool.h>

// Размер поля
#define T_FIELD_SIZE 6
// Размер массива строк описывающих тест 
#define T_X_SIZE \
    T_FIELD_SIZE * T_FIELD_SIZE * 2 + 1 + 1

typedef struct TestSet {
    // Был ли произведен ввод?
    bool input_done, assert_done;
    // Индекс в массиве x, где находится курсор считывающий команды в данный
    // момент
    int idx;
    const char *description;
    // для imgui
    bool selected;
    // Входные данные
    char *x[T_X_SIZE];
} TestSet;

