#ifndef PC_STRUCTURE_BACK_H
#define PC_STRUCTURE_BACK_H

#include "PR/gbi.h"

/* Missing surfaces only, in the original police-box model frame. The caller
 * supplies the live structure palette in segment 8 and the model matrix. */
Gfx* pc_police_back(int winter);

/* Museum patches use the original model's fixed seasonal body palette. */
Gfx* pc_museum_back(int winter);

#endif
