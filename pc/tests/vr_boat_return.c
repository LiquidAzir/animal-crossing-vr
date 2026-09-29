/* Real lifecycle functions; field, animation and allocation are test seams. */
#include <stdio.h>
#include <string.h>
#include "m_common_data.h"
#include "ac_boat.h"
#include "ac_boat_demo.h"
#define bzero(p,n) memset(p,0,n)

common_data_t common_data;
int g_pc_town_residency;
static GAME_PLAY play;
static mDemo_Clip_c aBTD_clip;
static u8 aBTD_island_ldr[1], aBTD_island_prg[1];
cKF_Skeleton_R_c cKF_bs_r_obj_e_boat;
cKF_Animation_R_c cKF_ba_r_obj_e_boat;
static int deletions, collision_deletes, collision_births, last_collision, fg_sets;
static xyz_t restored;
static ACTOR* deleted[8];
static int checks, failures;
#define CHECK(x,why) do { checks++; if(!(x)){failures++;printf("FAIL %d: %s\n",__LINE__,why);} }while(0)
int mFI_Wpos2BlockNum(int* x, int* z, xyz_t p) {
    *x=(int)(p.x/640); *z=(int)(p.z/640); return p.x>=0 && p.z>=0;
}
int mFI_CheckBlockKind(int x,int z,u32 kind) {
    return kind==mRF_BLOCKKIND_ISLAND && x>=4 && x<=5 && z==8;
}
int mFI_SetFG_common(mActor_name_t item,xyz_t p,int update) {
    CHECK(item==BOAT && !update,"destructor restores BOAT without field refresh");
    restored=p;fg_sets++;return TRUE;
}
void Actor_delete(ACTOR* a) { deleted[deletions++ % 8]=a; a->mv_proc=NULL;a->dw_proc=NULL; }
void xyz_t_move(xyz_t* dst,const xyz_t* src) { *dst=*src; }
f32 mCoBG_GetWaterHeight_File(xyz_t p,char* file,int line) { return 0; }
void cKF_SkeletonInfo_R_ct(cKF_SkeletonInfo_R_c* k,cKF_Skeleton_R_c* s,cKF_Animation_R_c* a,s_xyz* w,s_xyz* t) { k->skeleton=s; }
void cKF_SkeletonInfo_R_init(cKF_SkeletonInfo_R_c* k,cKF_Skeleton_R_c* s,cKF_Animation_R_c* a,
    f32 b,f32 c,f32 d,f32 e,f32 f,int mode,s_xyz* t) {}
int cKF_SkeletonInfo_R_play(cKF_SkeletonInfo_R_c* k) { return 0; }
void cKF_SkeletonInfo_R_dt(cKF_SkeletonInfo_R_c* k) {}
static void aBT_setupAction(BOAT_ACTOR* b,GAME_PLAY* p,int action) { b->action=action; }
static void aBTD_setupAction(BOAT_DEMO_ACTOR* b,GAME_PLAY* p,int action) { b->action=action; }
void mCoBG_CrossOffMoveBg(int idx) { collision_deletes++;last_collision=idx; }
ACTOR* Actor_info_fgName_search(Actor_info* info,mActor_name_t name,int part) {
    return Actor_info_fgName_search_sub(info->list[part].actor,name);
}
#include "boat_return_source.inc"
void mCoBG_MakeBoatCollision(ACTOR* a,xyz_t* p,s16* angle) {
    collision_births++;l_mCoBG_boat_move_bg_data[0].actor=a;
    l_mCoBG_boat_move_bg_data[0].move_bg_idx=7;
}
static void place(BOAT_ACTOR* b,int x,int z) {
    memset(b,0,sizeof(*b));b->actor_class.npc_id=BOAT;
    b->actor_class.block_x=x;b->actor_class.block_z=z;
    b->actor_class.world.position.x=x*640+20;b->actor_class.world.position.z=z*640+20;
    b->actor_class.home.position=b->actor_class.world.position;
}
static void run_trip(int from_island) {
    BOAT_ACTOR b, candidate, duplicate;
    BOAT_DEMO_ACTOR demo;
    NPC_SENDO_ACTOR sendo;
    int origin_z=from_island?8:6, destination_z=from_island?6:8;
    memset(&play,0,sizeof(play));memset(&demo,0,sizeof(demo));memset(&sendo,0,sizeof(sendo));
    memset(l_mCoBG_boat_move_bg_data,0,sizeof(l_mCoBG_boat_move_bg_data));
    deletions=collision_deletes=collision_births=fg_sets=0;
    g_pc_town_residency=1;
    place(&b,5,origin_z);play.actor_info.list[ACTOR_PART_ITEM].actor=&b.actor_class;
    play.block_table.block_x=from_island?4:1;play.block_table.block_z=from_island?8:1;
    aBT_actor_ct(&b.actor_class,(GAME*)&play);
    CHECK(!deletions && collision_births==1,"distant boat in correct region initializes");
    CHECK(b.direction==(from_island?aBT_DIRECTION_FROM_ISLAND:aBT_DIRECTION_TO_ISLAND),"constructor chooses correct outgoing direction");
    aBTD_actor_ct((ACTOR*)&demo,(GAME*)&play);
    CHECK(demo.at_island==from_island,"demo origin comes from boat despite distant player");
    b.actor_class.parent_actor=(ACTOR*)&demo;demo.boat_actor=&b;demo.npc_sendo_actor=&sendo;
    for(int action=0;action<aBT_ACTION_ANCHOR;action++) {
        b.action=action;
        CHECK(aBT_check_alive(&b,&play),"initial dock and every active trip phase remain resident");
    }
    b.actor_class.world.position.z=destination_z*640+20;
    b.action=aBT_ACTION_ANCHOR;
    play.block_table.block_x=5;play.block_table.block_z=destination_z;
    aBT_anchor(&b,&play);
    CHECK(b.action==aBT_ACTION_ANCHOR,"arrival stays anchored while player remains in destination acre");
    play.block_table.block_x=4;
    aBT_anchor(&b,&play);
    CHECK(b.action==-1,"completed trip retires after walking to next acre");
    aBT_actor_dt(&b.actor_class,(GAME*)&play);
    CHECK(!demo.boat_actor && collision_deletes==1 && last_collision==7,"retired boat releases only its own controller link and collision");
    CHECK(restored.z==origin_z*640+20 && fg_sets==1,"retired boat restores its original dock foreground");
    aBTD_anchor(&demo,&play);
    CHECK(deletions==2 && deleted[0]==(ACTOR*)&sendo && deleted[1]==(ACTOR*)&demo,"stock controller cleanup removes old passenger and controller");
    play.actor_info.list[ACTOR_PART_ITEM].actor=NULL;

    /* A town residency sweep can now find the restored origin BOAT first. */
    place(&candidate,5,origin_z);int births=collision_births;
    aBT_actor_ct(&candidate.actor_class,(GAME*)&play);
    CHECK(deletions==3 && collision_births==births,"opposite-region old dock cannot occupy the return boat slot");
    aBT_actor_dt(&candidate.actor_class,(GAME*)&play);
    CHECK(restored.z==origin_z*640+20 && collision_deletes==1,"rejected birth restores only its own foreground and no collision");
    place(&b,5,destination_z);play.actor_info.list[ACTOR_PART_ITEM].actor=&b.actor_class;
    aBT_actor_ct(&b.actor_class,(GAME*)&play);
    CHECK(b.direction==(from_island?aBT_DIRECTION_TO_ISLAND:aBT_DIRECTION_FROM_ISLAND),"new boat has opposite direction for return journey");
    memset(&demo,0,sizeof(demo));aBTD_actor_ct((ACTOR*)&demo,(GAME*)&play);
    CHECK(demo.at_island==!from_island && demo.action==aBTD_ACTION_SENDO_BIRTH_WAIT,"new demo is ready to create Kapp'n for return boarding");
    b.actor_class.parent_actor=(ACTOR*)&demo;demo.boat_actor=&b;
    place(&duplicate,5,destination_z);duplicate.actor_class.parent_actor=(ACTOR*)&demo;
    births=collision_births;
    aBT_actor_ct(&duplicate.actor_class,(GAME*)&play);
    CHECK(deletions==4 && collision_births==births,"same-region duplicate cannot replace live boat");
    aBT_actor_dt(&duplicate.actor_class,(GAME*)&play);
    CHECK(demo.boat_actor==&b && collision_deletes==1 && l_mCoBG_boat_move_bg_data[0].actor==&b.actor_class,
          "rejected sibling cleanup preserves active pair and owned collision");
    g_pc_town_residency=0;play.actor_info.list[ACTOR_PART_ITEM].actor=NULL;
    CHECK(aBT_check_other_boat(&candidate.actor_class,(GAME*)&play),"ordinary mode keeps stock cross-region spawning rule");
    b.action=aBT_ACTION_WAIT;
    CHECK(!aBT_check_alive(&b,&play),"ordinary mode still deletes a waiting boat outside player acre");
    play.block_table.block_x=5;
    CHECK(aBT_check_alive(&b,&play),"ordinary mode retains local dock");
}
int main(void) {
    run_trip(0);run_trip(1);
    printf("Boat return lifecycle: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
