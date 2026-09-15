#ifndef PS1_RASTERIZER_H
#define PS1_RASTERIZER_H

#include "core/ps1_math.h"
#include "graphics/ps1_depth.h"
#include "graphics/ps1_framebuffer.h"
#include "graphics/ps1_texture.h"
#include "graphics/ps1_triangle.h"

typedef struct {
    Ps1Depth16* depth;
    const Ps1Texture* tex;
    rgb555_t color;
    u8 shade_level;
} Ps1RasterParams;

u32 ps1_raster_triangle(Ps1Framebuffer* fb, const Ps1RasterParams* p,
                        const Ps1ScreenVert* a, const Ps1ScreenVert* b,
                        const Ps1ScreenVert* c);

#endif