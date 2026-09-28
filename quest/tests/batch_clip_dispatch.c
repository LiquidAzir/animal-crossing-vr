/* The runner extracts actual production helpers and the flush prefix here.
 * Only the later GL uniform/upload tail is replaced with an observation. */
#include "quest_batch_clip.h"
#include <stdio.h>
#include <stdlib.h>
typedef unsigned GLuint;
typedef unsigned long long Uint64;
typedef unsigned u32;
enum { GX_TRIANGLES=0x90, GX_QUADS=0x80, GX_TRIANGLESTRIP=0x98,
       GX_PERSPECTIVE=0, GX_ORTHOGRAPHIC=1,
       PC_GX_DIRTY_BLEND=16, PC_GX_DIRTY_COLOR_MASK=32, PC_PROF_TIMER_GX_FLUSH=3 };
typedef struct { float position[3]; unsigned char rest[84]; } PCGXVertex;
typedef struct { GLuint prog; } PCGXShaderVariant;
static struct {
    int current_vertex_idx, pending_verts, current_primitive, pending_prim;
    unsigned dirty; GLuint current_shader;
    int projection_type, current_mtx;
    float pos_mtx[4][3][4], projection_mtx[4][4], viewport[6];
    PCGXVertex vertex_buffer[32];
} g_gx;
static unsigned checks, failures;
#define CHECK(c) do { ++checks; if(!(c)){++failures;printf("FAIL line %u: %s\n",__LINE__,#c);} }while(0)
static int scene, flat, skip_ui, s_vr_ui_routed, routed, reapplied, skies, depth_ranges;
static int pending_draws, accepted, flushes, timers;
static unsigned prior_draw_shader, prior_dirty;
static float correction[12], eye_projection[16];
static const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static PCGXShaderVariant variant;
static int pc_vr_in_scene_pass(void){return scene;}
static int pc_vr_flat_scene_active(void){return flat;}
static int pc_vr_skip_ui_draws(void){return skip_ui;}
static const float* pc_vr_view_correction(void){return correction;}
static const float* pc_vr_eye_projection(void){return eye_projection;}
static void pc_vr_bind_ui_target(int target){routed=target+1;}
static void pc_vr_set_scene_depth_range(float a,float b){(void)a;(void)b;++depth_ranges;}
static void pc_gx_reapply_viewport_scissor(void){++reapplied;}
static void pc_gx_dirty_set(unsigned bits){g_gx.dirty|=bits;}
static void pc_sky_draw(const float* p,const float* x){(void)p;(void)x;++skies;}
static Uint64 pc_profiler_begin_timer(void){return 1;}
static void pc_profiler_add_count_flush(void){++flushes;}
static void pc_profiler_add_time(int category,Uint64 start){(void)category;(void)start;++timers;}
static PCGXShaderVariant* pc_gx_tev_get_variant(void){return &variant;}
static void pc_gx_draw_pending(void){
    if(!g_gx.pending_verts)return;
    ++pending_draws;prior_draw_shader=g_gx.current_shader;prior_dirty=g_gx.dirty;
    int extra=g_gx.current_vertex_idx-g_gx.pending_verts;
    memmove(g_gx.vertex_buffer,g_gx.vertex_buffer+g_gx.pending_verts,extra*sizeof(PCGXVertex));
    g_gx.current_vertex_idx=extra;g_gx.pending_verts=0;
}
#include "batch_clip_actual.inc"
static void reset(void){
    memset(&g_gx,0,sizeof(g_gx));memcpy(g_gx.pos_mtx[0],identity,12*sizeof(float));
    memcpy(correction,identity,sizeof(correction));memcpy(eye_projection,identity,sizeof(eye_projection));
    scene=1;flat=skip_ui=s_vr_ui_routed=routed=reapplied=skies=depth_ranges=0;
    pending_draws=accepted=flushes=timers=0;variant.prog=7;
    g_gx.current_shader=3;g_gx.dirty=15;g_gx.current_primitive=GX_TRIANGLES;
    g_gx.current_vertex_idx=3;
    for(int i=0;i<3;++i)g_gx.vertex_buffer[i].position[0]=4;
}
int main(void){
    reset();pc_gx_flush_vertices();
    CHECK(g_gx.current_vertex_idx==0);CHECK(!accepted);CHECK(g_gx.dirty==15);
    CHECK(g_gx.current_shader==3);CHECK(skies==1&&depth_ranges==1);CHECK(timers==1&&flushes==1);
    reset();g_gx.pending_verts=3;g_gx.current_vertex_idx=6;
    for(int i=0;i<3;++i)g_gx.vertex_buffer[i+3]=g_gx.vertex_buffer[i];
    memset(g_gx.vertex_buffer,0,3*sizeof(PCGXVertex));pc_gx_flush_vertices();
    CHECK(pending_draws==1);CHECK(prior_draw_shader==3&&prior_dirty==15);CHECK(!accepted);
    CHECK(g_gx.pending_verts==0&&g_gx.current_vertex_idx==0);CHECK(g_gx.dirty==15);
    /* Same-state coalescing retains the visible deferred run and its extras. */
    reset();g_gx.pending_verts=3;g_gx.current_vertex_idx=6;g_gx.dirty=0;
    g_gx.pending_prim=GX_TRIANGLES;g_gx.current_shader=variant.prog;pc_gx_flush_vertices();
    CHECK(g_gx.pending_verts==6);CHECK(!pending_draws&&!accepted&&!skies);
    for(int exclusion=0;exclusion<8;++exclusion){
        reset();
        if(exclusion==0)scene=0;
        if(exclusion==1)flat=1;
        if(exclusion==2)g_gx.projection_type=GX_ORTHOGRAPHIC;
        if(exclusion==3)g_gx.current_primitive=GX_TRIANGLESTRIP;
        if(exclusion==4)variant.prog=0;
        if(exclusion==5)eye_projection[0]=NAN;
        if(exclusion==6)g_gx.vertex_buffer[2].position[1]=INFINITY;
        if(exclusion==7)g_gx.vertex_buffer[0].position[0]=0;
        pc_gx_flush_vertices();CHECK(accepted==1);CHECK(g_gx.current_vertex_idx==3);
    }
    reset();flat=skip_ui=1;pc_gx_flush_vertices();
    CHECK(!accepted&&g_gx.current_vertex_idx==0);CHECK(routed==2&&reapplied==1);
    CHECK(g_gx.dirty==(15|PC_GX_DIRTY_BLEND|PC_GX_DIRTY_COLOR_MASK));CHECK(!skies);
    reset();g_gx.current_primitive=GX_QUADS;g_gx.current_vertex_idx=4;
    g_gx.vertex_buffer[3]=g_gx.vertex_buffer[0];pc_gx_flush_vertices();CHECK(!accepted);
    /* Next visible batch must see every dirty group retained by rejection. */
    for(int i=0;i<4;++i)g_gx.vertex_buffer[i].position[0]=0;
    g_gx.current_vertex_idx=4;pc_gx_flush_vertices();CHECK(accepted==1&&g_gx.dirty==15);
    /* Shared exact MV helper, both VR and ordinary path. */
    for(int state=0;state<3;++state)for(int test=0;test<25;++test){
        reset();scene=state!=0;flat=state==2;
        for(int i=0;i<12;++i){((float*)g_gx.pos_mtx[0])[i]=(i-5)*.17f+test;correction[i]=(6-i)*.23f-test*.03f;}
        float actual[16],expected[16];memcpy(expected,identity,sizeof(expected));
        const float* src=(float*)g_gx.pos_mtx[0];
        for(int r=0;r<3;++r)for(int c=0;c<4;++c)
            expected[r*4+c]=(scene&&!flat)?correction[r*4]*src[c]+correction[r*4+1]*src[4+c]+correction[r*4+2]*src[8+c]+(c==3?correction[r*4+3]:0):src[r*4+c];
        pc_gx_current_modelview(actual);CHECK(memcmp(actual,expected,sizeof(actual))==0);
    }
    printf("dispatch checks=%u failures=%u\n",checks,failures);return failures!=0;
}
