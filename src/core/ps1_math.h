#ifndef PS1_MATH_H
#define PS1_MATH_H

#include "core/ps1_fixed.h"

typedef struct { fixed_t x, y; } FVec2;
typedef struct { fixed_t x, y, z; } FVec3;
typedef struct { fixed_t m[4][4]; } Ps1Mat4;

u32 ps1_isqrt64(u64 x);
fixed_t fx_sqrt(fixed_t x);

FVec3 fv3(fixed_t x, fixed_t y, fixed_t z);
FVec3 fv3_add(const FVec3* a, const FVec3* b);
FVec3 fv3_sub(const FVec3* a, const FVec3* b);
FVec3 fv3_scale(const FVec3* a, fixed_t s);
FVec3 fv3_neg(const FVec3* a);
fixed_t fv3_dot(const FVec3* a, const FVec3* b);
FVec3 fv3_cross(const FVec3* a, const FVec3* b);
fixed_t fv3_len(const FVec3* a);
FVec3 fv3_normalize(const FVec3* a);

void ps1_mat4_identity(Ps1Mat4* m);
void ps1_mat4_mul(const Ps1Mat4* a, const Ps1Mat4* b, Ps1Mat4* out);
void ps1_mat4_transform(const Ps1Mat4* m, const FVec3* v, FVec3* out);
void ps1_mat4_direction(const Ps1Mat4* m, const FVec3* v, FVec3* out);
void ps1_mat4_translate(const FVec3* t, Ps1Mat4* out);
void ps1_mat4_rot_x(fixed_t a, Ps1Mat4* out);
void ps1_mat4_rot_y(fixed_t a, Ps1Mat4* out);
void ps1_mat4_rot_z(fixed_t a, Ps1Mat4* out);
void ps1_mat4_scale3(const FVec3* s, Ps1Mat4* out);
void ps1_matrix_model(const FVec3* pos, const FVec3* rot, const FVec3* scl,
                      Ps1Mat4* out, Ps1Mat4* normal);

#endif