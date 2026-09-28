/* Real draw code; only tracking, rendering, and world queries are seams. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "m_player_lib.h"
#include "m_play.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "ac_insect.h"
#include "ac_gyoei.h"
#include "ac_uki.h"
#include "ac_shop_goods_h.h"
#include "bg_item_h.h"
#include "ef_effect_control.h"
#include "sys_matrix.h"
#include "pc_vr.h"

static PLAYER_ACTOR player;
static PLAYER_ACTOR* current_player = &player;
static GAME_PLAY play;
static GRAPH graph;
static aINS_INSECT_ACTOR insect;
static aGYO_CTRL_ACTOR fish;
static UKI_ACTOR rod;
static eEC_Effect_c effect;
common_data_t common_data;
GAME* gamePT = &play.game;
static Gfx opa[128], xlu[128], dummy_model[1];
Gfx ef_kore_modelT[1];
static Gfx* frames[4] = {dummy_model, dummy_model, dummy_model, dummy_model};
static Gfx** aINS_displayList[64];
static Gfx** aGYO_displayList[64];
static int anchor_enabled, anchor_calls, field_type, draws, shadow_calls, matrix_count, checks, failures;
static float anchor[12] = {0,0,1, 100, 0,1,0, 60, -1,0,0, 200};
static xyz_t emitted_position;
static float emitted_scale;
static MtxF matrix_now, matrix_stack[8], submitted[8];
static int matrix_depth;
static Mtx allocated[8];
#define CHECK(c, label) do { ++checks; if (!(c)) { ++failures; if (failures < 20) \
    printf("FAIL: %s (line %d)\n", label, __LINE__); } } while (0)

int pc_vr_item_presentation_mtx(float out[12]) {
    ++anchor_calls;
    if (!anchor_enabled) return 0;
    memcpy(out, anchor, sizeof(anchor));
    return 1;
}
PLAYER_ACTOR* get_player_actor_withoutCheck(GAME_PLAY* g) { return current_player; }
int mPlib_get_player_actor_main_index(GAME* g) { return player.now_main_index; }
static void identity(MtxF* m) { memset(m,0,sizeof(*m)); m->xx=m->yy=m->zz=m->ww=1; }
void Matrix_get(MtxF* m) { *m=matrix_now; }
void Matrix_put(MtxF* m) { matrix_now=*m; }
void Matrix_push(void) { matrix_stack[matrix_depth++]=matrix_now; }
void Matrix_pull(void) { matrix_now=matrix_stack[--matrix_depth]; }
void Matrix_mult(MtxF* b,u8 mode) {
    MtxF a=matrix_now, result;
    if (mode==MTX_LOAD) { matrix_now=*b; return; }
    for(int col=0;col<4;col++) for(int row=0;row<4;row++) {
        result.mf[col][row]=0;
        for(int k=0;k<4;k++) result.mf[col][row]+=a.mf[k][row]*b->mf[col][k];
    }
    matrix_now=result;
}
void Matrix_translate(f32 x,f32 y,f32 z,u8 mode) {
    MtxF m; identity(&m); m.xw=x;m.yw=y;m.zw=z; Matrix_mult(&m,mode);
}
void Matrix_scale(f32 x,f32 y,f32 z,u8 mode) {
    MtxF m; identity(&m); m.xx=x;m.yy=y;m.zz=z; Matrix_mult(&m,mode);
}
static void rotate(int axis,s16 angle,int mode) {
    MtxF m; identity(&m); float a=angle*(6.28318530718f/65536),s=sinf(a),c=cosf(a);
    if(axis==0){m.yy=c;m.yz=-s;m.zy=s;m.zz=c;}
    if(axis==1){m.xx=c;m.xz=s;m.zx=-s;m.zz=c;}
    if(axis==2){m.xx=c;m.xy=-s;m.yx=s;m.yy=c;}
    Matrix_mult(&m,mode);
}
void Matrix_RotateX(s16 a,int mode){rotate(0,a,mode);}
void Matrix_RotateY(s16 a,int mode){rotate(1,a,mode);}
void Matrix_RotateZ(s16 a,int mode){rotate(2,a,mode);}
void Matrix_Position_Zero(xyz_t* p){p->x=matrix_now.xw;p->y=matrix_now.yw;p->z=matrix_now.zw;}
Mtx* _Matrix_to_Mtx_new(GRAPH* g) { submitted[matrix_count]=matrix_now;return &allocated[matrix_count++]; }
void _texture_z_light_fog_prim_xlu(GRAPH* g) {}
void mAc_UnagiActorShadow(ACTOR* a,GAME* g,xyz_t offset) { shadow_calls++; }
static float test_sin(s16 angle) { return sinf(angle*(6.28318530718f/65536)); }
#define sin_s test_sin
#undef RANDOM
#undef RANDOM_F
#undef RANDOM2
#define RANDOM(n) 1
#define RANDOM_F(n) 0.0f
#define RANDOM2(n) 1
#undef GETREG
#define GETREG(group,index) 0
#undef mFI_GET_TYPE
#define mFI_GET_TYPE(id) field_type
#undef mFI_GetFieldId
#define mFI_GetFieldId() 0
int mFI_Wpos2UtNum(int* ux,int* uz,xyz_t p){*ux=2;*uz=3;return 1;}
static void bg_draw(GAME* g,mActor_name_t item,xyz_t* pos,f32 scale,bIT_DRAW_BF_PROC bf,bIT_DRAW_AF_PROC af,rgba_t* c) {
    ++draws;emitted_position=*pos;emitted_scale=scale;
}
static void shop_draw(GAME* g,mActor_name_t item,xyz_t* pos,f32 scale,s16 angle,int flag) {
    ++draws;emitted_position=*pos;emitted_scale=scale;
}
static s16 shop_angle(int uz,int ux,int flag) {return 123;}
static void auto_matrix(GAME* g,xyz_t* p,xyz_t* scale) {
    Matrix_translate(p->x,p->y,p->z,MTX_LOAD);
    Matrix_mult(&play.billboard_matrix,MTX_MULT);
    Matrix_scale(scale->x,scale->y,scale->z,MTX_MULT);
    _Matrix_to_Mtx_new(g->graph);
}
#define eYajirushi_ALPHA effect->effect_specific[0]
#include "item_display_source.inc"

static void reset(void) {
    static bIT_Clip_c bg;
    static aSG_Clip_c shop;
    static eEC_EffectControl_Clip_c effects;
    memset(&player,0,sizeof(player));memset(&play,0,sizeof(play));memset(&graph,0,sizeof(graph));
    memset(&insect,0,sizeof(insect));memset(&fish,0,sizeof(fish));memset(&rod,0,sizeof(rod));memset(&effect,0,sizeof(effect));
    memset(&common_data,0,sizeof(common_data));memset(submitted,0,sizeof(submitted));
    current_player=&player; anchor_enabled=1;anchor_calls=draws=shadow_calls=matrix_count=matrix_depth=0;
    graph.polygon_opaque_thaga.thaGfx.head_p=opa;graph.polygon_translucent_thaga.thaGfx.head_p=xlu;
    graph.dt_num_60fps_frames=1;play.game.graph=&graph;
    identity(&matrix_now);identity(&play.billboard_matrix);
    field_type=mFI_FIELD_FG;
    bg.single_draw_proc=bg_draw;shop.single_draw_proc=shop_draw;shop.single_get_angle_y_proc=shop_angle;
    effects.auto_matrix_xlu_proc=auto_matrix;
    common_data.clip.bg_item_clip=&bg;common_data.clip.shop_goods_clip=&shop;common_data.clip.effect_clip=&effects;
    player.now_main_index=mPlayer_INDEX_GET_SCOOP;player.keyframe0.frame_control.current_frame=80;
    player.left_hand_pos=(xyz_t){10,20,30};player.scoop_pos=(xyz_t){11,21,31};
    player.main_data.get_scoop.item=1;player.main_data.get_scoop.scale=.01f;
    player.item_net_catch_label=(u32)&insect;player.item_net_catch_type=mPlayer_NET_CATCH_TYPE_INSECT;
    player.fishing_rod_actor_p=(ACTOR*)&rod;rod.child_actor=(ACTOR*)&fish;fish.linked_actor=(ACTOR*)&rod;
    fish.draw_type=aGYO_DRAW_TYPE_FISH;fish.gyo_type=aGYO_TYPE_CRUCIAN_CARP;
    ((ACTOR*)&fish)->world.position=(xyz_t){10,20,30};((ACTOR*)&fish)->scale=(xyz_t){.01f,.02f,.03f};
    insect.type=aINS_INSECT_TYPE_COMMON_BUTTERFLY;insect.tools_actor.init_matrix=1;
    identity(&insect.tools_actor.matrix_work);
    insect.tools_actor.matrix_work.xw=10;insect.tools_actor.matrix_work.yw=20;insect.tools_actor.matrix_work.zw=30;
    insect.tools_actor.actor_class.world.position=(xyz_t){10,20,30};
    insect.tools_actor.actor_class.scale=(xyz_t){.01f,.02f,.03f};
    effect.position=(xyz_t){10,20,30};effect.scale=(xyz_t){.1f,.2f,.3f};effect.item_name=RSV_NO;
    for(int i=0;i<64;i++){aINS_displayList[i]=frames;aGYO_displayList[i]=frames;}
}
static int anchor_position(xyz_t p) { return p.x==anchor[3]&&p.y==anchor[7]&&p.z==anchor[11]; }
static int same_position(xyz_t a,xyz_t b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static int matrix_position(MtxF* m) {return m->xw==anchor[3]&&m->yw==anchor[7]&&m->zw==anchor[11];}

static void test_gates(void) {
    for(int state=-1;state<=mPlayer_INDEX_NUM;state++) {
        xyz_t pos;MtxF m;
        reset();player.now_main_index=state;
        CHECK(Player_actor_vr_find_position(&player,&play,&pos)==(state==mPlayer_INDEX_GET_SCOOP||state==mPlayer_INDEX_PUTAWAY_SCOOP),
              "only find/putaway state changes fossil rendering");
        CHECK(aINS_vr_catch_matrix(&insect,&play,&m)==(state==mPlayer_INDEX_PULL_NET||state==mPlayer_INDEX_NOTICE_NET||state==mPlayer_INDEX_PUTAWAY_NET),
              "only held-catch insect states change bug rendering");
        CHECK(aGYO_vr_catch_matrix(&fish,&play,0,&m)==(state==mPlayer_INDEX_NOTICE_ROD||state==mPlayer_INDEX_PUTAWAY_ROD),
              "only displayed catch fish states change fish rendering");
    }
    for(int disabled=0;disabled<3;disabled++) {
        xyz_t pos={1,2,3};MtxF m, original; memset(&m,0x3c,sizeof(m));original=m;
        reset();if(disabled==0)anchor_enabled=0;if(disabled==1)play.submenu.menu_type=mSM_OVL_INVENTORY;
        if(disabled==2)play.submenu.current_menu_type=mSM_OVL_MAP;
        CHECK(!Player_actor_vr_find_position(&player,&play,&pos)&&pos.x==1&&pos.y==2&&pos.z==3,"flat/invalid tracking/menu fossil fallback leaves output untouched");
        player.now_main_index=mPlayer_INDEX_NOTICE_NET;
        CHECK(!aINS_vr_catch_matrix(&insect,&play,&m)&&!memcmp(&m,&original,sizeof(m)),"flat/invalid tracking/menu insect fallback");
        player.now_main_index=mPlayer_INDEX_NOTICE_ROD;
        CHECK(!aGYO_vr_catch_matrix(&fish,&play,0,&m)&&!memcmp(&m,&original,sizeof(m)),"flat/invalid tracking/menu fish fallback leaves matrix untouched");
    }
    for(int defect=0;defect<5;defect++) {
        MtxF m;reset();player.now_main_index=mPlayer_INDEX_NOTICE_ROD;
        if(defect==0)current_player=NULL;if(defect==1)player.fishing_rod_actor_p=NULL;
        if(defect==2)rod.child_actor=NULL;if(defect==3)fish.linked_actor=NULL;if(defect==4)fish.draw_type=aGYO_DRAW_TYPE_GYOEI;
        CHECK(!aGYO_vr_catch_matrix(&fish,&play,0,&m),"unrelated/free/released/shadow fish stays in world");
    }
    for(int defect=0;defect<3;defect++) {
        MtxF m;reset();player.now_main_index=mPlayer_INDEX_NOTICE_NET;
        if(defect==0)current_player=NULL;if(defect==1)player.item_net_catch_label=0;
        if(defect==2)player.item_net_catch_type=mPlayer_NET_CATCH_TYPE_ANT;
        CHECK(!aINS_vr_catch_matrix(&insect,&play,&m),"unrelated/unconverted bug stays in world");
    }
}

static void test_fossils(void) {
    for(int indoor=0;indoor<2;indoor++)for(int active=0;active<2;active++)for(int stage=0;stage<4;stage++) {
        reset();anchor_enabled=active;field_type=indoor?mFI_FIELD_ROOM0:mFI_FIELD_FG;
        player.keyframe0.frame_control.current_frame=stage==0?42.0f:42.01f;
        if(stage==2){player.now_main_index=mPlayer_INDEX_PUTAWAY_SCOOP;player.main_data.putaway_scoop.item=1;player.main_data.putaway_scoop.scale=.007f;}
        if(stage==3){player.now_main_index=mPlayer_INDEX_PICKUP;player.main_data.pickup.item=1;player.main_data.pickup.scale=.006f;player.main_data.pickup.item_pos=(xyz_t){12,22,32};}
        PLAYER_ACTOR saved=player;
        test_fossil_draw((ACTOR*)&player,&play.game);
        CHECK(draws==1,"existing outdoor/indoor item callback still called once");
        CHECK(anchor_position(emitted_position)==(active&&stage!=0&&stage!=3),"find only moves once off shovel, remains ahead through putaway");
        if(stage==0)CHECK(same_position(emitted_position,player.scoop_pos),"shovel extraction position unchanged through exact frame42");
        if(stage==3)CHECK(same_position(emitted_position,player.main_data.pickup.item_pos),"ordinary ground pickup unchanged");
        CHECK(!memcmp(&saved,&player,sizeof(player)),"fossil draw does not change item/hand/dig/drop/collision state");
        CHECK(emitted_scale==(stage==2?.007f:stage==3?.006f:.01f),"original appearance and shrinking scale preserved");
    }
    reset();player.main_data.get_scoop.scale=0;test_fossil_draw((ACTOR*)&player,&play.game);
    CHECK(draws==0&&anchor_calls==0,"invisible find is not prematurely presented");
}

static void test_insects(void) {
    const int types[]={aINS_INSECT_TYPE_COMMON_BUTTERFLY,aINS_INSECT_TYPE_SPIDER,aINS_INSECT_TYPE_GRASSHOPPER,aINS_INSECT_TYPE_FIREFLY,aINS_INSECT_TYPE_SPIRIT,aINS_INSECT_TYPE_BEE,aINS_INSECT_TYPE_ANT};
    for(unsigned type=0;type<sizeof(types)/sizeof(types[0]);type++)for(int stage=0;stage<4;stage++) {
        MtxF baseline; aINS_INSECT_ACTOR after_stock;
        for(int active=0;active<2;active++) {
            reset();anchor_enabled=active;insect.type=types[type];player.now_main_index=stage<2?mPlayer_INDEX_PULL_NET:stage==2?mPlayer_INDEX_NOTICE_NET:mPlayer_INDEX_PUTAWAY_NET;
            player.keyframe0.frame_control.current_frame=stage==0?15.0f:15.01f;
            aINS_actor_draw_sub(&graph,&insect,&play.game,0,200);
            CHECK(matrix_count==1,"insect layer submits exactly one original model matrix");
            if(!active){baseline=submitted[0];after_stock=insect;}
            else {
                MtxF compare=submitted[0];compare.xw=baseline.xw;compare.yw=baseline.yw;compare.zw=baseline.zw;
                int billboard=types[type]==aINS_INSECT_TYPE_FIREFLY||types[type]==aINS_INSECT_TYPE_SPIRIT;
                if(billboard&&stage!=0) {
                    CHECK(submitted[0].xz==.03f&&submitted[0].zx==-.01f&&submitted[0].yy==.02f,
                          "caught glow/spirit sprite uses headset-facing basis and original scale");
                } else {
                    CHECK(!memcmp(&compare,&baseline,sizeof(compare)),"ordinary species rotation/scale remain identical to stock insect rendering");
                }
                CHECK(matrix_position(&submitted[0])==(stage!=0),"insect transfers only after frame15 and stays ahead during putaway");
                CHECK(!memcmp(&after_stock,&insect,sizeof(insect)),"VR adds no insect world/matrix/collision mutations beyond stock attachment draw");
            }
            {
                int matrices=0;
                for(Gfx* g=xlu;g<graph.polygon_translucent_thaga.thaGfx.head_p;g++)
                    if((g->words.w0>>24)==G_MTX)matrices++;
                int billboard=types[type]==aINS_INSECT_TYPE_FIREFLY||types[type]==aINS_INSECT_TYPE_SPIRIT;
                CHECK(matrices==(1+(billboard&&(!active||stage==0))),
                      "headset billboard replaces only caught sprite's old-camera matrix command");
            }
        }
    }
}

static void test_fish_and_arrow(void) {
    for(int active=0;active<2;active++)for(int putaway=0;putaway<2;putaway++) {
        reset();anchor_enabled=active;player.now_main_index=putaway?mPlayer_INDEX_PUTAWAY_ROD:mPlayer_INDEX_NOTICE_ROD;
        xyz_t original=((ACTOR*)&fish)->world.position;MtxF old_billboard=play.billboard_matrix;
        aGYO_actor_draw_fish(&graph,&fish,&play.game);
        CHECK(matrix_count==1&&shadow_calls==1,"fish model and original shadow still submitted once");
        CHECK(matrix_position(&submitted[0])==active,"current caught fish render follows gaze anchor");
        CHECK(same_position(original,((ACTOR*)&fish)->world.position)&&!memcmp(&old_billboard,&play.billboard_matrix,sizeof(old_billboard)),
              "fish physics/rod position and shared scene billboard unchanged");
        CHECK(active?(submitted[0].xz==.03f&&submitted[0].zx==-.01f):(submitted[0].xx==.01f&&submitted[0].zz==.03f),
              "fish billboard faces headset while preserving nonuniform original scale");
    }
    reset();player.now_main_index=mPlayer_INDEX_NOTICE_ROD;fish.gyo_type=aGYO_TYPE_CARP;
    aGYO_actor_draw_fish(&graph,&fish,&play.game);
    CHECK(fabsf(submitted[0].yw-(anchor[7]-2.2f))<1e-5f,"original species-specific fish height correction retained");
    for(int active=0;active<2;active++)for(int net=0;net<2;net++) {
        reset();anchor_enabled=active;player.now_main_index=net?mPlayer_INDEX_NOTICE_NET:mPlayer_INDEX_NOTICE_ROD;
        if(net)player.main_data.notice_net.end_effect_flag=1;else player.main_data.notice_rod.end_effect_flag=1;
        eEC_Effect_c saved=effect;MtxF saved_stack=matrix_now;
        eYajirushi_dw(&effect,&play.game);
        CHECK(matrix_count==1&&matrix_position(&submitted[0])==active,"tiny-catch pointer follows same presentation anchor");
        CHECK(!memcmp(&saved,&effect,sizeof(effect)),"pointer world position/lifetime/alpha unchanged");
        if(active)CHECK(matrix_depth==0&&!memcmp(&saved_stack,&matrix_now,sizeof(saved_stack)),"pointer preserves caller matrix stack");
    }
    for(int defect=0;defect<4;defect++) {
        MtxF m;reset();player.now_main_index=mPlayer_INDEX_NOTICE_NET;player.main_data.notice_net.end_effect_flag=1;
        if(defect==0)effect.item_name=1;if(defect==1)player.main_data.notice_net.end_effect_flag=0;
        if(defect==2)player.now_main_index=mPlayer_INDEX_WAIT;if(defect==3)play.submenu.menu_type=mSM_OVL_MAP;
        CHECK(!eYajirushi_vr_presentation_matrix(&effect,&play,&m),"non-catch/inactive/menu pointer keeps original placement");
    }
}

int main(void) {
    test_gates();test_fossils();test_insects();test_fish_and_arrow();
    printf("VR catch/find drawing: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
