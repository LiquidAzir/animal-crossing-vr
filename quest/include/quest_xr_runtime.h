#pragma once

// Shared by the ABI/session probe and the game backend. No game/32-bit headers.
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "quest_host_vector.h"

void quest_xr_log(const char* format, ...);
double quest_xr_clock_ms();

enum QuestXrTimingSlot {
    QUEST_XR_WAIT_FRAME, QUEST_XR_BEGIN_FRAME, QUEST_XR_LOCATE_VIEWS,
    QUEST_XR_ACQUIRE_WAIT, QUEST_XR_BIND_TARGET, QUEST_XR_RENDER_SPAN,
    QUEST_XR_GL_FLUSH, QUEST_XR_RELEASE, QUEST_XR_END_FRAME,
    QUEST_XR_FRAME_GAP, QUEST_XR_ACTIONS, QUEST_XR_TIMING_COUNT
};

struct QuestXrEye {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    QuestHostVector<XrSwapchainImageOpenGLESKHR> images;
    QuestHostVector<GLuint> fbos, depths;
    uint32_t index = 0;
    int width = 0, height = 0;
    bool acquired = false, waited = false;
};

struct QuestXrRuntime {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace local_space = XR_NULL_HANDLE;
    XrSpace view_space = XR_NULL_HANDLE;
    XrSessionState state = XR_SESSION_STATE_UNKNOWN;
    XrFrameState frame = {XR_TYPE_FRAME_STATE};
    XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    QuestXrEye eyes[2];
    bool running = false, focused = false, resumed = true;
    bool frame_begun = false, should_render = false, views_valid = false;
    bool exit_requested = false;
    unsigned completed_frames = 0;
    XrTime recenter_change_time = 0;
    // Optional wall-clock attribution only: no GL queries, fences or readback.
    bool timing_enabled = false;
    double timing_frame[QUEST_XR_TIMING_COUNT] = {};
    double timing_total[QUEST_XR_TIMING_COUNT] = {};
    double timing_peak[QUEST_XR_TIMING_COUNT] = {};
    double timing_render_start = 0, timing_last_end = 0;
    unsigned timing_frames = 0;

    bool init(JavaVM* vm, jobject activity);
    void poll_events();
    bool begin_frame();
    bool acquire_eye(int eye);
    bool end_frame(bool rendered);
    bool locate_head(XrSpaceLocation* location) const;
    void shutdown();
    bool check(XrResult result, const char* operation) const;
    double timing_tick() const { return timing_enabled ? quest_xr_clock_ms() : 0; }
    void timing_record(QuestXrTimingSlot slot, double start);
    void timing_finish();
};
