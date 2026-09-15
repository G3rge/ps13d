#ifndef PS1_LUT_H
#define PS1_LUT_H

#include "core/ps1_fixed.h"

#define PS1_LUT_ANG 1024
#define PS1_LUT_RECIP 256

void ps1_lut_init(void);

fixed_t ps1_sin(fixed_t angle);
fixed_t ps1_cos(fixed_t angle);
fixed_t ps1_fast_recip(fixed_t z);

#endif