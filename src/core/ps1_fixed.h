#ifndef PS1_FIXED_H
#define PS1_FIXED_H

#include "core/ps1_types.h"

typedef s32 fixed_t;

#define FX_SHIFT PS1_FIXED_SHIFT
#define FX_ONE ((fixed_t)1 << FX_SHIFT)
#define FX_FRAC_MASK (FX_ONE - 1)

static inline fixed_t fx_int(s32 v) { return (fixed_t)(v << FX_SHIFT); }
static inline s32 ps1_int(fixed_t v) { return v >> FX_SHIFT; }
static inline fixed_t fx_f32(float f) { return (fixed_t)(f * (float)FX_ONE); }
static inline float ps1_f32(fixed_t v) { return (float)v / (float)FX_ONE; }

static inline fixed_t fx_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((s64)a * (s64)b) >> FX_SHIFT);
}

static inline fixed_t fx_div(fixed_t a, fixed_t b) {
    if (b == 0) return (a < 0) ? (fixed_t)0x80000000 : (fixed_t)0x7FFFFFFF;
    return (fixed_t)(((s64)a << FX_SHIFT) / (s64)b);
}

static inline fixed_t fx_min(fixed_t a, fixed_t b) { return (a < b) ? a : b; }
static inline fixed_t fx_max(fixed_t a, fixed_t b) { return (a > b) ? a : b; }
static inline fixed_t fx_abs(fixed_t a) { return (a < 0) ? (fixed_t)(-a) : a; }

static inline fixed_t fx_clamp(fixed_t v, fixed_t lo, fixed_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#define ps1_scr_f(v) ((fixed_t)((s32)(v) << PS1_SCR_SHIFT))
#define ps1_scr_f32(f) ((fixed_t)((f) * (float)(1 << PS1_SCR_SHIFT)))
#define ps1_scr_int(x) ((int)((x) >> PS1_SCR_SHIFT))
#define ps1_uv_f(v) ((fixed_t)((s32)(v) << PS1_SCR_SHIFT))
#define ps1_uv_int(x) ((int)((x) >> PS1_SCR_SHIFT))

#endif