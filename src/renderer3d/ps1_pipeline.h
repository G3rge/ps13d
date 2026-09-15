#ifndef PS1_PIPELINE_H
#define PS1_PIPELINE_H

#include "core/ps1_math.h"
#include "graphics/ps1_depth.h"
#include "graphics/ps1_framebuffer.h"
#include "renderer3d/ps1_camera.h"
#include "renderer3d/ps1_lighting.h"
#include "renderer3d/ps1_material.h"
#include "renderer3d/ps1_mesh.h"
#include "renderer3d/ps1_transform.h"
#include "sys/ps1_profiler.h"

typedef struct {
    const Ps1Camera* camera;
    const Ps1Lighting* lighting;
    const Ps1Material* materials;
    Ps1Framebuffer* framebuffer;
    Ps1Depth16* depth;
    u8 cull_mode;
    u8 depth_mode;
    u8 lighting_enabled;
    u8 texture_enabled;
    Ps1Profiler* prof;
} Ps1PipeConfig;

void ps1_pipe_draw_mesh(const Ps1PipeConfig* cfg, const Ps1Mesh* mesh,
                        const Ps1Transform* tf);

#endif