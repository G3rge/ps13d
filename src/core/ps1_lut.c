#include <math.h>

#include "core/ps1_lut.h"

static fixed_t g_sin_lut[PS1_LUT_ANG];
static fixed_t g_cos_lut[PS1_LUT_ANG];
static u16 g_recip_lut[PS1_LUT_RECIP];
static fixed_t g_ang_scale;

static int aux_clz32(u32 x) {
    int c = 0;
    while ((x & 0x80000000u) == 0) { c++; x <<= 1; if (c >= 32) break; }
    return c;
}

void ps1_lut_init(void) {
    int i;
    for (i = 0; i < PS1_LUT_ANG; i++) {
        float a = (float)i * 6.2831853f / (float)PS1_LUT_ANG;
        g_sin_lut[i] = (fixed_t)((float)FX_ONE * sinf(a));
        g_cos_lut[i] = (fixed_t)((float)FX_ONE * cosf(a));
    }
    for (i = 0; i < PS1_LUT_RECIP; i++) {
        u64 inv = (1u << 23) / (u64)(128 + i);
        if (inv > 65535ull) inv = 65535ull;
        g_recip_lut[i] = (u16)inv;
    }
    g_ang_scale = (fixed_t)((float)FX_ONE * ((float)PS1_LUT_ANG / 6.2831853f));
}

fixed_t ps1_sin(fixed_t angle) {
    int idx = (int)ps1_int(fx_mul(angle, g_ang_scale));
    return g_sin_lut[idx & (PS1_LUT_ANG - 1)];
}

fixed_t ps1_cos(fixed_t angle) {
    int idx = (int)ps1_int(fx_mul(angle, g_ang_scale));
    return g_cos_lut[idx & (PS1_LUT_ANG - 1)];
}

fixed_t ps1_fast_recip(fixed_t z) {
    int n;
    u32 mant;
    s64 r;
    s32 e;
    if (z <= 0) return (fixed_t)0x7FFFFFFF;
    {
        u32 u = (u32)z;
        n = 31 - aux_clz32(u);
        if (n < 7) mant = (u << (7 - n)) & 0xFFu;
        else mant = (u >> (n - 7)) & 0xFFu;
    }
    r = (s64)g_recip_lut[(int)mant - 128];
    e = 2 * FX_SHIFT - 16 - n;
    if (e >= 0) r = r << e;
    else r = r >> (-e);
    if (r > 0x7FFFFFFF) r = 0x7FFFFFFF;
    {
        fixed_t guess = (fixed_t)r;
        fixed_t t = fx_int(2) - fx_mul(z, guess);
        guess = fx_mul(guess, t);
        return guess;
    }
}