#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/ps1_obj_loader.h"
#include "editor/ps1_editor.h"
#include "graphics/ps1_texture.h"
#include "sys/ps1_png.h"
#include "sys/ps1_window.h"

#define SF_LINE_MAX 512

#define SCENE_MAGIC "PS1SCENE"
#define SCENE_VER 1

/* ------------------------------------------------------------------ */
/* Fixed point <-> decimal text (integer arithmetic only)              */
/* ------------------------------------------------------------------ */

static void fx_tostr(fixed_t v, char* out, u32 cap) {
    int neg = (v < 0);
    u64 ua = neg ? (u64)(-(s64)v) : (u64)v;
    u64 ip = ua >> FX_SHIFT;
    u64 fr = ua & FX_FRAC_MASK;
    u64 fp = (fr * 1000000u) >> FX_SHIFT;
    if (fp == 0) {
        snprintf(out, cap, "%llu", (unsigned long long)ip);
        return;
    }
    while (fp > 0 && (fp % 10) == 0) fp /= 10;
    snprintf(out, cap, "%s%llu.%llu", neg ? "-" : "",
             (unsigned long long)ip, (unsigned long long)fp);
}

static int fx_fromstr(const char* s, fixed_t* out) {
    const char* p = s;
    s64 ip = 0;
    s64 fp = 0;
    s64 p10 = 1;
    int neg = 0;
    int frac = 0;
    s64 res;

    if (!p) return 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') {
        ip = ip * 10 + (*p - '0');
        if (ip > 0x7FFFFFFFLL) return 0;
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && frac < 6) {
            fp = fp * 10 + (*p - '0');
            p10 *= 10;
            frac++;
            p++;
        }
        fp = ((fp << FX_SHIFT) + (p10 >> 1)) / p10;
    }
    res = (ip << FX_SHIFT) + fp;
    if (res > 0x7FFFFFFFLL) return 0;
    *out = neg ? (fixed_t)(-res) : (fixed_t)res;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Simple quote-aware tokenizer over a mutable line buffer             */
/* ------------------------------------------------------------------ */

static char* tok_next(char** pp) {
    char* p = *pp;
    char* out;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return NULL;
    if (*p == '"') {
        p++;
        out = p;
        while (*p && *p != '"') p++;
        if (*p) {
            *p = 0;
            p++;
        }
    } else {
        out = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) {
            *p = 0;
            p++;
        }
    }
    *pp = p;
    return out;
}

static void strip_quotes(char* s) {
    for (; *s; s++)
        if (*s == '"') *s = '_';
}

static s8 find_free_tex_slot(void) {
    s8 s;
    for (s = 0; s < PS1_MAX_TEXTURES; s++)
        if (!ps1_tex_get(s)) return s;
    return -1;
}

static s8 scene_tex_for_slot(const Ps1Editor* e, s8 slot) {
    if (slot < 0 || slot >= PS1_MAX_TEXTURES) return -1;
    if (!e->tex_path[slot][0]) return -1;
    if (!ps1_tex_get(slot)) return -1;
    return slot;
}

/* ------------------------------------------------------------------ */
/* Mesh / material / entity helpers (editor state mutation)            */
/* ------------------------------------------------------------------ */

static int scene_new_mesh(Ps1Editor* e, Ps1MeshKind kind,
                          const char* obj_path) {
    Ps1Mesh* mesh;
    if (e->hub.mesh_count >= PS1_MAX_MESHES) return -1;
    mesh = ps1_res_new_mesh(&e->hub);
    if (!mesh) return -1;
    if (kind == MESH_KIND_OBJ) {
        Ps1ObjResult res;
        if (!obj_path || !ps1_obj_load(obj_path, mesh, &res, -1)) return -1;
    } else {
        if (!ps1_mesh_build_kind(mesh, kind)) return -1;
    }
    return (int)(e->hub.mesh_count - 1);
}

static s16 scene_new_mat(Ps1Editor* e, rgb555_t color, s8 tex) {
    Ps1Material* m;
    if (e->hub.mat_count >= PS1_MAX_MATERIALS) return -1;
    m = &e->hub.material[e->hub.mat_count];
    ps1_material_init(m, color, tex);
    e->hub.mat_used[e->hub.mat_count] = 1;
    e->hub.mat_count++;
    return (s16)(e->hub.mat_count - 1);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void ps1_editor_to_edit(Ps1Editor* e) {
    e->mode = MODE_EDIT;
    /* leaving PLAY/PAUSED behind: a fresh scene must never keep running
     * with a stale play snapshot (STOP would restore the wrong scene). */
    e->state = STATE_EDIT;
}

int ps1_editor_new_scene(Ps1Editor* e) {
    ps1_scene_clear(&e->scene);
    ps1_res_init(&e->hub);
    ps1_tex_reset();
    memset(e->tex_path, 0, sizeof e->tex_path);
    e->selected = -1;
    e->scene_path[0] = 0;
    e->scene_dirty = 0;
    ps1_editor_camera_reset(e);
    ps1_editor_to_edit(e);
    ps1_editor_snap(e);
    ps1_editor_log(e, "NEW SCENE (empty)");
    return 1;
}

int ps1_editor_save_scene(Ps1Editor* e, const char* path) {
    FILE* f;
    s16 i;

    if (!path || !path[0]) return 0;
    f = fopen(path, "w");
    if (!f) {
        ps1_editor_log(e, "SAVE FAILED: %s", path);
        return 0;
    }

    fprintf(f, "%s %d\n", SCENE_MAGIC, SCENE_VER);

    {
        s8 s;
        for (s = 0; s < PS1_MAX_TEXTURES; s++)
            if (scene_tex_for_slot(e, s) >= 0)
                fprintf(f, "SYSTEX %d %s\n", (int)s, e->tex_path[s]);
    }

    {
        char bx[24], by[24], bz[24], bya[24], bpi[24];
        fx_tostr(e->camera.pos.x, bx, sizeof bx);
        fx_tostr(e->camera.pos.y, by, sizeof by);
        fx_tostr(e->camera.pos.z, bz, sizeof bz);
        fx_tostr(e->camera.yaw, bya, sizeof bya);
        fx_tostr(e->camera.pitch, bpi, sizeof bpi);
        fprintf(f, "CAM %s %s %s %s %s\n", bx, by, bz, bya, bpi);
    }
    {
        char ba[24], bx[24], by[24], bz[24];
        fx_tostr(e->lighting.ambient, ba, sizeof ba);
        fx_tostr(e->lighting.dir.x, bx, sizeof bx);
        fx_tostr(e->lighting.dir.y, by, sizeof by);
        fx_tostr(e->lighting.dir.z, bz, sizeof bz);
        fprintf(f, "LIGHT %s %s %s %s %u\n", ba, bx, by, bz,
                (unsigned)e->lighting.levels);
    }

    {
        s16 i;
        for (i = 0; i < (s16)e->scene.count; i++) {
            const Ps1Object* o = &e->objs[i];
            const Ps1Transform* t = &e->scene.entities[i].transform;
            char px[24], py[24], pz[24], rx[24], ry[24], rz[24];
            if (!o->is_camera) continue;
            fx_tostr(t->position.x, px, sizeof px);
            fx_tostr(t->position.y, py, sizeof py);
            fx_tostr(t->position.z, pz, sizeof pz);
            fx_tostr(t->rotation.x, rx, sizeof rx);
            fx_tostr(t->rotation.y, ry, sizeof ry);
            fx_tostr(t->rotation.z, rz, sizeof rz);
            fprintf(f, "CAMNODE %s %s %s %s %s %s\n", px, py, pz, rx, ry, rz);
        }
    }

    for (i = 0; i < (s16)e->scene.count; i++) {
        const Ps1Entity* en = &e->scene.entities[i];
        const Ps1Object* o = &e->objs[i];
        const Ps1Material* m = NULL;
        char name[PS1_OBJ_NAME_MAX];
        char px[24], py[24], pz[24], rx[24], ry[24], rz[24];
        char sx[24], sy[24], sz[24], path[PS1_OBJ_PATH_MAX];
        int mat_idx = (int)o->mat_idx;

        if (mat_idx >= 0 && mat_idx < (int)e->hub.mat_count)
            m = &e->hub.material[mat_idx];

        snprintf(name, sizeof name, "%s", o->name);
        strip_quotes(name);
        fx_tostr(en->transform.position.x, px, sizeof px);
        fx_tostr(en->transform.position.y, py, sizeof py);
        fx_tostr(en->transform.position.z, pz, sizeof pz);
        fx_tostr(en->transform.rotation.x, rx, sizeof rx);
        fx_tostr(en->transform.rotation.y, ry, sizeof ry);
        fx_tostr(en->transform.rotation.z, rz, sizeof rz);
        fx_tostr(en->transform.scale.x, sx, sizeof sx);
        fx_tostr(en->transform.scale.y, sy, sizeof sy);
        fx_tostr(en->transform.scale.z, sz, sizeof sz);

        fprintf(f, "OBJ \"%s\" %d %s %s %s %s %s %s %s %s %s %u %d %d",
                name, (int)o->mesh_kind, px, py, pz, rx, ry, rz, sx, sy, sz,
                (unsigned)(m ? (unsigned)m->color : 0u),
                m ? (int)m->texture : -1, m ? (int)m->two_sided : 0);

        if (o->mesh_kind == MESH_KIND_OBJ) {
            snprintf(path, sizeof path, "%s", o->mesh_path);
            strip_quotes(path);
            fprintf(f, " \"%s\"", path);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    snprintf(e->scene_path, sizeof e->scene_path, "%s", path);
    e->scene_dirty = 0;
    ps1_editor_log(e, "SCENE SAVED: %s", path);
    return 1;
}

int ps1_editor_load_scene(Ps1Editor* e, const char* path) {
    FILE* f;
    char line[SF_LINE_MAX];
    s8 old_to_new[PS1_MAX_TEXTURES];
    int i;
    int loaded = 0;

    if (!path || !path[0]) return 0;
    f = fopen(path, "r");
    if (!f) {
        ps1_editor_log(e, "OPEN FAILED: %s", path);
        return 0;
    }

    ps1_scene_clear(&e->scene);
    ps1_res_init(&e->hub);
    ps1_tex_reset();
    memset(e->tex_path, 0, sizeof e->tex_path);
    e->selected = -1;
    for (i = 0; i < PS1_MAX_TEXTURES; i++) old_to_new[i] = -1;

    while (fgets(line, sizeof line, f)) {
        char* p = line;
        char* tok = tok_next(&p);
        if (!tok) continue;

        if (!strcmp(tok, "SYSTEX")) {
            char* tslot = tok_next(&p);
            char* tpath = tok_next(&p);
            s8 slot;
            s8 nslot;
            if (!tslot) continue;
            slot = (s8)atoi(tslot);
            if (!tpath || slot < 0 || slot >= PS1_MAX_TEXTURES) continue;
            nslot = ps1_png_load_texture(tpath, find_free_tex_slot());
            if (nslot >= 0) {
                snprintf(e->tex_path[nslot], PS1_OBJ_PATH_MAX, "%s", tpath);
                old_to_new[slot] = nslot;
            }
        } else if (!strcmp(tok, "CAM")) {
            char* bx = tok_next(&p);
            char* by = tok_next(&p);
            char* bz = tok_next(&p);
            char* bya = tok_next(&p);
            char* bpi = tok_next(&p);
            if (bx && by && bz && bya && bpi) {
                fixed_t tx, ty, tz, tyaw, tpitch;
                if (fx_fromstr(bx, &tx) && fx_fromstr(by, &ty) &&
                    fx_fromstr(bz, &tz) && fx_fromstr(bya, &tyaw) &&
                    fx_fromstr(bpi, &tpitch)) {
                    e->camera.pos = fv3(tx, ty, tz);
                    e->camera.yaw = tyaw;
                    e->camera.pitch = tpitch;
                }
            }
        } else if (!strcmp(tok, "LIGHT")) {
            char* ba = tok_next(&p);
            char* bx = tok_next(&p);
            char* by = tok_next(&p);
            char* bz = tok_next(&p);
            char* bl = tok_next(&p);
            fixed_t amb, dx, dy, dz;
            if (ba && bx && by && bz && bl && fx_fromstr(ba, &amb) &&
                fx_fromstr(bx, &dx) && fx_fromstr(by, &dy) &&
                fx_fromstr(bz, &dz)) {
                e->lighting.ambient = amb;
                e->lighting.dir = fv3(dx, dy, dz);
                e->lighting.dir = fv3_normalize(&e->lighting.dir);
                {
                    u32 lv = (u32)strtoul(bl, NULL, 10);
                    if (lv >= 2 && lv <= 32) e->lighting.levels = (u8)lv;
                }
            }
        } else if (!strcmp(tok, "OBJ")) {
            char* tname = tok_next(&p);
            char* tkind = tok_next(&p);
            char* v[9];
            char *tmat, *ttex, *ttwo, *tpath;
            int n = 0;
            fixed_t tf[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
            Ps1Transform gt;
            int mi;
            s16 mati;
            Ps1MeshKind kind;
            rgb555_t color = (rgb555_t)0x7FFF;
            s8 tex = -1;
            u8 two = 0;

            if (!tname || !tkind) continue;
            kind = (Ps1MeshKind)atoi(tkind);
            if (kind < MESH_KIND_CUBE || kind >= MESH_KIND_COUNT) continue;

            for (n = 0; n < 9; n++) v[n] = tok_next(&p);
            {
                int ok = 1;
                for (n = 0; n < 9; n++)
                    if (!v[n] || !fx_fromstr(v[n], &tf[n])) ok = 0;
                if (!ok) continue;
            }

            tmat = tok_next(&p);
            ttex = tok_next(&p);
            ttwo = tok_next(&p);
            tpath = tok_next(&p);
            if (!tmat || !ttex || !ttwo) continue;

            color = (rgb555_t)((u32)strtoul(tmat, NULL, 10) & 0xFFFFu);
            tex = (s8)atoi(ttex);
            if (tex >= 0 && tex < PS1_MAX_TEXTURES && old_to_new[tex] >= 0)
                tex = old_to_new[tex];
            else if (tex < 0)
                tex = -1;
            else
                tex = -1;
            two = (u8)(atoi(ttwo) ? 1 : 0);

            mi = scene_new_mesh(e, kind, tpath);
            if (mi < 0) continue;
            mati = scene_new_mat(e, color, tex);
            if (mati < 0) continue;
            {
                u32 k;
                Ps1Mesh* mm = &e->hub.mesh[mi];
                for (k = 0; k < mm->triangle_count; k++)
                    mm->tris[k].material = (u16)mati;
                e->hub.material[mati].two_sided = two;
            }

            gt.position = fv3(tf[0], tf[1], tf[2]);
            gt.rotation = fv3(tf[3], tf[4], tf[5]);
            gt.scale = fv3(tf[6], tf[7], tf[8]);
            if (gt.scale.x == 0) gt.scale.x = FX_ONE;
            if (gt.scale.y == 0) gt.scale.y = FX_ONE;
            if (gt.scale.z == 0) gt.scale.z = FX_ONE;

            {
                int idx = ps1_scene_add(&e->scene, &e->hub.mesh[mi], &gt);
                if (idx < 0) continue;
                snprintf(e->objs[idx].name, sizeof e->objs[idx].name, "%s",
                         tname[0] ? tname : "OBJECT");
                e->objs[idx].mesh_idx = (u16)mi;
                e->objs[idx].mat_idx = mati;
                e->objs[idx].mesh_kind = (s16)kind;
                if (kind == MESH_KIND_OBJ && tpath)
                    snprintf(e->objs[idx].mesh_path, PS1_OBJ_PATH_MAX, "%s",
                             tpath);
                else
                    e->objs[idx].mesh_path[0] = 0;
            }
            loaded++;
        } else if (!strcmp(tok, "CAMNODE")) {
            char* v[6];
            int n = 0;
            fixed_t tf[6] = {0, 0, 0, 0, 0, 0};
            int idx;
            for (n = 0; n < 6; n++) v[n] = tok_next(&p);
            {
                int ok = 1;
                for (n = 0; n < 6; n++)
                    if (!v[n] || !fx_fromstr(v[n], &tf[n])) ok = 0;
                if (ok) {
                    Ps1Transform gt;
                    gt.position = fv3(tf[0], tf[1], tf[2]);
                    gt.rotation = fv3(tf[3], tf[4], tf[5]);
                    gt.scale = fv3(fx_int(1), fx_int(1), fx_int(1));
                    idx = ps1_scene_add(&e->scene, NULL, &gt);
                    if (idx >= 0) {
                        snprintf(e->objs[idx].name,
                                 sizeof e->objs[idx].name, "Camera");
                        e->objs[idx].mesh_idx = 0;
                        e->objs[idx].mat_idx = -1;
                        e->objs[idx].mesh_kind = (s16)MESH_KIND_CAMERA;
                        e->objs[idx].mesh_path[0] = 0;
                        e->objs[idx].is_camera = 1;
                        loaded++;
                    }
                }
            }
        }
    }

    fclose(f);
    snprintf(e->scene_path, sizeof e->scene_path, "%s", path);
    e->scene_dirty = 0;
    ps1_editor_to_edit(e);
    ps1_editor_snap(e);
    if (loaded > 0) ps1_editor_camera_reset(e);
    ps1_editor_log(e, "SCENE LOADED: %s (%d objects)", path, loaded);
    return loaded > 0;
}

/* Scans the working directory for saved scene files (for the project
 * manager). Returns the number of names copied. */
int ps1_editor_scan_scene_files(char (*paths)[PS1_OBJ_PATH_MAX], int cap) {
    DIR* d;
    struct dirent* de;
    int n = 0;
    if (cap <= 0) return 0;
    d = opendir(".");
    if (!d) return 0;
    while ((de = readdir(d)) != NULL) {
        size_t len;
        if (n >= cap) break;
        len = strlen(de->d_name);
        if (len < strlen(PS1_SCENE_EXT)) continue;
        if (strcmp(de->d_name + len - strlen(PS1_SCENE_EXT), PS1_SCENE_EXT) !=
            0)
            continue;
        snprintf(paths[n], PS1_OBJ_PATH_MAX, "%s", de->d_name);
        n++;
    }
    closedir(d);
    return n;
}