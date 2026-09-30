/* Isolated integration probe. Normal actor update and real GX rendering run
 * between the same climate/background-controller operations used by the boat.
 * This is a lifecycle regression, not scripted boarding or an island visit. */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_vr.h"
#include "m_play.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_bg_item.h"

extern int pc_gx_draw_call_count;
extern GAME* game_class_p;
extern void play_main(GAME*);
extern void pc_gx_draw_pending(void);
extern void __real_pc_platform_swap_buffers(void);
extern void __real_Actor_info_draw_actor(GAME_PLAY*, Actor_info*);
extern SDL_Window* __real_SDL_CreateWindow(const char*,int,int,int,int,Uint32);
static unsigned frames, checks, phase, gaps[2], restored[2], gap_houses[2];
static GAME_PLAY* probe_play;
static FILE* report;

static void check(int okay,const char* label) {
    ++checks;
    if(!okay){fprintf(report,"FAIL frame=%u %s\n",frames,label);fflush(report);exit(9);}
}
static unsigned houses(GAME_PLAY* play,int drawn_only) {
    unsigned count=0;
    for(ACTOR* a=play->actor_info.list[ACTOR_PART_ITEM].actor;a;a=a->next_actor)
        if(a->id==mAc_PROFILE_HOUSE&&a->ct_proc==NULL&&(!drawn_only||a->drawn))++count;
    return count;
}
SDL_Window* __wrap_SDL_CreateWindow(const char* title,int x,int y,int w,int h,Uint32 flags) {
    flags&=~(SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP);
    return __real_SDL_CreateWindow(title,x,y,w,h,flags|SDL_WINDOW_HIDDEN);
}
static void capture(const char* filename) {
    int w,h;SDL_GL_GetDrawableSize(g_pc_window,&w,&h);
    size_t stride=(size_t)w*4;
    unsigned char* pixels=malloc(stride*h),*upright=malloc(stride*h);
    check(pixels&&upright,"capture storage");
    glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(int y=0;y<h;++y)memcpy(upright+y*stride,pixels+(h-y-1)*stride,stride);
    SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(upright,w,h,32,(int)stride,SDL_PIXELFORMAT_RGBA32);
    check(surface!=NULL,"capture surface");check(SDL_SaveBMP(surface,filename)==0,"capture written");
    SDL_FreeSurface(surface);free(pixels);free(upright);
    check(glGetError()==GL_NO_ERROR,"no GL errors");
    fprintf(report,"CAPTURE %s houses=%u game_frame=%u\n",filename,houses(probe_play,1),probe_play->game_frame);fflush(report);
}
void __wrap_Actor_info_draw_actor(GAME_PLAY* play,Actor_info* info) {
    int gap=phase&&play==probe_play&&Common_Get(clip).bg_item_clip==NULL;
    if(gap){
        fprintf(report,"GAP_BEFORE_DRAW phase=%u frame=%u resident_houses=%u clip=NULL\n",phase,frames,houses(play,0));fflush(report);
        check(houses(play,0)>0,"resident house present during controller gap");
    }
    __real_Actor_info_draw_actor(play,info);
    if(gap){
        ++gaps[phase-1];gap_houses[phase-1]+=houses(play,1);
        fprintf(report,"GAP_AFTER_DRAW phase=%u drawn_houses=%u\n",phase,houses(play,1));fflush(report);
        check(houses(play,1)>0,"actual house body draw completes without shadow provider");
    }else if(phase&&play==probe_play&&gaps[phase-1])++restored[phase-1];
}
void __wrap_pc_platform_swap_buffers(void) {
    pc_gx_draw_pending();
    if(!report){report=fopen("island-transition-results.txt","w");if(!report)exit(8);
        g_pc_settings.vr_town_residency=1;pc_full_world_enable();}
    if(pc_gx_draw_call_count>100)++frames;
    if(frames==90){
        check(game_class_p&&game_class_p->exec==play_main,"actual title play scene");
        probe_play=(GAME_PLAY*)game_class_p;
        check(g_pc_town_residency&&g_pc_full_world,"full-world residency active");
        check(houses(probe_play,1)>0,"resident houses drawn before transition");
        check(Common_Get(clip).bg_item_clip!=NULL,"initial controller live");
        capture("town-before.bmp");phase=1;
        mFI_SetClimate(mFI_CLIMATE_ISLAND);mFM_toSummer();mBI_change_bg_item(probe_play);
        fprintf(report,"CHANGE phase=1 town-to-island climate=%d oldclip=%p\n",mFI_GetClimate(),Common_Get(clip).bg_item_clip);fflush(report);
    }
    if(frames==100){
        check(game_class_p==&probe_play->game,"scene instance retained");
        check(gaps[0]>0&&restored[0]>0,"outward controller gap and recovery observed");
        check(Common_Get(clip).bg_item_clip!=NULL,"outward controller restored");
        capture("island-climate-restored.bmp");phase=2;
        mFI_SetClimate(mFI_CLIMATE_0);mFM_returnSeason();mBI_change_bg_item(probe_play);
        fprintf(report,"CHANGE phase=2 island-to-town climate=%d oldclip=%p\n",mFI_GetClimate(),Common_Get(clip).bg_item_clip);fflush(report);
    }
    if(frames==110){
        check(gaps[1]>0&&restored[1]>0,"return controller gap and recovery observed");
        check(Common_Get(clip).bg_item_clip!=NULL,"return controller restored");
        capture("town-return-restored.bmp");
        fprintf(report,"PASS %u checks; gap_frames=%u,%u recovery_frames=%u,%u gap_house_draws=%u,%u; actual actor/GX lifecycle, no boat navigation or island scene change\n",checks,gaps[0],gaps[1],restored[0],restored[1],gap_houses[0],gap_houses[1]);
        fclose(report);exit(0);
    }
    __real_pc_platform_swap_buffers();
}
