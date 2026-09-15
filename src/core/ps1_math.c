#include "core/ps1_math.h"
#include "core/ps1_lut.h"

u32 ps1_isqrt64(u64 x) {
    u64 res = 0;
    u64 bit = 1ULL << 62;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (u32)res;
}

fixed_t fx_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    return (fixed_t)ps1_isqrt64((u64)(u32)x << FX_SHIFT);
}

FVec3 fv3(fixed_t x, fixed_t y, fixed_t z) {
    FVec3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

FVec3 fv3_add(const FVec3* a, const FVec3* b) {
    return fv3(a->x + b->x, a->y + b->y, a->z + b->z);
}

FVec3 fv3_sub(const FVec3* a, const FVec3* b) {
    return fv3(a->x - b->x, a->y - b->y, a->z - b->z);
}

FVec3 fv3_scale(const FVec3* a, fixed_t s) {
    return fv3(fx_mul(a->x, s), fx_mul(a->y, s), fx_mul(a->z, s));
}

FVec3 fv3_neg(const FVec3* a) {
    return fv3(-a->x, -a->y, -a->z);
}

fixed_t fv3_dot(const FVec3* a, const FVec3* b) {
    return fx_mul(a->x, b->x) + fx_mul(a->y, b->y) + fx_mul(a->z, b->z);
}

FVec3 fv3_cross(const FVec3* a, const FVec3* b) {
    FVec3 r;
    r.x = fx_mul(a->y, b->z) - fx_mul(a->z, b->y);
    r.y = fx_mul(a->z, b->x) - fx_mul(a->x, b->z);
    r.z = fx_mul(a->x, b->y) - fx_mul(a->y, b->x);
    return r;
}

fixed_t fv3_len(const FVec3* a) {
    return fx_sqrt(fv3_dot(a, a));
}

FVec3 fv3_normalize(const FVec3* a) {
    fixed_t len = fv3_len(a);
    FVec3 r = *a;
    if (len > 0) {
        r.x = fx_div(r.x, len);
        r.y = fx_div(r.y, len);
        r.z = fx_div(r.z, len);
    }
    return r;
}

void ps1_mat4_identity(Ps1Mat4* m) {
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            m->m[r][c] = (r == c) ? FX_ONE : 0;
}

void ps1_mat4_mul(const Ps1Mat4* a, const Ps1Mat4* b, Ps1Mat4* out) {
    Ps1Mat4 t;
    int r, c, k;
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            s64 sum = 0;
            for (k = 0; k < 4; k++)
                sum += (s64)a->m[r][k] * (s64)b->m[k][c];
            t.m[r][c] = (fixed_t)(sum >> FX_SHIFT);
        }
    }
    *out = t;
}

void ps1_mat4_transform(const Ps1Mat4* m, const FVec3* v, FVec3* out) {
    FVec3 t;
    t.x = (fixed_t)(((s64)m->m[0][0] * v->x + (s64)m->m[0][1] * v->y +
                     (s64)m->m[0][2] * v->z) >> FX_SHIFT) + m->m[0][3];
    t.y = (fixed_t)(((s64)m->m[1][0] * v->x + (s64)m->m[1][1] * v->y +
                     (s64)m->m[1][2] * v->z) >> FX_SHIFT) + m->m[1][3];
    t.z = (fixed_t)(((s64)m->m[2][0] * v->x + (s64)m->m[2][1] * v->y +
                     (s64)m->m[2][2] * v->z) >> FX_SHIFT) + m->m[2][3];
    *out = t;
}

void ps1_mat4_direction(const Ps1Mat4* m, const FVec3* v, FVec3* out) {
    FVec3 t;
    t.x = (fixed_t)(((s64)m->m[0][0] * v->x + (s64)m->m[0][1] * v->y +
                     (s64)m->m[0][2] * v->z) >> FX_SHIFT);
    t.y = (fixed_t)(((s64)m->m[1][0] * v->x + (s64)m->m[1][1] * v->y +
                     (s64)m->m[1][2] * v->z) >> FX_SHIFT);
    t.z = (fixed_t)(((s64)m->m[2][0] * v->x + (s64)m->m[2][1] * v->y +
                     (s64)m->m[2][2] * v->z) >> FX_SHIFT);
    *out = t;
}

void ps1_mat4_translate(const FVec3* t, Ps1Mat4* out) {
    ps1_mat4_identity(out);
    out->m[0][3] = t->x;
    out->m[1][3] = t->y;
    out->m[2][3] = t->z;
}

void ps1_mat4_rot_x(fixed_t a, Ps1Mat4* out) {
    fixed_t c = ps1_cos(a), s = ps1_sin(a);
    ps1_mat4_identity(out);
    out->m[1][1] = c;  out->m[1][2] = -s;
    out->m[2][1] = s;  out->m[2][2] = c;
}

void ps1_mat4_rot_y(fixed_t a, Ps1Mat4* out) {
    fixed_t c = ps1_cos(a), s = ps1_sin(a);
    ps1_mat4_identity(out);
    out->m[0][0] = c;  out->m[0][2] = s;
    out->m[2][0] = -s; out->m[2][2] = c;
}

void ps1_mat4_rot_z(fixed_t a, Ps1Mat4* out) {
    fixed_t c = ps1_cos(a), s = ps1_sin(a);
    ps1_mat4_identity(out);
    out->m[0][0] = c;  out->m[0][1] = -s;
    out->m[1][0] = s;  out->m[1][1] = c;
}

void ps1_mat4_scale3(const FVec3* s, Ps1Mat4* out) {
    ps1_mat4_identity(out);
    out->m[0][0] = s->x;
    out->m[1][1] = s->y;
    out->m[2][2] = s->z;
}

void ps1_matrix_model(const FVec3* pos, const FVec3* rot, const FVec3* scl,
                      Ps1Mat4* out, Ps1Mat4* normal) {
    Ps1Mat4 ry, rx, rz, r, rs, tr;
    ps1_mat4_rot_y(rot->y, &ry);
    ps1_mat4_rot_x(rot->x, &rx);
    ps1_mat4_rot_z(rot->z, &rz);
    ps1_mat4_mul(&ry, &rx, &r);
    ps1_mat4_mul(&r, &rz, &r);
    ps1_mat4_scale3(scl, &rs);
    ps1_mat4_mul(&r, &rs, &rs);
    ps1_mat4_translate(pos, &tr);
    ps1_mat4_mul(&tr, &rs, out);
    *normal = rs;
}