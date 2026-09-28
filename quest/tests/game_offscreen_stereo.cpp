/* Shell-only fixed-pose host. Production render helpers are extracted verbatim
 * by game_offscreen_profile_run.py into the generated include below. */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_vr.h"
#include "pc_vr_swing.h"
#include <openxr/openxr.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>
#include "gles_device_context.h"

static void* host_game;
static void* symbol(const char* name) {
    void* result=dlsym(host_game,name);
    if(!result){fprintf(stderr,"stereo dlsym %s: %s\n",name,dlerror());_Exit(3);}
    return result;
}
static PCSettings* host_settings;
static u32* host_frame;
static int *host_target_w,*host_target_h,*host_window_w,*host_window_h,*host_paused;
#define g_pc_settings (*host_settings)
#define pc_frame_counter (*host_frame)
#define g_pc_target_w (*host_target_w)
#define g_pc_target_h (*host_target_h)
#define g_pc_window_w (*host_window_w)
#define g_pc_window_h (*host_window_h)
/* The extracted flat-scene helper declares this extern locally; provide the
 * real variable's value through a small helper instead of a replacement global. */
static int offscreen_paused(void) {return *host_paused;}
static void pcvr_log(const char* format,...) {
    va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");
}
#define BRIDGE_VOID0(name) static void bridge_##name(void) { \
    static auto call=(void(*)(void))symbol(#name);call(); }
BRIDGE_VOID0(pc_gx_draw_pending)
BRIDGE_VOID0(pc_gx_flush_if_begin_complete)
BRIDGE_VOID0(pc_gx_restore_after_nes)
BRIDGE_VOID0(pc_gx_mark_new_pass)
BRIDGE_VOID0(pc_gx_vr_reset_routing)
BRIDGE_VOID0(pc_sky_begin_pass)
static void bridge_pc_gx_get_clear(float* rgba,float* depth) {
    static auto call=(void(*)(float*,float*))symbol("pc_gx_get_clear");call(rgba,depth);
}
static void bridge_pc_sky_set_view(const float* view) {
    static auto call=(void(*)(const float*))symbol("pc_sky_set_view");call(view);
}
static int bridge_pc_fp_view_is_active(void) {
    static auto call=(int(*)(void))symbol("pc_fp_view_is_active");return call();
}
#define pc_gx_draw_pending bridge_pc_gx_draw_pending
#define pc_gx_flush_if_begin_complete bridge_pc_gx_flush_if_begin_complete
#define pc_gx_restore_after_nes bridge_pc_gx_restore_after_nes
#define pc_gx_mark_new_pass bridge_pc_gx_mark_new_pass
#define pc_gx_vr_reset_routing bridge_pc_gx_vr_reset_routing
#define pc_gx_get_clear bridge_pc_gx_get_clear
#define pc_sky_begin_pass bridge_pc_sky_begin_pass
#define pc_sky_set_view bridge_pc_sky_set_view
#define pc_fp_view_is_active bridge_pc_fp_view_is_active
typedef float M34[3][4];
struct PCVRTarget {GLuint fbo,color,depth_rbo;int w,h;};
static struct {XrView views[2];} s_xr;
static u32 s_frame_stamp=(u32)-1;
static void pcvr_update_eye_projection(int eye);
static void pcvr_update_view_correction(void);
/* No tracked controllers or synthetic gameplay actions in this title harness. */
static void pcvr_draw_empty_hands(void) {}
#define PC_VR_NEAR_M 0.05f
#include "game_offscreen_stereo_generated.c_inc"

static int stereo_enabled;
static unsigned stereo_frames;
extern "C" int pc_vr_active(void) {return stereo_enabled;}

extern "C" void offscreen_stereo_init(void* library,int enabled,int eye_size) {
    host_game=library;
    host_settings=(PCSettings*)symbol("g_pc_settings");
    host_frame=(u32*)symbol("pc_frame_counter");
    host_target_w=(int*)symbol("g_pc_target_w");host_target_h=(int*)symbol("g_pc_target_h");
    host_window_w=(int*)symbol("g_pc_window_w");host_window_h=(int*)symbol("g_pc_window_h");
    host_paused=(int*)symbol("g_pc_paused");
    if(!enabled)return;
    memset(&s_vr,0,sizeof(s_vr));
    s_vr.current_eye=-1;s_vr.flat_scene_stamp=(u32)-1000;
    s_vr.world_scale=fmaxf(0.0001f,g_pc_settings.vr_world_scale/1000.0f);
    s_vr.ui_distance=fmaxf(0.5f,g_pc_settings.vr_ui_distance/100.0f);
    s_vr.ui_size=fmaxf(0.5f,g_pc_settings.vr_ui_size/100.0f);
    s_vr.height_offset=g_pc_settings.vr_height_offset/100.0f;
    s_vr.ui_dist_k=1;
    m34_identity(s_vr.game_view);m34_identity(s_vr.head_pose);
    m34_identity(s_vr.world_from_seated);
    for(int eye=0;eye<2;++eye){
        if(!pcvr_create_target(&s_vr.eye[eye],eye_size,eye_size))_Exit(4);
        s_xr.views[eye].fov={-0.785398163f,0.785398163f,0.785398163f,-0.785398163f};
        // Fixed 64 mm IPD and 20 degrees downward pitch: no runtime tracking.
        const float pitch=-20.0f*(float)PC_PI/180.0f;
        M34 seated_eye;
        m34_identity(seated_eye);
        seated_eye[1][1]=seated_eye[2][2]=cosf(pitch);
        seated_eye[1][2]=-sinf(pitch);seated_eye[2][1]=sinf(pitch);
        seated_eye[0][3]=eye?0.032f:-0.032f;
        m34_invert_rigid(seated_eye,s_vr.inv_eye_pose[eye]);
        pcvr_update_eye_projection(eye);
    }
    if(!pcvr_create_target(&s_vr.ui,1280,960)||!pcvr_create_panel_gl())_Exit(4);
    s_vr.active=stereo_enabled=1;
    *(float*)symbol("g_pc_vr_cull_expand")=24;
    *(float*)symbol("g_pc_vr_cull_znear_slack")=8;
    pcvr_update_view_correction();
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    printf("OFFSCREEN_STEREO fixed poses; %dx%d per eye,90deg FOV,IPD64mm,pitch-20deg; production matrix/UI helpers; no XR\n",eye_size,eye_size);
}

extern "C" void pc_vr_frame_begin(void) {
    if(!stereo_enabled||s_frame_stamp==pc_frame_counter)return;
    s_frame_stamp=pc_frame_counter;s_vr.submitted_this_frame=0;
    pc_gx_draw_pending();
    glBindFramebuffer(GL_FRAMEBUFFER,s_vr.ui.fbo);
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    glClearColor(0,0,0,0);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
extern "C" void pc_vr_compose_and_submit(void) {
    if(!stereo_enabled||s_vr.submitted_this_frame)return;
    pc_gx_draw_pending();
    float target=pc_fp_view_is_active()?0.6f:1.0f;
    s_vr.ui_dist_k+=(target-s_vr.ui_dist_k)*0.2f;
    if(fabsf(target-s_vr.ui_dist_k)<0.01f)s_vr.ui_dist_k=target;
    pcvr_draw_panel(0);pcvr_draw_panel(1);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    s_vr.submitted_this_frame=1;++stereo_frames;
    pc_gx_restore_after_nes();glEnable(GL_DEPTH_TEST);
}
extern "C" void pc_vr_mirror_to_window(void) {
    if(!stereo_enabled)return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,s_vr.eye[0].fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0,0,s_vr.eye[0].w,s_vr.eye[0].h,80,0,560,480,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
extern "C" int offscreen_stereo_capture(void) {
    if(!stereo_enabled)return 1;
    const int width=s_vr.eye[0].w,height=s_vr.eye[0].h;
    unsigned char* pixels=(unsigned char*)malloc((size_t)width*height*4);
    if(!pixels)return 0;
    unsigned hashes[2]={2166136261u,2166136261u};
    int okay=1;
    for(int eye=0;eye<2;++eye){
        glBindFramebuffer(GL_FRAMEBUFFER,s_vr.eye[eye].fbo);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        for(int i=0;i<width*height*4;++i)hashes[eye]=(hashes[eye]^pixels[i])*16777619u;
        okay&=device_save_bmp(eye?"right.bmp":"left.bmp",width,height,pixels);
    }
    free(pixels);glBindFramebuffer(GL_FRAMEBUFFER,0);
    printf("OFFSCREEN_STEREO_RESULT composed=%u left_hash=%08x right_hash=%08x\n",stereo_frames,hashes[0],hashes[1]);
    return okay&&stereo_frames>=100&&hashes[0]!=hashes[1];
}
