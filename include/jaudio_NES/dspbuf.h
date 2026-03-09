#ifndef DSPBUF_H
#define DSPBUF_H

#include "types.h"
#include "jaudio_NES/audiocommon.h"
#include "jaudio_NES/rate.h"

#ifdef __cplusplus
extern "C" {
#endif

extern s16* DspbufProcess(DSPBUF_EVENTS event);
extern s16* MixDsp(s32 nSamples);
extern void UpdateDSP(void);
extern void DspFrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif
