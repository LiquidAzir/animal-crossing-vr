/* Diagnostic-only linker wrappers. Reuses built game objects, private config,
 * private ROM copy, empty save folders, and a hidden SDL window. No headset. */
#include "pc_platform.h"
#include "pc_pause_menu.h"
#include "pc_settings.h"
#include "m_play.h"

extern int pc_gx_draw_call_count;
extern GAME* game_class_p;
extern void play_main(GAME*);
extern void pc_gx_draw_pending(void);
extern void __real_pc_platform_swap_buffers(void);
extern SDL_Window* __real_SDL_CreateWindow(const char*,int,int,int,int,Uint32);

static unsigned world_frames,checks;
static GAME_PLAY* paused_play;
static unsigned paused_frame;
static PCSettings original;
static FILE* report;

static void check(int okay,const char* label) {
    ++checks;
    if(!okay){fprintf(report,"FAIL world_frame=%u %s\n",world_frames,label);fflush(report);exit(9);}
}

SDL_Window* __wrap_SDL_CreateWindow(const char* title,int x,int y,int w,int h,Uint32 flags) {
    flags&=~(SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP);
    return __real_SDL_CreateWindow(title,x,y,w,h,flags|SDL_WINDOW_HIDDEN);
}

static void capture(unsigned frame) {
    int w,h;SDL_GL_GetDrawableSize(g_pc_window,&w,&h);
    size_t stride=(size_t)w*4;
    unsigned char* pixels=malloc(stride*h),*upright=malloc(stride*h);
    check(pixels&&upright,"capture storage");
    glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(int y=0;y<h;++y)memcpy(upright+y*stride,pixels+(h-y-1)*stride,stride);
    SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(upright,w,h,32,(int)stride,SDL_PIXELFORMAT_RGBA32);
    check(surface!=NULL,"capture surface");
    char name[64];snprintf(name,sizeof(name),"menu-%03u.bmp",frame);
    check(SDL_SaveBMP(surface,name)==0,"capture written");
    SDL_FreeSurface(surface);free(pixels);free(upright);
    check(glGetError()==GL_NO_ERROR,"no GL errors");
    fprintf(report,"CAPTURE %s %dx%d paused=%d game_frame=%u\n",name,w,h,g_pc_paused,paused_play->game_frame);
    fflush(report);
}

void __wrap_pc_platform_swap_buffers(void) {
    pc_gx_draw_pending();
    if(!report){report=fopen("native-menu-results.txt","w");if(!report)exit(8);}
    if(pc_gx_draw_call_count>100)++world_frames;
    if(world_frames==30){
        check(game_class_p&&game_class_p->exec==play_main,"actual title play scene");
        paused_play=(GAME_PLAY*)game_class_p;paused_frame=paused_play->game_frame;
        original=g_pc_settings;
        /* Fixture-only guard bypass: use title scenery without opening saves. */
        g_pc_title_main_menu_visible=0;
        check(pc_pause_menu_open_vr_settings()&&g_pc_paused,"VR settings opens");
        fprintf(report,"OPEN game_frame=%u; title guard bypass is test-only\n",paused_frame);
    }
    if(world_frames>=30&&world_frames<=54){
        check(game_class_p==&paused_play->game,"same game instance");
        check(paused_play->game_frame==paused_frame,"gameplay frame frozen");
        float x=0,y=0;int confirm=0;
        switch(world_frames){
            case 34:case 38:case 42:x=1;break;
            case 46:x=-1;break;
            case 36:case 40:case 44:case 48:case 52:y=-1;break;
            case 50:case 54:confirm=1;break;
        }
        check(pc_pause_menu_vr_input(x,y,confirm,0)==1,"input consumed");
        check(g_pc_paused==(world_frames<54),"pause until Resume");
        if(world_frames<50)check(memcmp(&g_pc_settings,&original,sizeof(original))==0,"edits pending before Apply");
        if(world_frames==50){
            check(g_pc_settings.vr_empty_hands!=original.vr_empty_hands,"hands applied");
            check(g_pc_settings.fp_snap_degrees!=original.fp_snap_degrees,"turning applied");
            check(g_pc_settings.vr_motion_swing!=original.vr_motion_swing,"motion applied");
            check(g_pc_settings.master_volume<original.master_volume,"volume applied");
        }
    }
    if(world_frames==31||world_frames==35||world_frames==39||world_frames==43||
       world_frames==47||world_frames==51||world_frames==53||world_frames==55)capture(world_frames);
    if(world_frames==55){
        check(!g_pc_paused&&paused_play->game_frame>paused_frame,"gameplay resumes");
        fprintf(report,"PASS %u checks; actual game/menu/font/GX; hidden desktop GL, no VR runtime\n",checks);
        fclose(report);exit(0);
    }
    __real_pc_platform_swap_buffers();
}
