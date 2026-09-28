#include "quest_xr_runtime.h"
#include "quest_eye_size.h"
#include <GLES2/gl2ext.h>
#include <android/log.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <time.h>

double quest_xr_clock_ms() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return double(now.tv_sec) * 1000.0 + double(now.tv_nsec) * 0.000001;
}

void QuestXrRuntime::timing_record(QuestXrTimingSlot slot, double start) {
    if (timing_enabled && start != 0)
        timing_frame[slot] += quest_xr_clock_ms() - start;
}

void QuestXrRuntime::timing_finish() {
    if (!timing_enabled) return;
    timing_last_end = quest_xr_clock_ms();
    for (int slot = 0; slot < QUEST_XR_TIMING_COUNT; ++slot) {
        timing_total[slot] += timing_frame[slot];
        timing_peak[slot] = std::max(timing_peak[slot], timing_frame[slot]);
    }
    if (++timing_frames < 30) return;
    const char* names[] = {"wait", "begin", "views", "acquire_wait", "fbo_bind",
                          "render_span", "gl_flush", "release", "end", "game_gap", "actions"};
    // render_span includes actions/acquisition plus application draw submission;
    // game_gap covers CPU work between the last XR end and the next XR wait.
    quest_xr_log("XR_TIMING frames=%u period_ms=%.3f submitted=%u state=%d focused=%d render=%d eyes=%dx%d (avg/max wall ms; render_span inclusive)",
                 timing_frames, double(frame.predictedDisplayPeriod) / 1000000.0, completed_frames,
                 int(state), focused, should_render, eyes[0].width, eyes[0].height);
    for (int slot = 0; slot < QUEST_XR_TIMING_COUNT; ++slot)
        quest_xr_log("XR_TIMING %s=%.3f/%.3f", names[slot],
                     timing_total[slot] / timing_frames, timing_peak[slot]);
    memset(timing_total, 0, sizeof(timing_total));
    memset(timing_peak, 0, sizeof(timing_peak));
    timing_frames = 0;
}

void quest_xr_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    __android_log_vprint(ANDROID_LOG_INFO, "ACQuestXR", format, args);
    va_end(args);
}

bool QuestXrRuntime::check(XrResult result, const char* operation) const {
    if (XR_SUCCEEDED(result)) return true;
    char name[XR_MAX_RESULT_STRING_SIZE] = {};
    if (instance) xrResultToString(instance, result, name);
    quest_xr_log("%s failed: %d %s", operation, (int)result, name);
    return false;
}

bool QuestXrRuntime::init(JavaVM* vm, jobject activity) {
    if (!vm || !activity) { quest_xr_log("Missing Android VM/activity"); return false; }
    quest_xr_log("Startup ABI: pointer=%u bits; GLES=%s", unsigned(sizeof(void*) * 8),
                 (const char*)glGetString(GL_VERSION));
    PFN_xrInitializeLoaderKHR initialize_loader = nullptr;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                         reinterpret_cast<PFN_xrVoidFunction*>(&initialize_loader));
    if (initialize_loader) {
        XrLoaderInitInfoAndroidKHR info = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        info.applicationVM = vm;
        info.applicationContext = activity;
        if (!check(initialize_loader(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&info)),
                   "xrInitializeLoaderKHR")) return false;
    }
    uint32_t count = 0;
    if (!check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr),
               "xrEnumerateInstanceExtensionProperties")) return false;
    QuestHostVector<XrExtensionProperties> properties(count, {XR_TYPE_EXTENSION_PROPERTIES});
    if (!check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, properties.data()),
               "xrEnumerateInstanceExtensionProperties(data)")) return false;
    const char* extensions[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                                XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
    for (const char* required : extensions) {
        bool found = false;
        for (const auto& p : properties) found |= !strcmp(p.extensionName, required);
        if (!found) { quest_xr_log("Required extension absent: %s", required); return false; }
    }
    XrInstanceCreateInfoAndroidKHR android = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    android.applicationVM = vm; android.applicationActivity = activity;
    XrInstanceCreateInfo create = {XR_TYPE_INSTANCE_CREATE_INFO};
    create.next = &android;
    strcpy(create.applicationInfo.applicationName, "Animal Crossing Quest");
    strcpy(create.applicationInfo.engineName, "AC native port");
    create.applicationInfo.applicationVersion = 1;
    // Request core 1.0 for broad runtime/ABI probing; newer SDK is only headers.
    create.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    create.enabledExtensionCount = 2;
    create.enabledExtensionNames = extensions;
    if (!check(xrCreateInstance(&create, &instance), "xrCreateInstance")) return false;
    XrInstanceProperties runtime = {XR_TYPE_INSTANCE_PROPERTIES};
    xrGetInstanceProperties(instance, &runtime);
    quest_xr_log("Runtime: %s %u.%u.%u", runtime.runtimeName,
                 unsigned(XR_VERSION_MAJOR(runtime.runtimeVersion)),
                 unsigned(XR_VERSION_MINOR(runtime.runtimeVersion)),
                 unsigned(XR_VERSION_PATCH(runtime.runtimeVersion)));
    XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!check(xrGetSystem(instance, &system_info, &system), "xrGetSystem")) return false;
    PFN_xrGetOpenGLESGraphicsRequirementsKHR requirements_fn = nullptr;
    if (!check(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                 reinterpret_cast<PFN_xrVoidFunction*>(&requirements_fn)), "get GLES requirements function")) return false;
    XrGraphicsRequirementsOpenGLESKHR requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    if (!check(requirements_fn(instance, system, &requirements), "xrGetOpenGLESGraphicsRequirementsKHR")) return false;
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    EGLint config_id = 0, config_count = 0;
    EGLConfig config = nullptr;
    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT ||
        !eglQueryContext(display, context, EGL_CONFIG_ID, &config_id)) {
        quest_xr_log("No current EGL context for XR"); return false;
    }
    const EGLint config_attribs[] = {EGL_CONFIG_ID, config_id, EGL_NONE};
    if (!eglChooseConfig(display, config_attribs, &config, 1, &config_count) || config_count != 1) return false;
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
    XrVersion version = XR_MAKE_VERSION(major, minor, 0);
    if (version < requirements.minApiVersionSupported || version > requirements.maxApiVersionSupported) {
        quest_xr_log("Current GLES version outside runtime supported range"); return false;
    }
    XrGraphicsBindingOpenGLESAndroidKHR binding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    binding.display = display; binding.config = config; binding.context = context;
    XrSessionCreateInfo session_info = {XR_TYPE_SESSION_CREATE_INFO};
    session_info.next = &binding; session_info.systemId = system;
    if (!check(xrCreateSession(instance, &session_info, &session), "xrCreateSession")) return false;
    XrReferenceSpaceCreateInfo space = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space.poseInReferenceSpace.orientation.w = 1;
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    if (!check(xrCreateReferenceSpace(session, &space, &local_space), "create LOCAL space")) return false;
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if (!check(xrCreateReferenceSpace(session, &space, &view_space), "create VIEW space")) return false;
    XrViewConfigurationView configs[2] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    if (!check(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                2, &count, configs), "enumerate stereo views") || count != 2) return false;
    uint32_t format_count = 0;
    if (!check(xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr), "enumerate swapchain formats")) return false;
    QuestHostVector<int64_t> formats(format_count);
    if (!check(xrEnumerateSwapchainFormats(session, format_count, &format_count, formats.data()), "get swapchain formats")) return false;
    // The GX shaders already produce gamma-encoded colors. Label the image
    // sRGB for the compositor, while disabling GL's additional write encoding
    // so the bytes remain identical to the PC backend's ColorSpace_Gamma.
    // Quest 3 driver readback verifies this extension preserves all channels.
    bool srgb_write_control = false;
    GLint extension_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
    for (GLint i = 0; i < extension_count; ++i)
        srgb_write_control |= !strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)),
                                     "GL_EXT_sRGB_write_control");
    if (!srgb_write_control) {
        quest_xr_log("Missing GL_EXT_sRGB_write_control required for original game colors");
        return false;
    }
    int64_t format = GL_SRGB8_ALPHA8;
    if (std::find(formats.begin(), formats.end(), format) == formats.end()) {
        quest_xr_log("Runtime does not expose sRGB8 swapchains"); return false;
    }
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    for (int eye = 0; eye < 2; ++eye) {
        QuestXrEye& target = eyes[eye];
        const XrViewConfigurationView& config = configs[eye];
        if (!quest_select_eye_size(config.recommendedImageRectWidth,
                                   config.recommendedImageRectHeight,
                                   config.maxImageRectWidth, config.maxImageRectHeight,
                                   QUEST_MAX_EYE_DIMENSION, &target.width, &target.height)) {
            quest_xr_log("Invalid eye %d dimensions: recommended=%ux%u runtime_max=%ux%u",
                         eye, config.recommendedImageRectWidth, config.recommendedImageRectHeight,
                         config.maxImageRectWidth, config.maxImageRectHeight);
            return false;
        }
        quest_xr_log("Eye %d resolution: recommended=%ux%u selected=%dx%d runtime_max=%ux%u app_max_edge=%u",
                     eye, config.recommendedImageRectWidth, config.recommendedImageRectHeight,
                     target.width, target.height, config.maxImageRectWidth, config.maxImageRectHeight,
                     QUEST_MAX_EYE_DIMENSION);
        XrSwapchainCreateInfo swap = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swap.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swap.format = format; swap.sampleCount = 1;
        swap.width = target.width; swap.height = target.height;
        swap.faceCount = swap.arraySize = swap.mipCount = 1;
        if (!check(xrCreateSwapchain(session, &swap, &target.swapchain), "xrCreateSwapchain")) return false;
        if (!check(xrEnumerateSwapchainImages(target.swapchain, 0, &count, nullptr), "enumerate swapchain images")) return false;
        target.images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        if (!check(xrEnumerateSwapchainImages(target.swapchain, count, &count,
                     reinterpret_cast<XrSwapchainImageBaseHeader*>(target.images.data())), "get swapchain images")) return false;
        target.fbos.resize(count); target.depths.resize(count);
        glGenFramebuffers(count, target.fbos.data());
        glGenRenderbuffers(count, target.depths.data());
        for (uint32_t i = 0; i < count; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, target.fbos[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.images[i].image, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, target.depths[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, target.width, target.height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depths[i]);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                quest_xr_log("Eye framebuffer incomplete"); return false;
            }
        }
        quest_xr_log("Eye %d swapchain: %dx%d images=%u", eye, target.width, target.height, count);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glBindRenderbuffer(GL_RENDERBUFFER, 0);
    quest_xr_log("SESSION_CREATED pointer_bits=%u", unsigned(sizeof(void*) * 8));
    return true;
}

void QuestXrRuntime::poll_events() {
    if (!instance) return;
    XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* change = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
            if (change->session == session) {
                state = change->state;
                focused = state == XR_SESSION_STATE_FOCUSED;
                quest_xr_log("Session state=%d", int(state));
                if (state == XR_SESSION_STATE_STOPPING && running) {
                    if (frame_begun) end_frame(false);
                    check(xrEndSession(session), "xrEndSession");
                    running = false;
                }
                if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
                    exit_requested = true;
            }
        } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            exit_requested = true;
        } else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
            auto* change = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&event);
            if (change->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)
                recenter_change_time = change->changeTime;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
    if (state == XR_SESSION_STATE_READY && resumed && !running && !exit_requested) {
        XrSessionBeginInfo begin = {XR_TYPE_SESSION_BEGIN_INFO};
        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        running = check(xrBeginSession(session, &begin), "xrBeginSession");
        if (running) quest_xr_log("SESSION_RUNNING pointer_bits=%u", unsigned(sizeof(void*) * 8));
    }
}

bool QuestXrRuntime::begin_frame() {
    poll_events();
    views_valid = should_render = false;
    if (!running || exit_requested) return false;
    if (frame_begun) end_frame(false);
    memset(timing_frame, 0, sizeof(timing_frame));
    double start = timing_tick();
    if (start && timing_last_end) timing_frame[QUEST_XR_FRAME_GAP] = start - timing_last_end;
    XrFrameWaitInfo wait = {XR_TYPE_FRAME_WAIT_INFO};
    frame = {XR_TYPE_FRAME_STATE};
    XrResult result = xrWaitFrame(session, &wait, &frame);
    timing_record(QUEST_XR_WAIT_FRAME, start);
    if (!check(result, "xrWaitFrame")) return false;
    XrFrameBeginInfo begin = {XR_TYPE_FRAME_BEGIN_INFO};
    start = timing_tick();
    result = xrBeginFrame(session, &begin);
    timing_record(QUEST_XR_BEGIN_FRAME, start);
    if (!check(result, "xrBeginFrame")) return false;
    frame_begun = true;
    should_render = frame.shouldRender && resumed;
    XrViewLocateInfo locate = {XR_TYPE_VIEW_LOCATE_INFO};
    locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime = frame.predictedDisplayTime;
    locate.space = local_space;
    XrViewState view_state = {XR_TYPE_VIEW_STATE};
    uint32_t count = 0;
    start = timing_tick();
    result = xrLocateViews(session, &locate, &view_state, 2, &count, views);
    timing_record(QUEST_XR_LOCATE_VIEWS, start);
    if (check(result, "xrLocateViews")) {
        const XrViewStateFlags valid = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        views_valid = count == 2 && (view_state.viewStateFlags & valid) == valid;
    }
    should_render &= views_valid;
    timing_render_start = timing_tick();
    return true;
}

bool QuestXrRuntime::acquire_eye(int eye) {
    if (eye < 0 || eye > 1 || !frame_begun || !should_render) return false;
    auto& target = eyes[eye];
    double start = timing_tick();
    if (!target.acquired) {
        XrSwapchainImageAcquireInfo acquire = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (!check(xrAcquireSwapchainImage(target.swapchain, &acquire, &target.index), "xrAcquireSwapchainImage")) return false;
        target.acquired = true;
    }
    if (!target.waited) {
        XrSwapchainImageWaitInfo wait = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        XrResult result = xrWaitSwapchainImage(target.swapchain, &wait);
        // TIMEOUT_EXPIRED is a positive OpenXR result, but does not transfer
        // image ownership to the renderer. Retry the wait next frame.
        if (result == XR_TIMEOUT_EXPIRED) return false;
        if (!check(result, "xrWaitSwapchainImage")) return false;
        target.waited = true;
    }
    timing_record(QUEST_XR_ACQUIRE_WAIT, start);
    start = timing_tick();
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbos[target.index]);
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    glViewport(0, 0, target.width, target.height);
    timing_record(QUEST_XR_BIND_TARGET, start);
    return true;
}

bool QuestXrRuntime::end_frame(bool rendered) {
    if (!frame_begun) return false;
    XrCompositionLayerProjectionView projection_views[2] = {
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    bool complete = rendered && should_render && views_valid;
    timing_record(QUEST_XR_RENDER_SPAN, timing_render_start);
    double start = timing_tick();
    glFlush();
    timing_record(QUEST_XR_GL_FLUSH, start);
    for (int eye = 0; eye < 2; ++eye) {
        auto& target = eyes[eye];
        complete &= target.acquired && target.waited;
        if (target.acquired && target.waited) {
            XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            start = timing_tick();
            complete &= check(xrReleaseSwapchainImage(target.swapchain, &release), "xrReleaseSwapchainImage");
            timing_record(QUEST_XR_RELEASE, start);
            target.acquired = target.waited = false;
        }
        projection_views[eye].pose = views[eye].pose;
        projection_views[eye].fov = views[eye].fov;
        projection_views[eye].subImage.swapchain = target.swapchain;
        projection_views[eye].subImage.imageRect.extent = {target.width, target.height};
    }
    XrCompositionLayerProjection projection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projection.space = local_space;
    projection.viewCount = 2; projection.views = projection_views;
    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)};
    XrFrameEndInfo end = {XR_TYPE_FRAME_END_INFO};
    end.displayTime = frame.predictedDisplayTime;
    end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount = complete ? 1 : 0;
    end.layers = complete ? layers : nullptr;
    start = timing_tick();
    bool success = check(xrEndFrame(session, &end), "xrEndFrame");
    timing_record(QUEST_XR_END_FRAME, start);
    frame_begun = false;
    if (success && complete && ++completed_frames == 1)
        quest_xr_log("FIRST_STEREO_FRAME pointer_bits=%u", unsigned(sizeof(void*) * 8));
    timing_finish();
    return success;
}

bool QuestXrRuntime::locate_head(XrSpaceLocation* location) const {
    if (!frame_begun) return false;
    *location = {XR_TYPE_SPACE_LOCATION};
    if (!check(xrLocateSpace(view_space, local_space, frame.predictedDisplayTime, location), "locate head")) return false;
    const XrSpaceLocationFlags valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    return (location->locationFlags & valid) == valid;
}

void QuestXrRuntime::shutdown() {
    if (frame_begun) end_frame(false);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (auto& eye : eyes) {
        glDeleteFramebuffers(eye.fbos.size(), eye.fbos.data());
        glDeleteRenderbuffers(eye.depths.size(), eye.depths.data());
        if (eye.swapchain) xrDestroySwapchain(eye.swapchain);
        eye = QuestXrEye{};
    }
    if (view_space) xrDestroySpace(view_space);
    if (local_space) xrDestroySpace(local_space);
    if (session) xrDestroySession(session);
    if (instance) xrDestroyInstance(instance);
    // Permit a clean reinitialization after a lifecycle-driven teardown.
    bool was_resumed = resumed;
    *this = QuestXrRuntime{};
    resumed = was_resumed;
}
