#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "core/ps1_timing.h"

#ifdef _WIN32
static LARGE_INTEGER g_freq;
#endif

void ps1_time_init(void) {
#ifdef _WIN32
    QueryPerformanceFrequency(&g_freq);
#endif
}

u64 ps1_time_now_us(void) {
#ifdef _WIN32
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (u64)(((u64)c.QuadPart * 1000000ULL) / (u64)g_freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000ULL + (u64)ts.tv_nsec / 1000ULL;
#endif
}

void ps1_timer_begin(Ps1Timer* t) {
    t->last_us = ps1_time_now_us();
}

void ps1_timer_end(Ps1Timer* t) {
    u64 now = ps1_time_now_us();
    t->frame_us = now - t->last_us;
    t->last_us = now;
    t->frame_count++;
    t->accum_us += t->frame_us;
    if (t->accum_us >= 1000000ULL) {
        t->fps = 1000000u / (u32)(t->accum_us / (u64)(t->frame_count ? t->frame_count : 1));
        t->frame_count = 0;
        t->accum_us = 0;
    }
}