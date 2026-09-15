#ifndef PS1_CLIPPING_H
#define PS1_CLIPPING_H

#include "core/ps1_math.h"
#include "graphics/ps1_triangle.h"

void ps1_clip_triangle(const Ps1ClipVert* a, const Ps1ClipVert* b,
                       const Ps1ClipVert* c, fixed_t near_z,
                       Ps1ClipVert* out_poly, u8* out_count);

#endif