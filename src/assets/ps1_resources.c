#include <stdio.h>
#include <string.h>

#include "assets/ps1_resources.h"

void ps1_res_init(Ps1ResourceHub* hub) {
    memset(hub, 0, sizeof(*hub));
}

Ps1Mesh* ps1_res_new_mesh(Ps1ResourceHub* hub) {
    if (hub->mesh_count >= PS1_MAX_MESHES) {
        fprintf(stderr, "[RES] mesh limit reached\n");
        return NULL;
    }
    ps1_mesh_init(&hub->mesh[hub->mesh_count]);
    hub->mesh_used[hub->mesh_count] = 1;
    return &hub->mesh[hub->mesh_count++];
}

const Ps1Material* ps1_res_find_material(const Ps1ResourceHub* hub,
                                         u16 index) {
    if (index >= hub->mat_count) return NULL;
    if (!hub->mat_used[index]) return NULL;
    return &hub->material[index];
}