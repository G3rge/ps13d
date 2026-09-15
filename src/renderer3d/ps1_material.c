#include "renderer3d/ps1_material.h"

void ps1_material_init(Ps1Material* m, rgb555_t color, s8 texture) {
    m->color = color;
    m->texture = texture;
    m->two_sided = 0;
}