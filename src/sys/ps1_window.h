#ifndef PS1_WINDOW_H
#define PS1_WINDOW_H

#include "core/ps1_types.h"
#include "graphics/ps1_framebuffer.h"

typedef struct Ps1Window Ps1Window;

int ps1_win_open(Ps1Window** out, const char* title, int scale);
void ps1_win_close(Ps1Window* w);
void ps1_win_poll(Ps1Window* w);
u8 ps1_win_key(const Ps1Window* w, u32 scancode);
u32 ps1_win_quit(const Ps1Window* w);
void ps1_win_present(Ps1Window* w, const Ps1Framebuffer* fb);

int ps1_win_mouse_x(const Ps1Window* w);
int ps1_win_mouse_y(const Ps1Window* w);
u8 ps1_win_mouse_down(const Ps1Window* w, u8 button);

/* Returns the accumulated mouse-wheel delta (in clicks, +up/-down) since the
 * last call and resets the counter. */
s32 ps1_win_wheel(Ps1Window* w);

#endif