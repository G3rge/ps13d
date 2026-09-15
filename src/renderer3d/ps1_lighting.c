#include "renderer3d/ps1_lighting.h"

void ps1_light_init(Ps1Lighting* l) {
    FVec3 d = fv3(fx_div(fx_int(1), fx_int(2)),
                   fx_div(fx_int(-1), fx_int(2)),
                   fx_div(fx_int(3), fx_int(4)));
    l->dir = fv3_normalize(&d);
    l->ambient = fx_div(fx_int(22), fx_int(100));

    l->levels = 8;
}

u8 ps1_light_shade(const Ps1Lighting* l, const FVec3* world_normal) {
    fixed_t ndl = fv3_dot(world_normal, &l->dir);
    fixed_t lum;
    s32 level;
    if (ndl < 0) ndl = 0;
    lum = ndl + l->ambient;
    if (lum > FX_ONE) lum = FX_ONE;
    level = ps1_int(fx_mul(lum, fx_int(l->levels)));
    if (level < 0) level = 0;
    if (level > (s32)(l->levels - 1)) level = (s32)(l->levels - 1);
    return (u8)level;
}