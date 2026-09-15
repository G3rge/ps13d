#include <string.h>

#include "renderer3d/ps1_mesh.h"

void ps1_mesh_init(Ps1Mesh* m) {
    memset(m, 0, sizeof(*m));
}

int ps1_mesh_add_vertex_ex(Ps1Mesh* m, FVec3 pos, FVec3 normal,
                           fixed_t u, fixed_t v) {
    Ps1Vertex* vert;
    if (m->vertex_count >= PS1_MAX_VERTS) return -1;
    vert = &m->verts[m->vertex_count];
    vert->pos = pos;
    vert->normal = normal;
    vert->u = u;
    vert->v = v;
    return (int)m->vertex_count++;
}

int ps1_mesh_add_vertex(Ps1Mesh* m, FVec3 pos, fixed_t u, fixed_t v) {
    return ps1_mesh_add_vertex_ex(m, pos, fv3(0, 0, 0), u, v);
}

int ps1_mesh_add_triangle(Ps1Mesh* m, u16 v0, u16 v1, u16 v2, u16 material) {
    FVec3* va;
    FVec3* vb;
    FVec3* vc;
    FVec3 e1, e2, n;
    Ps1Triangle* tri;
    if (m->triangle_count >= PS1_MAX_TRIS) return -1;
    va = &m->verts[v0].pos;
    vb = &m->verts[v1].pos;
    vc = &m->verts[v2].pos;
    e1 = fv3_sub(vb, va);
    e2 = fv3_sub(vc, va);
    n = fv3_cross(&e1, &e2);
    n = fv3_normalize(&n);

    tri = &m->tris[m->triangle_count];
    tri->v[0] = v0;
    tri->v[1] = v1;
    tri->v[2] = v2;
    tri->material = material;
    tri->normal = n;
    return (int)m->triangle_count++;
}