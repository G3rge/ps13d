#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "core/ps1_config.h"
#include "core/ps1_lut.h"
#include "core/ps1_timing.h"
#include "editor/ps1_editor.h"
#include "sys/ps1_png.h"
#include "sys/ps1_window.h"

#define TARGET_FRAME_US 16743u

static Ps1Editor g_editor;

static void print_help(const char* exe) {
    printf("ps1-editor - PS1-style software 3D engine (affine texture\n");
    printf("mapping, vertex jitter, RGB555, 240x160 software renderer)\n\n");
    printf("Usage: %s [options]\n\n", exe);
    printf("Options:\n");
    printf("  --obj <path>      OBJ model to load at startup\n");
    printf("  --tex <path>      PNG texture to load as default albedo\n");
    printf("  --scene <path>    PS13D scene file to open at startup\n");
    printf("  --selftest [N]    render N frames headless (default 60) and export a PNG\n");
    printf("  --png <path>      PNG file to write (default ps1_frame.png)\n");
    printf("  --frames <N>      render N frames headless, no window, no PNG\n");
    printf("  --scale <N>       window scale (default 4)\n");
    printf("  --help            show this help\n");
}

static int parse_uint(const char* s, u32* out) {
    char* end;
    unsigned long v;
    if (!s || !*s) return 0;
    v = strtoul(s, &end, 10);
    if (*end) return 0;
    *out = (u32)v;
    return 1;
}

static int run_selftest(Ps1Editor* e, u32 frames, const char* png) {
    u32 i;
    for (i = 0; i < frames; i++) {
        ps1_editor_update(e, NULL);
        ps1_prof_begin_frame(&e->prof);
        ps1_editor_render(e);
        ps1_prof_end_frame(&e->prof);
    }
    if (png) ps1_fb_export_png(&e->fb, png);
    printf("[SELFTEST] rendered %u frames\n", (unsigned)frames);
    printf("[SELFTEST] saved %s\n", png ? png : "(none)");
    printf("[SELFTEST] TEXEL READS: %llu\n",
           (unsigned long long)e->prof.texel_reads);
    printf("[SELFTEST] PIXELS: %llu\n",
           (unsigned long long)e->prof.pixels_written);
    printf("[SELFTEST] TRIS_IN: %u, RASTERIZED: %u\n",
           (unsigned)e->prof.tris_in, (unsigned)e->prof.tris_rasterized);
    printf("[SELFTEST] culled %u, clipped %u\n", (unsigned)e->prof.tris_culled,
           (unsigned)e->prof.tris_clipped);
    printf("[SELFTEST] objects: %u\n", (unsigned)e->scene.count);
    printf("[SELFTEST] PASS\n");
    return 0;
}

static int run_interactive(Ps1Editor* e, int scale) {
    Ps1Window* win = NULL;
    if (!ps1_win_open(&win, "PS1 Style 3D Engine - RGB555", scale)) {
        printf("ERROR: could not open SDL window\n");
        return 1;
    }

    while (!ps1_win_quit(win)) {
        u64 t0, elapsed;
        u32 rem_ms;

        if (ps1_win_key(win, SDL_SCANCODE_ESCAPE)) break;
        ps1_win_poll(win);

        t0 = ps1_time_now_us();
        ps1_editor_update(e, win);
        ps1_prof_begin_frame(&e->prof);
        ps1_editor_render(e);
        ps1_prof_end_frame(&e->prof);
        ps1_win_present(win, &e->fb);

        elapsed = ps1_time_now_us() - t0;
        if (elapsed < TARGET_FRAME_US) {
            rem_ms = (u32)((TARGET_FRAME_US - elapsed) / 1000);
            if (rem_ms) SDL_Delay(rem_ms);
        }
    }

    printf("rendered %u frames, last fps %u\n", (unsigned)e->frames,
           (unsigned)e->timer.fps);
    printf("profiler: %u tris rasterized, %llu pixels\n",
           (unsigned)e->prof.tris_rasterized,
           (unsigned long long)e->prof.pixels_written);

    ps1_win_close(win);
    return 0;
}

int main(int argc, char** argv) {
    Ps1Editor* editor = &g_editor;
    u32 scale = 2;
    u32 frames = 60;
    const char* png = NULL;
    const char* obj = NULL;
    const char* tex = NULL;
    const char* scene = NULL;
    int selftest = 0;
    int headless = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--obj")) {
            if (i + 1 < argc)
                obj = argv[++i];
            else {
                fprintf(stderr, "--obj needs a path\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--tex")) {
            if (i + 1 < argc)
                tex = argv[++i];
            else {
                fprintf(stderr, "--tex needs a path\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--scene")) {
            if (i + 1 < argc) {
                scene = argv[++i];
            } else {
                fprintf(stderr, "--scene needs a path\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--selftest")) {
            selftest = 1;
            if (i + 1 < argc && parse_uint(argv[i + 1], &frames)) i++;
        }
        else if (!strcmp(argv[i], "--frames")) {
            if (i + 1 < argc && parse_uint(argv[i + 1], &frames)) i++;
            headless = 1;
        }
        else if (!strcmp(argv[i], "--png")) {
            if (i + 1 < argc) {
                png = argv[++i];
            } else {
                fprintf(stderr, "--png needs a path\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--scale")) {
            if (i + 1 < argc && parse_uint(argv[i + 1], &scale)) i++;
        }
        else if (!strcmp(argv[i], "--help")) {
            print_help(argv[0]);
            return 0;
        }
        else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 2;
        }
    }

    if (!ps1_editor_init(editor)) {
        fprintf(stderr, "ERROR: could not init editor\n");
        return 1;
    }

    if (tex) {
        s8 slot = (s8)editor->default_tex;
        if (editor->default_tex < 0) slot = 0;
        slot = ps1_png_load_texture(tex, (s8)(slot >= 0 ? slot : 0));
        if (slot >= 0) {
            u8 j;
            for (j = 0; j < editor->scene.count; j++)
                editor->hub.material[editor->objs[j].mat_idx].texture = slot;
            editor->default_tex = slot;
            snprintf(editor->tex_path[slot], PS1_OBJ_PATH_MAX, "%s", tex);
        }
    }
    if (obj) ps1_editor_load_obj(editor, obj);
    if (scene) ps1_editor_load_scene(editor, scene);

    if (selftest || headless) {
        if (!png) png = "ps1_frame.png";
        ps1_editor_to_edit(editor);
        return run_selftest(editor, frames, selftest ? png : NULL);
    }
    if (obj || scene) ps1_editor_to_edit(editor);
    return run_interactive(editor, (int)scale);
}