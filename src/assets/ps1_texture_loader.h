#ifndef PS1_TEXTURE_LOADER_H
#define PS1_TEXTURE_LOADER_H

#include "core/ps1_types.h"
#include "graphics/ps1_color.h"

s8 ps1_tex_load_checker(u8 w, u8 h, s8 slot, rgb555_t c0, rgb555_t c1);

#endif