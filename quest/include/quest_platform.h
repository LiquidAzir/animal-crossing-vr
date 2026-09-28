#ifndef QUEST_PLATFORM_H
#define QUEST_PLATFORM_H
#ifdef __cplusplus
extern "C" {
#endif
int quest_platform_prepare(void);
void quest_platform_bind_vr(void);
void quest_platform_lifecycle(unsigned int event_type);
#ifdef __cplusplus
}
#endif
#endif
