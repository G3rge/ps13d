#include "graphics/ps1_rasterizer.h"
#include "graphics/ps1_color.h"

typedef struct {
    s64 a;
    s64 b;
    s64 c;
} Edge;

typedef struct {
    s64 nx;
    s64 ny;
    s64 denom;
} Grad;

/* Signed integer division rounded to the nearest integer. */
static s64 grad_round(s64 num, s64 den) {
    s64 q, r, rr;
    if (den == 0) return 0;
    if (den < 0) {
        num = -num;
        den = -den;
    }
    q = num / den;
    r = num % den;
    rr = (r < 0) ? -r : r;
    if (rr * 2 >= den) q += (r < 0) ? -1 : 1;
    return q;
}

static void grad_init(Grad* g, fixed_t a0, fixed_t a1, fixed_t a2,
                      s64 dx01, s64 dy01, s64 dx02, s64 dy02, s64 denom) {
    g->nx = ((s64)a1 - a0) * dy02 - ((s64)a2 - a0) * dy01;
    g->ny = ((s64)a2 - a0) * dx01 - ((s64)a1 - a0) * dx02;
    g->denom = denom;
}

static s64 grad_incx(const Grad* g) {
    return grad_round(g->nx << PS1_SCR_SHIFT, g->denom);
}

/* Attribute value at screen position (px, py), both in PS1_SCR_SHIFT units.
 * The interpolation plane is anchored at vertex 0 (screen position ox, oy):
 * A(X,Y) = A0 + px*(X - ox) + py*(Y - oy). Without the origin bias the
 * texture would depend on absolute screen coordinates instead of the
 * triangle. */
static s64 grad_value(const Grad* g, fixed_t a0, s64 px, s64 py, s64 ox,
                      s64 oy) {
    return (s64)a0 + grad_round(g->nx * (px - ox), g->denom) +
           grad_round(g->ny * (py - oy), g->denom);
}

/* PS1-style affine texture fetch: u and v are interpolated linearly in
 * screen space (no perspective correction) and snapped to a coarse sub-texel
 * grid, so textures warp and swim across the polygon like on real PS1. */
static inline fixed_t tex_quant(fixed_t uv) {
    fixed_t mask = (fixed_t)((1u << PS1_UV_CLEAR) - 1u);
    return uv & ~mask;
}

u32 ps1_raster_triangle(Ps1Framebuffer* fb, const Ps1RasterParams* p,
                        const Ps1ScreenVert* a, const Ps1ScreenVert* b,
                        const Ps1ScreenVert* c) {
    const Ps1ScreenVert* v[3];
    Edge e[3];
    s64 dx01, dy01, dx02, dy02, denom;
    s64 area2;
    s64 s;
    fixed_t mix, maxx, miy, maxy;
    int x0, x1, y0, y1, x, y;
    u32 pixels = 0;
    u8 need_depth = (p->depth != NULL);
    u8 need_tex = (p->tex != NULL);
    u8 need_attrs = (need_depth || need_tex);
    Grad gz = {0}, gu = {0}, gv = {0};
    s64 iz_acc = 0, u_acc = 0, v_acc = 0;
    s64 iz_incx = 0, u_incx = 0, v_incx = 0;
    rgb555_t cc;

    v[0] = a; v[1] = b; v[2] = c;

    dx01 = (s64)v[1]->sx - (s64)v[0]->sx;
    dy01 = (s64)v[1]->sy - (s64)v[0]->sy;
    dx02 = (s64)v[2]->sx - (s64)v[0]->sx;
    dy02 = (s64)v[2]->sy - (s64)v[0]->sy;
    area2 = dx01 * dy02 - dx02 * dy01;
    if (area2 == 0) return 0;
    s = (area2 < 0) ? -1 : 1;
    denom = area2;

    {
        int i;
        for (i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            e[i].a = (s64)v[i]->sy - (s64)v[j]->sy;
            e[i].b = (s64)v[j]->sx - (s64)v[i]->sx;
            e[i].c = (s64)v[i]->sx * (s64)v[j]->sy -
                     (s64)v[j]->sx * (s64)v[i]->sy;
        }
    }

    mix = fx_min(fx_min(v[0]->sx, v[1]->sx), v[2]->sx);
    maxx = fx_max(fx_max(v[0]->sx, v[1]->sx), v[2]->sx);
    miy = fx_min(fx_min(v[0]->sy, v[1]->sy), v[2]->sy);
    maxy = fx_max(fx_max(v[0]->sy, v[1]->sy), v[2]->sy);

    x0 = (int)(mix >> PS1_SCR_SHIFT);
    x1 = (int)((maxx + ((1 << PS1_SCR_SHIFT) - 1)) >> PS1_SCR_SHIFT);
    y0 = (int)(miy >> PS1_SCR_SHIFT);
    y1 = (int)((maxy + ((1 << PS1_SCR_SHIFT) - 1)) >> PS1_SCR_SHIFT);

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PS1_SCREEN_W - 1) x1 = PS1_SCREEN_W - 1;
    if (y1 > PS1_SCREEN_H - 1) y1 = PS1_SCREEN_H - 1;
    if (x0 > x1 || y0 > y1) return 0;

    if (need_attrs) {
        if (need_depth) {
            grad_init(&gz, v[0]->invz, v[1]->invz, v[2]->invz, dx01, dy01,
                      dx02, dy02, denom);
            iz_incx = grad_incx(&gz);
        }
        if (need_tex) {
            grad_init(&gu, v[0]->u, v[1]->u, v[2]->u, dx01, dy01, dx02, dy02,
                      denom);
            grad_init(&gv, v[0]->v, v[1]->v, v[2]->v, dx01, dy01, dx02, dy02,
                      denom);
            u_incx = grad_incx(&gu);
            v_incx = grad_incx(&gv);
        }
    }

    for (y = y0; y <= y1; y++) {
        s64 fty = (s64)y << PS1_SCR_SHIFT;
        s64 fx0 = (s64)x0 << PS1_SCR_SHIFT;
        s64 e0 = e[0].a * fx0 + e[0].b * fty + e[0].c;
        s64 e1 = e[1].a * fx0 + e[1].b * fty + e[1].c;
        s64 e2 = e[2].a * fx0 + e[2].b * fty + e[2].c;
        s32 row = y * PS1_SCREEN_W;

        if (need_attrs) {
            if (need_depth)
                iz_acc =
                    grad_value(&gz, v[0]->invz, fx0, fty, v[0]->sx, v[0]->sy);
            if (need_tex) {
                u_acc = grad_value(&gu, v[0]->u, fx0, fty, v[0]->sx, v[0]->sy);
                v_acc = grad_value(&gv, v[0]->v, fx0, fty, v[0]->sx, v[0]->sy);
            }
        }

    for (x = x0; x <= x1; x++) {
        if ((s * e0) >= 0 && (s * e1) >= 0 && (s * e2) >= 0) {
                u32 idx = (u32)row + (u32)x;
                if (need_depth) {
                    u16 enc = (u16)((iz_acc >> PS1_DEPTH_SHIFT));
                    if (enc > p->depth->data[idx]) {
                        p->depth->data[idx] = enc;
                        if (need_tex) {
                            cc = ps1_color_shade(
                                ps1_tex_sample(p->tex->slot,
                                               tex_quant((fixed_t)u_acc),
                                               tex_quant((fixed_t)v_acc)),
                                p->shade_level);
                        } else {
                            cc = p->color;
                        }
                        fb->px[idx] = cc;
                        pixels++;
                    }
                } else {
                    if (need_tex) {
                        cc = ps1_color_shade(
                            ps1_tex_sample(p->tex->slot,
                                           tex_quant((fixed_t)u_acc),
                                           tex_quant((fixed_t)v_acc)),
                            p->shade_level);
                    } else {
                        cc = p->color;
                    }
                    fb->px[idx] = cc;
                    pixels++;
                }
            }
            e0 += e[0].a << PS1_SCR_SHIFT;
            e1 += e[1].a << PS1_SCR_SHIFT;
            e2 += e[2].a << PS1_SCR_SHIFT;
            if (need_attrs) {
                if (need_depth) iz_acc += iz_incx;
                if (need_tex) {
                    u_acc += u_incx;
                    v_acc += v_incx;
                }
            }
        }
    }
    return pixels;
}