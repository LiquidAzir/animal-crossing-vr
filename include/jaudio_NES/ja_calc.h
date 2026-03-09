#ifndef _JAUDIO_JA_CALC_H
#define _JAUDIO_JA_CALC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/////////// JAUDIO MATH DEFINITIONS ///////////
f32 sqrtf2(f32);
f32 atanf2(f32, f32);
void Jac_InitSinTable();
f32 sinf3(f32);

///////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
