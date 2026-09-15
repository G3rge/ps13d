#ifndef PS1_CAMERA_H
#define PS1_CAMERA_H

#include "core/ps1_math.h"

typedef struct {
    FVec3 pos;
    fixed_t yaw;
    fixed_t pitch;
    fixed_t focal;
    fixed_t near_z;
    fixed_t far_z;
    Ps1Mat4 view;
} Ps1Camera;

void ps1_cam_init(Ps1Camera* cam);
void ps1_cam_update(Ps1Camera* cam);

#endif