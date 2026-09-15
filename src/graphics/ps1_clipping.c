#include "core/ps1_fixed.h"
#include "graphics/ps1_clipping.h"

static Ps1ClipVert clip_intersect(const Ps1ClipVert* a, const Ps1ClipVert* b,
                                  fixed_t near_z) {
    Ps1ClipVert r;
    fixed_t dz = b->vp.z - a->vp.z;
    fixed_t t = fx_div(near_z - a->vp.z, dz);
    fixed_t dx = b->vp.x - a->vp.x;
    fixed_t dy = b->vp.y - a->vp.y;
    r.vp.x = a->vp.x + fx_mul(dx, t);
    r.vp.y = a->vp.y + fx_mul(dy, t);
    r.vp.z = near_z;
    r.u = a->u + fx_mul(b->u - a->u, t);
    r.v = a->v + fx_mul(b->v - a->v, t);
    return r;
}

void ps1_clip_triangle(const Ps1ClipVert* a, const Ps1ClipVert* b,
                       const Ps1ClipVert* c, fixed_t near_z,
                       Ps1ClipVert* out_poly, u8* out_count) {
    Ps1ClipVert buf[2][4];
    const Ps1ClipVert* in;
    Ps1ClipVert* out;
    u32 n = 3, m = 0, j;
    buf[0][0] = *a;
    buf[0][1] = *b;
    buf[0][2] = *c;
    in = buf[0];
    out = buf[1];

    for (j = 0; j < n; j++) {
        const Ps1ClipVert* cv = &in[j];
        const Ps1ClipVert* nv = &in[(j + 1) % n];
        if (cv->vp.z >= near_z) {
            if (nv->vp.z >= near_z) {
                out[m++] = *nv;
            } else {
                out[m++] = clip_intersect(cv, nv, near_z);
            }
        } else {
            if (nv->vp.z >= near_z) {
                out[m++] = clip_intersect(cv, nv, near_z);
                out[m++] = *nv;
            }
        }
    }

    *out_count = (u8)m;
    for (j = 0; j < m; j++) out_poly[j] = out[j];
}