#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Called on the SDL/render thread before pc_vr_init. The backend retains its
// own global activity reference; the caller may release SDL's local reference.
void quest_vr_set_android(void* java_vm, void* activity);
void quest_vr_set_resumed(int resumed);
#ifdef __cplusplus
}
#endif
