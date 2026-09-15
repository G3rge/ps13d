#ifndef PS1_ENTITY_H
#define PS1_ENTITY_H

#include "renderer3d/ps1_mesh.h"
#include "renderer3d/ps1_transform.h"

typedef struct {
    Ps1Transform transform;
    const Ps1Mesh* mesh;
    u8 active;
} Ps1Entity;

#endif