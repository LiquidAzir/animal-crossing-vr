/* Quest-native OpenXR backend for the existing pc_vr C contract.
 * Game-space math, UI routing and gameplay gates preserve the PC behavior.
 * Runtime/session/action implementation is in quest_vr_openxr.c_inc.
 * This file is selected only by the Quest build; pc_vr.cpp remains untouched. */
#include "quest_xr_runtime.h"
#include "quest_vr_android.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_vr.h"
#include "pc_fp_camera.h"
#include "pc_vr_swing.h"
#include "pc_vr_hands.h"
#include "pc_sky.h"
#include "pc_profiler.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#define pcvr_log quest_xr_log
extern "C" {
void pc_gx_draw_pending(void);
void pc_gx_flush_if_begin_complete(void);
void pc_gx_restore_after_nes(void);
void pc_gx_viewport_state_invalidate(void);
void pc_gx_mark_new_pass(void);
void pc_gx_vr_reset_routing(void);
void pc_gx_dirty_colormask(void);
void pc_gx_get_clear(float* rgba, float* depth);
extern u32 pc_frame_counter;
}
typedef float M34[3][4];


static void m34_identity(M34 m) {
    memset(m, 0, sizeof(M34));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
}

static void m34_mul(const M34 a, const M34 b, M34 out) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
                      + (j == 3 ? a[i][3] : 0.0f);
        }
    }
}

static void m34_invert_rs(const M34 m, M34 out) {
    float s2 = m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0];
    float k = (s2 > 1e-12f) ? 1.0f / s2 : 1.0f;   /* (sR)^T / s^2 = R^T / s */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = m[j][i] * k;
    for (int i = 0; i < 3; i++)
        out[i][3] = -(out[i][0] * m[0][3] + out[i][1] * m[1][3] + out[i][2] * m[2][3]);
}

static void m34_invert_rigid(const M34 m, M34 out) {
    /* R^T */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = m[j][i];
    /* -R^T * t */
    for (int i = 0; i < 3; i++)
        out[i][3] = -(out[i][0] * m[0][3] + out[i][1] * m[1][3] + out[i][2] * m[2][3]);
}


struct PCVRTarget { GLuint fbo, color, depth_rbo; int w, h; };
static QuestXrRuntime s_xr;
static JavaVM* s_vm;
static jobject s_activity;
static M34 s_seated_from_local;
static int s_have_seated_origin;
static u32 s_frame_stamp = (u32)-1;
static struct {
    int active, in_scene_pass, current_eye, ui_bound;
    PCVRTarget eye[2], ui, sink; // eye targets borrow runtime-owned swapchain FBOs
    M34 head_pose, inv_eye_pose[2], game_view, anchor_yaw;
    int have_pose, head_pose_valid, have_anchor;
    M34 view_correction[2];
    float eye_projection[2][4][4], eye_depth_range[2][2];
    float world_scale, ui_distance, ui_size, height_offset, ui_dist_k;
    GLuint panel_prog, panel_vao, panel_vbo;
    GLint panel_u_mvp, panel_u_tex;
    XrActionSet action_set;
    XrAction act_move, act_camera, act_a, act_b, act_x, act_y, act_l, act_r, act_z, act_start;
    XrAction act_trigger_l, act_trigger_r, act_hand_r, act_empty_hand[2], act_haptic_l, act_haptic_r;
    XrSpace aim_space, grip_space[2];
    int input_ready, input_synced, recenter_latch;
    M34 world_from_seated, hand_world, tool_local;
    int hand_valid;
    PCVRSwing swing;
    Uint32 active_since_ticks;
    u32 flat_scene_stamp;
    M34 empty_hand_pose[2];
    int empty_hand_valid[2], empty_hand_seen[2], empty_hands_missing_logged;
    int empty_hands_available, empty_hands_render_failed;
    u32 empty_hands_stamp;
    int submitted_this_frame;
} s_vr;
float g_pc_vr_cull_expand = 0.0f;
float g_pc_vr_cull_znear_slack = 0.0f;
int g_pc_town_residency = 0;
#define PC_VR_NEAR_M 0.05f
static void pcvr_update_eye_projection(int eye);
static void pcvr_update_view_correction(void);
static int pcvr_digital(XrAction action);
static void pcvr_analog(XrAction action, float* x, float* y);


extern "C" int pc_vr_draw_radius(void) {
    return g_pc_settings.vr_draw_radius;
}

static float pcvr_far_m(void) {
    float f = 6000.0f * s_vr.world_scale;
    return f < 200.0f ? 200.0f : f;
}

void pc_vr_set_fp_scale(int fp_active) {
    if (!s_vr.active) return;
    float want = fp_active
        ? (g_pc_settings.vr_fp_world_scale / 1000.0f)
        : (g_pc_settings.vr_world_scale / 1000.0f);
    if (want <= 0.0001f) want = fp_active ? 0.025f : 0.01f;
    if (want == s_vr.world_scale) return;
    s_vr.world_scale = want;
    pcvr_update_eye_projection(0);
    pcvr_update_eye_projection(1);
    pcvr_update_view_correction();
    pcvr_log("world scale -> %.4f m/unit (%s)", want, fp_active ? "first person" : "camera");
}

static int pcvr_create_target(PCVRTarget* t, int w, int h) {
    memset(t, 0, sizeof(*t));
    t->w = w;
    t->h = h;
    glGenFramebuffers(1, &t->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);

    glGenTextures(1, &t->color);
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->color, 0);

    glGenRenderbuffers(1, &t->depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, t->depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t->depth_rbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        pcvr_log("FBO %dx%d incomplete: 0x%04X", w, h, status);
        return 0;
    }
    return 1;
}

static void pcvr_destroy_target(PCVRTarget* t) {
    if (t->fbo) glDeleteFramebuffers(1, &t->fbo);
    if (t->color) glDeleteTextures(1, &t->color);
    if (t->depth_rbo) glDeleteRenderbuffers(1, &t->depth_rbo);
    memset(t, 0, sizeof(*t));
}

static const char* s_panel_vs =
    "#version 300 es\nprecision highp float;\nprecision highp int;\n"
    "layout(location=0) in vec2 a_pos;\n"   /* -1..1 quad */
    "layout(location=1) in vec2 a_uv;\n"
    "uniform mat4 u_mvp;\n"
    "uniform vec2 u_half;\n"                /* panel half extents, meters */
    "uniform vec3 u_center;\n"              /* panel center, seated meters */
    "out vec2 v_uv;\n"
    "void main() {\n"
    "  vec3 p = u_center + vec3(a_pos.x * u_half.x, a_pos.y * u_half.y, 0.0);\n"
    "  gl_Position = u_mvp * vec4(p, 1.0);\n"
    "  v_uv = a_uv;\n"
    "}\n";

static const char* s_panel_fs =
    "#version 300 es\nprecision highp float;\nprecision highp int;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "  vec4 c = texture(u_tex, v_uv);\n"
    "  frag = c;\n"
    "}\n";

static GLint s_panel_u_half, s_panel_u_center;

static GLuint pcvr_compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        pcvr_log("panel shader compile failed: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static int pcvr_create_panel_gl(void) {
    GLuint vs = pcvr_compile(GL_VERTEX_SHADER, s_panel_vs);
    GLuint fs = pcvr_compile(GL_FRAGMENT_SHADER, s_panel_fs);
    if (!vs || !fs) return 0;
    s_vr.panel_prog = glCreateProgram();
    glAttachShader(s_vr.panel_prog, vs);
    glAttachShader(s_vr.panel_prog, fs);
    glLinkProgram(s_vr.panel_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(s_vr.panel_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        pcvr_log("panel shader link failed");
        return 0;
    }
    s_vr.panel_u_mvp = glGetUniformLocation(s_vr.panel_prog, "u_mvp");
    s_vr.panel_u_tex = glGetUniformLocation(s_vr.panel_prog, "u_tex");
    s_panel_u_half   = glGetUniformLocation(s_vr.panel_prog, "u_half");
    s_panel_u_center = glGetUniformLocation(s_vr.panel_prog, "u_center");

    /* Unit quad; the UI FBO is rendered with GL's bottom-left origin so
     * sampling is direct (v=0 at bottom). */
    static const float quad[] = {
        /* pos      uv */
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
        -1.f,  1.f, 0.f, 1.f,
         1.f,  1.f, 1.f, 1.f,
    };
    glGenVertexArrays(1, &s_vr.panel_vao);
    glGenBuffers(1, &s_vr.panel_vbo);
    glBindVertexArray(s_vr.panel_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vr.panel_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
    return 1;
}



static void pcvr_update_view_correction(void) {
    /* Camera pose from V (rigid): C = -R^T t, forward = -row2 */
    const M34* V = &s_vr.game_view;
    float fx = -(*V)[2][0], fz = -(*V)[2][2];
    float len = sqrtf(fx * fx + fz * fz);

    M34 A;
    if (len > 1e-4f) {
        fx /= len; fz /= len;
        /* Leveled view basis: back = (-fx, 0, -fz), up = +Y,
         * right = up x back = (bz, 0, -bx) */
        float bx = -fx, bz = -fz;
        float rx = bz, rz = -bx;
        A[0][0] = rx;  A[0][1] = 0.0f; A[0][2] = rz;
        A[1][0] = 0.0f; A[1][1] = 1.0f; A[1][2] = 0.0f;
        A[2][0] = bx;  A[2][1] = 0.0f; A[2][2] = bz;
        /* camera position C = -R^T t */
        float cx = -((*V)[0][0] * (*V)[0][3] + (*V)[1][0] * (*V)[1][3] + (*V)[2][0] * (*V)[2][3]);
        float cy = -((*V)[0][1] * (*V)[0][3] + (*V)[1][1] * (*V)[1][3] + (*V)[2][1] * (*V)[2][3]);
        float cz = -((*V)[0][2] * (*V)[0][3] + (*V)[1][2] * (*V)[1][3] + (*V)[2][2] * (*V)[2][3]);
        A[0][3] = -(A[0][0] * cx + A[0][1] * cy + A[0][2] * cz);
        A[1][3] = -(A[1][0] * cx + A[1][1] * cy + A[1][2] * cz);
        A[2][3] = -(A[2][0] * cx + A[2][1] * cy + A[2][2] * cz);
        memcpy(s_vr.anchor_yaw, A, sizeof(M34));
        s_vr.have_anchor = 1;
    } else if (s_vr.have_anchor) {
        memcpy(A, s_vr.anchor_yaw, sizeof(M34));
    } else {
        m34_identity(A);
    }

    /* W = T_height * Scale * A : world units -> seated meters */
    M34 W;
    float s = s_vr.world_scale;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            W[i][j] = A[i][j] * s;
    W[1][3] -= s_vr.height_offset;

    M34 Vinv;
    m34_invert_rigid(*V, Vinv);

    for (int eye = 0; eye < 2; eye++) {
        M34 t1, X;
        m34_mul(W, Vinv, t1);                       /* W * V^-1 */
        m34_mul(s_vr.inv_eye_pose[eye], t1, X);     /* (H*E)^-1 * W * V^-1 */
        memcpy(s_vr.view_correction[eye], X, sizeof(M34));
    }

    /* Motion tools need seated-meters -> game-world (W^-1) */
    m34_invert_rs(W, s_vr.world_from_seated);
}

void pc_vr_notify_game_view(const float* mtx34) {
    /* A batch may still be buffered from before this view change — flush it
     * so it renders with the correction that was current when it was drawn. */
    pc_gx_flush_if_begin_complete();
    pc_sky_set_view(mtx34);
    if (!s_vr.active) return;
    memcpy(s_vr.game_view, mtx34, sizeof(M34));
    pcvr_update_view_correction();
}

void pc_vr_begin_eye(int eye) {
    if (!s_vr.active) return;
    pc_gx_draw_pending();
    pc_sky_begin_pass();
    s_vr.in_scene_pass = 1;
    s_vr.current_eye = eye;
    s_vr.ui_bound = 0;
    s_vr.eye_depth_range[eye][0] = 0.0f;
    s_vr.eye_depth_range[eye][1] = 1.0f;
    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[eye].fbo);
    g_pc_target_w = s_vr.eye[eye].w;
    g_pc_target_h = s_vr.eye[eye].h;
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, s_vr.eye[eye].w, s_vr.eye[eye].h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    /* Use the game's current copy-clear color for the sky. */
    float rgba[4], depth;
    pc_gx_get_clear(rgba, &depth);
    glClearColor(rgba[0], rgba[1], rgba[2], 1.0f);
    glClearDepthf(depth);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Per-eye matrices differ: every uniform group must re-upload to every
     * shader program this pass, and ortho routing starts on the eye FBO. */
    pc_gx_vr_reset_routing();
    pc_gx_mark_new_pass();
}

int pc_vr_current_eye(void) {
    return s_vr.in_scene_pass ? s_vr.current_eye : -1;
}

int pc_vr_in_scene_pass(void) {
    return s_vr.active && s_vr.in_scene_pass;
}

const float* pc_vr_eye_projection(void) {
    return &s_vr.eye_projection[s_vr.current_eye == 1 ? 1 : 0][0][0];
}

const float* pc_vr_view_correction(void) {
    return &s_vr.view_correction[s_vr.current_eye == 1 ? 1 : 0][0][0];
}

void pc_vr_set_scene_depth_range(float near_depth, float far_depth) {
    if (!s_vr.active || !s_vr.in_scene_pass || s_vr.ui_bound ||
        s_vr.current_eye < 0 || s_vr.current_eye > 1 ||
        !isfinite(near_depth) || !isfinite(far_depth)) return;
    if (s_vr.eye_depth_range[s_vr.current_eye][0] == near_depth &&
        s_vr.eye_depth_range[s_vr.current_eye][1] == far_depth) return;
    /* Match glDepthRange's clamping, including emu64's 1022/1023 far value. */
    s_vr.eye_depth_range[s_vr.current_eye][0] = fmaxf(0.0f, fminf(1.0f, near_depth));
    s_vr.eye_depth_range[s_vr.current_eye][1] = fmaxf(0.0f, fminf(1.0f, far_depth));
}

unsigned int pc_vr_bind_ui_target(int ui) {
    if (!s_vr.active) return 0;
    if (ui) {
        s_vr.ui_bound = 1;
        glBindFramebuffer(GL_FRAMEBUFFER, s_vr.ui.fbo);
        g_pc_target_w = s_vr.ui.w;
        g_pc_target_h = s_vr.ui.h;
        return s_vr.ui.fbo;
    }
    s_vr.ui_bound = 0;
    if (s_vr.in_scene_pass) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[s_vr.current_eye].fbo);
        g_pc_target_w = s_vr.eye[s_vr.current_eye].w;
        g_pc_target_h = s_vr.eye[s_vr.current_eye].h;
        return s_vr.eye[s_vr.current_eye].fbo;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_pc_target_w = g_pc_window_w;
    g_pc_target_h = g_pc_window_h;
    return 0;
}

int pc_vr_skip_ui_draws(void) {
    return s_vr.active && s_vr.in_scene_pass && s_vr.current_eye == 1;
}

float pc_vr_world_scale(void) {
    return s_vr.world_scale;
}

int pc_vr_tool_input_allowed(void) {
    extern int g_pc_paused;
    return s_vr.active && s_xr.focused && s_xr.resumed && s_vr.head_pose_valid && !g_pc_paused &&
        pc_fp_view_is_active() && !pc_vr_flat_scene_active() && !pc_fp_in_talk();
}

float pc_vr_head_yaw_offset_bang(void) {
    if (!s_vr.active || !s_vr.have_pose) return 0.0f;
    return atan2f(s_vr.head_pose[0][2], s_vr.head_pose[2][2])
         * (65536.0f / (float)(2.0 * PC_PI));
}

void pc_vr_set_empty_hands_available(int available) {
    s_vr.empty_hands_available = available != 0;
    s_vr.empty_hands_stamp = pc_frame_counter;
}

static int pcvr_empty_hands_visible(void) {
    return g_pc_settings.vr_empty_hands && pc_vr_tool_input_allowed() &&
        s_vr.empty_hands_available && s_vr.empty_hands_stamp == pc_frame_counter;
}

void pc_vr_set_flat_scene(int on) {
    s_vr.flat_scene_stamp = on ? pc_frame_counter : (u32)(pc_frame_counter - 1000u);
}

int pc_vr_flat_scene_active(void) {
    /* The PC pause menu gates Game_play_move (no stamping) but drawing
     * continues — freeze the answer while paused, like the FP flag, or a
     * paused submenu re-renders its 3D items with VR matrices. */
    extern int g_pc_paused;
    static int s_last;
    if (!g_pc_paused) {
        s_last = (u32)(pc_frame_counter - s_vr.flat_scene_stamp) <= 1u;
    }
    return s_vr.active && s_last;
}

static void pcvr_draw_empty_hands(void) {
    if (!pcvr_empty_hands_visible() || s_vr.empty_hands_render_failed) return;
    if (!s_vr.empty_hands_missing_logged &&
        (!s_vr.empty_hand_seen[0] || !s_vr.empty_hand_seen[1]) &&
        SDL_GetTicks() - s_vr.active_since_ticks > 10000) {
        s_vr.empty_hands_missing_logged = 1;
        pcvr_log("empty hands enabled but an optional controller pose has not arrived. "
                 "Wake both controllers and resume the immersive session. "
                 "Tool aim tracking and empty-hand grip tracking are independent.");
    }
    if (!s_vr.empty_hand_valid[0] && !s_vr.empty_hand_valid[1]) return;

    GLint draw_fbo, read_fbo, viewport[4];
    GLfloat depth_range[2];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetFloatv(GL_DEPTH_RANGE, depth_range);
    for (int eye = 0; eye < 2; ++eye) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_vr.eye[eye].fbo);
        glViewport(0, 0, s_vr.eye[eye].w, s_vr.eye[eye].h);
        glDepthRangef(s_vr.eye_depth_range[eye][0], s_vr.eye_depth_range[eye][1]);
        for (int hand = 0; hand < 2; ++hand) {
            if (!s_vr.empty_hand_valid[hand]) continue;
            M34 eye_from_grip;
            m34_mul(s_vr.inv_eye_pose[eye], s_vr.empty_hand_pose[hand], eye_from_grip);
            /* A malformed tracking frame is temporary, not a shader failure. */
            int finite = 1;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col)
                    if (!isfinite(eye_from_grip[row][col])) finite = 0;
            if (!finite) continue;
            if (!pc_vr_hands_draw(&eye_from_grip[0][0], &s_vr.eye_projection[eye][0][0], hand)) {
                s_vr.empty_hands_render_failed = 1;
                pcvr_log("optional empty-hand renderer unavailable; continuing without hands");
                break;
            }
        }
        if (s_vr.empty_hands_render_failed) break;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDepthRangef(depth_range[0], depth_range[1]);
}

void pc_vr_end_scene_passes(void) {
    if (!s_vr.active) return;
    pc_gx_draw_pending();
    pcvr_draw_empty_hands();
    s_vr.in_scene_pass = 0;
    s_vr.current_eye = -1;
    s_vr.ui_bound = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_pc_target_w = g_pc_window_w;
    g_pc_target_h = g_pc_window_h;
}

static void pcvr_draw_panel(int eye) {
    /* MVP = P_eye * (H*E)^-1, panel verts already in seated meters */
    float m44[4][4];
    const M34* v = &s_vr.inv_eye_pose[eye];
    float tmp[4][4];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            tmp[i][j] = (*v)[i][j];
    tmp[3][0] = 0.0f; tmp[3][1] = 0.0f; tmp[3][2] = 0.0f; tmp[3][3] = 1.0f;

    const float (*P)[4] = s_vr.eye_projection[eye];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m44[i][j] = P[i][0] * tmp[0][j] + P[i][1] * tmp[1][j]
                      + P[i][2] * tmp[2][j] + P[i][3] * tmp[3][j];

    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[eye].fbo);
    glViewport(0, 0, s_vr.eye[eye].w, s_vr.eye[eye].h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    /* UI content is rendered over transparent black (premultiplied-ish) */
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s_vr.panel_prog);
    float column_major[16];
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        column_major[c * 4 + r] = m44[r][c];
    glUniformMatrix4fv(s_vr.panel_u_mvp, 1, GL_FALSE, column_major);
    /* First person: the panel (and its apparent size) pulls in so dialogue
     * doesn't sit stereo-behind a villager standing a meter away. dist_k is
     * smoothed once per frame in compose (both eyes must agree). */
    float dist_k = s_vr.ui_dist_k;
    float half_w = s_vr.ui_size * 0.5f * dist_k;
    float half_h = half_w * 0.75f; /* 4:3 */
    glUniform2f(s_panel_u_half, half_w, half_h);
    glUniform3f(s_panel_u_center, 0.0f, -0.05f * dist_k, -s_vr.ui_distance * dist_k);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_vr.ui.color);
    glUniform1i(s_vr.panel_u_tex, 0);
    glBindVertexArray(s_vr.panel_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void pc_vr_ensure_submitted(void) {
    if (!s_vr.active) return;
    if (s_frame_stamp != pc_frame_counter) pc_vr_frame_begin();
    if (s_vr.submitted_this_frame) return;
    pc_gx_draw_pending();
    for (int eye = 0; eye < 2; eye++) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[eye].fbo);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepthf(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pc_vr_compose_and_submit();
}

void pc_vr_merge_pad(unsigned short* buttons,
                                signed char* stickX, signed char* stickY,
                                signed char* cstickX, signed char* cstickY,
                                unsigned char* triggerL, unsigned char* triggerR) {
    if (!s_vr.active || !s_vr.input_ready) return;

    /* dolphin/pad.h values (avoid header dependency in C++ TU) */
    enum {
        BTN_LEFT = 0x0001, BTN_RIGHT = 0x0002, BTN_DOWN = 0x0004, BTN_UP = 0x0008,
        BTN_Z = 0x0010, BTN_R = 0x0020, BTN_L = 0x0040,
        BTN_A = 0x0100, BTN_B = 0x0200, BTN_X = 0x0400, BTN_Y = 0x0800,
        BTN_START = 0x1000
    };

    float mx, my, cx, cy;
    pcvr_analog(s_vr.act_move, &mx, &my);
    pcvr_analog(s_vr.act_camera, &cx, &cy);

    int l_held = pcvr_digital(s_vr.act_l);

    if (l_held) {
        /* Left grip held: left stick becomes the D-pad (tool switching) */
        if (my > 0.5f)  *buttons |= BTN_UP;
        if (my < -0.5f) *buttons |= BTN_DOWN;
        if (mx < -0.5f) *buttons |= BTN_LEFT;
        if (mx > 0.5f)  *buttons |= BTN_RIGHT;
    } else if (fabsf(mx) > 0.12f || fabsf(my) > 0.12f) {
        int sx = (int)(mx * 100.0f), sy = (int)(my * 100.0f);
        if (sx > 100) sx = 100; if (sx < -100) sx = -100;
        if (sy > 100) sy = 100; if (sy < -100) sy = -100;
        *stickX = (signed char)sx;
        *stickY = (signed char)sy;
    }
    if (fabsf(cx) > 0.12f || fabsf(cy) > 0.12f) {
        int sx = (int)(cx * 100.0f), sy = (int)(cy * 100.0f);
        if (sx > 100) sx = 100; if (sx < -100) sx = -100;
        if (sy > 100) sy = 100; if (sy < -100) sy = -100;
        *cstickX = (signed char)sx;
        *cstickY = (signed char)sy;
    }

    /* Left grip + Y: toggle first person. Requires Y to RISE while the grip
     * is already held (holding Y then squeezing grip must not fire), and X
     * must be up (X+Y is the recenter chord). Y is swallowed while gripped.
     * Not while the pause menu is open — the camera is frozen there. */
    extern int g_pc_paused;
    static int s_y_prev = 0;
    int y_now = pcvr_digital(s_vr.act_y);
    if (l_held && y_now && !s_y_prev && !pcvr_digital(s_vr.act_x) && !g_pc_paused) {
        pc_fp_toggle();
    }
    s_y_prev = y_now;

    /* Motion swing: inject the tool-use press (ORed with real A) */
    /* Selecting a tool must not also use the old tool from a pending gesture.
     * Real A is still merged independently below. */
    if (!pc_vr_tool_input_allowed() || l_held) pc_vr_swing_cancel(&s_vr.swing);
    if (s_vr.swing.pulse > 0) *buttons |= BTN_A;

    if (pcvr_digital(s_vr.act_a)) *buttons |= BTN_A;
    if (pcvr_digital(s_vr.act_b)) *buttons |= BTN_B;
    if (pcvr_digital(s_vr.act_x)) *buttons |= BTN_X;
    if (y_now && !l_held) *buttons |= BTN_Y;
    if (pcvr_digital(s_vr.act_z)) *buttons |= BTN_Z;
    if (pcvr_digital(s_vr.act_start)) *buttons |= BTN_START;
    if (l_held) { *buttons |= BTN_L; *triggerL = 255; }
    if (pcvr_digital(s_vr.act_r)) { *buttons |= BTN_R; *triggerR = 255; }
}

int pc_vr_hand_tool_mtx(float out[12]) {
    if (!s_vr.active || !s_vr.hand_valid || !g_pc_settings.vr_tool_on_hand)
        return 0;
    if (!pc_fp_view_is_active())
        return 0;
    memcpy(out, s_vr.hand_world, sizeof(float) * 12);
    return 1;
}

int pc_vr_item_presentation_mtx(float out[12]) {
    extern int g_pc_paused;
    if (!out || !s_vr.active || !s_vr.head_pose_valid || !s_vr.have_anchor ||
        !pc_fp_view_is_active() || pc_vr_flat_scene_active() || g_pc_paused ||
        !isfinite(s_vr.world_scale) || s_vr.world_scale <= 0.0001f)
        return 0;

    /* CPU actor drawing uses the most recent tracking sample, like tools.
     * W^-1 * H places the head in game world; its basis contains 1/scale.
     * Offset in physical meters, then normalize only the orientation so the
     * caller retains the original item size and animation. */
    M34 head_world, result;
    m34_mul(s_vr.world_from_seated, s_vr.head_pose, head_world);
    for (int row = 0; row < 3; ++row) {
        result[row][3] = head_world[row][3] - 0.14f * head_world[row][1]
                                             - 0.90f * head_world[row][2];
        for (int col = 0; col < 3; ++col)
            result[row][col] = head_world[row][col] * s_vr.world_scale;
        for (int col = 0; col < 4; ++col)
            if (!isfinite(result[row][col])) return 0;
    }
    memcpy(out, result, sizeof(result));
    return 1;
}

#include "quest_vr_openxr.c_inc"
