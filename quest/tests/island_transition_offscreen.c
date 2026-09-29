/* Shell-only ARM32 integration adapter; reuses the proven isolated game host.
 * It neither launches the installed Activity nor accesses its private saves. */
#define pc_platform_swap_buffers island_base_swap
#include "game_offscreen_profile.c"
#undef pc_platform_swap_buffers
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_bg_item.h"
#include "pc_vr.h"
#define common_data (*(common_data_t*)required(game_library,"common_data"))
#define game_class_p (*(GAME**)required(game_library,"game_class_p"))
#define play_main ((void(*)(GAME*))required(game_library,"play_main"))
#define g_pc_settings (*(PCSettings*)required(game_library,"g_pc_settings"))
#define g_pc_full_world (*(int*)required(game_library,"g_pc_full_world"))
#define g_pc_town_residency (*(int*)required(game_library,"g_pc_town_residency"))
#define pc_gx_draw_call_count (*frame_draws)
#define pc_gx_draw_pending ((void(*)(void))required(game_library,"pc_gx_draw_pending"))
#define pc_full_world_enable ((void(*)(void))required(game_library,"pc_full_world_enable"))
#define mFI_GetClimate ((int(*)(void))required(game_library,"mFI_GetClimate"))
#define mFI_SetClimate ((void(*)(int))required(game_library,"mFI_SetClimate"))
#define mFM_toSummer ((void(*)(void))required(game_library,"mFM_toSummer"))
#define mFM_returnSeason ((void(*)(void))required(game_library,"mFM_returnSeason"))
#define mBI_change_bg_item ((void(*)(GAME_PLAY*))required(game_library,"mBI_change_bg_item"))
#define __real_Actor_info_draw_actor ((void(*)(GAME_PLAY*,Actor_info*))required(game_library,"Actor_info_draw_actor"))
#define __real_pc_platform_swap_buffers island_base_swap
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
static void capture(const char* filename) {
    unsigned colored,error;
    check(capture_mirror(filename,&colored,&error),"actual GLES capture written");
    check(colored>1000&&error==0,"nonempty render and no GL errors");
    fprintf(report,"CAPTURE %s houses=%u game_frame=%u\n",filename,houses(probe_play,1),probe_play->game_frame);fflush(report);
}
void Actor_info_draw_actor(GAME_PLAY* play,Actor_info* info) {
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
void pc_platform_swap_buffers(void) {
    pc_gx_draw_pending();
    if(!report){scripted_stop=1000;report=fopen("island-transition-results.txt","w");if(!report)exit(8);
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
