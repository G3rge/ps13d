#ifndef PS1_FONT_HDR
#define PS1_FONT_HDR

#include "core/ps1_types.h"
#include "graphics/ps1_color.h"
#include "graphics/ps1_framebuffer.h"

#define PS1_FONT_W 5
#define PS1_FONT_H 7
#define PS1_FONT_STEP 6

int ps1_font_text_width(const char* text);
void ps1_font_draw(Ps1Framebuffer* fb, int x, int y, rgb555_t color,
                   const char* text);

#endif