#ifndef NEOSTHREAD_H
#define NEOSTHREAD_H

#include "types.h"
#include "jaudio_NES/audiostruct.h"

#ifdef __cplusplus
extern "C" {
#endif

extern u32 Neos_Update(s16* dst);
extern void ImageLoaded(u32 param);
extern BOOL Neos_CheckBoot(void);
extern void* neosproc(void* param);

#ifdef TARGET_PC
void pc_neos_init_sync(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
