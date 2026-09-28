/* Original ROM meshes and all six rod animations; production item draw,
 * matrix functions and rod-tip callback. GPU submission is the only draw seam. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "m_player.h"
#include "m_play.h"
#include "sys_matrix.h"
#include "m_skin_matrix.h"
#include "pc_disc.h"
#include "libforest/gbi_extensions.h"

int g_pc_verbose,g_pc_item_main_index_now;
void pc_load_asset(const char* name,void* data,unsigned size,unsigned offset,int kind,int swap) {}
static GAME_PLAY play;
static GRAPH graph;
static PLAYER_ACTOR player;
static Gfx display[16];
GAME* gamePT=&play.game;
static MtxF stack[16],pose[5],hand_input,other_tool;
static MtxF* Matrix_now=stack;
static int attached,checks,failures;
#define CHECK(x, why) do {++checks;if(!(x)){if(failures++<12)printf("FAIL: %s (%d)\n",why,__LINE__);}}while(0)
static int near(float a,float b){return fabsf(a-b)<0.03f;}
f32 sin_s(s16 a){return sinf(a*(6.283185307179586f/65536.0f));}
f32 cos_s(s16 a){return cosf(a*(6.283185307179586f/65536.0f));}
int pc_vr_hand_tool_mtx(float out[12]) {
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)out[r*4+c]=hand_input.mf[c][r];
    return attached;
}
void pc_vr_set_empty_hands_available(int available) {}
int mEv_CheckTitleDemo(void){return 0;}
void _texture_z_light_fog_prim(GRAPH* g) {}
int mPlib_get_player_actor_main_index(GAME* game){return 0;}
Mtx* _Matrix_to_Mtx_new(GRAPH* g){static Mtx m;return &m;}
extern Gfx tol_sponge_1_model[];
typedef void (*mPlayer_item_draw_proc)(ACTOR*,GAME*);
#define OTHER_TOOL(name) static void Player_actor_Item_draw_##name(ACTOR*a,GAME*g){other_tool=*Matrix_now;}
OTHER_TOOL(axe) OTHER_TOOL(umbrella) OTHER_TOOL(net) OTHER_TOOL(scoop)
OTHER_TOOL(balloon) OTHER_TOOL(windmill) OTHER_TOOL(fan)
static int cKF_FrameControl_play(cKF_FrameControl_c* fc){return 0;}
static void cKF_SkeletonInfo_R_morphJoint(cKF_SkeletonInfo_R_c* k){abort();}
typedef struct {s16 frame,value,tangent;} cKF_AnimKey_c;
typedef void (*mPlayer_item_net_draw_proc)(void*,GAME*,cKF_SkeletonInfo_R_c*,Gfx**,u8*,s_xyz*,xyz_t*);
#include "rod_source.inc"

extern cKF_Skeleton_R_c cKF_bs_r_tol_sao_1,cKF_bs_r_tol_sao_2;
extern Vtx tol_sao_1_v[],tol_sao_2_v[];
extern cKF_Animation_R_c cKF_ba_r_tol_sao_1_sao_get_t1,cKF_ba_r_tol_sao_1_sao_move1;
extern cKF_Animation_R_c cKF_ba_r_tol_sao_1_sao_sinari1,cKF_ba_r_tol_sao_1_sao_swing1;
extern cKF_Animation_R_c cKF_ba_r_tol_sao_1_sao_wait1,cKF_ba_r_tol_sao_1_not_sao_swing1;

static void walk(cKF_SkeletonInfo_R_c*k,int*index,cKF_draw_callback after,void*arg) {
    int i=(*index)++;
    cKF_Joint_R_c*j=&k->skeleton->joint_table[i];
    xyz_t t={j->translation.x,j->translation.y,j->translation.z};
    if(!i)t=(xyz_t){k->current_joint[0].x,k->current_joint[0].y,k->current_joint[0].z};
    s_xyz rot=k->current_joint[i+1];Gfx* model=j->model;u8 flags=j->flags;
    Matrix_push();Matrix_softcv3_mult(&t,&rot);pose[i]=*Matrix_now;
    if(after)after(&play.game,k,i,&model,&flags,arg,&rot,&t);
    for(int n=0;n<j->child;++n)walk(k,index,after,arg);
    Matrix_pull();
}
void cKF_Si3_draw_R_SV(GAME*game,cKF_SkeletonInfo_R_c*k,Mtx*matrices,
                       cKF_draw_callback before,cKF_draw_callback after,void*arg) {
    int index=0;
    CHECK(before==NULL,"no new joint overrides");
    walk(k,&index,after,arg);
    CHECK(index==5,"all five original joints drawn");
}
static xyz_t transform(MtxF*m,xyz_t p) {
    xyz_t out;Matrix_push();Matrix_put(m);Matrix_Position(&p,&out);Matrix_pull();return out;
}
static xyz_t hand_local(xyz_t p) {
    p.x-=hand_input.xw;p.y-=hand_input.yw;p.z-=hand_input.zw;
    return (xyz_t){p.x*hand_input.xx+p.y*hand_input.yx+p.z*hand_input.zx,
                   p.x*hand_input.xy+p.y*hand_input.yy+p.z*hand_input.zy,
                   p.x*hand_input.xz+p.y*hand_input.yz+p.z*hand_input.zz};
}
/* Display lists explicitly load previous/current segment matrices for the
 * overlapping mesh rings; this mapping is from those real gsSPVertex ranges. */
static int vertex_joint(int i) {
    if(i<3)return 3;if(i<16)return 4;if(i<19)return 2;
    if(i<34)return 3;if(i<37)return 1;if(i<52)return 2;return 1;
}
static void draw(int vr) {
    attached=vr;Matrix_now=stack;*Matrix_now=hand_input;
    player.right_hand_mtx=hand_input;graph.polygon_opaque_thaga.thaGfx.head_p=display;
    Player_actor_Item_draw(&player.actor_class,&play.game);
    CHECK(Matrix_now==stack,"balanced matrix stack");
    CHECK(!memcmp(Matrix_now,&hand_input,sizeof(hand_input)),"caller matrix restored");
    CHECK(player.item_rod_top_pos_set,"line endpoint remains available");
    xyz_t tip=transform(&pose[4],(xyz_t){1050,0,0});
    CHECK(near(tip.x,player.item_rod_top_pos.x)&&near(tip.y,player.item_rod_top_pos.y)&&near(tip.z,player.item_rod_top_pos.z),
          "line endpoint follows the actual final joint");
}
static void one_pose(int kind,cKF_Animation_R_c*animation,int frame,int yaw,int pitch,int roll,float scale,int trim,FILE*preview) {
    s_xyz joints[6]={{0}},target[6]={{0}};
    cKF_SkeletonInfo_R_c*k=&player.item_keyframe;
    memset(k,0,sizeof(*k));k->skeleton=kind?&cKF_bs_r_tol_sao_2:&cKF_bs_r_tol_sao_1;
    k->animation=animation;k->current_joint=joints;k->target_joint=target;k->frame_control.current_frame=frame;
    cKF_SkeletonInfo_R_play(k);s_xyz saved[6];memcpy(saved,joints,sizeof(saved));
    Skin_Matrix_SetRotateXyz_s(&hand_input,pitch,yaw,roll);
    hand_input.xw=10;hand_input.yw=20;hand_input.zw=30;
    player.item_scale=scale;player.item_rod_angle_z=trim;
    player.main_data.uki.cast_goal_point=(xyz_t){37,55,91};
    xyz_t expected_goal=player.main_data.uki.cast_goal_point;
    MtxF original_pose[5];
    draw(0);memcpy(original_pose,pose,sizeof(pose));
    xyz_t original_tip=hand_local(player.item_rod_top_pos),virtual_tip=player.item_rod_virtual_top_pos;
    draw(1);
    xyz_t corrected_tip=hand_local(player.item_rod_top_pos);
    CHECK(near(corrected_tip.x,original_tip.y)&&near(corrected_tip.y,-original_tip.x)&&near(corrected_tip.z,original_tip.z),
          "controller rod bend rolls into its vertical plane");
    if(!trim)CHECK(fabsf(corrected_tip.x)<0.03f,"all animation frames bend vertically relative to hand");
    CHECK(!memcmp(&virtual_tip,&player.item_rod_virtual_top_pos,sizeof(virtual_tip)),"virtual cast point unchanged");
    CHECK(!memcmp(&expected_goal,&player.main_data.uki.cast_goal_point,sizeof(expected_goal)),"cast destination unchanged");
    CHECK(!memcmp(saved,joints,sizeof(saved)),"authored animation state unchanged");
    Vtx*v=kind?tol_sao_2_v:tol_sao_1_v;
    for(int i=0;i<58;++i){
        xyz_t p={v[i].v.ob[0],v[i].v.ob[1],v[i].v.ob[2]};
        xyz_t old=hand_local(transform(&original_pose[vertex_joint(i)],p));
        xyz_t now=hand_local(transform(&pose[vertex_joint(i)],p));
        CHECK(near(now.x,old.y)&&near(now.y,-old.x)&&near(now.z,old.z),"every original mesh vertex receives only the shaft roll");
    }
    if(preview){
        for(int vr=0;vr<2;++vr){
            MtxF* matrices=vr?pose:original_pose;
            for(int joint=1;joint<5;++joint){
                xyz_t p=hand_local(transform(&matrices[joint],(xyz_t){0,0,0}));
                fprintf(preview,"%d,%d,%d,%d,%.5f,%.5f,%.5f\n",kind,frame,vr,joint,p.x,p.y,p.z);
            }
            xyz_t tip=vr?corrected_tip:original_tip;
            fprintf(preview,"%d,%d,%d,5,%.5f,%.5f,%.5f\n",kind,frame,vr,tip.x,tip.y,tip.z);
        }
    }
}
int main(int argc,char**argv) {
    play.game.graph=&graph;
    if(!pc_disc_init())return 2;
    u8*rel=pc_disc_extract_rel();if(!rel)return 3;
    for(int kind=0;kind<2;++kind){
        Vtx*v=kind?tol_sao_2_v:tol_sao_1_v;unsigned offset=kind?0x453BA0:0x453380;
        for(int i=0;i<58;++i){
            memcpy(&v[i],rel+offset+i*16,16);u8*p=(u8*)&v[i];
            for(int j=0;j<12;j+=2){u8 temp=p[j];p[j]=p[j+1];p[j+1]=temp;}
        }
    }
    FILE*preview=argc>1?fopen(argv[1],"w"):NULL;
    if(preview)fputs("kind,frame,vr,joint,x,y,z\n",preview);
    cKF_Animation_R_c*animations[]={&cKF_ba_r_tol_sao_1_sao_get_t1,&cKF_ba_r_tol_sao_1_sao_move1,
        &cKF_ba_r_tol_sao_1_sao_sinari1,&cKF_ba_r_tol_sao_1_sao_swing1,
        &cKF_ba_r_tol_sao_1_sao_wait1,&cKF_ba_r_tol_sao_1_not_sao_swing1};
    player.now_item_main_index=mPlayer_ITEM_MAIN_ROD_CAST;
    for(int kind=0;kind<2;++kind)for(int a=0;a<6;++a)for(int frame=1;frame<=animations[a]->frames;++frame)
        one_pose(kind,animations[a],frame,0,0,0,1,0,a==3&&(frame==8||frame==16)?preview:NULL);
    for(int kind=0;kind<2;++kind)for(int yaw=0;yaw<65536;yaw+=8192)
        for(int pitch=-16384;pitch<=16384;pitch+=8192)
            one_pose(kind,animations[3],8,yaw,pitch,8192,0.25f,0,NULL);
    for(int state=mPlayer_ITEM_MAIN_ROD_NORMAL;state<=mPlayer_ITEM_MAIN_ROD_PUTAWAY;++state){
        player.now_item_main_index=state;
        for(int kind=0;kind<2;++kind)one_pose(kind,animations[2],230,8192,-8192,0,1.25f,2048,NULL);
    }
    for(int item=mPlayer_ITEM_MAIN_AXE_NORMAL;item<=mPlayer_ITEM_MAIN_FAN_NORMAL;++item){
        if(item>=mPlayer_ITEM_MAIN_ROD_NORMAL&&item<=mPlayer_ITEM_MAIN_ROD_PUTAWAY)continue;
        for(attached=0;attached<2;++attached){
            player.now_item_main_index=item;player.item_scale=1;
            Matrix_now=stack;*Matrix_now=hand_input;player.right_hand_mtx=hand_input;
            graph.polygon_opaque_thaga.thaGfx.head_p=display;
            Player_actor_Item_draw(&player.actor_class,&play.game);
            CHECK(!memcmp(&other_tool,&hand_input,sizeof(hand_input)),"other tool dispatch retains its hand basis");
        }
    }
    if(preview)fclose(preview);
    free(rel);pc_disc_shutdown();
    printf("Rod orientation: %d checks,%d failures (both ROM meshes,all six animations,9 states,varied grips)\n",checks,failures);
    return failures!=0;
}
