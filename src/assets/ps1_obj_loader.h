#ifndef PS1_OBJ_LOADER_H
#define PS1_OBJ_LOADER_H

#include "core/ps1_types.h"
#include "graphics/ps1_color.h"
#include "graphics/ps1_texture.h"
#include "renderer3d/ps1_material.h"
#include "renderer3d/ps1_mesh.h"

#define PS1_OBJ_PATH_MAX 260
#define PS1_OBJ_NAME_MAX 64
#define PS1_OBJ_FACE_MAX 32

typedef struct {
    FVec3 center;
    rgb555_t color;
    s8 texture;
    u16 vertex_count;
    u16 triangle_count;
    char name[PS1_OBJ_NAME_MAX];
    char texture_path[PS1_OBJ_PATH_MAX];
} Ps1ObjResult;

/* Parses an OBJ (v / vt / vn / f), deduplicates corners, fan-triangulates
 * n-gons and stores positions, normals and UVs into mesh. If tex_slot refers
 * to an existing texture and the OBJ declares a map_Kd, UVs are scaled to
 * pixel units and res->texture is set to that slot. Returns 0 on failure. */
int ps1_obj_load(const char* obj_path, Ps1Mesh* mesh, Ps1ObjResult* res,
                 s8 tex_slot);

#endif