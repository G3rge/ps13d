#ifndef PS1_COLOR_H
#define PS1_COLOR_H

#include "core/ps1_types.h"

typedef u16 rgb555_t;

void ps1_color_lut_init(void);

rgb555_t ps1_color_pack(u8 r, u8 g, u8 b);
void ps1_color_unpack(rgb555_t c, u8* r, u8* g, u8* b);
u8 ps1_color_quantize5(u8 v8);
rgb555_t ps1_color_shade(rgb555_t color, u8 level);
u32 ps1_color_to888(rgb555_t c);

#endif