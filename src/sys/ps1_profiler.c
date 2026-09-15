#include "sys/ps1_profiler.h"

#define PS1_BUDGET_CYCLES 281000u

void ps1_prof_reset(Ps1Profiler* p) {
    p->verts_in = 0;
    p->tris_in = 0;
    p->tris_clipped = 0;
    p->tris_culled = 0;
    p->tris_rasterized = 0;
    p->pixels_written = 0;
    p->math_ops = 0;
    p->texel_reads = 0;
}

void ps1_prof_begin_frame(Ps1Profiler* p) {
    ps1_prof_reset(p);
}

void ps1_prof_end_frame(Ps1Profiler* p) {
    (void)p;
}

u32 ps1_prof_est_cycles(const Ps1Profiler* p) {
    u32 c = 0;
    c += p->verts_in * 300u;
    c += p->tris_in * 1800u;
    c += p->tris_clipped * 800u;
    c += p->tris_rasterized * 300u;
    c += (u32)p->pixels_written * 10u;
    c += (u32)p->texel_reads * 8u;
    return c;
}

u32 ps1_prof_est_load_percent(const Ps1Profiler* p) {
    u64 c = ps1_prof_est_cycles(p);
    c = c * 100ULL;
    if (c > (u64)PS1_BUDGET_CYCLES * 10000ULL) c = (u64)PS1_BUDGET_CYCLES * 10000ULL;
    return (u32)(c / (u64)PS1_BUDGET_CYCLES);
}