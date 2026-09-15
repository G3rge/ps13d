#include "graphics/ps1_color.h"

#define PS1_SHADE_LEVELS 8

static u8 g_shade_lut[PS1_SHADE_LEVELS][32];

void ps1_color_lut_init(void) {
    int l, c;
    for (l = 0; l < PS1_SHADE_LEVELS; l++) {
        for (c = 0; c < 32; c++) {
            g_shade_lut[l][c] = (u8)((c * l) / (PS1_SHADE_LEVELS - 1));
        }
    }
}

rgb555_t ps1_color_pack(u8 r, u8 g, u8 b) {
    return (rgb555_t)(((r & 31) << 10) | ((g & 31) << 5) | (b & 31));
}

void ps1_color_unpack(rgb555_t c, u8* r, u8* g, u8* b) {
    *r = (u8)((c >> 10) & 31);
    *g = (u8)((c >> 5) & 31);
    *b = (u8)(c & 31);
}

u8 ps1_color_quantize5(u8 v8) {
    return (u8)((v8 * 31u + 127u) / 255u);
}

rgb555_t ps1_color_shade(rgb555_t color, u8 level) {
    u8 r, g, b;
    if (level >= PS1_SHADE_LEVELS) level = PS1_SHADE_LEVELS - 1;
    ps1_color_unpack(color, &r, &g, &b);
    return ps1_color_pack(g_shade_lut[level][r], g_shade_lut[level][g],
                          g_shade_lut[level][b]);
}

u32 ps1_color_to888(rgb555_t c) {
    u8 r, g, b;
    ps1_color_unpack(c, &r, &g, &b);
    r = (u8)((r * 255u) / 31u);
    g = (u8)((g * 255u) / 31u);
    b = (u8)((b * 255u) / 31u);
    return ((u32)r << 16) | ((u32)g << 8) | b;
}