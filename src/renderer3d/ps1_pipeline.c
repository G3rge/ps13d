#include "core/ps1_lut.h"
#include "graphics/ps1_clipping.h"
#include "graphics/ps1_color.h"
#include "graphics/ps1_rasterizer.h"
#include "renderer3d/ps1_pipeline.h"

typedef struct {
    Ps1ScreenVert v[3];
    fixed_t zavg;
    rgb555_t color;
    u8 shade;
    s8 tex;
} WorkTri;

static WorkTri g_pool[2 * PS1_MAX_TRIS];
static u16 g_order[2 * PS1_MAX_TRIS];
static FVec3 g_view[PS1_MAX_VERTS];
static Ps1ClipVert g_clip[PS1_MAX_CLIP_VERTS];

static fixed_t fxto_scr12(fixed_t v) {
#if (PS1_FIXED_SHIFT >= PS1_SCR_SHIFT)
    return v >> (PS1_FIXED_SHIFT - PS1_SCR_SHIFT);
#else
    return v << (PS1_SCR_SHIFT - PS1_FIXED_SHIFT);
#endif
}

/* Quantize a fixed-point value by dropping its low bits (floor). Used to
 * deliberately limit the precision of projected vertex positions and UVs. */
static inline fixed_t ps1_quant(fixed_t v, u32 clear_bits) {
    fixed_t mask = (fixed_t)((1u << clear_bits) - 1u);
    return v & ~mask;
}

static void project_vert(const Ps1ClipVert* cv, fixed_t focal12,
                         Ps1ScreenVert* out) {
    fixed_t xr, yr, xr12, yr12;
    out->invz = ps1_fast_recip(cv->vp.z);
    out->z = cv->vp.z;
    xr = fx_mul(cv->vp.x, out->invz);
    yr = fx_mul(cv->vp.y, out->invz);
    /* Low-precision perspective like the PS1 GTE: the normalized coordinates
     * are snapped to a coarse grid before they are scaled into screen space.
     * As the camera moves, vertices therefore jump in small discrete steps
     * instead of translating smoothly (PS1 vertex jitter). */
    xr = ps1_quant(xr, PS1_RATIO_CLEAR);
    yr = ps1_quant(yr, PS1_RATIO_CLEAR);
    xr12 = fxto_scr12(xr);
    yr12 = fxto_scr12(yr);
    out->sx = ps1_scr_f(PS1_VIEW_X + (PS1_VIEW_W >> 1)) +
              (fixed_t)(((s64)focal12 * xr12) >> PS1_SCR_SHIFT);
    out->sy = ps1_scr_f(PS1_VIEW_Y + (PS1_VIEW_H >> 1)) -
              (fixed_t)(((s64)focal12 * yr12) >> PS1_SCR_SHIFT);
    /* GTE screen-space output also has limited precision (subpixel snap). */
    out->sx = ps1_quant(out->sx, PS1_SCREEN_CLEAR);
    out->sy = ps1_quant(out->sy, PS1_SCREEN_CLEAR);
    out->u = cv->u;
    out->v = cv->v;
}

void ps1_pipe_draw_mesh(const Ps1PipeConfig* cfg, const Ps1Mesh* mesh,
                        const Ps1Transform* tf) {
    Ps1Profiler* pr = cfg->prof;
    Ps1Mat4 model, nm, comb;
    fixed_t focal12 = fxto_scr12(cfg->camera->focal);
    u32 i, k, t;
    u32 queue = 0;

    ps1_transform_model(tf, &model, &nm);
    ps1_mat4_mul(&cfg->camera->view, &model, &comb);

    for (i = 0; i < (u32)mesh->vertex_count; i++)
        ps1_mat4_transform(&comb, &mesh->verts[i].pos, &g_view[i]);
    pr->verts_in += mesh->vertex_count;
    pr->math_ops += (u64)mesh->vertex_count * 12u;

    for (t = 0; t < (u32)mesh->triangle_count; t++) {
        const Ps1Triangle* tri = &mesh->tris[t];
        const Ps1Material* mat = &cfg->materials[tri->material];
        Ps1ClipVert cv[3];
        Ps1ClipVert* poly = g_clip;
        u8 pcount;
        u8 shade = 7;
        u8 all_in = 1;
        u8 p1;

        pr->tris_in++;

        if (cfg->lighting_enabled && cfg->lighting) {
            FVec3 wn;
            ps1_mat4_direction(&nm, &tri->normal, &wn);
            shade = ps1_light_shade(cfg->lighting, &wn);
            pr->math_ops += 6u;
        }

        for (k = 0; k < 3; k++) {
            u16 vi = tri->v[k];
            cv[k].vp = g_view[vi];
            cv[k].u = mesh->verts[vi].u;
            cv[k].v = mesh->verts[vi].v;
            if (cv[k].vp.z < cfg->camera->near_z) all_in = 0;
        }

        if (cv[0].vp.z < cfg->camera->near_z &&
            cv[1].vp.z < cfg->camera->near_z &&
            cv[2].vp.z < cfg->camera->near_z) {
            continue;
        }

        if (all_in) {
            poly[0] = cv[0];
            poly[1] = cv[1];
            poly[2] = cv[2];
            pcount = 3;
        } else {
            ps1_clip_triangle(&cv[0], &cv[1], &cv[2], cfg->camera->near_z,
                              poly, &pcount);
            if (pcount >= 3) pr->tris_clipped++;
        }
        if (pcount < 3) continue;
        pr->math_ops += 4u;

        for (p1 = 1; (int)p1 + 1 < (int)pcount; p1++) {
            u16 vi[3];
            Ps1ScreenVert sv[3];
            s64 area;
            u8 front;
            WorkTri* w;

            vi[0] = 0;
            vi[1] = p1;
            vi[2] = p1 + 1;
            for (k = 0; k < 3; k++)
                project_vert(&poly[vi[k]], focal12, &sv[k]);

            area = (s64)(sv[1].sx - sv[0].sx) * (sv[2].sy - sv[0].sy) -
                   (s64)(sv[1].sy - sv[0].sy) * (sv[2].sx - sv[0].sx);

            front = (area <= 0) ? 1 : 0;
            if (mat->two_sided) front = 1;

            if (cfg->cull_mode == PS1_CULL_BACK && !front) {
                pr->tris_culled++;
                continue;
            }
            if (cfg->cull_mode == PS1_CULL_FRONT && front) {
                pr->tris_culled++;
                continue;
            }
            if (queue >= 2u * PS1_MAX_TRIS) continue;

            w = &g_pool[queue];
            w->v[0] = sv[0];
            w->v[1] = sv[1];
            w->v[2] = sv[2];
            w->zavg = (fixed_t)(((s64)sv[0].z + sv[1].z + sv[2].z) / 3);
            if (cfg->texture_enabled && mat->texture >= 0) {
                w->color = mat->color;
                w->shade = shade;
                w->tex = mat->texture;
            } else {
                w->color = ps1_color_shade(mat->color, shade);
                w->shade = shade;
                w->tex = -1;
            }
            queue++;
        }
    }

    for (i = 0; i < queue; i++) g_order[i] = (u16)i;

    if (cfg->depth_mode == PS1_DEPTH_PAINTER) {
        for (i = 1; i < queue; i++) {
            u16 key = g_order[i];
            fixed_t kz = g_pool[key].zavg;
            u32 j = i;
            while (j > 0 && g_pool[g_order[j - 1]].zavg < kz) {
                g_order[j] = g_order[j - 1];
                j--;
            }
            g_order[j] = key;
        }
    }

    {
        Ps1RasterParams rp;
        rp.depth = (cfg->depth_mode == PS1_DEPTH_Z16) ? cfg->depth : NULL;
        rp.color = 0;
        rp.tex = NULL;
        rp.shade_level = 0;
        for (i = 0; i < queue; i++) {
            WorkTri* w = &g_pool[g_order[i]];
            const Ps1Texture* tex = NULL;
            if (w->tex >= 0) tex = ps1_tex_get(w->tex);
            if (tex) {
                rp.color = w->color;
                rp.shade_level = w->shade;
                rp.tex = tex;
            } else {
                rp.color = w->color;
                rp.shade_level = 0;
                rp.tex = NULL;
            }
            {
                u32 px = ps1_raster_triangle(cfg->framebuffer, &rp,
                                             &w->v[0], &w->v[1], &w->v[2]);
                pr->pixels_written += px;
                if (tex) pr->texel_reads += px;
            }
            pr->tris_rasterized++;
        }
    }
}