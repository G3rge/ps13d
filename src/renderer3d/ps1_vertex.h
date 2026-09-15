#ifndef PS1_VERTEX_H
#define PS1_VERTEX_H

#include "core/ps1_math.h"

typedef struct {
    FVec3 pos;
    FVec3 normal;
    fixed_t u;
    fixed_t v;
} Ps1Vertex;

#endif