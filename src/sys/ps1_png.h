#ifndef PS1_PNG_H
#define PS1_PNG_H

#include "core/ps1_types.h"

/* Decodes a PNG file and stores it as an RGB555 texture in the PS1 texture
 * pool at the given slot. Scales down (box average) to a power-of-two size
 * so it fits the PS1 texture budget (max 64x64). Returns the slot on
 * success, -1 on failure. PC/import layer only. */
s8 ps1_png_load_texture(const char* path, s8 slot);

#endif