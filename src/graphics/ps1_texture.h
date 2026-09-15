#ifndef PS1_TEXTURE_H
#define PS1_TEXTURE_H

#include "core/ps1_fixed.h"
#include "core/ps1_types.h"
#include "graphics/ps1_color.h"

typedef struct {
    u16* pixels;
    u8 w;
    u8 h;
    u8 lw;
    u8 lh;
    u8 slot;
    u8 in_use;
} Ps1Texture;

s8 ps1_tex_create(u8 w, u8 h, s8 slot);
void ps1_tex_set_pixel(s8 slot, u8 x, u8 y, rgb555_t c);
const Ps1Texture* ps1_tex_get(s8 slot);
rgb555_t ps1_tex_sample(s8 slot, fixed_t u, fixed_t v);
u32 ps1_tex_used_bytes(void);

/* Frees every captured texture so a fresh scene can rebuild the pool. */
void ps1_tex_reset(void);

#endif