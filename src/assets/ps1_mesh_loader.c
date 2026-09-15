#include "assets/ps1_mesh_loader.h"
#include "core/ps1_lut.h"

/* PI and TAU as fixed point (355/113 approximation). */
#define PI_FX ((fixed_t)(((s64)FX_ONE * 355) / 113))
#define TAU_FX ((fixed_t)(((s64)FX_ONE * 710) / 113))

/* One UV tile covers the whole texture. UVs are stored in tile units at
 * PS1_SCR_SHIFT precision (see ps1_tex_sample). */
#define UV_TILE ((fixed_t)(1 << PS1_SCR_SHIFT))

typedef struct {
    FVec3 t1;
    FVec3 t2;
} CubeFace;

static const CubeFace FACES[6] = {
    { { 0, 0, FX_ONE }, { 0, (fixed_t)(-FX_ONE), 0 } },
    { { 0, (fixed_t)(-FX_ONE), 0 }, { 0, 0, FX_ONE } },
    { { 0, 0, FX_ONE }, { FX_ONE, 0, 0 } },
    { { FX_ONE, 0, 0 }, { 0, 0, FX_ONE } },
    { { FX_ONE, 0, 0 }, { 0, FX_ONE, 0 } },
    { { FX_ONE, 0, 0 }, { 0, (fixed_t)(-FX_ONE), 0 } },
};

static const fixed_t CORNER_S[4][2] = {
    { (fixed_t)(-FX_ONE), (fixed_t)(-FX_ONE) },
    { FX_ONE, (fixed_t)(-FX_ONE) },
    { FX_ONE, FX_ONE },
    { (fixed_t)(-FX_ONE), FX_ONE },
};

void ps1_mesh_build_cube(Ps1Mesh* m, fixed_t half) {
    int f;

    ps1_mesh_init(m);

    for (f = 0; f < 6; f++) {
        const CubeFace* face = &FACES[f];
        fixed_t u[4] = { 0, UV_TILE, UV_TILE, 0 };
        fixed_t v[4] = { 0, 0, UV_TILE, UV_TILE };
        int vi[4];
        int k;

        for (k = 0; k < 4; k++) {
            FVec3 cc;
            FVec3 n;
            fixed_t sx = CORNER_S[k][0];
            fixed_t sy = CORNER_S[k][1];
            n = fv3_cross(&face->t1, &face->t2);
            cc.x = fx_mul(n.x, half) +
                   fx_mul(fx_mul(face->t1.x, sx), half) +
                   fx_mul(fx_mul(face->t2.x, sy), half);
            cc.y = fx_mul(n.y, half) +
                   fx_mul(fx_mul(face->t1.y, sx), half) +
                   fx_mul(fx_mul(face->t2.y, sy), half);
            cc.z = fx_mul(n.z, half) +
                   fx_mul(fx_mul(face->t1.z, sx), half) +
                   fx_mul(fx_mul(face->t2.z, sy), half);
            vi[k] = ps1_mesh_add_vertex(m, cc, u[k], v[k]);
        }
        ps1_mesh_add_triangle(m, (u16)vi[0], (u16)vi[1], (u16)vi[2], (u16)f);
        ps1_mesh_add_triangle(m, (u16)vi[0], (u16)vi[2], (u16)vi[3], (u16)f);
    }
}

/* Adds a triangle, flipping the winding if the geometric normal points
 * against the heuristic outward direction (triangle centroid). Keeps CULL_BACK
 * from eating generated meshes. Material is always 0; callers re-assign. */
static void add_tri_oriented(Ps1Mesh* m, u16 a, u16 b, u16 c) {
    FVec3 pa = m->verts[a].pos;
    FVec3 pb = m->verts[b].pos;
    FVec3 pc = m->verts[c].pos;
    FVec3 e1 = fv3_sub(&pb, &pa);
    FVec3 e2 = fv3_sub(&pc, &pa);
    FVec3 n = fv3_cross(&e1, &e2);
    FVec3 centroid;
    u16 c2 = c;

    centroid.x = fx_div((s64)pa.x + (s64)pb.x + (s64)pc.x, 3);
    centroid.y = fx_div((s64)pa.y + (s64)pb.y + (s64)pc.y, 3);
    centroid.z = fx_div((s64)pa.z + (s64)pb.z + (s64)pc.z, 3);
    if (fv3_dot(&n, &centroid) < 0) {
        u16 tmp = b;
        b = c2;
        c2 = tmp;
    }
    ps1_mesh_add_triangle(m, a, b, c2, 0);
}

static void add_quad_oriented(Ps1Mesh* m, u16 v00, u16 v01, u16 v10, u16 v11) {
    add_tri_oriented(m, v00, v01, v11);
    add_tri_oriented(m, v00, v10, v11);
}

static fixed_t grid_u(u16 seg, u16 segs) {
    if (segs == 0) return 0;
    return UV_TILE * seg / segs;
}

static fixed_t grid_v(u16 ring, u16 rings) {
    if (rings == 0) return 0;
    return UV_TILE * ring / rings;
}

void ps1_mesh_build_sphere(Ps1Mesh* m, fixed_t radius, u16 segs, u16 rings) {
    u16 r, s;
    if (segs < 3) segs = 3;
    if (rings < 2) rings = 2;

    ps1_mesh_init(m);

    for (r = 0; r <= rings; r++) {
        fixed_t phi = fx_div(fx_int(rings - r), fx_int(rings));
        fixed_t y = fx_mul(radius, ps1_cos(fx_mul(phi, PI_FX)));
        fixed_t sr = fx_mul(radius, ps1_sin(fx_mul(phi, PI_FX)));
        for (s = 0; s <= segs; s++) {
            fixed_t th = fx_mul(fx_div(fx_int(s), fx_int(segs)), TAU_FX);
            FVec3 p = fv3(fx_mul(sr, ps1_cos(th)), y, fx_mul(sr, ps1_sin(th)));
            ps1_mesh_add_vertex_ex(m, p, p, grid_u(s, segs), grid_v(r, rings));
        }
    }

    for (r = 0; r < rings; r++) {
        for (s = 0; s < segs; s++) {
            u16 v00 = (u16)(r * (segs + 1) + s);
            u16 v01 = (u16)(v00 + 1);
            u16 v10 = (u16)(v00 + (segs + 1));
            u16 v11 = (u16)(v10 + 1);
            add_quad_oriented(m, v00, v01, v10, v11);
        }
    }
}

void ps1_mesh_build_plane(Ps1Mesh* m, fixed_t half, u16 segs) {
    u16 r, s;
    if (segs < 1) segs = 1;

    ps1_mesh_init(m);

    for (r = 0; r <= segs; r++) {
        fixed_t z = -half + fx_div(fx_mul(fx_int(2 * segs - 2 * r), half),
                                   fx_int(segs));
        for (s = 0; s <= segs; s++) {
            fixed_t x = -half + fx_div(fx_mul(fx_int(2 * s), half), fx_int(segs));
            ps1_mesh_add_vertex_ex(m, fv3(x, 0, z), fv3(0, FX_ONE, 0),
                                   grid_u(s, segs), grid_v(r, segs));
        }
    }

    for (r = 0; r < segs; r++) {
        for (s = 0; s < segs; s++) {
            u16 v00 = (u16)(r * (segs + 1) + s);
            u16 v01 = (u16)(v00 + 1);
            u16 v10 = (u16)(v00 + (segs + 1));
            u16 v11 = (u16)(v10 + 1);
            add_quad_oriented(m, v00, v01, v10, v11);
        }
    }
}

void ps1_mesh_build_cylinder(Ps1Mesh* m, fixed_t radius, fixed_t height,
                             u16 segs) {
    u16 s;
    fixed_t hy = fx_div(height, fx_int(2));
    int top_cap, bot_cap;
    if (segs < 3) segs = 3;

    ps1_mesh_init(m);

    for (s = 0; s <= segs; s++) {
        fixed_t th = fx_mul(fx_div(fx_int(s), fx_int(segs)), TAU_FX);
        fixed_t x = fx_mul(radius, ps1_cos(th));
        fixed_t z = fx_mul(radius, ps1_sin(th));
        ps1_mesh_add_vertex_ex(m, fv3(x, hy, z),
                               fv3(fx_div(x, radius), 0, fx_div(z, radius)),
                               grid_u(s, segs), grid_v(0, 1));
        ps1_mesh_add_vertex_ex(m, fv3(x, -hy, z),
                               fv3(fx_div(x, radius), 0, fx_div(z, radius)),
                               grid_u(s, segs), grid_v(1, 1));
    }
    for (s = 0; s < segs; s++) {
        u16 vi = (u16)(2 * s);
        add_quad_oriented(m, vi, (u16)(vi + 1), (u16)(vi + 2), (u16)(vi + 3));
    }

    top_cap = 2 * segs + 2;
    ps1_mesh_add_vertex_ex(m, fv3(0, hy, 0), fv3(0, FX_ONE, 0), 0, 0);
    bot_cap = top_cap + 1;
    ps1_mesh_add_vertex_ex(m, fv3(0, -hy, 0), fv3(0, (fixed_t)(-FX_ONE), 0), 0,
                           0);
    for (s = 0; s < segs; s++) {
        u16 a = (u16)(2 * s);
        u16 b = (u16)((2 * (s + 1)) % (2 * segs));
        add_tri_oriented(m, (u16)top_cap, b, a);
        add_tri_oriented(m, (u16)bot_cap, a, b);
    }
}

void ps1_mesh_build_capsule(Ps1Mesh* m, fixed_t radius, fixed_t height,
                            u16 segs, u16 rings) {
    u16 r, s;
    fixed_t total = height + fx_mul(radius, fx_int(2));
    if (segs < 3) segs = 3;
    if (rings < 4) rings = 4;

    ps1_mesh_init(m);

    for (r = 0; r <= rings; r++) {
        fixed_t sloc = fx_div(fx_mul(fx_int(rings - r), total), fx_int(rings));
        fixed_t rho;
        fixed_t yc;
        if (sloc < radius) {
            fixed_t dy = radius - sloc;
            rho = fx_sqrt(fx_mul(radius, radius) - fx_mul(dy, dy));
            yc = fx_div(height, fx_int(2)) + dy;
        } else if (sloc < height + radius) {
            rho = radius;
            yc = fx_div(height, fx_int(2)) - (sloc - radius);
        } else {
            fixed_t dy = sloc - (height + radius);
            rho = fx_sqrt(fx_mul(radius, radius) - fx_mul(dy, dy));
            yc = -fx_div(height, fx_int(2)) - dy;
        }
        for (s = 0; s <= segs; s++) {
            fixed_t th = fx_mul(fx_div(fx_int(s), fx_int(segs)), TAU_FX);
            FVec3 p = fv3(fx_mul(rho, ps1_cos(th)), yc,
                          fx_mul(rho, ps1_sin(th)));
            ps1_mesh_add_vertex_ex(m, p, fv3(0, 0, 0), grid_u(s, segs),
                                   grid_v(r, rings));
        }
    }

    for (r = 0; r < rings; r++) {
        for (s = 0; s < segs; s++) {
            u16 v00 = (u16)(r * (segs + 1) + s);
            u16 v01 = (u16)(v00 + 1);
            u16 v10 = (u16)(v00 + (segs + 1));
            u16 v11 = (u16)(v10 + 1);
            add_quad_oriented(m, v00, v01, v10, v11);
        }
    }
}

/* Editor defaults: one-unit geometry matches what the scene saver emits. */
int ps1_mesh_build_kind(Ps1Mesh* m, Ps1MeshKind kind) {
    switch (kind) {
    case MESH_KIND_CUBE:
        ps1_mesh_build_cube(m, FX_ONE);
        break;
    case MESH_KIND_SPHERE:
        ps1_mesh_build_sphere(m, FX_ONE, 12, 8);
        break;
    case MESH_KIND_PLANE:
        ps1_mesh_build_plane(m, fx_int(2), 1);
        break;
    case MESH_KIND_CYLINDER:
        ps1_mesh_build_cylinder(m, FX_ONE, fx_int(2), 12);
        break;
    case MESH_KIND_CAPSULE:
        ps1_mesh_build_capsule(m, FX_ONE, fx_int(2), 12, 8);
        break;
    default:
        return 0;
    }
    return m->triangle_count > 0;
}