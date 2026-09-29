/* Actual built ARM32 game/GX title path, isolated from Activity/XR/save data.
 * Host-only hooks below are executable ELF interpositions, never game edits. */
#define _GNU_SOURCE
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <elf.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "dolphin/pad.h"
#include "gles_device_context.h"

static void* game_library;
static void* sdl_library;
static unsigned swaps;
static unsigned world_frames;
static unsigned scene_frames;
static unsigned scripted_stop;
static int profiling_enabled=1;
static unsigned steady_samples;
static unsigned warmup_frames=30,sample_frames=90;
static int *frame_draws,*frame_commands,*frame_vertex_loads;
static uint64_t steady_draws,steady_commands,steady_vertex_loads;
static double last_swap,steady_elapsed,steady_peak;
typedef struct {unsigned frame,duration,buttons;int x,y;} TestPadEvent;
static TestPadEvent pad_events[128];
static unsigned pad_event_count,capture_frames[128],capture_count;
static double began, first_swap;
#ifdef OFFSCREEN_STEREO
extern void offscreen_stereo_init(void* library,int enabled,int eye_size);
extern int offscreen_stereo_capture(void);
#endif
static void* required(void* library,const char* name) {
    dlerror();void* value=dlsym(library,name);const char* error=dlerror();
    if(error){fprintf(stderr,"dlsym %s: %s\n",name,error);_Exit(3);}return value;
}
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void timeout(int signal) {(void)signal;write(2,"OFFSCREEN_TIMEOUT\n",18);_Exit(8);}
static void load_script(void) {
    FILE* input=fopen("input.txt","r");
    if(!input)return;
    char line[160];
    while(fgets(line,sizeof(line),input)){
        TestPadEvent event;unsigned frame;
        if(sscanf(line,"STOP %u",&frame)==1)scripted_stop=frame;
        else if(sscanf(line,"PAD %u %u %u %d %d",&event.frame,&event.duration,&event.buttons,&event.x,&event.y)==5){
            if(pad_event_count>=128)_Exit(3);
            pad_events[pad_event_count++]=event;
        }else if(sscanf(line,"CAPTURE %u",&frame)==1){
            if(capture_count>=128)_Exit(3);
            capture_frames[capture_count++]=frame;
        }else {_Exit(3);}
    }
    fclose(input);
    printf("OFFSCREEN_SCRIPT events=%u captures=%u stop_scene_frame=%u\n",pad_event_count,capture_count,scripted_stop);
}
static int capture_mirror(const char* filename,unsigned* colored,unsigned* error) {
    int width=*(int*)required(game_library,"g_pc_window_w");
    int height=*(int*)required(game_library,"g_pc_window_h");
    unsigned char* pixels=malloc((size_t)width*height*4);
    if(!pixels)return 0;
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    *colored=0;
    for(int i=0;i<width*height;++i)
        if(pixels[4*i]>20||pixels[4*i+1]>20||pixels[4*i+2]>20)++*colored;
    *error=glGetError();
    int saved=device_save_bmp(filename,width,height,pixels);free(pixels);
    return saved;
}

/* Use SDL timer/events and a direct EGL pbuffer. Android physical device
 * discovery requires an Activity/JNI and is deliberately excluded. */
int SDL_Init(Uint32 flags) {
    int(*real_init)(Uint32)=required(sdl_library,"SDL_Init");
    return real_init(flags&~(SDL_INIT_GAMECONTROLLER|SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC));
}
void quest_platform_bind_vr(void) {printf("OFFSCREEN: JNI/XR binding intentionally skipped\n");}
/* Android SDL's audio thread needs a JavaVM even with its dummy driver. Keep
 * actual sample generation on a native pthread while discarding device output.
 * This preserves the boot/audio state machine without requiring an Activity. */
void AIInit(u8* stack) {(void)stack;printf("OFFSCREEN: audio device intentionally skipped\n");}
void AIInitDMA(u32 address,u32 size) {(void)address;(void)size;}
static void* offscreen_audio(void* unused) {
    (void)unused;
    void(*process)(void)=required(game_library,"pc_audio_process_frame");
    for(;;){process();usleep(17500);}return NULL;
}
void pc_audio_start_producer_thread(void) {
    pthread_t thread;
    if(pthread_create(&thread,NULL,offscreen_audio,NULL))_Exit(4);
    pthread_detach(thread);
    printf("OFFSCREEN: native audio producer active; playback discarded\n");
}
void pc_platform_init(void) {
    // SDL's offscreen backend requires EXT_device_enumeration, absent on Quest.
    // Its default-display pbuffer path is available and already device-tested.
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_AUDIO|SDL_INIT_EVENTS)<0){fprintf(stderr,"SDL timer/audio: %s\n",SDL_GetError());_Exit(4);}
    if(!device_context_begin(640,480))_Exit(4);
    void(*gx)(void)=required(game_library,"pc_gx_init");gx();
    void(*packs)(void)=required(game_library,"pc_texture_pack_init");packs();
}
int pc_platform_poll_events(void) {return 1;}
BOOL PADInit(void) {return TRUE;}
u32 PADRead(PADStatus* status) {
    memset(status,0,sizeof(PADStatus)*4);
    for(int i=1;i<4;++i)status[i].err=PAD_ERR_NO_CONTROLLER;
    for(unsigned i=0;i<pad_event_count;++i){
        const TestPadEvent* event=&pad_events[i];
        if(scene_frames>=event->frame&&scene_frames-event->frame<event->duration){
            status[0].button|=event->buttons;
            status[0].stickX=event->x;status[0].stickY=event->y;
        }
    }
    return 0;
}
void PADControlMotor(s32 channel,u32 command) {(void)channel;(void)command;}
void pc_platform_swap_buffers(void) {
    void(*flush)(void)=required(game_library,"pc_gx_draw_pending");flush();
    /* Navigation scripts use game frames; menus otherwise spin much faster
     * than their real-time fades. Never pace ordinary performance runs. */
    if(scripted_stop&&last_swap>0){
        double remaining=1.0/60.0-(now()-last_swap);
        if(remaining>0)usleep((unsigned)(remaining*1000000));
    }
    double swap_time=now();
    ++swaps;
    if(*frame_draws>100){
        ++world_frames;
        if(world_frames>warmup_frames&&last_swap>0){
            double duration=swap_time-last_swap;
            steady_elapsed+=duration;++steady_samples;
            steady_draws+=*frame_draws;
            steady_commands+=*frame_commands;
            steady_vertex_loads+=*frame_vertex_loads;
            if(duration>steady_peak)steady_peak=duration;
        }
    }
    last_swap=swap_time;
    if(world_frames)++scene_frames;
    if(swaps==1)first_swap=now();
    if(swaps==30)*(int*)required(game_library,"g_pc_verbose")=0;
    if(swaps%30==0)printf("OFFSCREEN_PROGRESS frames=%u elapsed=%.3f\n",swaps,now()-first_swap);
    for(unsigned i=0;i<capture_count;++i)if(scene_frames==capture_frames[i]){
        char filename[64];unsigned colored,error;
        snprintf(filename,sizeof(filename),"capture-%06u.bmp",scene_frames);
        int saved=capture_mirror(filename,&colored,&error);
        printf("OFFSCREEN_CAPTURE scene_frame=%u saved=%d colored_pixels=%u gl_error=%x file=%s\n",scene_frames,saved,colored,error,filename);
        if(!saved||error)_Exit(7);
    }
#ifdef OFFSCREEN_STEREO
    int capture_ready=world_frames>=90||now()-began>=40;
#else
    int capture_ready=(swaps>=120&&now()-first_swap>=8)||now()-began>=25;
#endif
    if(!profiling_enabled)capture_ready=steady_samples>=sample_frames||now()-began>=40;
    if(scripted_stop)capture_ready=scene_frames>=scripted_stop||now()-began>=40;
    if(capture_ready){
        int width=*(int*)required(game_library,"g_pc_window_w");
        int height=*(int*)required(game_library,"g_pc_window_h");
        unsigned colored,error;
        int saved=capture_mirror("title.bmp",&colored,&error);
#ifdef OFFSCREEN_STEREO
        saved&=offscreen_stereo_capture();
        error|=glGetError();
#endif
        printf("OFFSCREEN_RESULT frames=%u world_frames=%u scene_frames=%u elapsed=%.3f size=%dx%d capture=%d colored_pixels=%u gl_error=%x\n",swaps,world_frames,scene_frames,now()-first_swap,width,height,saved,colored,error);
        if(steady_samples)printf("OFFSCREEN_STEADY profiling=%d warmup_world_frames=%u samples=%u elapsed_ms=%.3f avg_ms=%.3f peak_ms=%.3f cpu_frame_rate=%.2f\n",profiling_enabled,warmup_frames,steady_samples,steady_elapsed*1000,steady_elapsed*1000/steady_samples,steady_peak*1000,steady_samples/steady_elapsed);
        if(steady_samples)printf("OFFSCREEN_WORKLOAD avg_draws=%.3f avg_commands=%.3f avg_vertex_loads=%.3f\n",(double)steady_draws/steady_samples,(double)steady_commands/steady_samples,(double)steady_vertex_loads/steady_samples);
        fflush(NULL);_Exit(saved&&colored>1000&&!error?0:7);
    }
    eglSwapBuffers(test_display,test_surface);
}

int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IOLBF,0);setvbuf(stderr,NULL,_IONBF,0);
    signal(SIGALRM,timeout);alarm(50);began=now();
    setenv("SDL_VIDEODRIVER","offscreen",1);setenv("SDL_AUDIODRIVER","dummy",1);
    setenv("SDL_JOYSTICK_HIDAPI","0",1);
    load_script();
    sdl_library=dlopen("./libSDL2.so",RTLD_NOW|RTLD_GLOBAL);
    if(!sdl_library||!dlopen("./libopenxr_loader.so",RTLD_NOW|RTLD_GLOBAL)){fprintf(stderr,"dependencies: %s\n",dlerror());return 2;}
    game_library=dlopen("./libmain.so",RTLD_NOW|RTLD_LOCAL);
    if(!game_library){fprintf(stderr,"game: %s\n",dlerror());return 2;}
    frame_draws=(int*)required(game_library,"pc_gx_draw_call_count");
    frame_commands=(int*)required(game_library,"pc_emu64_frame_cmds");
    frame_vertex_loads=(int*)required(game_library,"pc_emu64_frame_vtx_cmds");
    printf("OFFSCREEN_BOOT pointer_bits=%u offscreen title; no XR session, no physical input, private temporary data only\n",(unsigned)(8*sizeof(void*)));
    // Match pc_main's range bookkeeping, using the game image rather than our
    // harness image, so segmented-address decoding recognizes relocated data.
    Dl_info image;
    if(!dladdr(required(game_library,"ac_entry"),&image)||!image.dli_fbase)return 3;
    Elf32_Ehdr* elf=image.dli_fbase;
    Elf32_Phdr* segments=(void*)((unsigned char*)elf+elf->e_phoff);
    uint32_t end=0;
    for(unsigned i=0;i<elf->e_phnum;++i)if(segments[i].p_type==PT_LOAD&&segments[i].p_vaddr+segments[i].p_memsz>end)end=segments[i].p_vaddr+segments[i].p_memsz;
    *(uint32_t*)required(game_library,"pc_image_base")=(uintptr_t)image.dli_fbase;
    *(uint32_t*)required(game_library,"pc_image_end")=(uintptr_t)image.dli_fbase+end;
    const char* profile_text=getenv("ACQUEST_TEST_PROFILE");
    profiling_enabled=!profile_text||atoi(profile_text)!=0;
    const char* warmup_text=getenv("ACQUEST_TEST_WARMUP");
    const char* samples_text=getenv("ACQUEST_TEST_SAMPLES");
    warmup_frames=warmup_text?(unsigned)atoi(warmup_text):30;
    sample_frames=samples_text?(unsigned)atoi(samples_text):90;
    if(warmup_frames<1||warmup_frames>600||sample_frames<1||sample_frames>1800)return 3;
    *(int*)required(game_library,"g_pc_profile_enabled")=profiling_enabled;
    *(int*)required(game_library,"g_pc_verbose")=1;
    *(int*)required(game_library,"g_pc_profile_interval")=30;
    *(int*)required(game_library,"g_pc_frame_limit_override")=60;
    const char* inits[]={"SDL_SetMainReady","pc_settings_load","pc_keybindings_load","pc_fp_init","pc_platform_init"};
    for(unsigned i=0;i<sizeof(inits)/sizeof(inits[0]);++i){
        printf("OFFSCREEN_INIT %s\n",inits[i]);
        if(i==4){pc_platform_init();continue;}
        void(*init)(void)=required(i?game_library:sdl_library,inits[i]);init();
    }
#ifdef OFFSCREEN_STEREO
    const char* eye_size_text=getenv("ACQUEST_TEST_EYE_SIZE");
    int eye_size=eye_size_text?atoi(eye_size_text):640;
    if(eye_size<64||eye_size>2048)_Exit(3);
    offscreen_stereo_init(game_library,1,eye_size);
#endif
    if(argc<2||strcmp(argv[1],"--stock-world")){
        void(*enable_world)(void)=required(game_library,"pc_full_world_enable");
        enable_world();
        printf("OFFSCREEN_WORLD full (Quest default, flat camera)\n");
    }else printf("OFFSCREEN_WORLD stock (flat camera)\n");
    printf("OFFSCREEN_GL renderer=%s version=%s video=direct-EGL-pbuffer audio=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION),SDL_GetCurrentAudioDriver());
    int(*disc_init)(void)=required(game_library,"pc_disc_init");
    int(*assets_init)(void)=required(game_library,"pc_assets_init");
    if(!disc_init()||!assets_init())return 4;
    void(*entry)(void)=required(game_library,"ac_entry");entry();
    void(*boot)(int,const char**)=required(game_library,"boot_main");
    const char* arguments[]={"offscreen-profile",NULL};
    printf("OFFSCREEN_GAME_START\n");boot(1,arguments);
    fprintf(stderr,"Game loop returned before bounded capture\n");return 5;
}
