#ifndef PS1_LIGHTING_H
#define PS1_LIGHTING_H

#include "core/ps1_math.h"

typedef struct {
    fixed_t ambient;
    FVec3 dir;
    u8 levels;
} Ps1Lighting;

void ps1_light_init(Ps1Lighting* l);
u8 ps1_light_shade(const Ps1Lighting* l, const FVec3* world_normal);

#endif