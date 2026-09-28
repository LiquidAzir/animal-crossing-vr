#include <openxr/openxr.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include "pc_vr_swing.h"
#include "quest_host_vector.h"
using std::isfinite;
typedef unsigned GLuint;
typedef void JavaVM;
typedef void* jobject;
struct XrSwapchainImageOpenGLESKHR { XrStructureType type; void* next; GLuint image; };
#include "runtime_class.inc"
typedef float M34[3][4];
static QuestXrRuntime s_xr;
static M34 s_seated_from_local;
static int s_have_seated_origin;
static struct {
    float eye_projection[2][4][4], world_scale;
    int input_synced;
    int hand_valid, head_pose_valid, empty_hand_valid[2], submitted_this_frame;
    int eye[2], sink;
    PCVRSwing swing;
    XrAction act_a, act_b, act_l, act_r, act_trigger_l, act_trigger_r;
} s_vr;
#define PC_VR_NEAR_M 0.05f
#define pcvr_log quest_xr_log
static int checks, failures;
static double test_clock_ms = 1000;
static unsigned clock_reads, timing_log_lines;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; fprintf(stderr,"line %d: %s\n",__LINE__,#c); } } while(0)
static void near(float a,float b,float eps=0.00003f) { CHECK(std::fabs(a-b)<=eps); }
void quest_xr_log(const char* format, ...) {
    if(!strncmp(format,"XR_TIMING",9))++timing_log_lines;
}
double quest_xr_clock_ms() {++clock_reads;return test_clock_ms;}

static std::vector<XrSessionState> events;
static std::vector<std::string> calls;
static XrResult wait_result, acquire_result, release_result, frame_wait_result;
static XrViewStateFlags view_flags;
static XrBool32 should_render, action_active, bool_value;
static float float_value;
static XrVector2f vector_value;
static unsigned layers, xr_action_calls;
enum { GL_FRAMEBUFFER = 1, GL_FRAMEBUFFER_SRGB_EXT = 2 };
void glBindFramebuffer(unsigned,unsigned) { calls.push_back("bind");test_clock_ms+=0.1; }
void glDisable(unsigned flag) { CHECK(flag==GL_FRAMEBUFFER_SRGB_EXT);calls.push_back("raw-gamma-write"); }
void glViewport(int,int,int,int) {test_clock_ms+=0.02;}
void glFlush() { calls.push_back("flush");test_clock_ms+=0.3; }
void pc_gx_draw_pending() { calls.push_back("gx-flush"); }

extern "C" {
XrResult XRAPI_CALL xrResultToString(XrInstance,XrResult,char buffer[XR_MAX_RESULT_STRING_SIZE]) { buffer[0]=0;return XR_SUCCESS; }
XrResult XRAPI_CALL xrPollEvent(XrInstance,XrEventDataBuffer* event) {
    if(events.empty()) return XR_EVENT_UNAVAILABLE;
    XrEventDataSessionStateChanged change={XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    change.session=s_xr.session;change.state=events.front();events.erase(events.begin());
    memcpy(event,&change,sizeof(change));return XR_SUCCESS;
}
XrResult XRAPI_CALL xrBeginSession(XrSession,const XrSessionBeginInfo* info) {
    CHECK(info->primaryViewConfigurationType==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);
    calls.push_back("begin-session");return XR_SUCCESS;
}
XrResult XRAPI_CALL xrEndSession(XrSession) { calls.push_back("end-session");return XR_SUCCESS; }
XrResult XRAPI_CALL xrWaitFrame(XrSession,const XrFrameWaitInfo*,XrFrameState* state) {
    calls.push_back("wait-frame");state->predictedDisplayTime=123456789;
    test_clock_ms+=7;
    state->shouldRender=should_render;return frame_wait_result;
}
XrResult XRAPI_CALL xrBeginFrame(XrSession,const XrFrameBeginInfo*) { calls.push_back("begin-frame");test_clock_ms+=2;return XR_SUCCESS; }
XrResult XRAPI_CALL xrLocateViews(XrSession,const XrViewLocateInfo* info,XrViewState* state,uint32_t,uint32_t* count,XrView* views) {
    CHECK(info->displayTime==s_xr.frame.predictedDisplayTime);CHECK(info->space==s_xr.local_space);
    test_clock_ms+=1;
    state->viewStateFlags=view_flags;*count=2;
    for(int eye=0;eye<2;++eye) {views[eye].pose.orientation.w=1;views[eye].pose.position.x=eye?0.032f:-0.032f;}
    return XR_SUCCESS;
}
XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    calls.push_back("acquire");*index=0;return acquire_result;
}
XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo* info) {
    CHECK(info->timeout==XR_INFINITE_DURATION);calls.push_back("wait-image");test_clock_ms+=0.5;return wait_result;
}
XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    calls.push_back("release");test_clock_ms+=0.4;return release_result;
}
XrResult XRAPI_CALL xrEndFrame(XrSession,const XrFrameEndInfo* info) {
    calls.push_back("end-frame");layers=info->layerCount;
    test_clock_ms+=1;
    CHECK(info->displayTime==s_xr.frame.predictedDisplayTime);
    CHECK(info->environmentBlendMode==XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    if(layers) {
        auto* layer=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
        CHECK(layer->space==s_xr.local_space);CHECK(layer->viewCount==2);
        for(int i=0;i<2;++i) {
            CHECK(layer->views[i].subImage.swapchain==s_xr.eyes[i].swapchain);
            near(layer->views[i].pose.position.x,i?0.032f:-0.032f);
        }
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL xrLocateSpace(XrSpace,XrSpace,XrTime time,XrSpaceLocation* location) {
    CHECK(time==s_xr.frame.predictedDisplayTime);location->locationFlags=view_flags;
    location->pose.orientation.w=1;return XR_SUCCESS;
}
XrResult XRAPI_CALL xrGetActionStateFloat(XrSession,const XrActionStateGetInfo*,XrActionStateFloat* state) {
    ++xr_action_calls;state->isActive=action_active;state->currentState=float_value;return XR_SUCCESS;
}
XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession,const XrActionStateGetInfo*,XrActionStateBoolean* state) {
    ++xr_action_calls;state->isActive=action_active;state->currentState=bool_value;return XR_SUCCESS;
}
XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession,const XrActionStateGetInfo*,XrActionStateVector2f* state) {
    ++xr_action_calls;state->isActive=action_active;state->currentState=vector_value;return XR_SUCCESS;
}
}

#include "contract_source.inc"

static void reset() {
    s_xr=QuestXrRuntime{};s_xr.instance=(XrInstance)1;s_xr.session=(XrSession)2;
    s_xr.local_space=(XrSpace)3;s_xr.view_space=(XrSpace)4;
    for(int i=0;i<2;++i) {s_xr.eyes[i].swapchain=(XrSwapchain)(uintptr_t)(5+i);s_xr.eyes[i].fbos={unsigned(10+i)};s_xr.eyes[i].width=100;s_xr.eyes[i].height=120;}
    events.clear();calls.clear();layers=99;xr_action_calls=0;
    test_clock_ms=1000;clock_reads=timing_log_lines=0;
    wait_result=acquire_result=release_result=frame_wait_result=XR_SUCCESS;
    view_flags=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
    should_render=action_active=1;bool_value=0;float_value=0;vector_value={0,0};
}
static void frame_tests() {
    reset();events={XR_SESSION_STATE_READY,XR_SESSION_STATE_FOCUSED};
    // READY may be followed by SYNCHRONIZED/FOCUSED only once BeginSession ran.
    events={XR_SESSION_STATE_READY};CHECK(s_xr.begin_frame());CHECK(s_xr.running);
    CHECK(s_xr.frame_begun&&s_xr.should_render);events={XR_SESSION_STATE_FOCUSED};s_xr.poll_events();CHECK(s_xr.focused);
    CHECK(!s_xr.acquire_eye(-1));CHECK(!s_xr.acquire_eye(2));
    CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));
    CHECK(s_xr.end_frame(true));CHECK(layers==1);CHECK(s_xr.completed_frames==1);
    CHECK(!s_xr.frame_begun);CHECK(!s_xr.end_frame(true));
    CHECK(std::count(calls.begin(),calls.end(),"acquire")==2);
    CHECK(std::count(calls.begin(),calls.end(),"release")==2);
    CHECK(calls.back()=="end-frame");
    reset();events={XR_SESSION_STATE_READY};s_xr.resumed=false;
    CHECK(!s_xr.begin_frame());CHECK(calls.empty());s_xr.resumed=true;CHECK(s_xr.begin_frame());
    should_render=false;CHECK(s_xr.begin_frame());CHECK(!s_xr.should_render);
    CHECK(!s_xr.acquire_eye(0));CHECK(s_xr.end_frame(false));CHECK(layers==0);
    for(auto flag:{XrViewStateFlags(0),XrViewStateFlags(XR_VIEW_STATE_ORIENTATION_VALID_BIT)}) {
        reset();events={XR_SESSION_STATE_READY};view_flags=flag;
        CHECK(s_xr.begin_frame());CHECK(!s_xr.should_render);CHECK(s_xr.end_frame(true));CHECK(layers==0);
    }
    reset();events={XR_SESSION_STATE_READY};CHECK(s_xr.begin_frame());
    wait_result=XR_TIMEOUT_EXPIRED;CHECK(!s_xr.acquire_eye(0));CHECK(s_xr.eyes[0].acquired&&!s_xr.eyes[0].waited);
    CHECK(s_xr.end_frame(true));CHECK(layers==0);CHECK(std::count(calls.begin(),calls.end(),"release")==0);
    wait_result=XR_SUCCESS;CHECK(s_xr.begin_frame());CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));CHECK(s_xr.end_frame(true));
    CHECK(layers==1);CHECK(std::count(calls.begin(),calls.end(),"acquire")==2);
    reset();events={XR_SESSION_STATE_READY};CHECK(s_xr.begin_frame());CHECK(s_xr.acquire_eye(0));
    events={XR_SESSION_STATE_STOPPING};s_xr.poll_events();CHECK(!s_xr.running&&!s_xr.frame_begun&&!s_xr.focused);
    CHECK(layers==0);CHECK(calls.back()=="end-session");
    reset();events={XR_SESSION_STATE_READY};frame_wait_result=XR_ERROR_SESSION_LOST;
    CHECK(!s_xr.begin_frame());CHECK(!s_xr.frame_begun);CHECK(!s_xr.end_frame(false));
    reset();events={XR_SESSION_STATE_EXITING};CHECK(!s_xr.begin_frame());CHECK(s_xr.exit_requested);
    // The Android lifecycle event arrives before VI's panel-only submission.
    // Images must be returned before SDL unbinds EGL and blocks on resume.
    reset();events={XR_SESSION_STATE_READY};CHECK(s_xr.begin_frame());CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));
    s_vr.input_synced=s_vr.hand_valid=s_vr.head_pose_valid=1;s_vr.empty_hand_valid[0]=s_vr.empty_hand_valid[1]=1;
    s_vr.swing.high=1;s_vr.swing.pulse=2;s_vr.eye[0]=10;s_vr.eye[1]=11;s_vr.sink=12;s_vr.submitted_this_frame=0;
    quest_vr_set_resumed(0);
    CHECK(!s_xr.resumed&&!s_xr.frame_begun);CHECK(layers==0);CHECK(s_vr.submitted_this_frame);
    CHECK(!s_xr.eyes[0].acquired&&!s_xr.eyes[1].acquired);
    CHECK(!s_vr.input_synced&&!s_vr.hand_valid&&!s_vr.head_pose_valid);
    CHECK(!s_vr.empty_hand_valid[0]&&!s_vr.empty_hand_valid[1]);
    CHECK(!s_vr.swing.high&&!s_vr.swing.pulse);CHECK(s_vr.eye[0]==12&&s_vr.eye[1]==12);
    auto count=calls.size();quest_vr_set_resumed(0);CHECK(calls.size()==count);
    quest_vr_set_resumed(1);CHECK(s_xr.resumed);CHECK(!s_vr.head_pose_valid&&!s_vr.input_synced);
    CHECK(calls.size()==count);CHECK(s_xr.begin_frame());CHECK(s_xr.end_frame(false));
}
static void math_tests() {
    M34 a,b,c;
    m34_identity(a);
    for(int r=0;r<3;++r)for(int col=0;col<4;++col)near(a[r][col],r==col?1:0);
    for(int degrees=-180;degrees<=180;degrees+=5) {
        float rad=degrees*0.01745329252f;
        XrPosef pose={{0,sinf(rad/2),0,cosf(rad/2)},{12.5f,1.6f,-8.0f}};
        CHECK(pcvr_from_pose(pose,a));near(a[0][0],cosf(rad));near(a[0][2],sinf(rad));
        m34_invert_rigid(a,b);m34_mul(a,b,c);
        for(int r=0;r<3;++r)for(int col=0;col<4;++col)near(c[r][col],r==col?1:0);
        pcvr_recenter(a);m34_mul(s_seated_from_local,a,b);
        CHECK(s_have_seated_origin);for(int r=0;r<3;++r)for(int col=0;col<4;++col)near(b[r][col],r==col?1:0);
        // A physical ten-centimeter lean after recenter remains ten centimeters.
        pose.position.x+=0.1f;CHECK(pcvr_from_pose(pose,c));m34_mul(s_seated_from_local,c,b);
        near(sqrtf(b[0][3]*b[0][3]+b[2][3]*b[2][3]),0.1f);
        // Quaternion scale never changes the basis.
        pose.orientation.y*=3;pose.orientation.w*=3;CHECK(pcvr_from_pose(pose,b));
        for(int r=0;r<3;++r)for(int col=0;col<4;++col)near(b[r][col],c[r][col]);
    }
    XrPosef invalid={};CHECK(!pcvr_from_pose(invalid,a));invalid.orientation.w=1;invalid.position.x=NAN;CHECK(!pcvr_from_pose(invalid,a));
    invalid.position.x=0;invalid.orientation.w=INFINITY;CHECK(!pcvr_from_pose(invalid,a));
    // Recenter removes yaw/translation, preserving pitch and roll. These are
    // constructed from independent axis matrices, not the quaternion helper.
    M34 yaw,pitch,roll,tilt,local;
    m34_identity(yaw);m34_identity(pitch);m34_identity(roll);
    float y=0.7f,p=-0.3f,r=0.2f;
    yaw[0][0]=yaw[2][2]=cosf(y);yaw[0][2]=sinf(y);yaw[2][0]=-sinf(y);
    pitch[1][1]=pitch[2][2]=cosf(p);pitch[1][2]=-sinf(p);pitch[2][1]=sinf(p);
    roll[0][0]=roll[1][1]=cosf(r);roll[0][1]=-sinf(r);roll[1][0]=sinf(r);
    m34_mul(pitch,roll,tilt);m34_mul(yaw,tilt,local);local[0][3]=3;local[1][3]=1.5f;local[2][3]=-4;
    pcvr_recenter(local);m34_mul(s_seated_from_local,local,a);
    for(int row=0;row<3;++row)for(int col=0;col<4;++col)near(a[row][col],tilt[row][col]);
    for(int axis:{0,2}) {
        XrPosef pose={{0,0,0,cosf(0.2f)},{0,0,0}};
        if(axis==0)pose.orientation.x=sinf(0.2f);else pose.orientation.z=sinf(0.2f);
        CHECK(pcvr_from_pose(pose,a));m34_invert_rigid(a,b);m34_mul(a,b,c);
        for(int row=0;row<3;++row)for(int col=0;col<4;++col)near(c[row][col],row==col?1:0);
    }
    for(float scale:{0.01f,0.025f,0.10f}) for(int eye=0;eye<2;++eye) {
        s_vr.world_scale=scale;s_xr.views[eye].fov={-0.92f,0.71f,0.80f,-0.76f};pcvr_update_eye_projection(eye);
        auto& p=s_vr.eye_projection[eye];auto& f=s_xr.views[eye].fov;
        for(float z:{0.05f,1.0f,10.0f}) {
            near((p[0][0]*tanf(f.angleLeft)*z-p[0][2]*z)/z,-1);
            near((p[0][0]*tanf(f.angleRight)*z-p[0][2]*z)/z,1);
            near((p[1][1]*tanf(f.angleDown)*z-p[1][2]*z)/z,-1);
            near((p[1][1]*tanf(f.angleUp)*z-p[1][2]*z)/z,1);
        }
        near((-p[2][2]*PC_VR_NEAR_M+p[2][3])/PC_VR_NEAR_M,-1);
        near((-p[2][2]*pcvr_far_m()+p[2][3])/pcvr_far_m(),0);
    }
}
static void timing_tests() {
    reset();events={XR_SESSION_STATE_READY};CHECK(s_xr.begin_frame());
    CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));CHECK(s_xr.end_frame(true));
    CHECK(clock_reads==0);CHECK(timing_log_lines==0);
    reset();s_xr.timing_enabled=true;events={XR_SESSION_STATE_READY};
    CHECK(s_xr.begin_frame());CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));
    test_clock_ms+=25;CHECK(s_xr.end_frame(true));
    near(s_xr.timing_frame[QUEST_XR_WAIT_FRAME],7);
    near(s_xr.timing_frame[QUEST_XR_BEGIN_FRAME],2);
    near(s_xr.timing_frame[QUEST_XR_LOCATE_VIEWS],1);
    near(s_xr.timing_frame[QUEST_XR_ACQUIRE_WAIT],1);
    near(s_xr.timing_frame[QUEST_XR_BIND_TARGET],0.24f);
    near(s_xr.timing_frame[QUEST_XR_RENDER_SPAN],26.24f);
    near(s_xr.timing_frame[QUEST_XR_GL_FLUSH],0.3f);
    near(s_xr.timing_frame[QUEST_XR_RELEASE],0.8f);
    near(s_xr.timing_frame[QUEST_XR_END_FRAME],1);
    CHECK(s_xr.timing_frames==1);CHECK(timing_log_lines==0);
    for(int i=1;i<30;++i){
        test_clock_ms+=4;CHECK(s_xr.begin_frame());CHECK(s_xr.acquire_eye(0));CHECK(s_xr.acquire_eye(1));
        test_clock_ms+=25;CHECK(s_xr.end_frame(true));
    }
    CHECK(timing_log_lines==1+QUEST_XR_TIMING_COUNT);CHECK(s_xr.timing_frames==0);
    near(s_xr.timing_frame[QUEST_XR_FRAME_GAP],4);
    for(int i=0;i<QUEST_XR_TIMING_COUNT;++i){near(s_xr.timing_total[i],0);near(s_xr.timing_peak[i],0);}
}
static void input_tests() {
    reset();s_vr.act_a=(XrAction)1;s_vr.act_b=(XrAction)2;s_vr.act_l=(XrAction)3;s_vr.act_r=(XrAction)4;
    s_vr.act_trigger_l=(XrAction)5;s_vr.act_trigger_r=(XrAction)6;
    for(int synced=0;synced<2;++synced)for(int focused=0;focused<2;++focused)for(int resumed=0;resumed<2;++resumed) {
        s_vr.input_synced=synced;s_xr.focused=focused;s_xr.resumed=resumed;
        xr_action_calls=0;bool_value=1;float_value=0.8f;vector_value={0.7f,-0.4f};
        bool live=synced&&focused&&resumed;CHECK(pcvr_digital(s_vr.act_a)==live);CHECK(pcvr_digital(s_vr.act_l)==live);
        float x,y;pcvr_analog(s_vr.act_a,&x,&y);near(x,live?0.7f:0);near(y,live?-0.4f:0);
        if(!live)CHECK(xr_action_calls==0);
    }
    bool_value=0;float_value=0.6f;CHECK(pcvr_digital(s_vr.act_a));CHECK(pcvr_digital(s_vr.act_b));
    float_value=0.5f;CHECK(!pcvr_digital(s_vr.act_a));CHECK(!pcvr_digital(s_vr.act_l));
    float_value=NAN;CHECK(!pcvr_digital(s_vr.act_a));float_value=1;action_active=0;CHECK(!pcvr_digital(s_vr.act_a));
    action_active=1;vector_value={NAN,0};float x,y;pcvr_analog(s_vr.act_a,&x,&y);near(x,0);near(y,0);
}
int main() {frame_tests();math_tests();input_tests();timing_tests();printf("Quest OpenXR contract: %d checks, %d failures\n",checks,failures);return failures?1:0;}
