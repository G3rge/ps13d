#ifndef PS1_SCENE_H
#define PS1_SCENE_H

#include "core/ps1_types.h"
#include "scene/ps1_entity.h"

typedef struct {
    Ps1Entity entities[PS1_MAX_ENTITIES];
    u8 used[PS1_MAX_ENTITIES];
    u16 count;
    u16 next;
} Ps1Scene;

void ps1_scene_init(Ps1Scene* sc);
int ps1_scene_add(Ps1Scene* sc, const Ps1Mesh* mesh,
                  const Ps1Transform* tf);
void ps1_scene_clear(Ps1Scene* sc);

#endif