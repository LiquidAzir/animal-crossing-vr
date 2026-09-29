/* Production actor lifecycle functions, with simulated field/allocation seams. */
#include <stdio.h>
#include <string.h>
#include "m_common_data.h"
#include "ac_birth_control.h"
#include "ac_boat.h"
#include "ac_boat_demo.h"
#include "ac_npc_sendo.h"
#define bzero(p,n) memset(p,0,n)

common_data_t common_data;
int g_pc_town_residency, g_mPlib_wade_disabled;
static GAME_PLAY play;
static BOAT_ACTOR boat;
static NPC_SENDO_ACTOR sendo;
static aNPC_Clip_c clip;
static int field_valid=1, field_x=5, field_z=6, island;
static int initializations, deletes, spawns, transitions, have_boat, have_sendo;
static int pooled, freed_pool, allocated, freed, checked_name, checks, failures;
static unsigned char memory[sizeof(NPC_SENDO_ACTOR)];
static mDemo_Clip_c aBTD_clip;
static u8 aBTD_island_ldr[1], aBTD_island_prg[1];
#define CHECK(x, msg) do {checks++; if(!(x)){printf("FAIL: %s:%d %s\n",__FILE__,__LINE__,msg); failures++;}}while(0)
int mFI_Wpos2BlockNum(int* x,int* z,xyz_t p){*x=field_x;*z=field_z;return field_valid;}
int mFI_CheckBlockKind(int x,int z,u32 kind){return island && x==field_x && z==field_z;}
void mGcgba_InitVar(void){initializations++;}
void Actor_delete(ACTOR* a){deletes++; a->mv_proc=NULL; a->dw_proc=NULL;}
ACTOR* Actor_info_fgName_search(Actor_info* info,mActor_name_t name,int part){
    if(name==BOAT) return have_boat ? &boat.actor_class : NULL;
    if(name==SP_NPC_SENDO) return have_sendo ? &sendo.npc_class.actor_class : NULL;
    return NULL;
}
static int setup(GAME_PLAY* p,mActor_name_t name,s8 idx,int mv,s16 arg,int bx,int bz,int ux,int uz){
    spawns++; have_sendo=1; return TRUE;
}
static void aBTD_setupAction(BOAT_DEMO_ACTOR* demo,GAME_PLAY* p,int action){demo->action=action;transitions++;}
static void aBT_setupAction(BOAT_ACTOR* b,GAME_PLAY* p,int action){transitions++;}
ACTOR* Actor_info_make_actor(Actor_info* info,GAME* g,s16 profile,f32 x,f32 y,f32 z,
    s16 rx,s16 ry,s16 rz,s8 bx,s8 bz,s16 mv,mActor_name_t name,s16 arg,s8 idx,int bank){spawns++;return &boat.actor_class;}
void* zelda_malloc(size_t size){allocated++; memset(memory,0,sizeof(memory));return memory;}
void zelda_free(void* p){freed++;memset(p,0xA5,sizeof(memory));}
static ACTOR* pool(size_t size,const char* file,int line){pooled++;return &sendo.npc_class.actor_class;}
static void pool_free(ACTOR* a){freed_pool++;}
static void dma(aNPC_draw_data_c* out,mActor_name_t name){}
static void actor_free_check(ACTOR_DLFTBL* d,mActor_name_t name){checked_name=name;}
static void restore_fgdata_one(ACTOR* a,GAME_PLAY* p){}
static void Actor_dt(ACTOR* a,GAME* g){}
static ACTOR* Actor_info_part_delete(Actor_info* info,ACTOR* a){return NULL;}
#include "dock_source.inc"

int main(void){
    BIRTH_CONTROL_ACTOR birth={0}; BOAT_DEMO_ACTOR demo={0}; ACTOR villager={0};
    common_data.clip.npc_clip=&clip; clip.setupActor_proc=setup;
    clip.get_actor_area_proc=pool;clip.free_actor_area_proc=pool_free;clip.dma_draw_data_proc=dma;
    play.block_table.block_x=1;play.block_table.block_z=1;
    g_pc_town_residency=1;
    CHECK(aBT_check_alive(&boat,&play),"distant boat stays resident");
    field_valid=0;CHECK(!aBT_check_alive(&boat,&play),"invalid boat location is not retained");field_valid=1;
    g_pc_town_residency=0;CHECK(!aBT_check_alive(&boat,&play),"flat distant boat retains vanilla deletion");
    play.block_table.block_x=5;play.block_table.block_z=6;CHECK(aBT_check_alive(&boat,&play),"flat local boat stays alive");
    play.block_table.block_x=1;play.block_table.block_z=1;g_pc_town_residency=1;
    CHECK(aBC_chk_near_boat_block(&birth,&play),"town can initialize dock from distant acre");
    CHECK(initializations==1 && birth.boat_spawned,"connection starts once");
    have_boat=1;CHECK(!aBC_chk_near_boat_block(&birth,&play) && initializations==1,"existing boat never resets connection");
    have_boat=0;island=1;play.block_table.block_x=field_x;play.block_table.block_z=field_z;
    CHECK(!aBC_chk_near_boat_block(&birth,&play),"island load does not seed town dock");island=0;
    g_pc_town_residency=0;birth.boat_spawned=0;play.block_table.block_x=1;play.block_table.block_z=1;
    CHECK(!aBC_chk_near_boat_block(&birth,&play),"flat spawn range unchanged");
    play.block_table.block_x=5;play.block_table.block_z=5;CHECK(aBC_chk_near_boat_block(&birth,&play),"original approach acre still initializes");
    g_pc_town_residency=g_mPlib_wade_disabled=1;
    sendo.npc_class.actor_class.npc_id=SP_NPC_SENDO;
    sendo.npc_class.actor_class.block_x=1;sendo.npc_class.actor_class.block_z=1;
    villager.block_x=1;villager.block_z=1;villager.npc_id=0;
    sendo.npc_class.actor_class.next_actor=&villager;
    play.actor_info.list[ACTOR_PART_NPC].actor=&sendo.npc_class.actor_class;
    deletes=0;aBC_deleteActor_part(&play,ACTOR_PART_NPC);
    CHECK(deletes==1,"distant villager can despawn while Kapp'n remains");
    g_pc_town_residency=0;deletes=0;aBC_deleteActor_part(&play,ACTOR_PART_NPC);
    CHECK(deletes==2,"flat NPC deletion unchanged");
    spawns=deletes=transitions=0;have_boat=0;
    aBTD_sendo_birth_wait(&demo,&play);
    CHECK(deletes==1 && spawns==0 && transitions==0,"missing boat removes controller without orphan passenger");
    have_boat=1;common_data.clip.npc_clip=NULL;aBTD_sendo_birth_wait(&demo,&play);
    CHECK(spawns==0,"waits safely for NPC system");common_data.clip.npc_clip=&clip;
    aBTD_sendo_birth_wait(&demo,&play);
    CHECK(spawns==1 && demo.boat_actor==&boat && demo.npc_sendo_actor==&sendo,"passenger pairs with existing boat");
    CHECK(boat.actor_class.parent_actor==(ACTOR*)&demo && sendo.npc_class.actor_class.parent_actor==(ACTOR*)&demo,"both parent links established");
    CHECK(demo.action==aBTD_ACTION_PL_RIDE_ON_START_WAIT,"normal boarding state retained");
    mDemo_Clip_c other={0};common_data.clip.demo_clip2=&other;spawns=0;
    aBT_demo_ctrl_birth_wait(&boat,&play);CHECK(spawns==0,"distant dock cannot replace another demo");
    common_data.clip.demo_clip2=NULL;aBT_demo_ctrl_birth_wait(&boat,&play);CHECK(spawns==1,"dock controller starts when slot is available");
    island=1;play.block_table.block_x=1;play.block_table.block_z=1;
    memset(&demo,0,sizeof(demo));aBTD_actor_ct((ACTOR*)&demo,(GAME*)&play);
    CHECK(demo.at_island && demo.island_npc_info_registered,"trip origin follows boat acre when player is elsewhere");
    other.demo_class=NULL;common_data.clip.demo_clip2=&other;aBTD_actor_dt((ACTOR*)&demo,(GAME*)&play);
    CHECK(common_data.clip.demo_clip2==&other,"teardown preserves another demo's clip");
    common_data.clip.demo_clip2=&aBTD_clip;aBTD_actor_dt((ACTOR*)&demo,(GAME*)&play);
    CHECK(common_data.clip.demo_clip2==NULL,"teardown clears its own clip");
    ACTOR* result; ACTOR_PROFILE profile={0}; ACTOR_DLFTBL table={0}; profile.class_size=sizeof(sendo);
    CHECK(Actor_malloc_actor_class(&result,&profile,&table,"Kappn",SP_NPC_SENDO),"Kapp'n allocates");
    CHECK(allocated==1 && pooled==0,"resident Kapp'n leaves villager pool untouched");
    result->npc_id=SP_NPC_SENDO;result->dlftbl=&table;table.ram_start=(void*)1;table.num_actors=1;
    Actor_info_delete(&play.actor_info,result,(GAME*)&play);
    CHECK(freed==1 && freed_pool==0 && table.num_actors==0,"Kapp'n allocation freed once");
    CHECK(checked_name==SP_NPC_SENDO,"cleanup uses cached name after allocation has been poisoned/freed");
    printf("Dock lifecycle: %d checks, %d failures\n",checks,failures);return failures!=0;
}
