/* Android host services. All paths belong to the standalone Quest package. */
#include "quest_platform.h"
#include "quest_vr_android.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int quest_platform_prepare(void) {
    const char* root = SDL_AndroidGetInternalStoragePath();
    if (!root || chdir(root) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ACQuest", "Cannot open app data root");
        return 0;
    }
    /* Internal app storage has stable ownership; the development build's
     * run-as transfer helper imports copies without shared-storage permissions. */
    const char* directories[] = {"rom", "save", "save/card_a", "save/card_b", "shaders"};
    for (unsigned i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        mkdir(directories[i], 0700);
    }
    freopen("quest-stdout.log", "w", stdout);
    freopen("quest-stderr.log", "w", stderr);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("[Quest] data=%s pointer_bytes=%u\n", root, (unsigned)sizeof(void*));
    __android_log_print(ANDROID_LOG_INFO, "ACQuest", "Starting in %s", root);
    return 1;
}

void quest_platform_bind_vr(void) {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    JavaVM* vm = NULL;
    if (!env || !activity || (*env)->GetJavaVM(env, &vm) != JNI_OK) return;
    /* Backend retains its own global reference for the XR session lifetime. */
    quest_vr_set_android(vm, activity);
    (*env)->DeleteLocalRef(env, activity);
    quest_vr_set_resumed(1);
}

void quest_platform_lifecycle(unsigned int event_type) {
    extern void pc_audio_set_paused(int paused);
    if (event_type == SDL_RENDER_DEVICE_RESET) {
        extern int g_pc_running;
        __android_log_print(ANDROID_LOG_ERROR, "ACQuest", "GL context was lost; closing for a clean restart");
        g_pc_running = 0;
        quest_vr_set_resumed(0);
        return;
    }
    if (event_type == SDL_APP_WILLENTERBACKGROUND) {
        quest_vr_set_resumed(0);
        pc_audio_set_paused(1);
    } else if (event_type == SDL_APP_DIDENTERFOREGROUND) {
        pc_audio_set_paused(0);
        quest_vr_set_resumed(1);
    }
}
