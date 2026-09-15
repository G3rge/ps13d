#ifndef PS1_TRANSFORM_H
#define PS1_TRANSFORM_H

#include "core/ps1_math.h"

typedef struct {
    FVec3 position;
    FVec3 rotation;
    FVec3 scale;
} Ps1Transform;

void ps1_transform_model(const Ps1Transform* t, Ps1Mat4* model,
                         Ps1Mat4* normal);

#endif