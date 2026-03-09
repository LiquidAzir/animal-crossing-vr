#ifndef WAVEREAD_H
#define WAVEREAD_H
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int WaveScene_Set(u32, u32);
void Wavegroup_Init();
int Wavegroup_Regist(void*, u32);
#ifdef __cplusplus
}
#endif

#endif
