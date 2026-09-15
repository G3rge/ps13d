#ifndef PS1_MATERIAL_H
#define PS1_MATERIAL_H

#include "core/ps1_types.h"
#include "graphics/ps1_color.h"

typedef struct {
    rgb555_t color;
    s8 texture;
    u8 two_sided;
} Ps1Material;

void ps1_material_init(Ps1Material* m, rgb555_t color, s8 texture);

#endif