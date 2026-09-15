#ifndef PS1_PROFILER_H
#define PS1_PROFILER_H

#include "core/ps1_types.h"

typedef struct {
    u32 verts_in;
    u32 tris_in;
    u32 tris_clipped;
    u32 tris_culled;
    u32 tris_rasterized;
    u64 pixels_written;
    u64 math_ops;
    u64 texel_reads;
    u32 frame_us;
    u32 fps;
} Ps1Profiler;

void ps1_prof_reset(Ps1Profiler* p);
void ps1_prof_begin_frame(Ps1Profiler* p);
void ps1_prof_end_frame(Ps1Profiler* p);
u32 ps1_prof_est_cycles(const Ps1Profiler* p);
u32 ps1_prof_est_load_percent(const Ps1Profiler* p);

#endif