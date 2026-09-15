#include <string.h>

#include "graphics/ps1_depth.h"

void ps1_depth_clear(Ps1Depth16* d) {
    memset(d->data, 0, sizeof(d->data));
}