#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/ps1_obj_loader.h"
#include "graphics/ps1_texture.h"

#define MAX_POS PS1_MAX_VERTS
#define MAX_UV PS1_MAX_VERTS
#define MAX_NOR PS1_MAX_VERTS
#define LINE_MAX 512
#define OBJ_MAT_NAME_MAX 64

#define UV_NONE 0xFFFFFFFFu

static FVec3 g_pos[MAX_POS];
static fixed_t g_u[MAX_UV];
static fixed_t g_v[MAX_UV];
static FVec3 g_nor[MAX_NOR];

static u32 g_s_v[PS1_MAX_VERTS];
static u32 g_s_t[PS1_MAX_VERTS];
static u32 g_s_n[PS1_MAX_VERTS];

static void trim(char* s) {
    char* p = s;
    char* end;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\n' || end[-1] == '\r'))
        end--;
    *end = 0;
}

/* Parses a decimal literal (sign, integer part, fraction) into fixed point
 * using integer arithmetic only - no floating point in the engine. */
static fixed_t parse_fixed(const char* s) {
    s64 ip = 0;
    s64 fp = 0;
    s64 p10 = 1;
    s32 sign = 1;
    s32 frac = 0;
    s64 out;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        ip = ip * 10 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && frac < 9) {
            fp = fp * 10 + (*s - '0');
            p10 *= 10;
            frac++;
            s++;
        }
        while (*s >= '0' && *s <= '9') s++;
        if (frac) fp = ((fp << FX_SHIFT) + (p10 >> 1)) / p10;
    }
    out = (ip << FX_SHIFT) + fp;
    return (fixed_t)(sign * out);
}

/* OBJ indices are 1-based; negative indices are relative to the current
 * count. Tokens may contain '/' (v/vt/vn) so strtol stops at the separator.
 * Returns index, or -1 if invalid. */
static s32 parse_index(const char* s, u32 count) {
    long v;
    if (!s || !*s) return -1;
    v = strtol(s, NULL, 10);
    if (v == 0) return -1;
    if (v > 0) {
        if ((u32)v > count) return -1;
        return (s32)(v - 1);
    }
    if ((u32)(-v) > count) return -1;
    return (s32)((long)count + v);
}

static char* token_next(char** pp) {
    char* p = *pp;
    char* out;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return NULL;
    out = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) {
        *p = 0;
        p++;
    }
    *pp = p;
    return out;
}

static void scan_box(FVec3* bmin, FVec3* bmax, fixed_t x, fixed_t y,
                     fixed_t z) {
    if (x < bmin->x) bmin->x = x;
    if (y < bmin->y) bmin->y = y;
    if (z < bmin->z) bmin->z = z;
    if (x > bmax->x) bmax->x = x;
    if (y > bmax->y) bmax->y = y;
    if (z > bmax->z) bmax->z = z;
}

static int parse_pos(char* p, u32* n, FVec3* bmin, FVec3* bmax) {
    char* sp = p;
    char* t1;
    char* t2;
    char* t3;
    if (*n >= MAX_POS) return 0;
    t1 = token_next(&sp);
    t2 = token_next(&sp);
    t3 = token_next(&sp);
    if (!t1 || !t2 || !t3) return 0;
    g_pos[*n] = fv3(parse_fixed(t1), parse_fixed(t2), parse_fixed(t3));
    scan_box(bmin, bmax, g_pos[*n].x, g_pos[*n].y, g_pos[*n].z);
    (*n)++;
    return 1;
}

static int parse_uv(char* p, u32* n) {
    char* sp = p;
    char* t1;
    char* t2;
    fixed_t u, v;
    if (*n >= MAX_UV) return 0;
    t1 = token_next(&sp);
    t2 = token_next(&sp);
    if (!t1 || !t2) return 0;
    u = parse_fixed(t1);
    v = parse_fixed(t2);
    g_u[*n] = u;
    g_v[*n] = v;
    (*n)++;
    return 1;
}

static int parse_normal(char* p, u32* n) {
    char* sp = p;
    char* t1;
    char* t2;
    char* t3;
    FVec3 v;
    if (*n >= MAX_NOR) return 0;
    t1 = token_next(&sp);
    t2 = token_next(&sp);
    t3 = token_next(&sp);
    if (!t1 || !t2 || !t3) return 0;
    v = fv3(parse_fixed(t1), parse_fixed(t2), parse_fixed(t3));
    g_nor[*n] = fv3_normalize(&v);
    (*n)++;
    return 1;
}

/* Parses one "f" line. Supports v, v/vt, v//vn and v/vt/vn formats. */
static int parse_face(char* line, u32 npos, u32 nuv, u32 nnor, u32* fv,
                      u32* ft, u32* fn, u32* nout) {
    char* tok;
    u32 nf = 0;
    tok = strtok(line, " \t\r\n");
    if (!tok) return 0;
    while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
        char* c;
        s32 vi = -1;
        s32 ti = -1;
        s32 ni = -1;
        if (nf >= PS1_OBJ_FACE_MAX) return 0;
        vi = parse_index(tok, npos);
        c = strchr(tok, '/');
        if (c) {
            c++;
            if (*c && *c != '/') ti = parse_index(c, nuv);
            c = strchr(c, '/');
            if (c) {
                c++;
                if (*c) ni = parse_index(c, nnor);
            }
        }
        if (vi < 0) return 0;
        fv[nf] = (u32)vi;
        ft[nf] = (ti >= 0) ? (u32)ti : UV_NONE;
        fn[nf] = (ni >= 0) ? (u32)ni : UV_NONE;
        nf++;
    }
    *nout = nf;
    return nf >= 3;
}

static int find_vertex(const Ps1Mesh* m, u32 v, u32 t, u32 n) {
    u32 i;
    for (i = 0; i < m->vertex_count; i++)
        if (g_s_v[i] == v && g_s_t[i] == t && g_s_n[i] == n) return (int)i;
    return -1;
}

static int add_corner(Ps1Mesh* m, u32 v, u32 t, u32 n) {
    int idx = find_vertex(m, v, t, n);
    FVec3 nor;
    fixed_t u;
    fixed_t vv;
    if (idx >= 0) return idx;
    if (m->vertex_count >= PS1_MAX_VERTS) return -1;
    nor = (n < MAX_NOR) ? g_nor[n] : fv3(0, 0, 0);
    u = (t < MAX_UV) ? g_u[t] : 0;
    vv = (t < MAX_UV) ? g_v[t] : 0;
    idx = ps1_mesh_add_vertex_ex(m, g_pos[v], nor, u, vv);
    if (idx < 0) return -1;
    g_s_v[idx] = v;
    g_s_t[idx] = t;
    g_s_n[idx] = n;
    return idx;
}

static int add_face(Ps1Mesh* m, u32 nf, u32* fv, u32* ft, u32* fn) {
    u32 idx[PS1_OBJ_FACE_MAX];
    u32 k;
    for (k = 0; k < nf; k++) {
        idx[k] = (u32)add_corner(m, fv[k], ft[k], fn[k]);
        if ((int)idx[k] < 0) return 0;
    }
    for (k = 1; k + 1 < nf; k++) {
        if (ps1_mesh_add_triangle(m, (u16)idx[0], (u16)idx[k],
                                  (u16)idx[k + 1], 0) < 0)
            return 0;
    }
    return 1;
}

/* True if the mesh has any UV data worth sampling (non-zero coordinates). */
static int mesh_has_uvs(const Ps1Mesh* m) {
    u32 i;
    for (i = 0; i < m->vertex_count; i++)
        if (m->verts[i].u != 0 || m->verts[i].v != 0) return 1;
    return 0;
}

/* Converts normalized OBJ UVs (0..FX_ONE) to tile-unit UVs used by the
 * renderer (1 tile = 1 << PS1_SCR_SHIFT per full texture) and flips V so
 * OBJ bottom-left maps to the texture pool's top row. */
static void scale_uvs(Ps1Mesh* m) {
    u32 i;
    for (i = 0; i < m->vertex_count; i++) {
        fixed_t nu = (fixed_t)(((s64)m->verts[i].u << PS1_SCR_SHIFT) >>
                               PS1_FIXED_SHIFT);
        m->verts[i].u = nu;
        m->verts[i].v =
            (fixed_t)(((s64)(FX_ONE - m->verts[i].v) << PS1_SCR_SHIFT) >>
                      PS1_FIXED_SHIFT);
    }
}

static void copy_clip(char* dst, u32 cap, const char* src) {
    u32 i = 0;
    if (!cap) return;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void path_join(const char* obj_path, const char* name, char* out,
                      u32 cap) {
    const char* slash = strrchr(obj_path, '/');
    const char* bslash = strrchr(obj_path, '\\');
    size_t n;
    if (!slash || (bslash && bslash > slash)) slash = bslash;
    if (slash) {
        n = (size_t)(slash - obj_path + 1);
        if (n >= cap) n = cap - 1;
        memcpy(out, obj_path, n);
        out[n] = 0;
        copy_clip(out + n, cap - n, name);
    } else {
        copy_clip(out, cap, name);
    }
}

static void load_mtl(const char* obj_path, const char* mtl_name,
                     const char* mat_name, rgb555_t* color_out,
                     char* tex_path_out, u32 texcap) {
    char path[PS1_OBJ_PATH_MAX];
    char line[LINE_MAX];
    FILE* f;
    int in_mat = 0;
    if (!mtl_name || !mtl_name[0] || !mat_name || !mat_name[0]) return;
    path_join(obj_path, mtl_name, path, sizeof path);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char tok[LINE_MAX];
        char* p = line;
        trim(line);
        if (!line[0]) continue;
        while (*p && *p != ' ' && *p != '\t') p++;
        memcpy(tok, line, (size_t)(p - line));
        tok[p - line] = 0;

        if (!strcmp(tok, "newmtl")) {
            char* q = p;
            while (*q == ' ' || *q == '\t') q++;
            trim(q);
            in_mat = (strcmp(q, mat_name) == 0);
            continue;
        }
        if (!in_mat) continue;

        if (!strcmp(tok, "Kd")) {
            char* sp = p;
            char* t1;
            char* t2;
            char* t3;
            s64 r, g, b;
            t1 = token_next(&sp);
            t2 = token_next(&sp);
            t3 = token_next(&sp);
            if (t1 && t2 && t3) {
                r = ((s64)parse_fixed(t1) * 31) >> PS1_FIXED_SHIFT;
                g = ((s64)parse_fixed(t2) * 31) >> PS1_FIXED_SHIFT;
                b = ((s64)parse_fixed(t3) * 31) >> PS1_FIXED_SHIFT;
                if (r < 0) r = 0;
                if (r > 31) r = 31;
                if (g < 0) g = 0;
                if (g > 31) g = 31;
                if (b < 0) b = 0;
                if (b > 31) b = 31;
                *color_out = ps1_color_pack((u8)r, (u8)g, (u8)b);
            }
        } else if (!strcmp(tok, "map_Kd")) {
            char* q = p;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '"' || *q == '\'') q++;
            trim(q);
            if (strchr(q, ' ') || strchr(q, '\t')) {
                char* last = q;
                char* r = q;
                while (*r) {
                    if (*r == ' ' || *r == '\t') last = r;
                    r++;
                }
                while (*last == ' ' || *last == '\t') last++;
                q = last;
            }
            trim(q);
            if (*q) {
                if (*q == '/' || *q == '\\' || strchr(q, ':') != NULL) {
                    copy_clip(tex_path_out, texcap, q);
                } else {
                    char dir[PS1_OBJ_PATH_MAX];
                    dir[0] = 0;
                    {
                        const char* slash = strrchr(path, '/');
                        const char* bslash = strrchr(path, '\\');
                        size_t dn;
                        if (!slash || (bslash && bslash > slash)) slash = bslash;
                        if (slash) {
                            dn = (size_t)(slash - path + 1);
                            if (dn >= sizeof dir) dn = sizeof dir - 1;
                            memcpy(dir, path, dn);
                            dir[dn] = 0;
                        }
                    }
                    path_join(dir, q, tex_path_out, texcap);
                }
            }
        }
    }
    fclose(f);
}

int ps1_obj_load(const char* obj_path, Ps1Mesh* mesh, Ps1ObjResult* res,
                 s8 tex_slot) {
    FILE* f;
    char line[LINE_MAX];
    char mtl_name[PS1_OBJ_PATH_MAX] = "";
    char mat_name[OBJ_MAT_NAME_MAX] = "";
    u32 face_v[PS1_OBJ_FACE_MAX], face_t[PS1_OBJ_FACE_MAX],
        face_n[PS1_OBJ_FACE_MAX];
    FVec3 bmin;
    FVec3 bmax;
    u32 num_v = 0;
    u32 num_vt = 0;
    u32 num_vn = 0;
    int have_box = 0;
    int ok = 0;

    if (!obj_path || !mesh || !res) return 0;
    ps1_mesh_init(mesh);
    memset(res, 0, sizeof(*res));
    res->texture = -1;
    res->color = (rgb555_t)0x7FFF;
    bmin = fv3(FX_ONE, FX_ONE, FX_ONE);
    bmax = fv3(-FX_ONE, -FX_ONE, -FX_ONE);

    f = fopen(obj_path, "r");
    if (!f) return 0;

    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (!line[0]) continue;
        switch (line[0]) {
        case 'o':
            if (line[1] == ' ' || line[1] == '\t') {
                char* p = line + 1;
                while (*p == ' ' || *p == '\t') p++;
                trim(p);
                if (*p) copy_clip(res->name, sizeof res->name, p);
            }
            break;
        case 'v':
            if (line[1] == 'n')
                parse_normal(line + 2, &num_vn);
            else if (line[1] == 't')
                parse_uv(line + 2, &num_vt);
            else
                have_box |= parse_pos(line + 1, &num_v, &bmin, &bmax);
            break;
        case 'm':
            if (strncmp(line, "mtllib", 6) == 0) {
                char* p = line + 6;
                while (*p == ' ' || *p == '\t') p++;
                trim(p);
                if (*p) copy_clip(mtl_name, sizeof mtl_name, p);
            }
            break;
        case 'u':
            if (strncmp(line, "usemtl", 6) == 0) {
                char* p = line + 6;
                while (*p == ' ' || *p == '\t') p++;
                trim(p);
                if (*p) copy_clip(mat_name, sizeof mat_name, p);
            }
            break;
        case 'f':
        case 'F': {
            u32 nf = 0;
            if (parse_face(line, num_v, num_vt, num_vn, face_v, face_t,
                           face_n, &nf)) {
                if (!add_face(mesh, nf, face_v, face_t, face_n)) goto done;
            }
            break;
        }
        default:
            break;
        }
    }

    if (!have_box || mesh->triangle_count == 0) goto done;

    res->center.x = (fixed_t)(((s64)bmin.x + (s64)bmax.x) >> 1);
    res->center.y = (fixed_t)(((s64)bmin.y + (s64)bmax.y) >> 1);
    res->center.z = (fixed_t)(((s64)bmin.z + (s64)bmax.z) >> 1);

    load_mtl(obj_path, mtl_name, mat_name, &res->color, res->texture_path,
             sizeof res->texture_path);

    if (mesh_has_uvs(mesh)) {
        scale_uvs(mesh);
        if (tex_slot >= 0) {
            const Ps1Texture* tex = ps1_tex_get(tex_slot);
            if (tex) res->texture = tex_slot;
        }
    }

    ok = 1;

done:
    fclose(f);
    return ok;
}