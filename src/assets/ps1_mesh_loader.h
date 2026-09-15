#ifndef PS1_MESH_LOADER_H
#define PS1_MESH_LOADER_H

#include "renderer3d/ps1_mesh.h"

typedef enum {
    MESH_KIND_CUBE = 0,
    MESH_KIND_SPHERE,
    MESH_KIND_PLANE,
    MESH_KIND_CYLINDER,
    MESH_KIND_CAPSULE,
    MESH_KIND_OBJ,
    MESH_KIND_CAMERA,
    MESH_KIND_COUNT
} Ps1MeshKind;

void ps1_mesh_build_cube(Ps1Mesh* m, fixed_t half);
void ps1_mesh_build_sphere(Ps1Mesh* m, fixed_t radius, u16 segs, u16 rings);
void ps1_mesh_build_plane(Ps1Mesh* m, fixed_t half, u16 segs);
void ps1_mesh_build_cylinder(Ps1Mesh* m, fixed_t radius, fixed_t height,
                             u16 segs);
void ps1_mesh_build_capsule(Ps1Mesh* m, fixed_t radius, fixed_t height,
                            u16 segs, u16 rings);

/* Builds one of the five parametric primitives with the editor's default
 * parameters. MESH_KIND_OBJ is not handled here (returns 0). Returns 1 on
 * success. */
int ps1_mesh_build_kind(Ps1Mesh* m, Ps1MeshKind kind);

#endif