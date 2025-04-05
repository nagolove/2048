#pragma once

#include "modelview.h"
#include "raylib.h"

void camera_process(Camera2D *cam);
void input(ModelView *mv);
extern struct Setup modelview_setup;
