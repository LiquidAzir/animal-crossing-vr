/* Seasonal actor replacement reproduces the island-trip provider gap.
 * Production m_bg_item and shadow dispatch are included verbatim; only the
 * actor pool and destructor are seams. Native rendering is tested separately. */
#include <stdio.h>
#include <string.h>
#include "m_common_data.h"
#include "bg_item_h.h"
#include "ac_npc.h"

common_data_t common_data;
static GAME_PLAY play;
static ACTOR actors[2];
static bIT_Clip_c providers[2];
static bIT_ShadowData_c shadow_data;
static ACTOR* current;
static int slot, makes, deletes, shadow_calls, retired_calls;
static GAME* received_game;
static bIT_ShadowData_c* received_data;
static int received_type, checks, failures, pit_calls, revive_requests;
static u16 aNPC_req_default_data[aNPC_REQUEST_ARG_NUM];
#define CHECK(value, msg) do { checks++; if (!(value)) { failures++; printf("FAIL %d: %s\n", __LINE__, msg); } } while (0)

static void live_shadow(GAME* game, bIT_ShadowData_c* data, int type) {
    shadow_calls++; received_game=game; received_data=data; received_type=type;
}
static void retired_shadow(GAME* game, bIT_ShadowData_c* data, int type) { retired_calls++; }
static void actor_proc(ACTOR* actor, GAME* game) {}
ACTOR* Actor_info_name_search(Actor_info* info, s16 profile, int part) {
    return current && current->id==profile ? current : NULL;
}
void Actor_delete(ACTOR* actor) {
    deletes++; actor->mv_proc=NULL; actor->dw_proc=NULL;
}
ACTOR* Actor_info_make_actor(Actor_info* info, GAME* game, s16 profile, f32 x, f32 y, f32 z,
    s16 rx, s16 ry, s16 rz, s8 bx, s8 bz, s16 mv, mActor_name_t name, s16 arg, s8 idx, int bank) {
    makes++; slot^=1; current=&actors[slot]; memset(current,0,sizeof(*current));
    current->id=profile; current->mv_proc=actor_proc; current->dw_proc=actor_proc;
    providers[slot].draw_shadow_proc=live_shadow;
    common_data.clip.bg_item_clip=&providers[slot];
    return current;
}
#include "src/game/m_bg_item.c"

static int pit_fall(mActor_name_t fg, int x, int z, mActor_name_t npc) { pit_calls++; return 0; }
static int pit_exit(mActor_name_t fg, int x, int z, mActor_name_t npc) { return 0; }
static int aNPC_chk_talk_start(NPC_ACTOR* actor) { return FALSE; }
static int aNPC_set_request_act(NPC_ACTOR* actor,u8 priority,u8 idx,u8 type,u16* args) {
    CHECK(idx==aNPC_ACT_REVIVE && priority==4 && args==aNPC_req_default_data,"original revive request preserved");
    revive_requests++; return TRUE;
}
static void aNPC_set_feel_info(NPC_ACTOR* actor,int feel,int time) {}
#define aNPC_GET_TYPE(npc) (ITEM_NAME_GET_TYPE((npc)->actor_class.npc_id))
#define aNPC_IS_NRM_NPC(npc) (aNPC_GET_TYPE(npc) == NAME_TYPE_NPC)
#include "island_npc_source.inc"

static void replacement_test(int from, int to) {
    memset(actors,0,sizeof(actors)); memset(providers,0,sizeof(providers));
    slot=0; current=&actors[0]; current->id=from; current->mv_proc=actor_proc;current->dw_proc=actor_proc;
    providers[0].draw_shadow_proc=live_shadow;common_data.clip.bg_item_clip=&providers[0];
    common_data.bg_item_profile=from;common_data.time.bgitem_profile=to;
    makes=deletes=shadow_calls=retired_calls=0; mBI_ct();
    bIT_draw_shadow_safe(common_data.clip.bg_item_clip,(GAME*)&play,&shadow_data,TRUE);
    CHECK(shadow_calls==1,"normal resident building shadow rendered");
    CHECK(received_game==(GAME*)&play && received_data==&shadow_data && received_type==TRUE,"exact callback args preserved");
    mBI_change_bg_item(&play);
    CHECK(deletes==1 && current->mv_proc==NULL && current->dw_proc==NULL,"existing seasonal actor uses normal deferred deletion");
    CHECK(common_data.bg_item_profile==to,"requested climate selects replacement actor");
    mBI_move(&play);
    CHECK(makes==0,"new actor is not made while retired one is still linked");
    /* Existing actor update calls bIT_clip_dt then unlinks the old actor. */
    current=NULL; common_data.clip.bg_item_clip=NULL; providers[0].draw_shadow_proc=retired_shadow;
    bIT_draw_shadow_safe(common_data.clip.bg_item_clip,(GAME*)&play,&shadow_data,FALSE);
    CHECK(shadow_calls==1 && retired_calls==0,"gap frame safely omits shadow without retaining retired callback");
    mBI_move(&play);
    CHECK(makes==1 && current && current->id==to && common_data.clip.bg_item_clip==&providers[1],"next update installs replacement provider");
    bIT_draw_shadow_safe(common_data.clip.bg_item_clip,(GAME*)&play,&shadow_data,FALSE);
    CHECK(shadow_calls==2 && retired_calls==0 && received_type==FALSE,"replacement provider resumes ordinary shadows");
    mBI_move(&play); CHECK(makes==1,"completed replacement does not repeat");
}

int main(void) {
    static const int profiles[]={mAc_PROFILE_BGCHERRYITEM,mAc_PROFILE_BGITEM,mAc_PROFILE_BGWINTERITEM,mAc_PROFILE_BGXMASITEM};
    for(int a=0;a<4;a++)for(int b=0;b<4;b++) replacement_test(profiles[a],profiles[b]);
    providers[0].draw_shadow_proc=NULL; shadow_calls=0;
    bIT_draw_shadow_safe(&providers[0],(GAME*)&play,&shadow_data,0);
    CHECK(shadow_calls==0,"incomplete clip has no callable shadow");
    NPC_ACTOR npc={0}; mActor_name_t foreground=BURIED_PITFALL_HOLE_START;
    npc.actor_class.npc_id=NPC_START; npc.condition_info.under_fg_p=&foreground;
    common_data.clip.bg_item_clip=NULL;
    CHECK(!aNPC_chk_pitfall(&npc) && !npc.condition_info.pitfall_flag && !pit_calls,"gap does not begin NPC pitfall without provider");
    providers[0].pit_fall_proc=NULL; common_data.clip.bg_item_clip=&providers[0];
    CHECK(!aNPC_chk_pitfall(&npc) && !pit_calls,"incomplete pitfall provider safely waits");
    providers[0].pit_fall_proc=pit_fall;
    CHECK(aNPC_chk_pitfall(&npc) && npc.condition_info.pitfall_flag && pit_calls==1,"pitfall begins normally after provider restoration");
    npc.action.step=aNPC_ACTION_END_STEP;npc.action.idx=aNPC_ACT_TALK;
    common_data.clip.bg_item_clip=NULL;
    for(int i=0;i<3;i++)aNPC_think_pitfall_main_proc(&npc,&play);
    CHECK(!revive_requests && npc.action.step==aNPC_ACTION_END_STEP && npc.action.idx==aNPC_ACT_TALK,"revive request remains retryable throughout provider gap");
    common_data.clip.bg_item_clip=&providers[0]; providers[0].pit_exit_proc=NULL;
    aNPC_think_pitfall_main_proc(&npc,&play);
    CHECK(!revive_requests,"one-shot revive init is not queued without its callback");
    providers[0].pit_exit_proc=pit_exit; aNPC_think_pitfall_main_proc(&npc,&play);
    CHECK(revive_requests==1,"next think frame queues revival after replacement ready");
    printf("Island provider lifecycle: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
