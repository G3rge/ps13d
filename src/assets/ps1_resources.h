#ifndef PS1_RESOURCES_H
#define PS1_RESOURCES_H

#include "renderer3d/ps1_mesh.h"
#include "renderer3d/ps1_material.h"

typedef struct {
    Ps1Mesh mesh[PS1_MAX_MESHES];
    Ps1Material material[PS1_MAX_MATERIALS];
    u8 mesh_used[PS1_MAX_MESHES];
    u8 mat_used[PS1_MAX_MATERIALS];
    u16 mesh_count;
    u16 mat_count;
} Ps1ResourceHub;

void ps1_res_init(Ps1ResourceHub* hub);
Ps1Mesh* ps1_res_new_mesh(Ps1ResourceHub* hub);
const Ps1Material* ps1_res_find_material(const Ps1ResourceHub* hub,
                                         u16 index);

#endif