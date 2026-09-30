#ifndef PC_CROSSING_BACK_H
#define PC_CROSSING_BACK_H

#include "PR/gbi.h"

/* Append after the original opaque acre with the same matrix. Unknown acres
 * and assets not loaded yet return NULL. No original display list is modified. */
Gfx* pc_crossing_back_lookup(Gfx* original);

#endif
