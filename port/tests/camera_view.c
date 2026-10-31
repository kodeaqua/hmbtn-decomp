#include "model.h"
#include "game_types.h"
#include <inline_c.h>
#include <stdio.h>

_Static_assert(sizeof(((CameraWork *)0)->flag) == sizeof(s32), "CameraWork GTE flag must be 32-bit");

int main(void) {
    MATRIX view = {0};
    InitGeom();
    camera_set_eye_roll(0, 0, 0, 0);
    camera_set_target(0, 0, 4096);
    model_load_lighting();
    gte_ReadRotMatrix(&view);
    if (view.m[0][0] != 4096 || view.m[1][1] != 4096 || view.m[2][2] != 4096 ||
        view.m[0][1] || view.m[0][2] || view.m[1][0] || view.m[1][2] ||
        view.m[2][0] || view.m[2][1]) {
        fprintf(stderr, "Camera view basis is incorrect\n");
        return 1;
    }
    return 0;
}
