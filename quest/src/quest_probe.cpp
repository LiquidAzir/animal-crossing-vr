// Minimal native OpenXR ABI/session probe. Does not load ROMs or game saves.
#include <SDL.h>
#include <SDL_system.h>
#include "quest_xr_runtime.h"

extern "C" int SDL_main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window* window = SDL_CreateWindow("Quest XR probe", 0, 0, 640, 480,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!context) { quest_xr_log("SDL EGL setup failed: %s", SDL_GetError()); SDL_Quit(); return 2; }
    SDL_GL_SetSwapInterval(0);
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    QuestXrRuntime xr;
    bool ok = xr.init(vm, activity);
    env->DeleteLocalRef(activity);
    Uint32 started = SDL_GetTicks();
    bool quit = false;
    while (ok && !quit && !xr.exit_requested && SDL_GetTicks() - started < 60000) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            if (event.type == SDL_APP_WILLENTERBACKGROUND) xr.resumed = false;
            if (event.type == SDL_APP_DIDENTERFOREGROUND) xr.resumed = true;
        }
        if (xr.begin_frame()) {
            bool rendered = xr.should_render;
            for (int eye = 0; eye < 2; ++eye) {
                if (!xr.acquire_eye(eye)) { rendered = false; continue; }
                glDisable(GL_SCISSOR_TEST);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glDepthMask(GL_TRUE);
                glClearColor(0.06f, eye ? 0.24f : 0.30f, 0.45f, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            xr.end_frame(rendered);
            SDL_GL_SwapWindow(window);
            if (xr.completed_frames >= 120) break;
        } else SDL_Delay(20);
    }
    quest_xr_log("PROBE_DONE initialized=%d stereo_frames=%u pointer_bits=%u",
                 ok, xr.completed_frames, unsigned(sizeof(void*) * 8));
    int result = ok && xr.completed_frames >= 1 ? 0 : 3;
    xr.shutdown();
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    return result;
}
