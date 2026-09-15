#include <SDL.h>
#include <SDL_image.h>

#include "graphics/ps1_color.h"
#include "graphics/ps1_texture.h"
#include "sys/ps1_png.h"

s8 ps1_png_load_texture(const char* path, s8 slot) {
    SDL_Surface* img;
    SDL_Surface* conv = NULL;
    u32 w, h, pw, ph;
    u32 x, y;
    s8 out = -1;

    if (!path || !path[0]) return -1;
    img = IMG_Load(path);
    if (!img) return -1;
    conv = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(img);
    if (!conv) return -1;

    w = (u32)conv->w;
    h = (u32)conv->h;
    if (w == 0 || h == 0) goto done;

    pw = 16;
    ph = 16;
    while (pw * 2 <= w && pw * 2 <= 128) pw *= 2;
    while (ph * 2 <= h && ph * 2 <= 128) ph *= 2;

    out = ps1_tex_create((u8)pw, (u8)ph, slot);
    if (out < 0) goto done;
    if (!ps1_tex_get(out)) {
        out = -1;
        goto done;
    }

    for (y = 0; y < ph; y++) {
        u32 y0 = y * h / ph;
        u32 y1 = (y + 1) * h / ph;
        for (x = 0; x < pw; x++) {
            u32 x0 = x * w / pw;
            u32 x1 = (x + 1) * w / pw;
            u32 r = 0, g = 0, b = 0, n = 0;
            u32 sy, sx;
            for (sy = y0; sy < y1; sy++) {
                const Uint8* row =
                    (const Uint8*)conv->pixels + (size_t)sy * conv->pitch;
                for (sx = x0; sx < x1; sx++) {
                    Uint32 px = *((const Uint32*)(row + (size_t)sx * 4));
                    Uint8 rr, gg, bb, aa;
                    SDL_GetRGBA(px, conv->format, &rr, &gg, &bb, &aa);
                    r += rr;
                    g += gg;
                    b += bb;
                    n++;
                }
            }
            if (n) {
                r /= n;
                g /= n;
                b /= n;
            }
            ps1_tex_set_pixel(
                out, (u8)x, (u8)y,
                ps1_color_pack(ps1_color_quantize5((u8)r),
                               ps1_color_quantize5((u8)g),
                               ps1_color_quantize5((u8)b)));
        }
    }

done:
    SDL_FreeSurface(conv);
    return out;
}