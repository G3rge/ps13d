#ifndef PS1_DEPTH_H
#define PS1_DEPTH_H

#include "core/ps1_fixed.h"
#include "graphics/ps1_framebuffer.h"

#define PS1_DEPTH_SHIFT ((PS1_FIXED_SHIFT > 12) ? (PS1_FIXED_SHIFT - 12) : 0)

typedef struct {
    u16 data[PS1_SCREEN_W * PS1_SCREEN_H];
} Ps1Depth16;

void ps1_depth_clear(Ps1Depth16* d);

#endif