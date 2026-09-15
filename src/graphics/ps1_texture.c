#include <string.h>

#include "graphics/ps1_texture.h"

#define PS1_TEX_POOL_SIZE (PS1_MAX_TEX_PIXELS * (u32)sizeof(u16))

static u16 g_tex_pixels[PS1_MAX_TEX_PIXELS];
static Ps1Texture g_slots[PS1_MAX_TEXTURES];
static u32 g_used_pixels;

static int g_lp2(u8 v) {
    int i = 0;
    while ((1 << i) < v) i++;
    return i;
}

s8 ps1_tex_create(u8 w, u8 h, s8 slot) {
    u32 needed = (u32)w * h;
    if (slot < 0 || slot >= PS1_MAX_TEXTURES) return -1;
    if (needed == 0 || needed > PS1_MAX_TEX_PIXELS - g_used_pixels) return -1;
    g_slots[slot].w = w;
    g_slots[slot].h = h;
    g_slots[slot].lw = (u8)g_lp2(w);
    g_slots[slot].lh = (u8)g_lp2(h);
    g_slots[slot].slot = (u8)slot;
    g_slots[slot].in_use = 1;
    g_slots[slot].pixels = g_tex_pixels + g_used_pixels;
    g_used_pixels += needed;
    return slot;
}

void ps1_tex_set_pixel(s8 slot, u8 x, u8 y, rgb555_t c) {
    if (slot < 0 || slot >= PS1_MAX_TEXTURES) return;
    if (!g_slots[slot].in_use) return;
    g_slots[slot].pixels[(u32)y * g_slots[slot].w + x] = c;
}

const Ps1Texture* ps1_tex_get(s8 slot) {
    if (slot < 0 || slot >= PS1_MAX_TEXTURES) return NULL;
    if (!g_slots[slot].in_use) return NULL;
    return &g_slots[slot];
}

rgb555_t ps1_tex_sample(s8 slot, fixed_t u, fixed_t v) {
    const Ps1Texture* t = ps1_tex_get(slot);
    u32 xu, yv;
    if (!t) return 0;
    /* UVs are stored in "tile units": one full texture = 1 << PS1_SCR_SHIFT.
     * Scale to the real texture size (which can be any power of two up to
     * 128) so a [0,1] UV always maps across the whole image regardless of
     * the texture's dimensions. */
    xu = (u32)((((s64)u * t->w) >> PS1_SCR_SHIFT) & (t->w - 1));
    yv = (u32)((((s64)v * t->h) >> PS1_SCR_SHIFT) & (t->h - 1));
    return t->pixels[yv * (u32)t->w + xu];
}

u32 ps1_tex_used_bytes(void) {
    return g_used_pixels * (u32)sizeof(u16);
}

void ps1_tex_reset(void) {
    g_used_pixels = 0;
    memset(g_slots, 0, sizeof g_slots);
}