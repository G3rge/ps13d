#include <SDL.h>

#include "graphics/ps1_color.h"
#include "sys/ps1_window.h"

struct Ps1Window {
    SDL_Window* win;
    SDL_Surface* screen;
    SDL_Surface* canvas;
    const Uint8* keys;
    SDL_Event ev;
    u32 pw;
    u32 ph;
    s32 m_x;
    s32 m_y;
    s32 m_wheel;
    u8 m_buttons[3];
    u8 quit;
};

static int win_source_scale(const Ps1Window* w) {
    return (w->pw > 0) ? (int)(w->pw / PS1_SCREEN_W) : PS1_WIN_SCALE;
}

int ps1_win_open(Ps1Window** out, const char* title, int scale) {
    Ps1Window* w;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 0;
    w = (Ps1Window*)SDL_calloc(1, sizeof(*w));
    if (!w) return 0;
    w->pw = (u32)(PS1_SCREEN_W * scale);
    w->ph = (u32)(PS1_SCREEN_H * scale);
    w->win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, (int)w->pw, (int)w->ph,
                              SDL_WINDOW_SHOWN);
    if (!w->win) {
        SDL_free(w);
        return 0;
    }
    w->screen = SDL_GetWindowSurface(w->win);
    SDL_FillRect(w->screen, NULL, SDL_MapRGB(w->screen->format, 0, 0, 0));
    SDL_UpdateWindowSurface(w->win);
    w->canvas = SDL_CreateRGBSurface(0, PS1_SCREEN_W, PS1_SCREEN_H, 32,
                                     0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    if (!w->canvas) {
        SDL_DestroyWindow(w->win);
        SDL_free(w);
        return 0;
    }
    w->keys = SDL_GetKeyboardState(NULL);
    w->quit = 0;
    *out = w;
    return 1;
}

void ps1_win_close(Ps1Window* w) {
    if (!w) return;
    if (w->canvas) SDL_FreeSurface(w->canvas);
    if (w->win) SDL_DestroyWindow(w->win);
    SDL_free(w);
    SDL_Quit();
}

void ps1_win_poll(Ps1Window* w) {
    while (SDL_PollEvent(&w->ev)) {
        if (w->ev.type == SDL_QUIT) w->quit = 1;
        if (w->ev.type == SDL_MOUSEMOTION) {
            int s = win_source_scale(w);
            w->m_x = w->ev.motion.x / s;
            w->m_y = w->ev.motion.y / s;
        }
        if (w->ev.type == SDL_MOUSEBUTTONDOWN ||
            w->ev.type == SDL_MOUSEBUTTONUP) {
            int s = win_source_scale(w);
            Uint8 b = w->ev.button.button;
            w->m_x = w->ev.button.x / s;
            w->m_y = w->ev.button.y / s;
            if (b >= 1 && b <= 3)
                w->m_buttons[b - 1] =
                    (w->ev.type == SDL_MOUSEBUTTONDOWN) ? 1 : 0;
        }
        if (w->ev.type == SDL_MOUSEWHEEL) w->m_wheel += w->ev.wheel.y;
        if (w->ev.type == SDL_WINDOWEVENT &&
            w->ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
            SDL_UpdateWindowSurface(w->win);
        }
    }
}

u8 ps1_win_key(const Ps1Window* w, u32 scancode) {
    if (!w->keys) return 0;
    return w->keys[scancode] ? 1 : 0;
}

u32 ps1_win_quit(const Ps1Window* w) { return w->quit ? 1u : 0u; }

int ps1_win_mouse_x(const Ps1Window* w) { return w->m_x; }
int ps1_win_mouse_y(const Ps1Window* w) { return w->m_y; }
u8 ps1_win_mouse_down(const Ps1Window* w, u8 button) {
    if (button < 1 || button > 3) return 0;
    return w->m_buttons[button - 1];
}

s32 ps1_win_wheel(Ps1Window* w) {
    s32 v = w->m_wheel;
    w->m_wheel = 0;
    return v;
}

void ps1_win_present(Ps1Window* w, const Ps1Framebuffer* fb) {
    u32* dst;
    int x, y;
    if (!w->canvas) return;
    if (SDL_LockSurface(w->canvas) != 0) return;
    dst = (u32*)w->canvas->pixels;
    for (y = 0; y < PS1_SCREEN_H; y++) {
        for (x = 0; x < PS1_SCREEN_W; x++) {
            u32 c = ps1_color_to888(fb->px[y * PS1_SCREEN_W + x]);
            dst[y * PS1_SCREEN_W + x] = c;
        }
    }
    SDL_UnlockSurface(w->canvas);
    if (SDL_BlitScaled(w->canvas, NULL, w->screen, NULL) == 0)
        SDL_UpdateWindowSurface(w->win);
}