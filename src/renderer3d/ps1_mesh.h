#ifndef PS1_MESH_H
#define PS1_MESH_H

#include "core/ps1_math.h"
#include "renderer3d/ps1_vertex.h"

typedef struct {
    u16 v[3];
    u16 material;
    FVec3 normal;
} Ps1Triangle;

typedef struct {
    u16 vertex_count;
    u16 triangle_count;
    Ps1Vertex verts[PS1_MAX_VERTS];
    Ps1Triangle tris[PS1_MAX_TRIS];
} Ps1Mesh;

void ps1_mesh_init(Ps1Mesh* m);
int ps1_mesh_add_vertex(Ps1Mesh* m, FVec3 pos, fixed_t u, fixed_t v);
int ps1_mesh_add_vertex_ex(Ps1Mesh* m, FVec3 pos, FVec3 normal,
                           fixed_t u, fixed_t v);
int ps1_mesh_add_triangle(Ps1Mesh* m, u16 v0, u16 v1, u16 v2, u16 material);

#endif