/* pc_vi.c - video interface → SDL window swap + frame pacing */
#include "pc_platform.h"

#define VI_TVMODE_NTSC_INT    0
#define VI_TVMODE_NTSC_DS     1
#define VI_TVMODE_PAL_INT     4
#define VI_TVMODE_MPAL_INT    8
#define VI_TVMODE_EURGB60_INT 20

static u32 retrace_count = 0;
u32 pc_frame_counter = 0;
static Uint64 frame_start_time = 0;
static Uint64 perf_freq = 0;
#define TARGET_FRAME_TIME_MS 16 /* ~60 FPS (16.67ms per frame) */
static void (*vi_pre_callback)(u32) = NULL;
static void (*vi_post_callback)(u32) = NULL;

void VIInit(void) { }

void VIConfigure(void* rm) { (void)rm; }

void VISetNextFrameBuffer(void* fb) { (void)fb; }

void VIFlush(void) {}

/* throttle threshold: ~3072 stereo frames = ~96ms at 32kHz */
#define AUDIO_THROTTLE_FILL 6144

void VIWaitForRetrace(void) {
    if (!perf_freq) perf_freq = SDL_GetPerformanceFrequency();

    if (!pc_platform_poll_events()) {
        g_pc_running = 0;
        return;
    }

    pc_platform_swap_buffers();

    if (!g_pc_no_framelimit) {
        if (pc_audio_is_active()) {
            /* audio-clock pacing: hold until ring buffer drains, locks game to 32kHz audio clock */
            Uint64 spin_start = SDL_GetPerformanceCounter();
            while (pc_audio_get_buffer_fill() > AUDIO_THROTTLE_FILL) {
                SDL_Delay(1);
                /* safety timeout: ~2 frames */
                Uint64 elapsed = SDL_GetPerformanceCounter() - spin_start;
                if (elapsed * 1000 / perf_freq > 34) break;
            }
        } else {
            /* fallback before audio starts */
            Uint64 now = SDL_GetPerformanceCounter();
            Uint64 elapsed_ms = (now - frame_start_time) * 1000 / perf_freq;
            if (elapsed_ms < TARGET_FRAME_TIME_MS) {
                SDL_Delay((Uint32)(TARGET_FRAME_TIME_MS - elapsed_ms));
            }
        }
    }

    {
        static Uint64 fps_start = 0;
        static int fps_count = 0;
        if (fps_start == 0) fps_start = SDL_GetPerformanceCounter();
        fps_count++;
        if (fps_count >= 60) {
            Uint64 now = SDL_GetPerformanceCounter();
            double secs = (double)(now - fps_start) / (double)perf_freq;
            double fps = (double)fps_count / secs;
            char title[64];
            snprintf(title, sizeof(title), "Animal Crossing - %.1f FPS", fps);
            SDL_SetWindowTitle(g_pc_window, title);
            fps_start = now;
            fps_count = 0;
        }
    }

    frame_start_time = SDL_GetPerformanceCounter();

    retrace_count++;
    pc_frame_counter++;
}

u32 VIGetRetraceCount(void) { return retrace_count; }

void VISetBlack(BOOL black) { (void)black; }

u32 VIGetTvFormat(void) { return 0; /* VI_NTSC */ }
u32 VIGetDTVStatus(void) { return 0; }

void* VISetPreRetraceCallback(void* cb) {
    void* old = (void*)vi_pre_callback;
    vi_pre_callback = (void (*)(u32))cb;
    return old;
}

void* VISetPostRetraceCallback(void* cb) {
    void* old = (void*)vi_post_callback;
    vi_post_callback = (void (*)(u32))cb;
    return old;
}

u32 VIGetCurrentLine(void) { return 0; }

void VISetNextXFB(void* xfb) { (void)xfb; }
