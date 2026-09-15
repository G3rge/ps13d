#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include "core/ps1_lut.h"
#include "core/ps1_math.h"
#include "assets/ps1_texture_loader.h"
#include "editor/ps1_editor.h"
#include "graphics/ps1_color.h"
#include "graphics/ps1_texture.h"
#include "renderer3d/ps1_pipeline.h"
#include "sys/ps1_font.h"
#include "sys/ps1_png.h"
#include "sys/ps1_dialog.h"
#include "sys/ps1_window.h"

#define KEY_UP SDL_SCANCODE_UP
#define KEY_DOWN SDL_SCANCODE_DOWN
#define KEY_LEFT SDL_SCANCODE_LEFT
#define KEY_RIGHT SDL_SCANCODE_RIGHT
#define KEY_W SDL_SCANCODE_W
#define KEY_A SDL_SCANCODE_A
#define KEY_S SDL_SCANCODE_S
#define KEY_D SDL_SCANCODE_D
#define KEY_Q SDL_SCANCODE_Q
#define KEY_E SDL_SCANCODE_E
#define KEY_R SDL_SCANCODE_R
#define KEY_I SDL_SCANCODE_I
#define KEY_B SDL_SCANCODE_B
#define KEY_T SDL_SCANCODE_T
#define KEY_TAB SDL_SCANCODE_TAB
#define KEY_P SDL_SCANCODE_P
#define KEY_F SDL_SCANCODE_F
#define KEY_Z SDL_SCANCODE_Z
#define KEY_Y SDL_SCANCODE_Y
#define KEY_C SDL_SCANCODE_C
#define KEY_G SDL_SCANCODE_G
#define KEY_L SDL_SCANCODE_L
#define KEY_LCTRL SDL_SCANCODE_LCTRL
#define KEY_RCTRL SDL_SCANCODE_RCTRL
#define KEY_DEL SDL_SCANCODE_DELETE
#define KEY_ENTER SDL_SCANCODE_RETURN
#define KEY_SPACE SDL_SCANCODE_SPACE
#define KEY_BACKSPACE SDL_SCANCODE_BACKSPACE
#define KEY_LSHIFT SDL_SCANCODE_LSHIFT
#define KEY_RSHIFT SDL_SCANCODE_RSHIFT
#define KEY_LALT SDL_SCANCODE_LALT
#define KEY_RALT SDL_SCANCODE_RALT
#define KEY_N SDL_SCANCODE_N
#define KEY_O SDL_SCANCODE_O

#define ROT_SPD ((fixed_t)(FX_ONE / 48))
#define MOVE_SPD ((fixed_t)(FX_ONE / 16))
#define PI_FX ((fixed_t)(((s64)FX_ONE * 355) / 113))

#define MAX_PROJ_FILES 8

/* Unity-style top toolbar geometry (shared by draw + click + HUD) */
#define TB_FX (PS1_VIEW_X + 2)
#define TB_FW 40
#define TB_CX (TB_FX + TB_FW + 4)
#define TB_CW 54
#define TB_LX (TB_CX + TB_CW + 4)
#define TB_LW 40
#define TB_T0 (TB_LX + TB_LW + 6)

/* PS1 dark engine theme - near-black panels, subtle borders, teal accent */
static rgb555_t c_panel(void) { return ps1_color_pack(7, 7, 9); }
static rgb555_t c_border(void) { return ps1_color_pack(13, 13, 17); }
static rgb555_t c_text(void) { return ps1_color_pack(27, 27, 29); }
static rgb555_t c_dim(void) { return ps1_color_pack(18, 18, 21); }
static rgb555_t c_hover(void) { return ps1_color_pack(12, 12, 16); }
static rgb555_t c_select(void) { return ps1_color_pack(6, 19, 17); }
static rgb555_t xp_blue(void) { return ps1_color_pack(6, 22, 20); }
static rgb555_t xp_white(void) { return ps1_color_pack(31, 31, 31); }
static rgb555_t xp_shadow(void) { return ps1_color_pack(4, 4, 6); }

/* Viewport overlay colors (bright, for the dark 3D scene background) */
#define VP_DIM   ((rgb555_t)((12u << 10) | (14u << 5) | 18u))
#define VP_GREEN ((rgb555_t)((4u << 10) | (24u << 5) | 6u))
#define VP_AMBER ((rgb555_t)((31u << 10) | (22u << 5) | 2u))
#define VP_WHITE ((rgb555_t)((31u << 10) | (31u << 5) | 31u))
#define VP_DARK  ((rgb555_t)((2u << 10) | (2u << 5) | 2u))

static void fb_rect(Ps1Framebuffer* fb, int x0, int y0, int x1, int y1,
                    rgb555_t c) {
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= PS1_SCREEN_W) x1 = PS1_SCREEN_W - 1;
    if (y1 >= PS1_SCREEN_H) y1 = PS1_SCREEN_H - 1;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            ps1_fb_put(fb, x, y, c);
}

static void fb_line(Ps1Framebuffer* fb, int x0, int y0, int x1, int y1,
                    rgb555_t c) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx < 0) ? -1 : 1;
    int sy = (dy < 0) ? -1 : 1;
    int err;
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;
    err = dx - dy;
    for (;;) {
        if (x0 >= 0 && x0 < PS1_SCREEN_W && y0 >= 0 && y0 < PS1_SCREEN_H)
            ps1_fb_put(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

/* Modern dark section header: subtle gradient + teal accent underline. */
static void xp_title_bar(Ps1Framebuffer* fb, int x0, int y0, int x1, int y1) {
    int h = y1 - y0 + 1;
    int y;
    for (y = y0; y <= y1; y++) {
        int t = (h > 1) ? ((y - y0) * 8 / (h - 1)) : 0;
        u8 r = (u8)(9 - t);
        u8 g = (u8)(9 - t);
        u8 b = (u8)(11 - t);
        fb_rect(fb, x0, y, x1, y, ps1_color_pack(r, g, b));
    }
    fb_rect(fb, x0, y1, x1, y1, xp_blue());
}

/* Flat dark raised button: thin top highlight, darker bottom edge. */
static void xp_btn(Ps1Framebuffer* fb, int x, int y, int w, int h,
                    rgb555_t fill) {
    rgb555_t hi = ps1_color_pack(20, 20, 23);
    rgb555_t lo = ps1_color_pack(4, 4, 6);
    fb_rect(fb, x, y, x + w - 1, y + h - 1, fill);
    fb_rect(fb, x, y, x + w - 2, y, hi);
    fb_rect(fb, x, y, x, y + h - 2, hi);
    fb_rect(fb, x + 1, y + h - 1, x + w - 1, y + h - 1, lo);
    fb_rect(fb, x + w - 1, y + 1, x + w - 1, y + h - 1, lo);
}

/* Sunken button for pressed/active state. */
static void xp_btn_pressed(Ps1Framebuffer* fb, int x, int y, int w, int h,
                            rgb555_t fill) {
    rgb555_t hi = ps1_color_pack(4, 4, 6);
    rgb555_t lo = ps1_color_pack(20, 20, 23);
    fb_rect(fb, x, y, x + w - 1, y + h - 1, fill);
    fb_rect(fb, x, y, x + w - 2, y, hi);
    fb_rect(fb, x, y, x, y + h - 2, hi);
    fb_rect(fb, x + 1, y + h - 1, x + w - 1, y + h - 1, lo);
    fb_rect(fb, x + w - 1, y + 1, x + w - 1, y + h - 1, lo);
}

static int in_rect(const Ps1Editor* e, int x, int y, int w, int h) {
    return e->m_x >= x && e->m_x < x + w && e->m_y >= y && e->m_y < y + h;
}

static int ui_button(Ps1Editor* e, const char* label, int x, int y, int w,
                     int h) {
    int over = in_rect(e, x, y, w, h);
    rgb555_t fill = over ? c_hover() : c_panel();
    rgb555_t lo = ps1_color_pack(4, 4, 6);
    rgb555_t hi = ps1_color_pack(20, 20, 23);
    fb_rect(&e->fb, x, y, x + w - 1, y + h - 1, fill);
    fb_rect(&e->fb, x, y, x + w - 2, y, hi);
    fb_rect(&e->fb, x, y, x, y + h - 2, hi);
    fb_rect(&e->fb, x + 1, y + h - 1, x + w - 1, y + h - 1, lo);
    fb_rect(&e->fb, x + w - 1, y + 1, x + w - 1, y + h - 1, lo);
    ps1_font_draw(&e->fb, x + 3, y + (h - PS1_FONT_H) / 2, c_text(), label);
    return (over && e->mouse_click) ? 1 : 0;
}

static int ui_nudge(Ps1Editor* e, int x, int y, int delta) {
    char buf[3];
    snprintf(buf, sizeof buf, "-");
    if (ui_button(e, buf, x, y, 9, 8)) return -delta;
    snprintf(buf, sizeof buf, "+");
    if (ui_button(e, buf, x + 10, y, 9, 8)) return delta;
    return 0;
}

static fixed_t obj_fit_scale(const Ps1Mesh* m) {
    fixed_t maxc = 0;
    u32 i;
    for (i = 0; i < m->vertex_count; i++) {
        fixed_t x = fx_abs(m->verts[i].pos.x);
        fixed_t y = fx_abs(m->verts[i].pos.y);
        fixed_t z = fx_abs(m->verts[i].pos.z);
        if (x > maxc) maxc = x;
        if (y > maxc) maxc = y;
        if (z > maxc) maxc = z;
    }
    if (maxc <= 0) return FX_ONE;
    return fx_div(fx_int(PS1_VIEW_W * 3), fx_mul(fx_int(208 * 2), maxc));
}

static s16 editor_new_mat(Ps1Editor* e, rgb555_t color, s8 tex) {
    Ps1Material* m;
    if (e->hub.mat_count >= PS1_MAX_MATERIALS) return -1;
    m = &e->hub.material[e->hub.mat_count];
    ps1_material_init(m, color, tex);
    e->hub.mat_used[e->hub.mat_count] = 1;
    e->hub.mat_count++;
    return (s16)(e->hub.mat_count - 1);
}

static s8 editor_free_tex_slot(void) {
    s8 s;
    for (s = 0; s < PS1_MAX_TEXTURES; s++)
        if (!ps1_tex_get(s)) return s;
    return -1;
}

static s8 editor_tex_load(Ps1Editor* e, const char* path) {
    s8 i;
    if (!path || !path[0]) return -1;
    /* reuse the slot if this file is already in the pool so every object
     * can own a texture without duplicating pixels. */
    for (i = 0; i < PS1_MAX_TEXTURES; i++)
        if (e->tex_path[i][0] && !strcmp(e->tex_path[i], path))
            return i;
    {
        s8 slot = ps1_png_load_texture(path, editor_free_tex_slot());
        if (slot >= 0)
            snprintf(e->tex_path[slot], PS1_OBJ_PATH_MAX, "%s", path);
        return slot;
    }
}

static int editor_add_primitive(Ps1Editor* e, Ps1MeshKind kind) {
    Ps1Mesh* mesh;
    Ps1Transform t;
    s16 mat;
    int idx, i;
    const char* tag;

    if (e->scene.count >= PS1_MAX_ENTITIES ||
        e->hub.mesh_count >= PS1_MAX_MESHES)
        return -1;
    if (e->hub.mat_count >= PS1_MAX_MATERIALS) return -1;
    mesh = ps1_res_new_mesh(&e->hub);
    if (!mesh) return -1;
    if (!ps1_mesh_build_kind(mesh, kind)) return -1;
    mat = editor_new_mat(e, ps1_color_pack(18, 18, 21), e->default_tex);
    if (mat < 0) return -1;

    for (i = 0; i < mesh->triangle_count; i++)
        mesh->tris[i].material = (u16)mat;

    t.position = fv3(fx_mul(fx_int(3), fx_int((int)e->scene.count + 1)), 0, 0);
    t.rotation = fv3(0, 0, 0);
    t.scale = fv3(fx_int(1), fx_int(1), fx_int(1));
    idx = ps1_scene_add(&e->scene, mesh, &t);
    if (idx < 0) return -1;

    switch (kind) {
    case MESH_KIND_CUBE: tag = "CUBE"; break;
    case MESH_KIND_SPHERE: tag = "SPHERE"; break;
    case MESH_KIND_PLANE: tag = "PLANE"; break;
    case MESH_KIND_CYLINDER: tag = "CYLINDER"; break;
    case MESH_KIND_CAPSULE: tag = "CAPSULE"; break;
    default: tag = "PRIM"; break;
    }
    snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "%s %d", tag,
             (int)(idx + 1));
    e->objs[idx].mesh_idx = (u16)(e->hub.mesh_count - 1);
    e->objs[idx].mat_idx = mat;
    e->objs[idx].mesh_kind = (s16)kind;
    e->objs[idx].mesh_path[0] = 0;
    e->selected = idx;
    ps1_editor_snap(e);
    ps1_editor_log(e, "ADD %s", tag);
    return idx;
}

static void basename_of(const char* path, char* out, u32 cap) {
    const char* p = path;
    const char* base = path;
    while (*p) p++;
    while (p > base && p[-1] != '/' && p[-1] != '\\') p--;
    snprintf(out, cap, "%s", (*p ? p : path));
}

int ps1_editor_load_obj(Ps1Editor* e, const char* path) {
    Ps1Mesh* mesh;
    Ps1ObjResult res;
    Ps1Transform t;
    s16 mat;
    fixed_t scl;
    char base[PS1_OBJ_NAME_MAX];
    int idx, i;
    if (!path || !path[0]) return 0;
    if (e->scene.count >= PS1_MAX_ENTITIES ||
        e->hub.mesh_count >= PS1_MAX_MESHES)
        return 0;
    if (e->hub.mat_count >= PS1_MAX_MATERIALS) return 0;
    mesh = ps1_res_new_mesh(&e->hub);
    if (!mesh) return 0;
    if (!ps1_obj_load(path, mesh, &res, -1)) return 0;
    {
        /* each loaded model gets its own texture slot from its MTL
         * (map_Kd) so objects never share textures unintentionally. */
        s8 tex = -1;
        if (res.texture_path[0]) {
            s8 slot = editor_tex_load(e, res.texture_path);
            if (slot >= 0) {
                tex = slot;
                ps1_editor_log(e, "OBJ TEX %s", res.texture_path);
            }
        }
        mat = editor_new_mat(e, res.color, tex);
    }
    if (mat < 0) return 0;
    for (i = 0; i < mesh->triangle_count; i++)
        mesh->tris[i].material = (u16)mat;
    scl = obj_fit_scale(mesh);
    t.position = fv3(-fx_mul(res.center.x, scl), -fx_mul(res.center.y, scl),
                     -fx_mul(res.center.z, scl));
    t.rotation = fv3(0, 0, 0);
    t.scale = fv3(scl, scl, scl);
    idx = ps1_scene_add(&e->scene, mesh, &t);
    if (idx < 0) return 0;
    basename_of(path, base, sizeof base);
    snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "%s",
             res.name[0] ? res.name : base);
    if (!e->objs[idx].name[0])
        snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "OBJ");
    e->objs[idx].mesh_idx = (u16)(e->hub.mesh_count - 1);
    e->objs[idx].mat_idx = mat;
    e->objs[idx].mesh_kind = (s16)MESH_KIND_OBJ;
    snprintf(e->objs[idx].mesh_path, PS1_OBJ_PATH_MAX, "%s", path);
    e->selected = idx;
    ps1_editor_snap(e);
    ps1_editor_log(e, "ADD OBJ %s", base);
    return 1;
}

static int editor_browse_albedo(Ps1Editor* e) {
    char path[PS1_OBJ_PATH_MAX];
    s8 slot;
    Ps1Material* m;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return 0;
    if (!ps1_dialog_open_file("SELECT ALBEDO TEXTURE", "PNG textures", "*.png",
                              "textures", path, sizeof path))
        return 0;
    slot = editor_tex_load(e, path);
    if (slot < 0) {
        ps1_editor_log(e, "TEX LOAD FAIL %s", path);
        return 0;
    }
    e->default_tex = slot;
    m = &e->hub.material[e->objs[e->selected].mat_idx];
    m->texture = slot;
    e->scene_dirty = 1;
    ps1_editor_log(e, "TEXTURE %d <- %s", (int)slot, path);
    return 1;
}

static u8 key_edge(const struct Ps1Window* win, u32 sc, u8* prev) {
    u8 now = ps1_win_key(win, sc) ? 1 : 0;
    u8 edge = now && !*prev;
    *prev = now;
    return edge;
}

static u8 ctrl_down(const struct Ps1Window* win) {
    return (ps1_win_key(win, KEY_LCTRL) || ps1_win_key(win, KEY_RCTRL)) ? 1
                                                                        : 0;
}

void ps1_editor_camera_reset(Ps1Editor* e) {
    e->camera.pos = fv3(0, 0, fx_int(6));
    e->camera.yaw = PI_FX;
    e->camera.pitch = 0;
}

static void editor_focus(Ps1Editor* e) {
    fixed_t cp, sp, cy, sy, dist, len;
    FVec3 f, toward, neg;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return;
    cp = ps1_cos(e->camera.pitch);
    sp = ps1_sin(e->camera.pitch);
    cy = ps1_cos(e->camera.yaw);
    sy = ps1_sin(e->camera.yaw);
    f = fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
    len = fv3_len(&e->scene.entities[e->selected].transform.scale);
    dist = fx_mul(len, fx_int(3));
    if (dist < fx_int(3)) dist = fx_int(3);
    if (dist > fx_int(40)) dist = fx_int(40);
    toward = fv3_scale(&f, dist);
    neg = fv3_neg(&toward);
    toward = fv3_add(&e->scene.entities[e->selected].transform.position, &neg);
    e->camera.pos = toward;
    ps1_editor_log(e, "FOCUS %s", e->objs[e->selected].name);
}

static void editor_delete(Ps1Editor* e) {
    int i;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return;
    for (i = e->selected; i + 1 < (int)e->scene.count; i++) {
        e->scene.entities[i] = e->scene.entities[i + 1];
        e->objs[i] = e->objs[i + 1];
    }
    e->scene.count--;
    if (e->selected >= (s32)e->scene.count) e->selected = 0;
    if (e->scene.count == 0) e->selected = -1;
    ps1_editor_snap(e);
    ps1_editor_log(e, "DELETE OBJECT");
}

static int editor_duplicate(Ps1Editor* e) {
    int src;
    Ps1Transform t;
    int idx;
    int i;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return -1;
    if (e->scene.count >= PS1_MAX_ENTITIES ||
        e->hub.mesh_count >= PS1_MAX_MESHES ||
        e->hub.mat_count >= PS1_MAX_MATERIALS)
        return -1;
    src = e->selected;
    {
        Ps1Mesh* mesh = ps1_res_new_mesh(&e->hub);
        s16 mat;
        if (!mesh) return -1;
        if (e->objs[src].mesh_kind == MESH_KIND_OBJ) {
            Ps1ObjResult res;
            if (!ps1_obj_load(e->objs[src].mesh_path, mesh, &res, -1))
                return -1;
        } else {
            if (!ps1_mesh_build_kind(mesh, (Ps1MeshKind)e->objs[src].mesh_kind))
                return -1;
        }
        mat = editor_new_mat(e, e->hub.material[e->objs[src].mat_idx].color,
                             e->hub.material[e->objs[src].mat_idx].texture);
        if (mat < 0) return -1;
        for (i = 0; i < mesh->triangle_count; i++)
            mesh->tris[i].material = (u16)mat;
    }

    t = e->scene.entities[src].transform;
    t.position.x += fx_int(1);
    idx = ps1_scene_add(&e->scene, &e->hub.mesh[e->hub.mesh_count - 1], &t);
    if (idx < 0) return -1;
    {
        char nm[PS1_OBJ_NAME_MAX];
        char mp[PS1_OBJ_PATH_MAX];
        snprintf(nm, sizeof nm, "%s COPY", e->objs[src].name);
        snprintf(mp, sizeof mp, "%s", e->objs[src].mesh_path);
        snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "%s", nm);
        e->objs[idx].mesh_idx = (u16)(e->hub.mesh_count - 1);
        e->objs[idx].mat_idx = (s16)(e->hub.mat_count - 1);
        e->objs[idx].mesh_kind = e->objs[src].mesh_kind;
        snprintf(e->objs[idx].mesh_path, PS1_OBJ_PATH_MAX, "%s", mp);
    }
    e->selected = idx;
    ps1_editor_snap(e);
    ps1_editor_log(e, "DUPLICATE %s", e->objs[idx].name);
    return idx;
}

/* ---- history ---- */

/* Adds a "camera node": a mesh-less entity that defines the game camera
 * (used by Play mode). */
static s32 find_camera_node(const Ps1Editor* e) {
    s32 i;
    for (i = 0; i < (s32)e->scene.count; i++)
        if (e->objs[i].is_camera) return i;
    return -1;
}

static int editor_add_camera(Ps1Editor* e) {
    Ps1Transform t;
    int idx;
    if (e->scene.count >= PS1_MAX_ENTITIES) return -1;
    t.position = fv3(0, fx_int(1), fx_int(6));
    t.rotation = fv3(0, PI_FX, 0);
    t.scale = fv3(fx_int(1), fx_int(1), fx_int(1));
    idx = ps1_scene_add(&e->scene, NULL, &t);
    if (idx < 0) return -1;
    snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "Camera");
    e->objs[idx].mesh_idx = 0;
    e->objs[idx].mat_idx = -1;
    e->objs[idx].mesh_kind = (s16)MESH_KIND_CAMERA;
    e->objs[idx].mesh_path[0] = 0;
    e->objs[idx].is_camera = 1;
    e->selected = idx;
    ps1_editor_snap(e);
    ps1_editor_log(e, "ADD CAMERA NODE");
    return idx;
}

static void snap_store(Ps1SceneSnap* d, const Ps1Editor* e) {
    d->scene = e->scene;
    memcpy(d->objs, e->objs, sizeof e->objs);
    d->mesh_count = e->hub.mesh_count;
    d->mat_count = e->hub.mat_count;
    d->selected = e->selected;
}

static void snap_apply(Ps1Editor* e, const Ps1SceneSnap* d) {
    e->scene = d->scene;
    memcpy(e->objs, d->objs, sizeof e->objs);
    e->hub.mesh_count = d->mesh_count;
    e->hub.mat_count = d->mat_count;
    e->selected = d->selected;
}

void ps1_editor_snap(Ps1Editor* e) {
    if (e->hist_len > e->hist_undo) e->hist_len = e->hist_undo;
    if (e->hist_len >= PS1_HIST_MAX) {
        memmove(e->snap_pool, e->snap_pool + 1,
                (PS1_HIST_MAX - 1) * sizeof(Ps1SceneSnap));
        e->hist_len--;
        e->hist_undo--;
    }
    snap_store(&e->snap_pool[e->hist_len], e);
    e->hist_len++;
    e->hist_undo = e->hist_len;
}

void ps1_editor_undo(Ps1Editor* e) {
    if (e->hist_undo < 2) return;
    e->hist_undo--;
    snap_apply(e, &e->snap_pool[e->hist_undo - 1]);
    ps1_editor_log(e, "UNDO");
}

void ps1_editor_redo(Ps1Editor* e) {
    if (e->hist_undo >= e->hist_len) return;
    e->hist_undo++;
    snap_apply(e, &e->snap_pool[e->hist_undo - 1]);
    ps1_editor_log(e, "REDO");
}

/* ---- logger ---- */

void ps1_editor_log(Ps1Editor* e, const char* fmt, ...) {
    char buf[PS1_LOG_LEN];
    va_list ap;
    u16 idx;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    buf[sizeof buf - 1] = 0;
    idx = e->log_head;
    snprintf(e->log_lines[idx], PS1_LOG_LEN, "%s", buf);
    e->log_head = (u16)((idx + 1) % PS1_LOG_LINES);
    if (e->log_count < PS1_LOG_LINES) e->log_count++;
}

/* ---- screen-space helpers and picking ---- */

static int project_screen(const Ps1Camera* cam, const Ps1Mat4* comb,
                          const FVec3* wp, int* sx, int* sy) {
    FVec3 v;
    fixed_t xz, yz;
    s64 px, py;
    ps1_mat4_transform(comb, wp, &v);
    if (v.z <= cam->near_z) return 0;
    xz = fx_div(v.x, v.z);
    yz = fx_div(v.y, v.z);
    px = (((s64)(PS1_VIEW_X + (PS1_VIEW_W >> 1))) << FX_SHIFT) +
         (((s64)cam->focal * xz) >> FX_SHIFT);
    py = (((s64)(PS1_VIEW_Y + (PS1_VIEW_H >> 1))) << FX_SHIFT) -
         (((s64)cam->focal * yz) >> FX_SHIFT);
    *sx = (int)(px >> FX_SHIFT);
    *sy = (int)(py >> FX_SHIFT);
    return 1;
}

static int pt_in_tri(int xs, int ys, int x0, int y0, int x1, int y1, int x2,
                     int y2) {
    s64 d0 = (s64)(x1 - x0) * (ys - y0) - (s64)(y1 - y0) * (xs - x0);
    s64 d1 = (s64)(x2 - x1) * (ys - y1) - (s64)(y2 - y1) * (xs - x1);
    s64 d2 = (s64)(x0 - x2) * (ys - y2) - (s64)(y0 - y2) * (xs - x2);
    return ((d0 >= 0 && d1 >= 0 && d2 >= 0) ||
            (d0 <= 0 && d1 <= 0 && d2 <= 0));
}

static void editor_pick(Ps1Editor* e) {
    u32 i;
    s32 best = -1;
    fixed_t bestz = 0;

    for (i = 0; i < e->scene.count; i++) {
        Ps1Entity* en = &e->scene.entities[i];
        Ps1Mat4 model, nm, comb;
        u32 t;
        if (!en->active || !en->mesh) continue;
        ps1_transform_model(&en->transform, &model, &nm);
        ps1_mat4_mul(&e->camera.view, &model, &comb);
        for (t = 0; t < (u32)en->mesh->triangle_count; t++) {
            const Ps1Triangle* tri = &en->mesh->tris[t];
            int sx[3], sy[3], k;
            FVec3 vv[3];
            fixed_t zc = 0;
            int ok = 1;
            for (k = 0; k < 3; k++) {
                if (!project_screen(&e->camera, &comb,
                                    &en->mesh->verts[tri->v[k]].pos, &sx[k],
                                    &sy[k])) {
                    ok = 0;
                    break;
                }
                ps1_mat4_transform(&comb, &en->mesh->verts[tri->v[k]].pos,
                                   &vv[k]);
                zc += vv[k].z;
            }
            if (!ok) continue;
            if (!pt_in_tri(e->m_x, e->m_y, sx[0], sy[0], sx[1], sy[1], sx[2],
                           sy[2]))
                continue;
            zc /= 3;
            if (best < 0 || zc < bestz) {
                best = (s32)i;
                bestz = zc;
            }
        }
    }
    e->selected = best;
    if (best >= 0) ps1_editor_log(e, "SELECT %s", e->objs[best].name);
}

/* ---- demo simulation for play mode ---- */

static void sim_demo(Ps1Editor* e) {
    u32 i;
    for (i = 0; i < e->scene.count; i++) {
        if (e->objs[i].is_camera) continue;
        e->scene.entities[i].transform.rotation.y += fx_div(ROT_SPD, fx_int(2));
        e->scene.entities[i].transform.rotation.x += fx_div(ROT_SPD, fx_int(6));
    }
}

/* Game-side input in Play mode: arrow keys steer the camera node (simple
 * first-person demo). */
static void sim_input(Ps1Editor* e, const struct Ps1Window* win) {
    s32 cam = find_camera_node(e);
    Ps1Transform* t;
    if (cam < 0) return;
    t = &e->scene.entities[cam].transform;
    if (ps1_win_key(win, KEY_UP)) t->rotation.x += ROT_SPD;
    if (ps1_win_key(win, KEY_DOWN)) t->rotation.x -= ROT_SPD;
    if (ps1_win_key(win, KEY_LEFT)) t->rotation.y += ROT_SPD;
    if (ps1_win_key(win, KEY_RIGHT)) t->rotation.y -= ROT_SPD;
    if (ps1_win_key(win, KEY_W)) {
        fixed_t cy = ps1_cos(t->rotation.y);
        fixed_t sy = ps1_sin(t->rotation.y);
        fixed_t cp = ps1_cos(t->rotation.x);
        fixed_t sp = ps1_sin(t->rotation.x);
        FVec3 dir = fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
        FVec3 mv = fv3_scale(&dir, MOVE_SPD);
        t->position = fv3_add(&t->position, &mv);
    }
    if (ps1_win_key(win, KEY_S)) {
        fixed_t cy = ps1_cos(t->rotation.y);
        fixed_t sy = ps1_sin(t->rotation.y);
        fixed_t cp = ps1_cos(t->rotation.x);
        fixed_t sp = ps1_sin(t->rotation.x);
        FVec3 dir = fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
        FVec3 mv = fv3_scale(&dir, -MOVE_SPD);
        t->position = fv3_add(&t->position, &mv);
    }
}

static void editor_toggle_play(Ps1Editor* e) {
    if (e->state == STATE_EDIT) {
        snap_store(&e->play_snap, e);
        e->state = STATE_PLAY;
        ps1_editor_log(e, "PLAY");
    } else if (e->state == STATE_PLAY) {
        e->state = STATE_PAUSED;
        ps1_editor_log(e, "PAUSED");
    } else {
        e->state = STATE_PLAY;
        ps1_editor_log(e, "RESUME");
    }
}

static void editor_stop_play(Ps1Editor* e) {
    if (e->state == STATE_EDIT) return;
    if (e->hist_len > e->hist_undo &&
        e->hist_undo == e->hist_len) {
        /* keep current history stack; restore snapshot as a new edit step */
    }
    snap_apply(e, &e->play_snap);
    e->state = STATE_EDIT;
    ps1_editor_snap(e);
    ps1_editor_log(e, "STOP");
}

/* ---- project manager ---- */

static FVec3 cam_forward(const Ps1Editor* e) {
    fixed_t cp = ps1_cos(e->camera.pitch);
    fixed_t sp = ps1_sin(e->camera.pitch);
    fixed_t cy = ps1_cos(e->camera.yaw);
    fixed_t sy = ps1_sin(e->camera.yaw);
    return fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
}

static int assets_dock_hit(const Ps1Editor* e) {
    return e->m_x >= PS1_VIEW_X &&
           e->m_x < PS1_SCREEN_W - PS1_PANEL_R &&
           e->m_y >= PS1_VIEW_Y + PS1_VIEW_H && e->m_y < PS1_SCREEN_H;
}

/* ---- asset dock ---- */

#define ASSET_MAX 32
static char g_assets_tex[ASSET_MAX][PS1_OBJ_PATH_MAX];
static char g_assets_obj[ASSET_MAX][PS1_OBJ_PATH_MAX];
static int g_assets_tex_n = 0;
static int g_assets_obj_n = 0;

static void assets_scan_dir(const char* dirname, const char* ext,
                            char (*out)[PS1_OBJ_PATH_MAX], int cap, int* n) {
    DIR* d;
    struct dirent* de;
    size_t elen = strlen(ext);
    d = opendir(dirname);
    if (!d) return;
    while ((de = readdir(d)) != NULL && *n < cap) {
        size_t len = strlen(de->d_name);
        if (len < elen) continue;
        if (dirname[0]) {
            if (strcmp(de->d_name + len - elen, ext) == 0) {
                size_t i2 = 0, j2 = 0;
                while (dirname[i2] && i2 < PS1_OBJ_PATH_MAX - 1) {
                    out[*n][i2] = dirname[i2];
                    i2++;
                }
                if (i2 < PS1_OBJ_PATH_MAX - 1) out[*n][i2++] = '/';
                while (de->d_name[j2] && i2 < PS1_OBJ_PATH_MAX - 1)
                    out[*n][i2++] = de->d_name[j2++];
                out[*n][i2] = 0;
                (*n)++;
            }
        } else {
            if (strcmp(de->d_name + len - elen, ext) == 0) {
                snprintf(out[*n], PS1_OBJ_PATH_MAX, "%s", de->d_name);
                (*n)++;
            }
        }
    }
    closedir(d);
}

static void assets_scan(void) {
    g_assets_tex_n = 0;
    g_assets_obj_n = 0;
    assets_scan_dir("textures", ".png", g_assets_tex, ASSET_MAX,
                    &g_assets_tex_n);
    assets_scan_dir("source", ".obj", g_assets_obj, ASSET_MAX,
                    &g_assets_obj_n);
    assets_scan_dir("", ".obj", g_assets_obj, ASSET_MAX, &g_assets_obj_n);
}

/* Applies a click inside the asset dock. Returns 1 if consumed. */
static int assets_click(Ps1Editor* e) {
    int x0 = PS1_VIEW_X + 2;
    int y = PS1_VIEW_Y + PS1_VIEW_H + 9;
    int i;
    if (e->m_y < PS1_VIEW_Y + PS1_VIEW_H + 8) return 0;
    for (i = e->assets_scroll; i < g_assets_tex_n && y < PS1_SCREEN_H - 1;
         i++, y += 7) {
        char* slot_name;
        if (in_rect(e, x0, y, 56, 7)) {
            slot_name = strrchr(g_assets_tex[i], '/');
            if (!slot_name) slot_name = g_assets_tex[i];
            else slot_name++;
            if (e->selected >= 0 && e->selected < (s32)e->scene.count &&
                e->objs[e->selected].mat_idx >= 0) {
                s8 slot = editor_tex_load(e, g_assets_tex[i]);
                if (slot >= 0) {
                    e->hub.material[e->objs[e->selected].mat_idx].texture =
                        slot;
                    e->default_tex = slot;
                    e->scene_dirty = 1;
                    ps1_editor_log(e, "MAT TEX %s", slot_name);
                }
            } else {
                s8 slot = editor_tex_load(e, g_assets_tex[i]);
                if (slot >= 0) {
                    e->default_tex = slot;
                    ps1_editor_log(e, "DEFAULT TEX %s", slot_name);
                }
            }
            return 1;
        }
    }
    y = PS1_VIEW_Y + PS1_VIEW_H + 9;
    x0 = PS1_VIEW_X + 62;
    for (i = e->assets_scroll; i < g_assets_obj_n && y < PS1_SCREEN_H - 1;
         i++, y += 7) {
        if (in_rect(e, x0, y, 56, 7)) {
            ps1_editor_load_obj(e, g_assets_obj[i]);
            return 1;
        }
    }
    if (e->m_x >= PS1_VIEW_X && e->m_x < PS1_SCREEN_W - PS1_PANEL_R &&
        e->m_y >= PS1_VIEW_Y + PS1_VIEW_H)
        return 1;
    return 0;
}

static char g_proj_files[MAX_PROJ_FILES][PS1_OBJ_PATH_MAX];
static int g_proj_count = 0;

static void project_refresh(void) {
    g_proj_count =
        ps1_editor_scan_scene_files(g_proj_files, MAX_PROJ_FILES);
}

static void project_open_dialog(Ps1Editor* e) {
    char path[PS1_OBJ_PATH_MAX];
    if (ps1_dialog_open_file("OPEN SCENE", "PS13D scenes", "*.ps13d", "",
                             path, sizeof path))
        ps1_editor_load_scene(e, path);
}

static void project_ui(Ps1Editor* e, const struct Ps1Window* win) {
    int i;
    project_refresh();
    if (e->mouse_click) {
        if (in_rect(e, 30, 40, 180, 14)) ps1_editor_new_scene(e);
        else if (in_rect(e, 30, 58, 180, 14)) project_open_dialog(e);
        else {
            for (i = 0; i < g_proj_count; i++) {
                int y = 78 + i * 9;
                if (in_rect(e, 16, y, 208, 8)) {
                    ps1_editor_load_scene(e, g_proj_files[i]);
                    break;
                }
            }
        }
    }
    if (ps1_win_key(win, KEY_N) && !e->key_n_prev) ps1_editor_new_scene(e);
    if (ps1_win_key(win, KEY_O) && !e->key_o_prev) project_open_dialog(e);
    if (ps1_win_key(win, KEY_UP)) e->proj_scroll--;
    if (ps1_win_key(win, KEY_DOWN)) e->proj_scroll++;
    if (e->proj_scroll < 0) e->proj_scroll = 0;
    if (e->proj_scroll > g_proj_count) e->proj_scroll = g_proj_count;
    e->key_n_prev = (u8)(ps1_win_key(win, KEY_N) ? 1 : 0);
    e->key_o_prev = (u8)(ps1_win_key(win, KEY_O) ? 1 : 0);
}

/* ---- keyboard ---- */

static void key_poll(Ps1Editor* e, const struct Ps1Window* win) {
    /* Camera movement is handled entirely by right-click + WASD in
     * ps1_editor_update; tools are switched via the toolbar. */
    if (key_edge(win, KEY_T, &e->key_t_prev))
        e->texture_enabled = (u8)!e->texture_enabled;
    if (key_edge(win, KEY_I, &e->key_i_prev)) {
        e->show_assets = (u8)!e->show_assets;
        if (e->show_assets) {
            ps1_editor_log(e, "ASSETS DOCK");
            assets_scan();
        }
    }
    if (key_edge(win, KEY_B, &e->key_b_prev)) editor_add_camera(e);
    if (key_edge(win, KEY_F, &e->key_f_prev)) editor_focus(e);
    if (key_edge(win, KEY_Z, &e->key_z_prev))
        e->depth_mode = (u8)((e->depth_mode == PS1_DEPTH_PAINTER)
                                 ? PS1_DEPTH_Z16
                                 : PS1_DEPTH_PAINTER);
    if (key_edge(win, KEY_L, &e->key_l_prev))
        e->lighting_enabled = (u8)!e->lighting_enabled;
    if (key_edge(win, KEY_C, &e->key_c_prev))
        e->cull_mode = (u8)((e->cull_mode == PS1_CULL_BACK)
                                ? PS1_CULL_FRONT
                                : (e->cull_mode == PS1_CULL_FRONT)
                                      ? PS1_CULL_NONE
                                      : PS1_CULL_BACK);
    if (key_edge(win, KEY_G, &e->key_g_prev)) {
        e->show_log = (u8)!e->show_log;
        if (e->show_log) ps1_editor_log(e, "CONSOLE");
    }
    if (key_edge(win, KEY_TAB, &e->key_tab_prev))
        e->show_profiler = (u8)!e->show_profiler;
    if (key_edge(win, KEY_P, &e->key_p_prev))
        ps1_fb_export_png(&e->fb, "ps13d_frame.png");

    if (ctrl_down(win)) {
        u8 ctrl_edge = !e->key_ctrl_prev;
        e->key_ctrl_prev = 1;
        if (ctrl_edge && ps1_win_key(win, KEY_Z)) {
            if (ps1_win_key(win, KEY_LSHIFT) || ps1_win_key(win, KEY_RSHIFT))
                ps1_editor_redo(e);
            else
                ps1_editor_undo(e);
        }
        if (ctrl_edge && ps1_win_key(win, KEY_Y)) ps1_editor_redo(e);
        if (ctrl_edge && ps1_win_key(win, KEY_D)) editor_duplicate(e);
    } else {
        e->key_ctrl_prev = 0;
    }

    if (key_edge(win, KEY_DEL, &e->key_del_prev)) editor_delete(e);
    if (key_edge(win, KEY_ENTER, &e->key_enter_prev)) editor_toggle_play(e);
    if (key_edge(win, KEY_BACKSPACE, &e->key_bs_prev)) editor_stop_play(e);
}

/* ---- editor UI interaction ---- */

static int menu_open(Ps1Editor* e, int x, int w, int rows);
static int menu_dropdown_w(const char* const* items, int n);
static int gizmo_try_grab(Ps1Editor* e);
static void gizmo_drag_move(Ps1Editor* e);

/* Unity-style top toolbar clicks: FILE / CREATE menus, tools, PLAY / STOP.
 * Kept callable outside STATE_EDIT so STOP (and PLAY) keep working while
 * the scene is running. Returns 1 when the click was consumed. */
static u8 editor_toolbar_interact(Ps1Editor* e) {
    int fx = TB_FX;
    int fw = TB_FW;
    int cx = TB_CX;
    int cw = TB_CW;
    int lx = TB_LX;
    int lw = TB_LW;
    int t0 = TB_T0;
    int rx = PS1_VIEW_X + PS1_VIEW_W - 1;
    int stop_x = rx - 28 + 1;
    int play_x = stop_x - 2 - 26;
    static const char* file_items[3] = {"SAVE", "OPEN", "HOME"};
        static const char* prim_names[5] = {"CUBE", "SPHERE", "PLANE", "CYL",
                                            "CAPS"};
        static const char* load_names[1] = {"MODEL"};
    static const Ps1MeshKind kinds[5] = {
        MESH_KIND_CUBE, MESH_KIND_SPHERE, MESH_KIND_PLANE,
        MESH_KIND_CYLINDER, MESH_KIND_CAPSULE};
    int i;

    if (menu_open(e, fx, fw, 3)) {
        int dw = menu_dropdown_w(file_items, 3);
        if (in_rect(e, fx, 11, dw, 8)) {
            char path[PS1_OBJ_PATH_MAX];
            if (ps1_dialog_save_file("SAVE SCENE", "PS13D scenes",
                                     "*.ps13d", "", path, sizeof path))
                ps1_editor_save_scene(e, path);
        } else if (in_rect(e, fx, 19, dw, 8)) {
            project_open_dialog(e);
        } else if (in_rect(e, fx, 27, dw, 8)) {
            e->mode = MODE_PROJECT;
            ps1_editor_log(e, "HOME");
        }
        return 1;
        } else if (menu_open(e, cx, cw, 5)) {
            int dw = menu_dropdown_w(prim_names, 5);
            for (i = 0; i < 5; i++) {
                if (in_rect(e, cx, 11 + i * 8, dw, 8)) {
                    editor_add_primitive(e, kinds[i]);
                    break;
                }
            }
            return 1;
        } else if (menu_open(e, lx, lw, 1)) {
            int dw = menu_dropdown_w(load_names, 1);
            if (in_rect(e, lx, 11, dw, 8)) {
                char path[PS1_OBJ_PATH_MAX];
                if (ps1_dialog_open_file("LOAD MODEL", "OBJ models", "*.obj",
                                         "source", path, sizeof path))
                    ps1_editor_load_obj(e, path);
            }
            return 1;
    } else if (in_rect(e, t0, 1, 16, 8)) { e->tool = TOOL_SELECT; return 1; }
    else if (in_rect(e, t0 + 17, 1, 16, 8)) { e->tool = TOOL_MOVE; return 1; }
    else if (in_rect(e, t0 + 34, 1, 16, 8)) { e->tool = TOOL_ROTATE; return 1; }
    else if (in_rect(e, t0 + 51, 1, 16, 8)) { e->tool = TOOL_SCALE; return 1; }
    else if (in_rect(e, play_x, 1, 26, 8)) { editor_toggle_play(e); return 1; }
    else if (in_rect(e, stop_x, 1, 28, 8)) { editor_stop_play(e); return 1; }
    return 0;
}

static int editor_ui_interact(Ps1Editor* e) {
    int xr = PS1_SCREEN_W - PS1_PANEL_R;
    int x = xr + 2;
    u8 consumed = 0;

    if (!e->mouse_click) return 0;

    if (in_rect(e, 1, 10, 15, 7)) {
        editor_add_camera(e);
        consumed = 1;
    } else {
        int i;
        int y = 20;
        for (i = 0; i < (s32)e->scene.count && y <= 128; i++, y += 9) {
            if (in_rect(e, 1, y, 50, 8)) {
                e->selected = i;
                consumed = 1;
                break;
            }
        }
    }

    if (!consumed && e->selected >= 0) {
        int sel = e->selected;
        s8 mat = e->objs[sel].mat_idx;
        int v;
        if (in_rect(e, x + 10, 66, 24, 7)) {
            editor_browse_albedo(e);
            consumed = 1;
        } else if (mat >= 0 && in_rect(e, x + 36, 66, 18, 7)) {
            e->hub.material[mat].two_sided = (u8)!
                e->hub.material[mat].two_sided;
            ps1_editor_snap(e);
            consumed = 1;
        }

        v = ui_nudge(e, x + 40, 84, 1);
        if (v) {
            e->lighting.ambient += fx_mul(v, fx_div(fx_int(1), fx_int(50)));
            if (e->lighting.ambient < 0) e->lighting.ambient = 0;
            if (e->lighting.ambient > FX_ONE) e->lighting.ambient = FX_ONE;
            consumed = 1;
        }
        v = ui_nudge(e, x + 40, 92, 1);
        if (v) {
            fixed_t cy = ps1_cos(fx_div(PI_FX, fx_int(45)));
            fixed_t sy = ps1_sin(fx_div(PI_FX, fx_int(45)));
            FVec3 d = e->lighting.dir;
            FVec3 nd = fv3(fx_mul(d.x, cy) + fx_mul(d.z, sy), d.y,
                           fx_mul(-d.x, sy) + fx_mul(d.z, cy));
            e->lighting.dir = fv3_normalize(&nd);
            consumed = 1;
        }
        v = ui_nudge(e, x + 40, 100, 1);
        if (v) {
            s32 lv = (s32)e->lighting.levels + v;
            if (lv >= 2 && lv <= 32) e->lighting.levels = (u8)lv;
            consumed = 1;
        }
        v = ui_nudge(e, x + 40, 108, 1);
        if (v) {
            fixed_t nr = e->scene.entities[sel].transform.rotation.y +
                         fx_mul(fx_int(v), fx_div(PI_FX, fx_int(90)));
            if (nr > PI_FX * 2) nr -= PI_FX * 2;
            if (nr < -PI_FX * 2) nr += PI_FX * 2;
            e->scene.entities[sel].transform.rotation.y = nr;
            e->scene_dirty = 1;
            ps1_editor_snap(e);
            consumed = 1;
        }
        v = ui_nudge(e, x + 40, 116, 1);
        if (v) {
            fixed_t f = fx_mul(e->scene.entities[sel].transform.scale.x,
                               fx_div(fx_int(20 + v), fx_int(20)));
            if (f < fx_div(fx_int(1), fx_int(100))) f = fx_div(fx_int(1), fx_int(100));
            if (f > fx_int(64)) f = fx_int(64);
            e->scene.entities[sel].transform.scale.x = f;
            e->scene.entities[sel].transform.scale.y = f;
            e->scene.entities[sel].transform.scale.z = f;
            e->scene_dirty = 1;
            ps1_editor_snap(e);
            consumed = 1;
        }
    }
    /* asset dock overlays the viewport */
    if (!consumed && e->show_assets && assets_click(e)) consumed = 1;

    /* viewport toolbar (Unity-style menu bar) */
    if (!consumed && editor_toolbar_interact(e)) consumed = 1;
    return consumed;
}

/* ---- update ---- */

void ps1_editor_update(Ps1Editor* e, struct Ps1Window* win) {
    s32 mx = win ? ps1_win_mouse_x(win) : 0;
    s32 my = win ? ps1_win_mouse_y(win) : 0;
    u8 left = win ? ps1_win_mouse_down(win, 1) : 0;
    u8 right = win ? ps1_win_mouse_down(win, 3) : 0;

    e->mouse_down = left;
    e->mouse_click = left && !e->mouse_prev;
    e->m_x = mx;
    e->m_y = my;

    if (win) {
        u8 in_viewport = (mx >= PS1_VIEW_X && mx < PS1_SCREEN_W - PS1_PANEL_R &&
                          my >= 0 && my < PS1_SCREEN_H);

        if (e->mode == MODE_PROJECT) {
            project_ui(e, win);
        } else {
            u8 consumed = 0;
            e->flying = 0;
            key_poll(e, win);

            if (e->state == STATE_EDIT) {
                consumed = (u8)editor_ui_interact(e);

                if (e->show_assets && assets_dock_hit(e) && e->mouse_click)
                    consumed = 1;

                if (!consumed && e->mouse_click && in_viewport &&
                    !assets_dock_hit(e)) {
                    /* gizmo axis first (MOVE tool), otherwise pick */
                    if (gizmo_try_grab(e))
                        consumed = 1;
                    else
                        editor_pick(e);
                }
            } else {
                /* PLAY / PAUSED: the toolbar (STOP above all) keeps working */
                consumed = editor_toolbar_interact(e);
                if (e->state == STATE_PLAY) sim_input(e, win);
            }
        }

        /* wheel: scroll the asset dock only (no camera zoom) */
        {
            s32 wheel = ps1_win_wheel(win);
            if (wheel && e->show_assets && in_viewport && assets_dock_hit(e))
                e->assets_scroll += (wheel > 0) ? -1 : 1;
        }
        if (e->assets_scroll < 0) e->assets_scroll = 0;
        if (e->assets_scroll >
            (g_assets_tex_n > g_assets_obj_n ? g_assets_tex_n
                                             : g_assets_obj_n))
            e->assets_scroll =
                (g_assets_tex_n > g_assets_obj_n ? g_assets_tex_n
                                                 : g_assets_obj_n);

        /* orbit: right-drag (Unity-style view rotation) */
        if (right && in_viewport) {
            if (!e->dragging) {
                e->dragging = 1;
                e->d_x0 = mx;
                e->d_y0 = my;
                e->drag_yaw = e->camera.yaw;
                e->drag_pitch = e->camera.pitch;
            }
            {
                fixed_t f = fx_div(PI_FX, fx_int(PS1_VIEW_W));
                fixed_t d = fx_mul(fx_int((int)(mx - e->d_x0)), f);
                e->camera.yaw = e->drag_yaw - d;
                d = fx_mul(fx_int((int)(my - e->d_y0)), f);
                e->camera.pitch = e->drag_pitch + d;
                if (e->camera.pitch > PI_FX / 2) e->camera.pitch = PI_FX / 2;
                if (e->camera.pitch < -PI_FX / 2) e->camera.pitch = -PI_FX / 2;
            }
        } else if (e->dragging) {
            e->dragging = 0;
        }
        if (e->dragging) e->flying = 1;

        /* fly: WASD moves the camera while orbiting, Q/E go up/down,
         * shift for sprint. Speed is constant, independent of distance. */
        if (e->flying && e->state != STATE_PLAY) {
            FVec3 f = cam_forward(e);
            FVec3 rv = fv3(-ps1_cos(e->camera.yaw), 0,
                           ps1_sin(e->camera.yaw));
            fixed_t step = MOVE_SPD;
            if (ps1_win_key(win, KEY_LSHIFT) || ps1_win_key(win, KEY_RSHIFT))
                step = fx_mul(step, fx_int(4));
            if (ps1_win_key(win, KEY_W)) {
                FVec3 mv = fv3_scale(&f, step);
                e->camera.pos = fv3_add(&e->camera.pos, &mv);
            }
            if (ps1_win_key(win, KEY_S)) {
                FVec3 mv = fv3_scale(&f, step);
                e->camera.pos = fv3_sub(&e->camera.pos, &mv);
            }
            if (ps1_win_key(win, KEY_D)) {
                FVec3 mv = fv3_scale(&rv, step);
                e->camera.pos = fv3_add(&e->camera.pos, &mv);
            }
            if (ps1_win_key(win, KEY_A)) {
                FVec3 mv = fv3_scale(&rv, step);
                e->camera.pos = fv3_sub(&e->camera.pos, &mv);
            }
            if (ps1_win_key(win, KEY_Q)) {
                FVec3 mv = fv3(fx_int(0), -step, fx_int(0));
                e->camera.pos = fv3_add(&e->camera.pos, &mv);
            }
            if (ps1_win_key(win, KEY_E)) {
                FVec3 mv = fv3(fx_int(0), step, fx_int(0));
                e->camera.pos = fv3_add(&e->camera.pos, &mv);
            }
        }

        /* gizmo drag: left button held after grabbing a MOVE axis */
        if (e->tool_drag && e->mode != MODE_PROJECT) {
            if (left) {
                if (e->state == STATE_EDIT) gizmo_drag_move(e);
            } else {
                if (e->state == STATE_EDIT && e->selected >= 0 &&
                    e->selected < (s32)e->scene.count) {
                    const FVec3* p =
                        &e->scene.entities[e->selected].transform.position;
                    if (p->x != e->t_pos0.x || p->y != e->t_pos0.y ||
                        p->z != e->t_pos0.z) {
                        e->scene_dirty = 1;
                        ps1_editor_snap(e);
                    }
                }
                e->tool_drag = 0;
            }
        }
    }

    if (e->state == STATE_PLAY) sim_demo(e);

    e->mouse_prev = e->mouse_down;
    ps1_timer_begin(&e->timer);
    ps1_timer_end(&e->timer);
    e->fps = e->timer.fps;
    e->frames++;
}

int ps1_editor_init(Ps1Editor* e) {
    memset(e, 0, sizeof *e);
    ps1_lut_init();
    ps1_color_lut_init();
    ps1_res_init(&e->hub);
    ps1_scene_init(&e->scene);
    ps1_cam_init(&e->camera);
    ps1_light_init(&e->lighting);
    ps1_time_init();
    e->selected = -1;
    e->bg = ps1_color_pack(6, 8, 16);
    e->cull_mode = PS1_CULL_BACK;
    e->depth_mode = PS1_DEPTH_PAINTER;
    e->lighting_enabled = 1;
    e->texture_enabled = 1;
    e->mode = MODE_PROJECT;
    e->tool = TOOL_SELECT;
    e->default_tex = ps1_png_load_texture("textures/texture.png", 0);
    if (e->default_tex >= 0)
        snprintf(e->tex_path[e->default_tex], PS1_OBJ_PATH_MAX, "%s",
                 "textures/texture.png");
    ps1_editor_camera_reset(e);
    assets_scan();
    ps1_timer_begin(&e->timer);
    editor_add_primitive(e, MESH_KIND_CUBE);
    ps1_editor_log(e, "PS1 EDITOR READY");
    project_refresh();
    return 1;
}

/* ---- rendering / panels ---- */

static void fmt_fx(fixed_t v, char* out, u32 cap) {
    int neg = (v < 0);
    u64 ua = neg ? (u64)(-(s64)v) : (u64)v;
    u64 ip = ua >> FX_SHIFT;
    u64 fr = ((ua & FX_FRAC_MASK) * 100u) >> FX_SHIFT;
    if (fr == 0)
        snprintf(out, cap, "%s%llu", neg ? "-" : "", (unsigned long long)ip);
    else if ((fr % 10) == 0)
        snprintf(out, cap, "%s%llu.%llu", neg ? "-" : "",
                 (unsigned long long)ip, (unsigned long long)(fr / 10));
    else
        snprintf(out, cap, "%s%llu.%02llu", neg ? "-" : "",
                 (unsigned long long)ip, (unsigned long long)fr);
}

static void editor_comb(const Ps1Editor* e,
                        const Ps1Transform* tf, Ps1Mat4* comb) {
    Ps1Mat4 model, nm;
    ps1_transform_model(tf, &model, &nm);
    ps1_mat4_mul(&e->camera.view, &model, comb);
}

static int obj_to_screen(const Ps1Editor* e, const Ps1Mat4* comb,
                         const FVec3* wp, int* sx, int* sy) {
    return project_screen(&e->camera, comb, wp, sx, sy);
}

static void draw_selection(Ps1Editor* e, const Ps1Entity* en) {
    Ps1Mat4 comb;
    u32 t;
    int sx[3], sy[3];
    editor_comb(e, &en->transform, &comb);
    for (t = 0; t < (u32)en->mesh->triangle_count; t++) {
        const Ps1Triangle* tri = &en->mesh->tris[t];
        int k, ok = 1;
        for (k = 0; k < 3; k++) {
            if (!obj_to_screen(e, &comb, &en->mesh->verts[tri->v[k]].pos,
                               &sx[k], &sy[k])) {
                ok = 0;
                break;
            }
        }
        if (!ok) continue;
        fb_line(&e->fb, sx[0], sy[0], sx[1], sy[1], VP_AMBER);
        fb_line(&e->fb, sx[1], sy[1], sx[2], sy[2], VP_AMBER);
        fb_line(&e->fb, sx[2], sy[2], sx[0], sy[0], VP_AMBER);
    }
}

static const FVec3 GIZ_AX[3] = {{FX_ONE, 0, 0},
                                {0, FX_ONE, 0},
                                {0, 0, FX_ONE}};

static fixed_t gizmo_len(const Ps1Transform* tf) {
    fixed_t len = fx_mul(fv3_len(&tf->scale), fx_int(2));
    if (len < fx_int(1)) len = fx_int(1);
    return len;
}

static int gizmo_axis_screen(const Ps1Editor* e, const FVec3* org, fixed_t len,
                             int a, int* x0, int* y0, int* x1, int* y1) {
    FVec3 ax = fv3_scale(&GIZ_AX[a], len);
    FVec3 end = fv3_add(org, &ax);
    if (!obj_to_screen(e, &e->camera.view, org, x0, y0)) return 0;
    if (!obj_to_screen(e, &e->camera.view, &end, x1, y1)) return 0;
    return 1;
}

static void draw_gizmo(Ps1Editor* e, const Ps1Transform* tf) {
    static const rgb555_t AX_C[3] = {0x001F, 0x03E0, 0x7C00};
    fixed_t len = gizmo_len(tf);
    int x0, y0, x1, y1;
    int a;
    for (a = 0; a < 3; a++) {
        if (!gizmo_axis_screen(e, &tf->position, len, a, &x0, &y0, &x1, &y1))
            continue;
        fb_line(&e->fb, x0, y0, x1, y1, AX_C[a]);
    }
}

static int gizmo_seg_dist2(int px, int py, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0, dy = y1 - y0;
    int denom = dx * dx + dy * dy;
    int t, cx, cy, ex, ey;
    if (denom <= 0) {
        ex = px - x0;
        ey = py - y0;
        return ex * ex + ey * ey;
    }
    t = (px - x0) * dx + (py - y0) * dy;
    if (t <= 0) {
        cx = x0;
        cy = y0;
    } else if (t >= denom) {
        cx = x1;
        cy = y1;
    } else {
        cx = x0 + dx * t / denom;
        cy = y0 + dy * t / denom;
    }
    ex = px - cx;
    ey = py - cy;
    return ex * ex + ey * ey;
}

static int gizmo_on_screen(int x0, int y0, int x1, int y1) {
    return x0 > -600 && x0 < 900 && y0 > -600 && y0 < 800 && x1 > -600 &&
           x1 < 900 && y1 > -600 && y1 < 800;
}

static int gizmo_try_grab(Ps1Editor* e) {
    const Ps1Transform* tf;
    fixed_t len;
    int a, best = -1, bestd = 49;
    int x0, y0, x1, y1;
    if (e->tool != TOOL_MOVE) return 0;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return 0;
    tf = &e->scene.entities[e->selected].transform;
    len = gizmo_len(tf);
    for (a = 0; a < 3; a++) {
        int d;
        if (!gizmo_axis_screen(e, &tf->position, len, a, &x0, &y0, &x1, &y1))
            continue;
        if (!gizmo_on_screen(x0, y0, x1, y1)) continue;
        d = gizmo_seg_dist2((int)e->m_x, (int)e->m_y, x0, y0, x1, y1);
        if (d < bestd) {
            bestd = d;
            best = a;
        }
    }
    if (best < 0) return 0;
    e->tool_drag = 1;
    e->t_axis = (s8)best;
    e->t_dx0 = e->m_x;
    e->t_dy0 = e->m_y;
    e->t_pos0 = tf->position;
    return 1;
}

static void gizmo_drag_move(Ps1Editor* e) {
    Ps1Transform* tf;
    FVec3 off;
    fixed_t len, world;
    int x0, y0, x1, y1, a;
    int mx, my, sx, sy, slen, dot;
    if (!e->tool_drag || e->t_axis < 0 || e->t_axis > 2) return;
    if (e->selected < 0 || e->selected >= (s32)e->scene.count) return;
    tf = &e->scene.entities[e->selected].transform;
    a = (int)e->t_axis;
    len = gizmo_len(tf);
    if (!gizmo_axis_screen(e, &e->t_pos0, len, a, &x0, &y0, &x1, &y1)) return;
    if (!gizmo_on_screen(x0, y0, x1, y1)) return;
    mx = (int)(e->m_x - e->t_dx0);
    my = (int)(e->m_y - e->t_dy0);
    sx = x1 - x0;
    sy = y1 - y0;
    slen = sx * sx + sy * sy;
    if (slen < 1) return;
    dot = mx * sx + my * sy;
    /* world offset = dot/slen * len, done in 64 bit (fixed-point safe) */
    world = (fixed_t)(((s64)dot * (s64)len) / (s64)slen);
    off = fv3_scale(&GIZ_AX[a], world);
    tf->position = fv3_add(&e->t_pos0, &off);
    e->scene_dirty = 1;
}

static FVec3 cam_forward_ent(const Ps1Transform* tf) {
    fixed_t cp = ps1_cos(tf->rotation.x);
    fixed_t sp = ps1_sin(tf->rotation.x);
    fixed_t cy = ps1_cos(tf->rotation.y);
    fixed_t sy = ps1_sin(tf->rotation.y);
    return fv3(fx_mul(cp, sy), sp, fx_mul(cp, cy));
}

/* Ground grid (y=0) and origin axes. */
static void draw_grid(Ps1Editor* e) {
    int sx, sy, ex, ey;
    int i, step;
    FVec3 p, q;
    for (i = -8; i <= 8; i++) {
        p = fv3(fx_int(i), 0, -fx_int(8));
        if (!project_screen(&e->camera, &e->camera.view, &p, &sx, &sy)) continue;
        q = fv3(fx_int(i), 0, fx_int(8));
        if (!project_screen(&e->camera, &e->camera.view, &q, &ex, &ey)) continue;
        if (sx < -600 || sx > 900 || sy < -600 || sy > 800) continue;
        if (ex < -600 || ex > 900 || ey < -600 || ey > 800) continue;
        fb_line(&e->fb, sx, sy, ex, ey, VP_DIM);

        p = fv3(-fx_int(8), 0, fx_int(i));
        if (!project_screen(&e->camera, &e->camera.view, &p, &sx, &sy)) continue;
        q = fv3(fx_int(8), 0, fx_int(i));
        if (!project_screen(&e->camera, &e->camera.view, &q, &ex, &ey)) continue;
        if (sx < -600 || sx > 900 || sy < -600 || sy > 800) continue;
        if (ex < -600 || ex > 900 || ey < -600 || ey > 800) continue;
        fb_line(&e->fb, sx, sy, ex, ey, VP_DIM);
    }
    /* origin axes */
    step = fx_int(3);
    p = fv3(0, 0, 0);
    if (project_screen(&e->camera, &e->camera.view, &p, &sx, &sy)) {
        int x2, y2;
        rgb555_t cols[3] = {0x001F, 0x03E0, 0x7C00};
        for (i = 0; i < 3; i++) {
            FVec3 ax = fv3(i == 0 ? step : 0, i == 1 ? step : 0,
                           i == 2 ? step : 0);
            FVec3 end = fv3_add(&p, &ax);
            if (project_screen(&e->camera, &e->camera.view, &end, &x2, &y2))
                fb_line(&e->fb, sx, sy, x2, y2, cols[i]);
        }
    }
}

/* Wireframe camera-node icon (frustum pyramid + forward axis). */
static void draw_camera_node(Ps1Editor* e, const Ps1Entity* en) {
    FVec3 f = cam_forward_ent(&en->transform);
    FVec3 rv = fv3(-ps1_cos(en->transform.rotation.y), 0,
                   ps1_sin(en->transform.rotation.y));
    FVec3 uv = fv3(
        fx_mul(-ps1_sin(en->transform.rotation.y),
               ps1_sin(en->transform.rotation.x)),
        ps1_cos(en->transform.rotation.x),
        fx_mul(-ps1_cos(en->transform.rotation.y),
               ps1_sin(en->transform.rotation.x)));
    fixed_t s = fx_mul(fv3_len(&en->transform.scale), fx_int(4));
    rgb555_t col = e->selected >= 0 &&
                           &e->scene.entities[e->selected] == en
                       ? VP_AMBER
                       : VP_DIM;
    FVec3 c[4], apex, tip, tmp, off;
    int s0, t0, i;
    if (s < fx_int(1)) s = fx_int(1);
    tmp = fv3_scale(&f, s);
    apex = fv3_add(&en->transform.position, &tmp);
    for (i = 0; i < 4; i++) {
        FVec3 ox = fv3_scale(&rv, fx_div((i & 1) ? s : -s, fx_int(2)));
        FVec3 oz = fv3_scale(&uv, fx_div((i & 2) ? s : -s, fx_int(2)));
        off = fv3_add(&ox, &oz);
        tmp = fv3_scale(&off, fx_div(fx_int(3), fx_int(4)));
        c[i] = fv3_add(&en->transform.position, &tmp);
    }
    tmp = fv3_scale(&f, fx_mul(s, fx_int(3)));
    tip = fv3_add(&en->transform.position, &tmp);
    if (obj_to_screen(e, &e->camera.view, &apex, &s0, &t0)) {
        for (i = 0; i < 4; i++) {
            int s1, t1;
            if (!obj_to_screen(e, &e->camera.view, &c[i], &s1, &t1)) continue;
            fb_line(&e->fb, s0, t0, s1, t1, col);
        }
    }
    if (obj_to_screen(e, &e->camera.view, &tip, &s0, &t0)) {
        for (i = 0; i < 4; i++) {
            int s1, t1;
            if (!obj_to_screen(e, &e->camera.view, &c[i], &s1, &t1)) continue;
            fb_line(&e->fb, s0, t0, s1, t1, col);
        }
        for (i = 0; i < 4; i++) {
            int s1, t1, s2, t2;
            if (!obj_to_screen(e, &e->camera.view, &c[i], &s1, &t1)) continue;
            if (!obj_to_screen(e, &e->camera.view, &c[(i + 1) & 3], &s2, &t2))
                continue;
            fb_line(&e->fb, s1, t1, s2, t2, col);
        }
    }
}

static int menu_open(Ps1Editor* e, int x, int w, int rows) {
    return in_rect(e, x, 1, w, 8) || in_rect(e, x, 9, w, rows * 8 + 4);
}

static int menu_dropdown_w(const char* const* items, int n) {
    int w = 0, i;
    for (i = 0; i < n; i++) {
        int tw = ps1_font_text_width(items[i]);
        if (tw > w) w = tw;
    }
    return w + 12;
}

static void menu_btn(Ps1Editor* e, int x, int y, int w, int h,
                     const char* label, int open) {
    int over = in_rect(e, x, y, w, h);
    rgb555_t fill = (over || open) ? c_hover() : c_panel();
    int cx = x + w - 6;
    int cy = y + h / 2;
    if (open && over) fb_rect(&e->fb, x, y, x + w - 1, y + h - 1, c_select());
    else fb_rect(&e->fb, x, y, x + w - 1, y + h - 1, fill);
    ps1_font_draw(&e->fb, x + 3, y + 1, c_text(), label);
    fb_line(&e->fb, cx - 2, cy - 1, cx + 2, cy - 1, c_dim());
    fb_line(&e->fb, cx - 1, cy, cx + 1, cy, c_dim());
    fb_line(&e->fb, cx, cy + 1, cx, cy + 1, c_dim());
}

static void draw_dropdown(Ps1Editor* e, int x, int y, int w,
                          const char* const* items, int n) {
    int h = 2 + n * 8;
    int i;
    fb_rect(&e->fb, x, y, x + w - 1, y + h - 1, ps1_color_pack(9, 9, 12));
    fb_rect(&e->fb, x, y, x + w - 2, y, c_border());
    fb_rect(&e->fb, x, y, x, y + h - 2, c_border());
    fb_line(&e->fb, x, y + h - 1, x + w - 1, y + h - 1, c_border());
    fb_line(&e->fb, x + w - 1, y + 1, x + w - 1, y + h - 1, c_border());
    for (i = 0; i < n; i++) {
        int oy = y + 1 + i * 8;
        int over = in_rect(e, x, oy, w, 8);
        if (over) fb_rect(&e->fb, x + 1, oy, x + w - 2, oy + 7, c_select());
        ps1_font_draw(&e->fb, x + 3, oy + 1,
                      over ? ps1_color_pack(31, 31, 31) : c_text(), items[i]);
    }
}

static void draw_toolbar(Ps1Editor* e) {
    static const char* file_items[3] = {"SAVE", "OPEN", "HOME"};
    static const char* create_items[5] = {"CUBE", "SPHERE", "PLANE", "CYL",
                                          "CAPS"};
    static const char* load_items[1] = {"MODEL"};
    static const char* tool_lbl[4] = {"SL", "MV", "RT", "SC"};
    int fx = TB_FX;
    int fw = TB_FW;
    int cx = TB_CX;
    int cw = TB_CW;
    int lx = TB_LX;
    int lw = TB_LW;
    int t0 = TB_T0;
    int rx = PS1_VIEW_X + PS1_VIEW_W - 1;
    int stop_x = rx - 28 + 1;
    int play_x = stop_x - 2 - 26;
    int i;
    int f_open = menu_open(e, fx, fw, 3);
    int c_open = menu_open(e, cx, cw, 5);
    int l_open = menu_open(e, lx, lw, 1);
    int playing = (e->state != STATE_EDIT);

    menu_btn(e, fx, 1, fw, 8, "FILE", f_open);
    menu_btn(e, cx, 1, cw, 8, "CREATE", c_open);
    menu_btn(e, lx, 1, lw, 8, "LOAD", l_open);

    fb_line(&e->fb, t0 - 3, 2, t0 - 3, 7, c_border());

    for (i = 0; i < 4; i++) {
        int x = t0 + i * 17;
        int over = in_rect(e, x, 1, 16, 8);
        int active = (i == 0 && e->tool == TOOL_SELECT) ||
                     (i == 1 && e->tool == TOOL_MOVE) ||
                     (i == 2 && e->tool == TOOL_ROTATE) ||
                     (i == 3 && e->tool == TOOL_SCALE);
        rgb555_t fill = over ? c_hover() : c_panel();
        rgb555_t txt = c_text();
        if (active) {
            fill = c_select();
            txt = ps1_color_pack(31, 31, 31);
        }
        if (active)
            xp_btn_pressed(&e->fb, x, 1, 16, 8, fill);
        else
            xp_btn(&e->fb, x, 1, 16, 8, fill);
        ps1_font_draw(&e->fb, x + 3, 2, txt, tool_lbl[i]);
    }

    {
        int over = in_rect(e, play_x, 1, 26, 8);
        rgb555_t fill = over ? c_hover() : c_panel();
        rgb555_t txt = ps1_color_pack(20, 27, 20);
        if (playing) {
            fill = c_select();
            txt = ps1_color_pack(31, 31, 31);
        }
        if (playing)
            xp_btn_pressed(&e->fb, play_x, 1, 26, 8, fill);
        else
            xp_btn(&e->fb, play_x, 1, 26, 8, fill);
        ps1_font_draw(&e->fb, play_x + 3, 2, txt, "PLAY");
    }
    {
        int over = in_rect(e, stop_x, 1, 28, 8);
        rgb555_t fill = over ? c_hover() : c_panel();
        rgb555_t txt = ps1_color_pack(26, 16, 16);
        if (over || playing)
            xp_btn_pressed(&e->fb, stop_x, 1, 28, 8, fill);
        else
            xp_btn(&e->fb, stop_x, 1, 28, 8, fill);
        ps1_font_draw(&e->fb, stop_x + 3, 2, txt, "STOP");
    }

    if (f_open)
        draw_dropdown(e, fx, 10, menu_dropdown_w(file_items, 3), file_items, 3);
    if (c_open)
        draw_dropdown(e, cx, 10, menu_dropdown_w(create_items, 5),
                      create_items, 5);
    if (l_open)
        draw_dropdown(e, lx, 10, menu_dropdown_w(load_items, 1),
                      load_items, 1);
}

static void draw_left_panel(Ps1Editor* e) {
    char name[16];
    int i, y;
    rgb555_t hi = ps1_color_pack(20, 20, 23);
    rgb555_t lo = ps1_color_pack(4, 4, 6);
    rgb555_t sel = c_select();
    rgb555_t txtcol = xp_white();
    fb_rect(&e->fb, 0, 0, PS1_PANEL_L - 1, PS1_SCREEN_H - 1, c_panel());
    fb_line(&e->fb, PS1_PANEL_L, 0, PS1_PANEL_L, PS1_SCREEN_H - 1, c_border());
    /* dark accent at the very top of the panel */
    xp_title_bar(&e->fb, 0, 0, PS1_PANEL_L - 1, 1);
    {
        int over = in_rect(e, 1, 10, 15, 7);
        rgb555_t fill = over ? c_hover() : c_panel();
        xp_btn(&e->fb, 1, 10, 16, 7, fill);
        ps1_font_draw(&e->fb, 3, 11, c_text(), "CAM");
    }
    ps1_font_draw(&e->fb, 18, 11, xp_blue(), "OBJECTS");
    fb_line(&e->fb, 1, 19, 52, 19, lo);

    y = 20;
    for (i = 0; i < (s32)e->scene.count; i++) {
        rgb555_t fill = (i == e->selected) ? sel : c_panel();
        rgb555_t tcol = (i == e->selected) ? txtcol : c_text();
        if (y > 128) break;
        fb_rect(&e->fb, 1, y, 52, y + 7, fill);
        if (i == e->selected) {
            fb_line(&e->fb, 1, y, 52, y, lo);
            fb_line(&e->fb, 1, y, 1, y + 7, lo);
            fb_line(&e->fb, 1, y + 7, 52, y + 7, hi);
            fb_line(&e->fb, 52, y, 52, y + 7, hi);
        } else {
            fb_line(&e->fb, 1, y + 7, 52, y + 7, lo);
            fb_line(&e->fb, 52, y, 52, y + 7, lo);
        }
        snprintf(name, sizeof name, "%s", e->objs[i].name);
        name[8] = 0;
        if (e->objs[i].is_camera) {
            char cam[8];
            size_t k;
            for (k = 0; k < 4 && name[k]; k++) cam[k] = name[k];
            cam[k] = 0;
            ps1_font_draw(&e->fb, 2, y + 1,
                          (i == e->selected) ? txtcol : xp_blue(), cam);
            ps1_font_draw(&e->fb, 7, y + 1,
                          (i == e->selected) ? txtcol : c_dim(), "*");
        } else {
            ps1_font_draw(&e->fb, 2, y + 1, tcol, name);
        }
        y += 9;
    }
}

static void draw_nudge(Ps1Editor* e, int x, int y) {
    rgb555_t fill = c_panel();
    rgb555_t hi = ps1_color_pack(20, 20, 23);
    rgb555_t lo = ps1_color_pack(4, 4, 6);
    fb_rect(&e->fb, x, y, x + 8, y + 6, fill);
    fb_rect(&e->fb, x, y, x + 7, y, hi);
    fb_rect(&e->fb, x, y, x, y + 5, hi);
    fb_rect(&e->fb, x + 1, y + 6, x + 8, y + 6, lo);
    fb_rect(&e->fb, x + 8, y + 1, x + 8, y + 6, lo);
    ps1_font_draw(&e->fb, x, y - 1, c_text(), "-");
    fb_rect(&e->fb, x + 11, y, x + 19, y + 6, fill);
    fb_rect(&e->fb, x + 11, y, x + 18, y, hi);
    fb_rect(&e->fb, x + 11, y, x + 11, y + 5, hi);
    fb_rect(&e->fb, x + 12, y + 6, x + 19, y + 6, lo);
    fb_rect(&e->fb, x + 19, y + 1, x + 19, y + 6, lo);
    ps1_font_draw(&e->fb, x + 13, y - 1, c_text(), "+");
}

static void draw_right_panel(Ps1Editor* e) {
    int x = PS1_SCREEN_W - PS1_PANEL_R + 2;
    char buf[40];
    int sel = e->selected;
    const Ps1Object* o;
    const Ps1Transform* tf;
    rgb555_t mcol = (u16)0x7FFF;
    const char* kind = "OBJ";
    u8 tex_ttl[40] = "TEX...";
    int over;

    fb_rect(&e->fb, PS1_SCREEN_W - PS1_PANEL_R, 0, PS1_SCREEN_W - 1,
            PS1_SCREEN_H - 1, c_panel());
    fb_line(&e->fb, PS1_SCREEN_W - PS1_PANEL_R - 1, 0, PS1_SCREEN_W - PS1_PANEL_R - 1,
            PS1_SCREEN_H - 1, c_border());
    /* dark section header */
    xp_title_bar(&e->fb, PS1_SCREEN_W - PS1_PANEL_R, 0, PS1_SCREEN_W - 1, 8);
    ps1_font_draw(&e->fb, x, 1, xp_white(), "INSPECTOR");

    if (sel < 0 || sel >= (s32)e->scene.count) {
        ps1_font_draw(&e->fb, x, 14, c_dim(), "no selection");
        return;
    }
    o = &e->objs[sel];
    tf = &e->scene.entities[sel].transform;
    if (o->mat_idx >= 0 && o->mat_idx < (s32)e->hub.mat_count)
        mcol = (rgb555_t)e->hub.material[o->mat_idx].color;

    snprintf(buf, sizeof buf, "%s", o->name);
    buf[10] = 0;
    ps1_font_draw(&e->fb, x, 10, c_text(), buf);
    switch (o->mesh_kind) {
    case MESH_KIND_CUBE: kind = "CUBE"; break;
    case MESH_KIND_SPHERE: kind = "SPHERE"; break;
    case MESH_KIND_PLANE: kind = "PLANE"; break;
    case MESH_KIND_CYLINDER: kind = "CYLINDER"; break;
    case MESH_KIND_CAPSULE: kind = "CAPSULE"; break;
    case MESH_KIND_CAMERA: kind = "CAMERA"; break;
    default: kind = "OBJ"; break;
    }
    ps1_font_draw(&e->fb, x, 18, c_dim(), kind);

    ps1_font_draw(&e->fb, x, 26, xp_blue(), "POS");
    fmt_fx(tf->position.x, buf, sizeof buf);
    ps1_font_draw(&e->fb, x, 34, c_dim(), "X");
    ps1_font_draw(&e->fb, x + 10, 34, c_text(), buf);
    fmt_fx(tf->position.y, buf, sizeof buf);
    ps1_font_draw(&e->fb, x, 42, c_dim(), "Y");
    ps1_font_draw(&e->fb, x + 10, 42, c_text(), buf);
    fmt_fx(tf->position.z, buf, sizeof buf);
    ps1_font_draw(&e->fb, x, 50, c_dim(), "Z");
    ps1_font_draw(&e->fb, x + 10, 50, c_text(), buf);

    /* material block */
    ps1_font_draw(&e->fb, x, 58, xp_blue(), "MATERIAL");
    fb_rect(&e->fb, x, 66, x + 7, 72, mcol);
    fb_line(&e->fb, x, 72, x + 7, 72, c_border());
    over = in_rect(e, x + 10, 66, 24, 7);
    xp_btn(&e->fb, x + 10, 66, 24, 7, over ? c_hover() : c_panel());
    if (o->mat_idx >= 0 && o->mat_idx < (s32)e->hub.mat_count &&
        e->hub.material[o->mat_idx].texture >= 0)
        snprintf((char*)tex_ttl, sizeof tex_ttl, "TEX%d",
                 e->hub.material[o->mat_idx].texture);
    ps1_font_draw(&e->fb, x + 12, 67, c_text(), (const char*)tex_ttl);
    over = in_rect(e, x + 36, 66, 18, 7);
    xp_btn(&e->fb, x + 36, 66, 18, 7, over ? c_hover() : c_panel());
    ps1_font_draw(&e->fb, x + 38, 67, c_text(),
                  (o->mat_idx >= 0 && o->mat_idx < (s32)e->hub.mat_count &&
                   e->hub.material[o->mat_idx].two_sided)
                      ? "2S"
                      : "--");

    /* lighting + transform nudges */
    ps1_font_draw(&e->fb, x, 76, xp_blue(), "ADJUST");
    snprintf(buf, sizeof buf, "AM%d", (int)ps1_int(e->lighting.ambient));
    ps1_font_draw(&e->fb, x, 84, c_dim(), buf);
    snprintf(buf, sizeof buf, "DR%d", (int)ps1_int(e->lighting.dir.y));
    ps1_font_draw(&e->fb, x, 92, c_dim(), buf);
    snprintf(buf, sizeof buf, "LV%d", (int)e->lighting.levels);
    ps1_font_draw(&e->fb, x, 100, c_dim(), buf);
    snprintf(buf, sizeof buf, "RT%d",
             (int)ps1_int(fx_mul(tf->rotation.y, fx_div(fx_int(180), PI_FX))));
    ps1_font_draw(&e->fb, x, 108, c_dim(), buf);
    snprintf(buf, sizeof buf, "SC%d",
             (int)ps1_int(fx_mul(tf->scale.x, fx_int(100))));
    ps1_font_draw(&e->fb, x, 116, c_dim(), buf);
    draw_nudge(e, x + 40, 84);
    draw_nudge(e, x + 40, 92);
    draw_nudge(e, x + 40, 100);
    draw_nudge(e, x + 40, 108);
    draw_nudge(e, x + 40, 116);

    ps1_font_draw(&e->fb, x, 130, c_dim(), "L-CLICK SELECT");
    ps1_font_draw(&e->fb, x, 138, c_dim(), "R-DRAG ORBIT");
    ps1_font_draw(&e->fb, x, 146, c_dim(), "R+WASD FLY");
    ps1_font_draw(&e->fb, x, 154, c_dim(), "Q/E UP-DN  SHIFT SPRINT");
    ps1_font_draw(&e->fb, x, 164, c_dim(), "CTRL-Z UNDO");
}

static void draw_top_bar(Ps1Editor* e) {
    int x0 = PS1_VIEW_X;
    int x1 = PS1_VIEW_X + PS1_VIEW_W - 1;
    fb_rect(&e->fb, x0, 0, x1, PS1_TOPBAR - 1, c_panel());
    fb_rect(&e->fb, x0, PS1_TOPBAR - 1, x1, PS1_TOPBAR - 1, c_border());
}

static void draw_bottom_bar(Ps1Editor* e) {
    int y0 = PS1_VIEW_Y + PS1_VIEW_H;
    int x1 = PS1_VIEW_X + PS1_VIEW_W - 1;
    char buf[64];
    fb_rect(&e->fb, PS1_VIEW_X, y0, x1, PS1_SCREEN_H - 1, c_panel());
    fb_rect(&e->fb, PS1_VIEW_X, y0, x1, y0, c_border());
    snprintf(buf, sizeof buf, "OBJ:%u   FPS:%u   TRI:%u",
             (unsigned)e->scene.count, (unsigned)e->fps,
             (unsigned)e->prof.tris_rasterized);
    ps1_font_draw(&e->fb, PS1_VIEW_X + 2, y0 + 1, c_dim(), buf);
}

static void draw_hud(Ps1Editor* e) {
    char per[32];
    int name_right = PS1_VIEW_X + PS1_VIEW_W - 26 - 2 - 28;
    if (e->selected >= 0 && e->selected < (s32)e->scene.count) {
        static const char* dots = "..";
        const char* shown = NULL;
        int spaces =
            name_right - 5 - (TB_T0 + 3 * 17 + 16 + 2);
        snprintf(per, sizeof per, "%s", e->objs[e->selected].name);
        per[12] = 0;
        if (ps1_font_text_width(per) <= spaces) {
            shown = per;
        } else if (spaces >= ps1_font_text_width(dots) + 6) {
            int n = (spaces - ps1_font_text_width(dots)) / 6;
            shown = per;
            while ((int)strlen(per) > n) per[strlen(per) - 1] = 0;
        }
        if (shown) {
            ps1_font_draw(&e->fb, name_right - ps1_font_text_width(shown) - 3,
                          1, xp_blue(), shown);
        }
    }
}

static void draw_log(Ps1Editor* e) {
    int hsl = PS1_VIEW_Y + PS1_VIEW_H;
    int n = e->log_count;
    int i;
    int y = hsl - 6 - n * 8;
    int w = 130;
    fb_rect(&e->fb, PS1_VIEW_X, 60, PS1_VIEW_X + w, hsl - 6, c_panel());
    xp_title_bar(&e->fb, PS1_VIEW_X, 60, PS1_VIEW_X + w, 68);
    ps1_font_draw(&e->fb, PS1_VIEW_X + 2, 61, xp_white(), "CONSOLE");
    if (y < 72) y = 72;
    for (i = 0; i < n && y < hsl - 6; i++) {
        u16 idx = (u16)((e->log_head + (u16)(PS1_LOG_LINES - n + i)) %
                        PS1_LOG_LINES);
        ps1_font_draw(&e->fb, PS1_VIEW_X + 2, y, c_text(), e->log_lines[idx]);
        y += 8;
    }
}

static void draw_play_overlay(Ps1Editor* e) {
    const char* msg = (e->state == STATE_PLAY) ? "PLAY MODE" : "PAUSED";
    int w = ps1_font_text_width(msg) + 8;
    int x = PS1_VIEW_X + (PS1_VIEW_W - w) / 2;
    rgb555_t lo = xp_shadow();
    xp_title_bar(&e->fb, x, PS1_VIEW_Y, x + w, PS1_VIEW_Y + 8);
    ps1_font_draw(&e->fb, x + 4, PS1_VIEW_Y + 1, xp_white(), msg);
    fb_line(&e->fb, x, PS1_VIEW_Y + 8, x + w, PS1_VIEW_Y + 8, lo);
    fb_line(&e->fb, x + w, PS1_VIEW_Y, x + w, PS1_VIEW_Y + 8, lo);
}

static void draw_profiler(Ps1Editor* e) {
    char buf[56];
    snprintf(buf, sizeof buf, "TRIS %u RAST %u CULL %u CLIP %u PIX %llu",
             (unsigned)e->prof.tris_in, (unsigned)e->prof.tris_rasterized,
             (unsigned)e->prof.tris_culled, (unsigned)e->prof.tris_clipped,
             (unsigned long long)e->prof.pixels_written);
    ps1_font_draw(&e->fb, PS1_VIEW_X + 1, 50, VP_DIM,
                  buf);
}

static void draw_assets(Ps1Editor* e) {
    int y0 = PS1_VIEW_Y + PS1_VIEW_H;
    int x0 = PS1_VIEW_X;
    int x1 = PS1_SCREEN_W - PS1_PANEL_R - 1;
    int ry, row;
    char name[16];
    draw_bottom_bar(e);
    xp_title_bar(&e->fb, x0 + 1, y0 + 1, x1 - 1, y0 + 8);
    ps1_font_draw(&e->fb, x0 + 2, y0 + 2, xp_white(), "ASSETS[I] TEX");
    ps1_font_draw(&e->fb, x0 + 62, y0 + 2, xp_white(), "MODELS");
    if (e->selected >= 0 && e->selected < (s32)e->scene.count) {
        snprintf(name, sizeof name, "%s", e->objs[e->selected].name);
        name[11] = 0;
        ps1_font_draw(&e->fb, x1 - ps1_font_text_width(name) - 2, y0 + 2,
                      xp_white(), name);
    }
    for (row = 0; row < 4; row++) {
        int it = e->assets_scroll + row;
        int io = e->assets_scroll + row;
        ry = y0 + 9 + row * 7;
        if (it < g_assets_tex_n) {
            const char* sl = strrchr(g_assets_tex[it], '/');
            int over = in_rect(e, x0 + 2, ry, 56, 7);
            xp_btn(&e->fb, x0 + 2, ry, 56, 7, over ? c_hover() : c_panel());
            snprintf(name, sizeof name, "%s", sl ? sl + 1 : g_assets_tex[it]);
            name[9] = 0;
            ps1_font_draw(&e->fb, x0 + 3, ry, c_text(), name);
        }
        if (io < g_assets_obj_n) {
            const char* sl = strrchr(g_assets_obj[io], '/');
            int over = in_rect(e, x0 + 62, ry, 56, 7);
            xp_btn(&e->fb, x0 + 62, ry, 56, 7, over ? c_hover() : c_panel());
            snprintf(name, sizeof name, "%s", sl ? sl + 1 : g_assets_obj[io]);
            name[9] = 0;
            ps1_font_draw(&e->fb, x0 + 63, ry, c_text(), name);
        }
    }
}

static void draw_editor(Ps1Editor* e) {
    Ps1PipeConfig cfg;
    Ps1Camera gcam;
    Ps1Camera* cam = &e->camera;
    u32 i;

    ps1_cam_update(&e->camera);
    ps1_fb_clear(&e->fb, e->bg);
    ps1_depth_clear(&e->depth);

    if (e->state != STATE_EDIT) {
        s32 nod = find_camera_node(e);
        if (nod >= 0) {
            const Ps1Transform* t = &e->scene.entities[nod].transform;
            gcam = e->camera;
            gcam.pos = t->position;
            gcam.yaw = t->rotation.y;
            gcam.pitch = t->rotation.x;
            ps1_cam_update(&gcam);
            cam = &gcam;
        }
    }

    cfg.camera = cam;
    cfg.lighting = &e->lighting;
    cfg.materials = e->hub.material;
    cfg.framebuffer = &e->fb;
    cfg.depth = &e->depth;
    cfg.cull_mode = e->cull_mode;
    cfg.depth_mode = e->depth_mode;
    cfg.lighting_enabled = e->lighting_enabled;
    cfg.texture_enabled = e->texture_enabled;
    cfg.prof = &e->prof;

    if (e->state == STATE_EDIT) draw_grid(e);

    for (i = 0; i < e->scene.count; i++) {
        const Ps1Entity* en = &e->scene.entities[i];
        if (e->objs[i].is_camera) {
            draw_camera_node(e, en);
            continue;
        }
        if (!en->active || !en->mesh) continue;
        ps1_pipe_draw_mesh(&cfg, en->mesh, &en->transform);
        if ((s32)i == e->selected && e->state == STATE_EDIT)
            draw_selection(e, en);
    }
    if (e->selected >= 0 && e->tool != TOOL_SELECT &&
        e->state == STATE_EDIT)
        draw_gizmo(e, &e->scene.entities[e->selected].transform);

    draw_top_bar(e);
    draw_toolbar(e);
    draw_left_panel(e);
    draw_right_panel(e);
    draw_hud(e);
    if (e->show_profiler) draw_profiler(e);
    if (e->state != STATE_EDIT) draw_play_overlay(e);
    if (e->show_assets && e->state == STATE_EDIT) draw_assets(e);
    else draw_bottom_bar(e);
    if (e->show_log) draw_log(e);
}

static void draw_project_manager(Ps1Editor* e) {
    int i;
    char name[40];
    int n = g_proj_count;
    int y = 78;
    ps1_fb_clear(&e->fb, c_panel());
    /* dark title bar */
    xp_title_bar(&e->fb, 0, 0, PS1_SCREEN_W - 1, 9);
    ps1_font_draw(&e->fb, 3, 1, xp_white(), "PS1 EDITOR");
    ps1_font_draw(&e->fb, 3, 11, c_text(), "PROJECT MANAGER");

    ui_button(e, "NEW SCENE", 30, 40, 180, 14);
    ui_button(e, "OPEN FILE", 30, 56, 180, 14);

    fb_rect(&e->fb, 16, 73, 224, 113, c_panel());
    fb_rect(&e->fb, 16, 73, 224, 73, c_border());
    fb_rect(&e->fb, 16, 113, 224, 113, c_border());
    xp_title_bar(&e->fb, 16, 73, 224, 80);
    ps1_font_draw(&e->fb, 20, 74, xp_white(), "RECENT");
    y = 82;
    for (i = e->proj_scroll; i < n && y < 112; i++, y += 9) {
        rgb555_t fill = c_panel();
        rgb555_t tcol = c_text();
        int sel = in_rect(e, 18, y, 220, 8);
        if (sel) { fill = c_select(); tcol = xp_white(); }
        fb_rect(&e->fb, 18, y, 220, y + 7, fill);
        snprintf(name, sizeof name, "%s", g_proj_files[i]);
        name[32] = 0;
        ps1_font_draw(&e->fb, 20, y + 1, tcol, name);
    }
    ps1_font_draw(&e->fb, 20, 120, c_dim(),
                  "[N] new  [O] open  [ESC] quit");
    ps1_font_draw(&e->fb, 20, 140, c_dim(), "FILES IN: ./");
    snprintf(name, sizeof name, "%d scenes", n);
    ps1_font_draw(&e->fb, 20, 148, c_dim(), name);
}

void ps1_editor_render(Ps1Editor* e) {
    if (e->mode == MODE_PROJECT)
        draw_project_manager(e);
    else
        draw_editor(e);
}
