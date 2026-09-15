#ifndef PS1_TIMING_H
#define PS1_TIMING_H

#include "core/ps1_types.h"

typedef struct {
    u64 last_us;
    u64 frame_us;
    u32 fps;
    u32 frame_count;
    u64 accum_us;
    u32 fps_window;
} Ps1Timer;

void ps1_time_init(void);
u64 ps1_time_now_us(void);
void ps1_timer_begin(Ps1Timer* t);
void ps1_timer_end(Ps1Timer* t);

#endif