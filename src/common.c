#include "common.h"

#include "raymath.h"

void camera_process(Camera2D *camera) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        camera->zoom = 1.;
        camera->offset = (Vector2) { 0., 0. };
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        Vector2 delta = GetMouseDelta();
        delta = Vector2Scale(delta, -1.0f/camera->zoom);

        camera->target = Vector2Add(camera->target, delta);
    }

    // Zoom based on mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        // Get the world point that is under the mouse
        Vector2 mp = GetMousePosition();
        Vector2 mouseWorldPos = GetScreenToWorld2D(mp, *camera);

        // Set the offset to where the mouse is
        camera->offset = GetMousePosition();

        // Set the target to match, so that the camera maps the world space point 
        // under the cursor to the screen space point under the cursor at any zoom
        camera->target = mouseWorldPos;

        // Zoom increment
        const float zoomIncrement = 0.125f;

        camera->zoom += (wheel*zoomIncrement);
        if (camera->zoom < zoomIncrement) camera->zoom = zoomIncrement;
    }
}

struct Setup modelview_setup = {
    .use_bonus = false,
    .field_size = 6,
    .tmr_block_time = 0.05,
    .tmr_put_time = 0.05,
    .use_gui = true,
    .auto_put = true,
    .win_value = 2048,
};

static void keyboard_swipe_cell(enum Direction *dir) {
#define N 2
    struct {
        enum Direction  dir;
        int             key[N];
    } keys_dirs[] = {
        {DIR_LEFT, { KEY_LEFT, KEY_H} },
        {DIR_RIGHT, { KEY_RIGHT, KEY_L} },
        {DIR_UP, { KEY_UP, KEY_J} },
        {DIR_DOWN, { KEY_DOWN, KEY_K} },
    };

    size_t dirs_num = sizeof(keys_dirs) / sizeof(keys_dirs[0]);
    for (int j = 0; j < dirs_num; ++j) {
        for (int k = 0; k < N; k++) {
            if (IsKeyPressed(keys_dirs[j].key[k])) {
                *dir = keys_dirs[j].dir;
                break;
            }
        }
    } 
#undef N
}

void input(ModelView *mv) {
    // Закончилась-ли анимация?
    if (mv->state != MVS_READY)
        return;

    enum Direction dir = {0};

    //mouse_swipe_cell(&dir);
    keyboard_swipe_cell(&dir);

    modelview_input(mv, dir);
}

