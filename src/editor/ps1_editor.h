#ifndef PS1_EDITOR_H
#define PS1_EDITOR_H

#include "assets/ps1_mesh_loader.h"
#include "assets/ps1_obj_loader.h"
#include "assets/ps1_resources.h"
#include "core/ps1_timing.h"
#include "graphics/ps1_color.h"
#include "graphics/ps1_depth.h"
#include "graphics/ps1_framebuffer.h"
#include "renderer3d/ps1_camera.h"
#include "renderer3d/ps1_lighting.h"
#include "scene/ps1_scene.h"
#include "sys/ps1_profiler.h"

struct Ps1Window;

#define PS1_PANEL_L PS1_VIEW_X
#define PS1_PANEL_R 72
#define PS1_TOPBAR PS1_VIEW_Y
#define PS1_BOTTOM 40

#define PS1_SCENE_EXT ".ps13d"
#define PS1_HIST_MAX 24
#define PS1_LOG_LINES 24
#define PS1_LOG_LEN 64

/* Transform tool handled by the viewport drag. */
typedef enum {
    TOOL_SELECT = 0,
    TOOL_MOVE,
    TOOL_ROTATE,
    TOOL_SCALE
} Ps1Tool;

typedef enum {
    MODE_PROJECT = 0,
    MODE_EDIT
} Ps1Mode;

typedef enum {
    STATE_EDIT = 0,
    STATE_PLAY,
    STATE_PAUSED
} Ps1PlayState;

typedef struct {
    u16 mesh_idx;
    s16 mat_idx;
    char name[PS1_OBJ_NAME_MAX];
    s16 mesh_kind; /* Ps1MeshKind */
    char mesh_path[PS1_OBJ_PATH_MAX];
    u8 is_camera;
} Ps1Object;

/* Deep snapshot of everything an edit op can change (emit/hub counts). */
typedef struct {
    Ps1Scene scene;
    Ps1Object objs[PS1_MAX_ENTITIES];
    u16 mesh_count;
    u16 mat_count;
    s32 selected;
} Ps1SceneSnap;

typedef struct {
    Ps1ResourceHub hub;
    Ps1Scene scene;
    Ps1Object objs[PS1_MAX_ENTITIES];
    Ps1Camera camera;
    Ps1Lighting lighting;
    Ps1Framebuffer fb;
    Ps1Depth16 depth;
    Ps1Profiler prof;
    Ps1Timer timer;
    rgb555_t bg;
    s8 default_tex;
    u32 fps;
    s32 selected;
    u8 depth_mode;
    u8 cull_mode;
    u8 lighting_enabled;
    u8 texture_enabled;
    u8 show_profiler;
    u8 key_tab_prev;
    u8 key_t_prev;
    u8 key_f_prev;
    u8 key_z_prev;
    u8 key_l_prev;
    u8 key_c_prev;
    u8 key_g_prev;
    u8 key_w_prev;
    u8 key_e_prev;
    u8 key_r_prev;
    u8 key_q_prev;
    u8 key_i_prev;
    u8 key_ctrl_prev;
    u8 key_p_prev;
    u8 key_b_prev;
    u8 key_del_prev;
    u8 key_d_prev;
    u8 key_enter_prev;
    u8 key_bs_prev;
    u8 key_n_prev;
    u8 key_o_prev;
    s32 m_x;
    s32 m_y;
    u8 mouse_down;
    u8 mouse_prev;
    u8 mouse_click;
    u8 dragging;
    u8 dragging_pan;
    u8 dragging_alt;
    u8 flying;
    u8 l_down;
    u8 l_moved;
    u8 l_pick;
    s32 l_x0;
    s32 l_y0;
    s32 d_x0;
    s32 d_y0;
    s32 p_x0;
    s32 p_y0;
    FVec3 p_pos0;
    fixed_t drag_yaw;
    fixed_t drag_pitch;
    u32 frames;

    Ps1Mode mode;
    Ps1PlayState state;
    Ps1Tool tool;
    u8 tool_drag;
    s8 t_axis;
    s32 t_dx0;
    s32 t_dy0;
    fixed_t t_sx0;
    fixed_t t_sy0;
    FVec3 t_pos0;
    FVec3 t_rot0;
    fixed_t t_scl0;

    Ps1SceneSnap snap_pool[PS1_HIST_MAX];
    u16 hist_len;
    u16 hist_undo;
    Ps1SceneSnap play_snap;

    char tex_path[PS1_MAX_TEXTURES][PS1_OBJ_PATH_MAX];
    char scene_path[PS1_OBJ_PATH_MAX];
    u8 scene_dirty;

    char log_lines[PS1_LOG_LINES][PS1_LOG_LEN];
    u16 log_head;
    u16 log_count;
    u8 show_log;
    u8 show_assets;
    s32 assets_scroll;

    s32 proj_scroll;
} Ps1Editor;

int ps1_editor_init(Ps1Editor* e);
int ps1_editor_load_obj(Ps1Editor* e, const char* path);
void ps1_editor_update(Ps1Editor* e, struct Ps1Window* win);
void ps1_editor_render(Ps1Editor* e);
void ps1_editor_camera_reset(Ps1Editor* e);
void ps1_editor_to_edit(Ps1Editor* e);
void ps1_editor_snap(Ps1Editor* e);
void ps1_editor_undo(Ps1Editor* e);
void ps1_editor_redo(Ps1Editor* e);
void ps1_editor_log(Ps1Editor* e, const char* fmt, ...);
int ps1_editor_save_scene(Ps1Editor* e, const char* path);
int ps1_editor_load_scene(Ps1Editor* e, const char* path);
int ps1_editor_new_scene(Ps1Editor* e);
int ps1_editor_scan_scene_files(char (*paths)[PS1_OBJ_PATH_MAX], int cap);

#endif