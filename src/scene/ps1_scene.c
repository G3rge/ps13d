#include <string.h>

#include "scene/ps1_scene.h"

void ps1_scene_init(Ps1Scene* sc) {
    memset(sc, 0, sizeof(*sc));
}

int ps1_scene_add(Ps1Scene* sc, const Ps1Mesh* mesh,
                  const Ps1Transform* tf) {
    Ps1Entity* e;
    if (sc->count >= PS1_MAX_ENTITIES) return -1;
    e = &sc->entities[sc->count];
    e->mesh = mesh;
    e->transform = *tf;
    e->active = 1;
    return (int)sc->count++;
}

void ps1_scene_clear(Ps1Scene* sc) {
    memset(sc, 0, sizeof(*sc));
}