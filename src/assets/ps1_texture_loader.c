#include "assets/ps1_texture_loader.h"
#include "graphics/ps1_texture.h"

s8 ps1_tex_load_checker(u8 w, u8 h, s8 slot, rgb555_t c0, rgb555_t c1) {
    s8 s = -1;
    u32 y, x;
    if ((w & (w - 1)) != 0 || (h & (h - 1)) != 0) return -1;
    if (w < 8 || w > 128 || h < 8 || h > 128) return -1;
    s = ps1_tex_create(w, h, slot);
    if (s < 0) return -1;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            ps1_tex_set_pixel(s, (u8)x, (u8)y,
                              (((x ^ y) & 1) ? c1 : c0));
        }
    }
    return s;
}