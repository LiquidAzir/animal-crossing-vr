/* Real net meshes, animation and transform functions. The only render seam
 * walks the six original skeleton joints and records their world matrices. */
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

int g_pc_verbose;
/* Unrelated tool textures are loaded by this model TU's startup constructor. */
void pc_load_asset(const char* name, void* data, unsigned size, unsigned offset, int kind, int swap) {}
static GAME_PLAY play;
static GRAPH graph;
static Gfx display[16];
static PLAYER_ACTOR player;
GAME* gamePT = &play.game;
static MtxF stack[16], pose[6];
static MtxF* Matrix_now = stack;
static int attached, checks, failures;
static int net_state = mPlayer_ITEM_MAIN_NET_NORMAL;
int g_pc_item_main_index_now;
/* Optional visual hands are tested separately; the net still uses its own pose. */
void pc_vr_set_empty_hands_available(int available) {}
int mEv_CheckTitleDemo(void) { return 0; }
static MtxF hand_input, other_tool;
#define CHECK(x, why) do {checks++; if (!(x)) {if (failures++ < 12) printf("FAIL: %s (%d)\n", why, __LINE__);}} while (0)
static int near(float a, float b) {return fabsf(a - b) < 0.02f;}
f32 sin_s(s16 a) {return sinf(a * (6.283185307179586f / 65536.0f));}
f32 cos_s(s16 a) {return cosf(a * (6.283185307179586f / 65536.0f));}
int pc_vr_hand_tool_mtx(float out[12]) {
    for(int r=0;r<3;r++) for(int c=0;c<4;c++) out[r*4+c]=hand_input.mf[c][r];
    return attached;
}
void _texture_z_light_fog_prim(GRAPH* g) {}
int mPlib_get_player_actor_main_index(GAME* game) {return 0;}
Mtx* _Matrix_to_Mtx_new(GRAPH* g) {static Mtx m;return &m;}
extern Gfx tol_sponge_1_model[];
typedef void (*mPlayer_item_draw_proc)(ACTOR*, GAME*);
#define OTHER_TOOL(name) static void Player_actor_Item_draw_##name(ACTOR* a,GAME* g){other_tool=*Matrix_now;}
OTHER_TOOL(axe) OTHER_TOOL(umbrella) OTHER_TOOL(rod) OTHER_TOOL(scoop)
OTHER_TOOL(balloon) OTHER_TOOL(windmill) OTHER_TOOL(fan)
/* The harness samples fixed frames without advancing/morphing the animation. */
static int cKF_FrameControl_play(cKF_FrameControl_c* fc) {return 0;}
static void cKF_SkeletonInfo_R_morphJoint(cKF_SkeletonInfo_R_c* k) {abort();}
typedef struct {s16 frame, value, tangent;} cKF_AnimKey_c;
typedef void (*mPlayer_item_net_draw_proc)(void*, GAME*, cKF_SkeletonInfo_R_c*, Gfx**, u8*, s_xyz*, xyz_t*);
#include "net_source.inc"

extern cKF_Skeleton_R_c cKF_bs_r_tol_net_1, cKF_bs_r_tol_net_2;
extern Vtx tol_net_1_v[], tol_net_2_v[];
extern cKF_Animation_R_c cKF_ba_r_tol_net_1_get_m1, cKF_ba_r_tol_net_1_net_swing1;
extern cKF_Animation_R_c cKF_ba_r_tol_net_1_kamae_main_m1, cKF_ba_r_tol_net_1_kokeru_getup_n1;
extern cKF_Animation_R_c cKF_ba_r_tol_net_1_kokeru_n1, cKF_ba_r_tol_net_1_swing_wait1, cKF_ba_r_tol_net_1_yatta_m1;

static void walk(cKF_SkeletonInfo_R_c* k, int* index, cKF_draw_callback after, void* arg) {
    int i = (*index)++;
    cKF_Joint_R_c* joint = &k->skeleton->joint_table[i];
    xyz_t t = {joint->translation.x, joint->translation.y, joint->translation.z};
    if (!i) {t.x = k->current_joint[0].x; t.y = k->current_joint[0].y; t.z = k->current_joint[0].z;}
    s_xyz rot = k->current_joint[i + 1];
    Gfx* model = joint->model;
    u8 flags = joint->flags;
    Matrix_push();
    Matrix_softcv3_mult(&t, &rot);
    pose[i] = *Matrix_now;
    if (after) after(&play.game, k, i, &model, &flags, arg, &rot, &t);
    for (int n = 0; n < joint->child; n++) walk(k, index, after, arg);
    Matrix_pull();
}
void cKF_Si3_draw_R_SV(GAME* game, cKF_SkeletonInfo_R_c* k, Mtx* matrices,
                       cKF_draw_callback before, cKF_draw_callback after, void* arg) {
    int index = 0;
    CHECK(before == NULL, "net has no pre-render override");
    walk(k, &index, after, arg);
    CHECK(index == 6, "all six original skeleton joints retained");
}

static xyz_t transform(MtxF* m, xyz_t p) {
    xyz_t out;
    Matrix_push(); Matrix_put(m); Matrix_Position(&p, &out); Matrix_pull();
    return out;
}
static xyz_t sub(xyz_t a, xyz_t b) {return (xyz_t){a.x-b.x, a.y-b.y, a.z-b.z};}
static float dot(xyz_t a, xyz_t b) {return a.x*b.x + a.y*b.y + a.z*b.z;}
static xyz_t normalized(xyz_t a) {float s=1.0f/sqrtf(dot(a,a));return (xyz_t){a.x*s,a.y*s,a.z*s};}
static xyz_t vertex(MtxF* m, Vtx* v) {
    return transform(m, (xyz_t){v->v.ob[0],v->v.ob[1],v->v.ob[2]});
}

static void one_pose(int kind, cKF_Animation_R_c* anim, int frame, int vr, int yaw, int pitch) {
    s_xyz joints[7] = {{0}}, target[7] = {{0}};
    cKF_SkeletonInfo_R_c* k = &player.item_keyframe;
    memset(k, 0, sizeof(*k));
    k->skeleton = kind ? &cKF_bs_r_tol_net_2 : &cKF_bs_r_tol_net_1;
    k->animation = anim; k->current_joint = joints; k->target_joint = target;
    k->frame_control.current_frame = frame;
    cKF_SkeletonInfo_R_play(k);
    s_xyz saved[7]; memcpy(saved, joints, sizeof(joints));
    attached = vr;
    Matrix_now = stack;
    /* Same +Z tool basis as pc_vr: controller -Z flipped into tool +Z. */
    Skin_Matrix_SetRotateXyz_s(Matrix_now, (s16)pitch, (s16)yaw, 0);
    MtxF hand = *Matrix_now;
    hand_input = hand;
    player.right_hand_mtx = hand;
    player.item_scale = 1.0f;
    player.now_item_main_index = net_state;
    graph.polygon_opaque_thaga.thaGfx.head_p = display;
    player.net_angle = (s_xyz){0, 182, -7281}; /* resting bag sway */
    Player_actor_Item_draw(&player.actor_class, &play.game);
    CHECK(Matrix_now == stack, "draw has no stack leak");
    CHECK(!memcmp(Matrix_now,&hand,sizeof(hand)), "item wrapper restores the caller matrix");
    CHECK(!memcmp(saved,joints,sizeof(joints)), "draw preserves original animation state");
    /* The shaft spans -5200..-1200 on the mesh's local Y. These vertices
     * come from the original ROM, with separate indices for the gold net. */
    Vtx* v = kind ? tol_net_2_v : tol_net_1_v;
    int shaft = kind ? 44 : 43;
    xyz_t tip = vertex(&pose[2], &v[shaft]);
    xyz_t base = vertex(&pose[2], &v[shaft+3]);
    xyz_t axis = normalized(sub(tip,base));
    xyz_t forward = {hand.xz, hand.yz, hand.zz};
    CHECK(dot(axis,forward) > 0.999f, "mesh shaft stays aligned with controller forward");
    /* Rim vertices are in x=0; the opening normal is local -X, opposite
     * the positive-X bag. Use the actual rim matrix after animation. */
    xyz_t opening = {-pose[2].xx, -pose[2].yx, -pose[2].zx};
    xyz_t intended = vr ? (xyz_t){-hand.xy,-hand.yy,-hand.zy} : (xyz_t){hand.xx,hand.yx,hand.zx};
    CHECK(dot(opening,intended)>0.999f, vr ? "hoop opens across palm, not sideways" : "flat opening unchanged");
    xyz_t center = transform(&hand,(xyz_t){vr?0:sin_s(3000)*4000,0,vr?4000:cos_s(3000)*4000});
    CHECK(near(player.net_pos.x,center.x) && near(player.net_pos.y,center.y) && near(player.net_pos.z,center.z),
          "catch center follows controller shaft or original flat probe");
    CHECK(near(player.net_top_col_pos.x,center.x) && near(player.net_top_col_pos.y,center.y) && near(player.net_top_col_pos.z,center.z), "top catch sample agrees");
    CHECK(near(player.net_bot_col_pos.x,center.x) && near(player.net_bot_col_pos.y,center.y) && near(player.net_bot_col_pos.z,center.z), "bottom catch sample agrees");
    xyz_t ray = normalized(sub(player.net_end_pos,player.net_start_pos));
    if (vr) CHECK(dot(ray,axis)>0.999f, "catch ray stays on visible shaft");
}

int main(void) {
    play.game.graph = &graph;
    if (!pc_disc_init()) {puts("No original disc available");return 2;}
    u8* rel=pc_disc_extract_rel(); if(!rel)return 3;
    for(int m=0;m<2;m++) {
        Vtx* v=m?tol_net_2_v:tol_net_1_v;
        int count=m?80:79; unsigned offset=m?0x452240:0x4514E0;
        for(int i=0;i<count;i++) {
            memcpy(&v[i],rel+offset+i*16,16);
            u8* p=(u8*)&v[i];
            for(int j=0;j<12;j+=2){u8 t=p[j];p[j]=p[j+1];p[j+1]=t;}
        }
    }
    cKF_Animation_R_c* anims[]={&cKF_ba_r_tol_net_1_get_m1,&cKF_ba_r_tol_net_1_net_swing1,
        &cKF_ba_r_tol_net_1_kamae_main_m1,&cKF_ba_r_tol_net_1_kokeru_getup_n1,
        &cKF_ba_r_tol_net_1_kokeru_n1,&cKF_ba_r_tol_net_1_swing_wait1,&cKF_ba_r_tol_net_1_yatta_m1};
    for(int m=0;m<2;m++) for(int a=0;a<7;a++) for(int f=1;f<=anims[a]->frames;f++)
        for(int vr=0;vr<2;vr++) one_pose(m,anims[a],f,vr,0,0);
    /* Head/body/world heading must not change the grip correction; exercise
     * upright and forward grips at cardinal and diagonal headings. */
    for(int m=0;m<2;m++) for(int yaw=0;yaw<65536;yaw+=8192) for(int pitch=-16384;pitch<=16384;pitch+=8192)
        for(int vr=0;vr<2;vr++) one_pose(m,anims[1],1,vr,yaw,pitch);
    for(net_state=mPlayer_ITEM_MAIN_NET_NORMAL;net_state<=mPlayer_ITEM_MAIN_NET_COMPLETE_COLLECTION;net_state++)
        for(int m=0;m<2;m++) for(int vr=0;vr<2;vr++) one_pose(m,anims[1],1,vr,8192,-8192);
    /* Every other item dispatch must keep its preexisting hand basis. */
    for(int item=mPlayer_ITEM_MAIN_AXE_NORMAL;item<=mPlayer_ITEM_MAIN_FAN_NORMAL;item++) {
        if(item>=mPlayer_ITEM_MAIN_NET_NORMAL && item<=mPlayer_ITEM_MAIN_NET_COMPLETE_COLLECTION)continue;
        for(attached=0;attached<2;attached++) {
            Matrix_now=stack;
            Skin_Matrix_SetRotateXyz_s(Matrix_now,0,0,0);
            Skin_Matrix_SetRotateXyz_s(&hand_input,8192,16384,-8192);
            hand_input.xw=10;hand_input.yw=25;hand_input.zw=37;
            player.right_hand_mtx=hand_input;
            player.now_item_main_index=item;
            graph.polygon_opaque_thaga.thaGfx.head_p=display;
            Player_actor_Item_draw(&player.actor_class,&play.game);
            CHECK(!memcmp(&other_tool,&hand_input,sizeof(hand_input)), "other tool grip unchanged");
        }
    }
    free(rel); pc_disc_shutdown();
    printf("Net orientation: %d checks, %d failures (both meshes, all 7 animations, varied grip poses)\n",checks,failures);
    return failures!=0;
}
