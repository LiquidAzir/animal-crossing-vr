#include <stdio.h>
#include <string.h>
#include "c_keyframe.h"
#include "sys_matrix.h"

static int checks, failures;
#define CHECK(c,msg) do { ++checks; if (!(c)) { ++failures; printf("FAIL: %s at %d\n",msg,__LINE__); } } while (0)

static int vr, fp;
int g_pc_solid_buildings = 1, g_pc_solid_shell_pct = 97;
int g_pc_model_viewer, g_pc_model_viewer_solid;
int pc_vr_active(void) { return vr; }
int pc_fp_view_is_active(void) { return fp; }
static Gfx house_list[1], tailor_list[1], post_list[1], shop_list[1], island_list[1], bridge_list[1], sentinel_list[1];
static cKF_Skeleton_R_c house_skel, tailor_skel, post_skel, shop_skel, island_skel, bridge_skel, other_skel;
static cKF_SkeletonInfo_R_c house, tailor, post_office, shop, island, bridge, other, sentinel;
static GAME game;
static GRAPH graph;
static Mtx matrices[64];
static int ready, draws, pre_calls, post_calls, measurements, resets, shell_draws, nested;
static Gfx* g_ckf_house_back;
static cKF_SkeletonInfo_R_c* g_ckf_house_back_owner;
int g_ckf_shell_pass;
float g_ckf_shell_scale;
static f32 g_ckf_shell_rel_x, g_ckf_shell_rel_y, g_ckf_shell_rel_z;
typedef struct { int state; f32 rel_x, rel_y, rel_z; } ckf_shell_cache_t;
static ckf_shell_cache_t legacy;
static Gfx tailor_original_light[1], tailor_repaired_light[1];
static Gfx post_original_light[1], post_repaired_light[1];
static Gfx *original_light, *repaired_light;
static int post_light_lookups;
static int light_lookups, stack_depth, child_pre, child_post, suppress_child;
void Matrix_push(void) { ++stack_depth; }
void Matrix_pull(void) { --stack_depth; }
void Matrix_translate(f32 x,f32 y,f32 z,u8 flag) { }
void Matrix_scale(f32 x,f32 y,f32 z,u8 flag) { }
void Matrix_softcv3_mult(xyz_t* pos,s_xyz* rot) { }
Mtx* _Matrix_to_Mtx(Mtx* m) { return m; }
Mtx* _Matrix_to_Mtx_new(GRAPH* g) { return matrices+63; }
static Gfx* pc_tailor_light_for_back(Gfx* d) {
    ++light_lookups;CHECK(child_pre==1,"light substitution runs after prerender tint callback");
    return d==tailor_original_light?tailor_repaired_light:d;
}

static Gfx* pc_post_office_light_for_back(Gfx* d) {
    ++post_light_lookups; CHECK(child_pre==1,"post-office light preserves prerender callback order");
    return d==post_original_light?post_repaired_light:d;
}

#undef GRAPH_ALLOC_TYPE
#define GRAPH_ALLOC_TYPE(g,t,n) (matrices)

static int pc_house_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &house_skel && ready ? house_list : NULL;
    return skel == &house_skel;
}
static int pc_tailor_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &tailor_skel && ready ? tailor_list : NULL;
    return skel == &tailor_skel;
}
static int pc_post_office_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &post_skel && ready ? post_list : NULL;
    return skel == &post_skel;
}
static int pc_shop_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &shop_skel && ready ? shop_list : NULL;
    return skel == &shop_skel;
}
static int pc_island_house_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &island_skel && ready ? island_list : NULL;
    return skel == &island_skel;
}
static int pc_bridge_back_lookup(cKF_Skeleton_R_c* skel, Gfx** out) {
    *out = skel == &bridge_skel && ready ? bridge_list : NULL;
    return skel == &bridge_skel;
}
static ckf_shell_cache_t* ckf_shell_measure(cKF_SkeletonInfo_R_c* keyframe) {
    ++measurements;
    return &legacy;
}
static void pipeline_reset(GRAPH* g) { ++resets; }
static int pre(GAME* g,cKF_SkeletonInfo_R_c* k,int j,Gfx** d,u8* f,void* a,s_xyz* r,xyz_t* t) {
    ++pre_calls; return 1;
}
static int post(GAME* g,cKF_SkeletonInfo_R_c* k,int j,Gfx** d,u8* f,void* a,s_xyz* r,xyz_t* t) {
    ++post_calls; return 1;
}

/* Submission seam: a single representative joint invokes the callbacks. */
void cKF_Si3_draw_R_SV(GAME* g, cKF_SkeletonInfo_R_c* k, Mtx* m,
                     cKF_draw_callback before, cKF_draw_callback after, void* arg) {
    ++draws;
    if (g_ckf_shell_pass) ++shell_draws;
    if (vr && g_pc_solid_buildings && (k == &house || k == &tailor || k == &post_office || k == &shop || k == &island || k == &bridge)) {
        CHECK(g_ckf_house_back_owner == k, "repair belongs to this skeleton instance");
        CHECK(g_ckf_house_back == (ready ? (k == &house ? house_list : k == &tailor ? tailor_list : k == &post_office ? post_list : k == &shop ? shop_list : k == &island ? island_list : bridge_list) : NULL),
              "loaded or deferred repair state reaches the original draw");
    }
    if (before) before(g,k,0,NULL,NULL,arg,NULL,NULL);
    if (nested && k == &house) {
        Gfx* outer_list = g_ckf_house_back;
        cKF_SkeletonInfo_R_c* outer_owner = g_ckf_house_back_owner;
        nested = 0;
        cKF_Si3_draw_R_SV_solid(g,&tailor,m,pre,post,NULL,pipeline_reset);
        CHECK(g_ckf_house_back == outer_list && g_ckf_house_back_owner == outer_owner,
              "nested structure restores the caller's repair and owner");
    }
    if (after) after(g,k,0,NULL,NULL,arg,NULL,NULL);
}

#include "structure_draw_source.inc"

static void reset(void) {
    vr=1; fp=0; g_pc_model_viewer=0; g_pc_model_viewer_solid=0;
    g_pc_solid_buildings=1; ready=1; legacy.state=1; nested=0;
    draws=pre_calls=post_calls=measurements=resets=shell_draws=0;
    g_ckf_shell_pass=0;
    g_ckf_house_back=sentinel_list; g_ckf_house_back_owner=&sentinel;
}

static int child_before(GAME* g,cKF_SkeletonInfo_R_c* k,int j,Gfx** d,u8* f,void* a,s_xyz* r,xyz_t* t) {
    ++child_pre;CHECK(*d==original_light,"prerender callback still receives original light list");
    if(suppress_child)*d=NULL;
    OPEN_DISP(g->graph);gDPSetPrimColor(NOW_POLY_OPA_DISP++,0,0,18,18,9,255);CLOSE_DISP(g->graph);
    return 1;
}
static int child_after(GAME* g,cKF_SkeletonInfo_R_c* k,int j,Gfx** d,u8* f,void* a,s_xyz* r,xyz_t* t) {
    ++child_post;CHECK(*d==(suppress_child?NULL:original_light),"postrender callback retains original model identity");return 1;
}
static void verify_light_routing(void) {
    for (int kind=0; kind<2; ++kind) {
    original_light=kind?post_original_light:tailor_original_light;
    repaired_light=kind?post_repaired_light:tailor_repaired_light;
    cKF_Joint_R_c joints[1]={{0}};s_xyz current[2]={{0}};
    cKF_Skeleton_R_c skel={1,1,joints};cKF_SkeletonInfo_R_c k={0};
    Gfx stream[16];joints[0].model=original_light;k.skeleton=&skel;k.current_joint=current;
    /* Plain flat draw, active matching repair, nested unrelated skeleton,
     * deferred rear, and callback-suppressed geometry. */
    for(int mode=0;mode<5;mode++){
        reset();post_light_lookups=light_lookups=stack_depth=child_pre=child_post=0;suppress_child=mode==4;
        g_ckf_house_back=mode==3?NULL:tailor_list;
        g_ckf_house_back_owner=mode==0?NULL:mode==2?&sentinel:&k;
        memset(stream,0,sizeof(stream));graph.polygon_opaque_thaga.thaGfx.head_p=stream;
        int joint=0;Mtx* matrix=matrices;
        cKF_Si3_draw_SV_R_child(&game,&k,&joint,child_before,child_after,NULL,&matrix);
        CHECK(light_lookups==(mode==1) && post_light_lookups==(mode==1),"light replacement only in loaded matching repair; flat and nested draws unchanged");
        CHECK(child_pre==1&&child_post==1&&stack_depth==0,"callbacks and matrix stack remain balanced");
        CHECK((stream[0].words.w0>>24)==G_SETPRIMCOLOR&&stream[0].words.w1==0x121209ff,"actor tint emitted before replacement draw");
        if(mode!=4){u32 word=stream[2].words.w1;uintptr_t p=pc_gbi_unpack_runtime_ptr(word);if(!p)p=word&~1u;CHECK((Gfx*)p==(mode==1?repaired_light:original_light),"submitted list matches repair mode");}
        CHECK(matrix==matrices+1,"same original joint matrix allocation count");
    }
    }
}

int main(void) {
    house.skeleton=&house_skel; tailor.skeleton=&tailor_skel; post_office.skeleton=&post_skel; shop.skeleton=&shop_skel; island.skeleton=&island_skel; bridge.skeleton=&bridge_skel; other.skeleton=&other_skel;
    game.graph=&graph;
    for (int loaded=0;loaded<2;++loaded) for (int kind=0;kind<6;++kind) {
        reset(); ready=loaded;
        cKF_Si3_draw_R_SV_solid(&game,kind==0?&house:kind==1?&tailor:kind==2?&post_office:kind==3?&shop:kind==4?&island:&bridge,matrices,pre,post,NULL,pipeline_reset);
        CHECK(draws==1 && pre_calls==1 && post_calls==1,"repaired model and callbacks run once");
        CHECK(measurements==0 && shell_draws==0 && resets==0,"no legacy facade even for deferred assets");
        CHECK(g_ckf_house_back==sentinel_list && g_ckf_house_back_owner==&sentinel,"temporary repair state restored");
    }
    reset(); nested=1;
    cKF_Si3_draw_R_SV_solid(&game,&house,matrices,pre,post,NULL,pipeline_reset);
    CHECK(draws==2 && pre_calls==2 && post_calls==2,"nested repaired models each draw once");
    CHECK(measurements==0 && shell_draws==0,"nested repair never uses legacy mirror");
    CHECK(g_ckf_house_back==sentinel_list && g_ckf_house_back_owner==&sentinel,"nested call restores initial state");
    for(int state=0;state<2;++state) {
        reset(); legacy.state=state;
        cKF_Si3_draw_R_SV_solid(&game,&other,matrices,pre,post,NULL,pipeline_reset);
        CHECK(draws==1+state && pre_calls==1+state && post_calls==1,"unmodified structure retains legacy callback behavior");
        CHECK(measurements==1 && shell_draws==state && resets==state,"legacy model retains pipeline reset");
    }
    for(int mode=0;mode<8;++mode) {
        reset(); vr=0; fp=mode==1; vr=mode==2;
        g_pc_model_viewer=mode==3 || mode==4;
        g_pc_model_viewer_solid=mode==4 || mode==5;
        if(mode==6){vr=1;g_pc_solid_buildings=0;}
        if(mode==7){fp=1;g_pc_solid_buildings=0;}
        CHECK(cKF_shell_wanted()==(mode==1 || mode==2 || mode==4),"repair opt-in for VR, first person, or explicit diagnostic only");
    }
    reset(); vr=0;
    cKF_Si3_draw_R_SV_solid(&game,&tailor,matrices,pre,post,NULL,pipeline_reset);
    CHECK(draws==1 && measurements==0 && shell_draws==0,"ordinary flat view keeps original model only");
    verify_light_routing();
    printf("Structure draw routing: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
