#ifndef PS1_CONFIG_H
#define PS1_CONFIG_H

/* The engine render target is 320x240 (PS1 native). The editor canvas is
 * larger so the UI panels sit around the viewport and never cover it:
 *   left panel 56 | viewport 320x240 | right panel 72
 *   top bar 9     | viewport        | bottom bar 40 (assets dock)
 */
#define PS1_VIEW_X 56
#define PS1_VIEW_Y 9
#define PS1_VIEW_W 320
#define PS1_VIEW_H 240
#define PS1_SCREEN_W (PS1_VIEW_X + PS1_VIEW_W + 72)
#define PS1_SCREEN_H (PS1_VIEW_Y + PS1_VIEW_H + 40)
#define PS1_SCREEN_PITCH (PS1_SCREEN_W)
#define PS1_FRAMEBUFFER_BYTES (PS1_SCREEN_W * PS1_SCREEN_H * 2u)
#define PS1_DEPTH16_BYTES (PS1_SCREEN_W * PS1_SCREEN_H * 2u)

#define PS1_TARGET_FRAME_US 16743u

#define PS1_SCR_SHIFT 12

/* PS1-faithful rendering limits. These deliberately reduce the precision of
 * the projected vertex positions and the UV interpolation to reproduce a
 * PlayStation 1 texture mapper: affine (non perspective-correct) warping,
 * vertex jitter and texel swimming. These are the only limits kept in the
 * engine: everything else (PS1 memory pool, budgets, strict audits) was
 * removed.
 *
 *   PS1_RATIO_CLEAR  low bits dropped from the perspective-divided
 *                        normalized coordinates (Q16) before screen scaling
 *   PS1_SCREEN_CLEAR low bits dropped from the final screen positions
 *                        (Q12) - GTE-style subpixel snap
 *   PS1_UV_CLEAR     low bits dropped from the sampled UV (Q12)
 *
 * Larger values = more aliasing / more visible jitter. Set to 0 to disable.
 */
#ifndef PS1_RATIO_CLEAR
#define PS1_RATIO_CLEAR 7
#endif
#ifndef PS1_SCREEN_CLEAR
#define PS1_SCREEN_CLEAR 8
#endif
#ifndef PS1_UV_CLEAR
#define PS1_UV_CLEAR 4
#endif

#ifndef PS1_FIXED_SHIFT
#define PS1_FIXED_SHIFT 16
#endif

#if (PS1_FIXED_SHIFT != 8) && (PS1_FIXED_SHIFT != 16)
#error "PS1_FIXED_SHIFT must be 8 (Q8.8) or 16 (Q16.16)"
#endif

#ifndef PS1_MAX_VERTS
#define PS1_MAX_VERTS 4096
#endif

#ifndef PS1_MAX_TRIS
#define PS1_MAX_TRIS 8192
#endif

#ifndef PS1_MAX_ENTITIES
#define PS1_MAX_ENTITIES 16
#endif

#ifndef PS1_MAX_MATERIALS
#define PS1_MAX_MATERIALS 32
#endif

#ifndef PS1_MAX_TEXTURES
#define PS1_MAX_TEXTURES 32
#endif

#ifndef PS1_MAX_TEX_PIXELS
#define PS1_MAX_TEX_PIXELS 524288
#endif

#ifndef PS1_MAX_MESHES
#define PS1_MAX_MESHES 16
#endif

#ifndef PS1_MAX_CLIP_VERTS
#define PS1_MAX_CLIP_VERTS 8
#endif

#define PS1_CULL_BACK 1
#define PS1_CULL_FRONT 2
#define PS1_CULL_NONE 3

#define PS1_DEPTH_PAINTER 1
#define PS1_DEPTH_Z16 2

#define PS1_WIN_SCALE 2

#endif