#ifndef PS1_FRAMEBUFFER_H
#define PS1_FRAMEBUFFER_H

#include "core/ps1_types.h"
#include "graphics/ps1_color.h"

typedef struct {
    u16 px[PS1_SCREEN_W * PS1_SCREEN_H];
} Ps1Framebuffer;

void ps1_fb_clear(Ps1Framebuffer* fb, u16 color);
void ps1_fb_put(Ps1Framebuffer* fb, int x, int y, u16 color);
u16 ps1_fb_get(const Ps1Framebuffer* fb, int x, int y);

int ps1_fb_export_bmp(const Ps1Framebuffer* fb, const char* path);
int ps1_fb_export_png(const Ps1Framebuffer* fb, const char* path);
int ps1_fb_export_raw555(const Ps1Framebuffer* fb, const char* path);

#endif