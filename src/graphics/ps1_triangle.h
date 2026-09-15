#ifndef PS1_TRIANGLE_H
#define PS1_TRIANGLE_H

#include "core/ps1_math.h"

typedef struct {
    FVec3 vp;
    fixed_t u;
    fixed_t v;
} Ps1ClipVert;

typedef struct {
    fixed_t sx;
    fixed_t sy;
    fixed_t z;
    fixed_t invz;
    fixed_t u;
    fixed_t v;
} Ps1ScreenVert;

#endif