/* pc_vr.cpp - SteamVR (OpenVR) backend: stereo FBOs, tracking, compositor
 * submit, UI panel composite, and Touch-controller input.
 *
 * C++ because openvr.h is a C++ header; everything exported is extern "C"
 * (see pc_vr.h). All calls run on the main/render thread.
 *
 * Spaces and conventions (see pc/VR_ARCHITECTURE.md):
 *   - Game view matrices are row-major 3x4, right-handed, -Z forward,
 *     game units (1 tile = 40 units).
 *   - OpenVR poses are row-major 3x4 (HmdMatrix34_t), meters, -Z forward.
 *   - The seated tracking origin is equated with a *leveled* (yaw-only)
 *     frame at the game camera: W = T_height * Scale * Anchor maps game
 *     world -> seated meters. Per eye: V_vr = (H*E)^-1 * W, and the
 *     correction applied to the game's combined view*model matrices is
 *     X = V_vr * V_game^-1.
 *   - Projections are kept in the GX depth convention (near -> NDC -1,
 *     far -> NDC 0) to match the rest of pc_gx.
 */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_diag.h"
#include "pc_vr.h"

#include "openvr.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

extern "C" {
void pc_gx_draw_pending(void);
void pc_gx_flush_if_begin_complete(void);
void pc_gx_restore_after_nes(void);
void pc_gx_viewport_state_invalidate(void);
void pc_gx_mark_new_pass(void);
void pc_gx_vr_reset_routing(void);
void pc_gx_get_clear(float* rgba, float* depth);
extern int g_pc_target_w, g_pc_target_h;   /* pc_gx.c: current render target dims */
}

/* ---------------------------------------------------------------- */
/* Small row-major matrix helpers (3x4 rigid-ish, implicit [0 0 0 1]) */

typedef float M34[3][4];

static void m34_identity(M34 m) {
    memset(m, 0, sizeof(M34));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
}

/* out = a * b  (apply b first, then a). out may not alias a or b. */
static void m34_mul(const M34 a, const M34 b, M34 out) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
                      + (j == 3 ? a[i][3] : 0.0f);
        }
    }
}

/* Inverse of a rigid transform (orthonormal rotation + translation). */
static void m34_invert_rigid(const M34 m, M34 out) {
    /* R^T */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = m[j][i];
    /* -R^T * t */
    for (int i = 0; i < 3; i++)
        out[i][3] = -(out[i][0] * m[0][3] + out[i][1] * m[1][3] + out[i][2] * m[2][3]);
}

static void m34_from_hmd(const vr::HmdMatrix34_t* src, M34 out) {
    memcpy(out, src->m, sizeof(M34));
}

/* ---------------------------------------------------------------- */
/* State */

typedef struct {
    GLuint fbo;
    GLuint color;
    GLuint depth_rbo;
    int w, h;
} PCVRTarget;

static struct {
    int active;                 /* VR session live */
    int requested;              /* 0=off, 1=auto, 2=forced on */
    vr::IVRSystem* sys;

    PCVRTarget eye[2];
    PCVRTarget ui;

    int in_scene_pass;          /* inside an eye pass */
    int current_eye;
    int ui_bound;               /* ortho routing currently targets UI FBO */

    /* Tracking */
    M34 head_pose;              /* H: head -> seated tracking, meters */
    M34 eye_to_head[2];         /* E */
    M34 inv_eye_pose[2];        /* (H*E)^-1, refreshed each frame */
    int have_pose;

    /* Game view + derived correction */
    M34 game_view;              /* V: world -> game camera view, game units */
    M34 anchor_yaw;             /* last good leveled anchor (fallback) */
    int have_anchor;
    M34 view_correction[2];     /* X = V_vr * V^-1, per eye */
    float eye_projection[2][4][4]; /* GX z-convention, row-major */

    /* Settings */
    float world_scale;          /* meters per game unit */
    float ui_distance;          /* meters */
    float ui_size;              /* panel width, meters */
    float height_offset;        /* meters */

    /* UI composite GL objects */
    GLuint panel_prog;
    GLuint panel_vao, panel_vbo;
    GLint  panel_u_mvp, panel_u_tex;

    /* Input */
    vr::VRActionSetHandle_t action_set;
    vr::VRActionHandle_t act_move, act_camera;
    vr::VRActionHandle_t act_a, act_b, act_x, act_y, act_l, act_r, act_z, act_start;
    vr::VRActionHandle_t act_recenter;
    vr::VRActionHandle_t act_haptic_l, act_haptic_r;
    int input_ready;
    int recenter_latch;

    int submit_fail_count;
    int submitted_this_frame;
    unsigned int frame_index;
} s_vr;

float g_pc_vr_cull_expand = 0.0f;
float g_pc_vr_cull_znear_slack = 0.0f;

/* GX depth convention rows: near -> -1, far -> 0 (see emu64 projection load) */
#define PC_VR_NEAR_M 0.05f
/* Far plane scales with the world so large vr_world_scale values don't clip
 * the horizon (game far is 1600 units; leave generous headroom). */
static float pcvr_far_m(void) {
    float f = 6000.0f * s_vr.world_scale;
    return f < 200.0f ? 200.0f : f;
}

/* ---------------------------------------------------------------- */
/* GL helpers */

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
        printf("[VR] FBO %dx%d incomplete: 0x%04X\n", w, h, status);
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
    "#version 330 core\n"
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
    "#version 330 core\n"
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
        printf("[VR] panel shader compile failed: %s\n", log);
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
        printf("[VR] panel shader link failed\n");
        return 0;
    }
    s_vr.panel_u_mvp = glGetUniformLocation(s_vr.panel_prog, "u_mvp");
    s_vr.panel_u_tex = glGetUniformLocation(s_vr.panel_prog, "u_tex");
    s_panel_u_half   = glGetUniformLocation(s_vr.panel_prog, "u_half");
    s_panel_u_center = glGetUniformLocation(s_vr.panel_prog, "u_center");

    /* Unit quad, UV flipped in V: the UI FBO is rendered with GL's
     * bottom-left origin, matching the game's Y-down flip in the viewport,
     * so sampling is direct (v=0 at bottom). */
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

/* ---------------------------------------------------------------- */
/* Input actions */

static void pcvr_input_init(void) {
    s_vr.input_ready = 0;

    char manifest[MAX_PATH];
    if (!GetFullPathNameA("vr_actions\\actionmanifest.json", sizeof(manifest), manifest, NULL))
        return;
    FILE* f = fopen(manifest, "rb");
    if (!f) {
        printf("[VR] no action manifest at %s - VR controllers disabled\n", manifest);
        return;
    }
    fclose(f);

    vr::EVRInputError err = vr::VRInput()->SetActionManifestPath(manifest);
    if (err != vr::VRInputError_None) {
        printf("[VR] SetActionManifestPath failed: %d\n", (int)err);
        return;
    }

    vr::VRInput()->GetActionSetHandle("/actions/main", &s_vr.action_set);
    vr::VRInput()->GetActionHandle("/actions/main/in/move",     &s_vr.act_move);
    vr::VRInput()->GetActionHandle("/actions/main/in/camera",   &s_vr.act_camera);
    vr::VRInput()->GetActionHandle("/actions/main/in/a",        &s_vr.act_a);
    vr::VRInput()->GetActionHandle("/actions/main/in/b",        &s_vr.act_b);
    vr::VRInput()->GetActionHandle("/actions/main/in/x",        &s_vr.act_x);
    vr::VRInput()->GetActionHandle("/actions/main/in/y",        &s_vr.act_y);
    vr::VRInput()->GetActionHandle("/actions/main/in/l",        &s_vr.act_l);
    vr::VRInput()->GetActionHandle("/actions/main/in/r",        &s_vr.act_r);
    vr::VRInput()->GetActionHandle("/actions/main/in/z",        &s_vr.act_z);
    vr::VRInput()->GetActionHandle("/actions/main/in/start",    &s_vr.act_start);
    vr::VRInput()->GetActionHandle("/actions/main/in/recenter", &s_vr.act_recenter);
    vr::VRInput()->GetActionHandle("/actions/main/out/haptic_left",  &s_vr.act_haptic_l);
    vr::VRInput()->GetActionHandle("/actions/main/out/haptic_right", &s_vr.act_haptic_r);
    s_vr.input_ready = 1;
}

static int pcvr_digital(vr::VRActionHandle_t h) {
    if (!s_vr.input_ready || h == vr::k_ulInvalidActionHandle) return 0;
    vr::InputDigitalActionData_t d;
    if (vr::VRInput()->GetDigitalActionData(h, &d, sizeof(d),
            vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None)
        return 0;
    return d.bActive && d.bState;
}

static void pcvr_analog(vr::VRActionHandle_t h, float* x, float* y) {
    *x = 0.0f; *y = 0.0f;
    if (!s_vr.input_ready || h == vr::k_ulInvalidActionHandle) return;
    vr::InputAnalogActionData_t d;
    if (vr::VRInput()->GetAnalogActionData(h, &d, sizeof(d),
            vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None)
        return;
    if (!d.bActive) return;
    *x = d.x; *y = d.y;
}

/* ---------------------------------------------------------------- */
/* Matrix pipeline */

static void pcvr_update_eye_projection(int eye) {
    vr::HmdMatrix44_t p = s_vr.sys->GetProjectionMatrix(
        eye == 0 ? vr::Eye_Left : vr::Eye_Right, PC_VR_NEAR_M, pcvr_far_m());

    float (*out)[4] = s_vr.eye_projection[eye];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            out[i][j] = p.m[i][j];

    /* Rewrite depth rows to the GX convention used by the whole pipeline:
     * z_ndc = (z*n + n*f) / (n-f) / -z  ->  near maps to -1, far to 0. */
    const float n = PC_VR_NEAR_M, f = pcvr_far_m();
    out[2][0] = 0.0f; out[2][1] = 0.0f;
    out[2][2] = n / (n - f);
    out[2][3] = (n * f) / (n - f);
    out[3][0] = 0.0f; out[3][1] = 0.0f; out[3][2] = -1.0f; out[3][3] = 0.0f;
}

/* Rebuild X = V_vr * V^-1 for both eyes from the current game view + pose. */
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
}

extern "C" void pc_vr_notify_game_view(const float* mtx34) {
    if (!s_vr.active) return;
    /* A batch may still be buffered from before this view change — flush it
     * so it renders with the correction that was current when it was drawn. */
    pc_gx_flush_if_begin_complete();
    memcpy(s_vr.game_view, mtx34, sizeof(M34));
    pcvr_update_view_correction();
}

/* ---------------------------------------------------------------- */
/* Lifecycle */

extern "C" void pc_vr_init(void) {
    memset(&s_vr, 0, sizeof(s_vr));
    s_vr.requested = g_pc_settings.vr_mode;
    s_vr.world_scale = g_pc_settings.vr_world_scale / 1000.0f;   /* stored as mm per unit */
    s_vr.ui_distance = g_pc_settings.vr_ui_distance / 100.0f;    /* stored as cm */
    s_vr.ui_size = g_pc_settings.vr_ui_size / 100.0f;            /* stored as cm */
    s_vr.height_offset = g_pc_settings.vr_height_offset / 100.0f;
    if (s_vr.world_scale <= 0.0001f) s_vr.world_scale = 0.01f;
    if (s_vr.ui_distance < 0.5f) s_vr.ui_distance = 2.0f;
    if (s_vr.ui_size < 0.5f) s_vr.ui_size = 2.4f;
    m34_identity(s_vr.game_view);

    if (s_vr.requested == 0) return;

    if (!vr::VR_IsRuntimeInstalled()) {
        printf("[VR] SteamVR is not installed - running flat\n");
        if (s_vr.requested == 2)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Animal Crossing VR",
                "SteamVR is not installed.\nInstall SteamVR from Steam, then relaunch.", g_pc_window);
        return;
    }
    if (s_vr.requested == 1 && !vr::VR_IsHmdPresent()) {
        printf("[VR] no HMD detected - running flat (use --vr to force)\n");
        return;
    }

    vr::EVRInitError err = vr::VRInitError_None;
    s_vr.sys = vr::VR_Init(&err, vr::VRApplication_Scene);
    if (err != vr::VRInitError_None) {
        printf("[VR] VR_Init failed: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(err));
        if (s_vr.requested == 2)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Animal Crossing VR",
                "Could not start SteamVR.\nIs the headset connected?", g_pc_window);
        s_vr.sys = NULL;
        return;
    }
    if (!vr::VRCompositor()) {
        printf("[VR] compositor unavailable - running flat\n");
        vr::VR_Shutdown();
        s_vr.sys = NULL;
        return;
    }
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);

    uint32_t rw = 0, rh = 0;
    s_vr.sys->GetRecommendedRenderTargetSize(&rw, &rh);
    if (rw < 640) rw = 1440;
    if (rh < 480) rh = 1584;

    if (!pcvr_create_target(&s_vr.eye[0], (int)rw, (int)rh) ||
        !pcvr_create_target(&s_vr.eye[1], (int)rw, (int)rh) ||
        !pcvr_create_target(&s_vr.ui, 1280, 960) ||
        !pcvr_create_panel_gl()) {
        printf("[VR] GL resource creation failed - running flat\n");
        pcvr_destroy_target(&s_vr.eye[0]);
        pcvr_destroy_target(&s_vr.eye[1]);
        pcvr_destroy_target(&s_vr.ui);
        vr::VR_Shutdown();
        s_vr.sys = NULL;
        return;
    }

    for (int e = 0; e < 2; e++) {
        vr::HmdMatrix34_t eth = s_vr.sys->GetEyeToHeadTransform(e == 0 ? vr::Eye_Left : vr::Eye_Right);
        m34_from_hmd(&eth, s_vr.eye_to_head[e]);
        m34_identity(s_vr.inv_eye_pose[e]);
        pcvr_update_eye_projection(e);
        m34_identity(s_vr.view_correction[e]);
    }
    m34_identity(s_vr.head_pose);

    pcvr_input_init();

    /* VR paces via WaitGetPoses: window vsync and the 60fps limiter would
     * fight it. */
    SDL_GL_SetSwapInterval(0);
    g_frame_limiter = 0;

    g_pc_vr_cull_expand = 24.0f;
    g_pc_vr_cull_znear_slack = 8.0f;

    s_vr.active = 1;
    s_vr.current_eye = -1;
    printf("[VR] SteamVR active: eye %ux%u, world scale %.4f m/unit\n",
           rw, rh, s_vr.world_scale);
}

extern "C" void pc_vr_shutdown(void) {
    if (s_vr.sys) {
        pcvr_destroy_target(&s_vr.eye[0]);
        pcvr_destroy_target(&s_vr.eye[1]);
        pcvr_destroy_target(&s_vr.ui);
        if (s_vr.panel_prog) glDeleteProgram(s_vr.panel_prog);
        if (s_vr.panel_vao) glDeleteVertexArrays(1, &s_vr.panel_vao);
        if (s_vr.panel_vbo) glDeleteBuffers(1, &s_vr.panel_vbo);
        vr::VR_Shutdown();
        s_vr.sys = NULL;
    }
    s_vr.active = 0;
}

static void pcvr_drop_to_flat(const char* why) {
    printf("[VR] %s - dropping to flat mode\n", why);
    g_pc_vr_cull_expand = 0.0f;
    g_pc_vr_cull_znear_slack = 0.0f;
    g_pc_target_w = g_pc_window_w;
    g_pc_target_h = g_pc_window_h;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pc_vr_shutdown();
    /* restore flat pacing */
    g_frame_limiter = (u32)(g_pc_settings.max_fps > 0 ? g_pc_settings.max_fps : 60);
    if (g_pc_settings.vsync) SDL_GL_SetSwapInterval(1);
}

extern "C" int pc_vr_active(void) {
    return s_vr.active;
}

/* ---------------------------------------------------------------- */
/* Frame flow */

extern "C" void pc_vr_frame_begin(void) {
    if (!s_vr.active) return;

    /* Runtime events */
    vr::VREvent_t ev;
    while (s_vr.sys && s_vr.sys->PollNextEvent(&ev, sizeof(ev))) {
        switch (ev.eventType) {
        case vr::VREvent_Quit:
        case vr::VREvent_DriverRequestedQuit:
            s_vr.sys->AcknowledgeQuit_Exiting();
            pcvr_drop_to_flat("SteamVR quit requested");
            return;
        case vr::VREvent_IpdChanged:
            for (int e = 0; e < 2; e++) {
                vr::HmdMatrix34_t eth = s_vr.sys->GetEyeToHeadTransform(e == 0 ? vr::Eye_Left : vr::Eye_Right);
                m34_from_hmd(&eth, s_vr.eye_to_head[e]);
            }
            break;
        default:
            break;
        }
    }
    if (!s_vr.active) return;

    /* Input */
    if (s_vr.input_ready) {
        vr::VRActiveActionSet_t as;
        memset(&as, 0, sizeof(as));
        as.ulActionSet = s_vr.action_set;
        vr::VRInput()->UpdateActionState(&as, sizeof(as), 1);

        /* X+Y chord (or a user-bound recenter action): recenter seated origin */
        int chord = pcvr_digital(s_vr.act_x) && pcvr_digital(s_vr.act_y);
        int recenter_held = chord || pcvr_digital(s_vr.act_recenter);
        if (recenter_held && !s_vr.recenter_latch) {
            vr::VRChaperone()->ResetZeroPose(vr::TrackingUniverseSeated);
            s_vr.recenter_latch = 1;
        } else if (!recenter_held) {
            s_vr.recenter_latch = 0;
        }
    }

    /* Blocks until ~3ms before vsync; paces the whole loop at HMD rate */
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::EVRCompositorError cerr =
        vr::VRCompositor()->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
    if (cerr == vr::VRCompositorError_None &&
        poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
        m34_from_hmd(&poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking,
                     s_vr.head_pose);
        s_vr.have_pose = 1;
    }

    /* (H*E)^-1 per eye, then refresh the correction with last frame's view
     * (the view notify updates it again as soon as the game loads one). */
    for (int e = 0; e < 2; e++) {
        M34 he;
        m34_mul(s_vr.head_pose, s_vr.eye_to_head[e], he);
        m34_invert_rigid(he, s_vr.inv_eye_pose[e]);
    }
    pcvr_update_view_correction();

    /* Clear the UI layer (transparent) */
    pc_gx_draw_pending();
    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.ui.fbo);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    s_vr.submitted_this_frame = 0;
    s_vr.frame_index++;
}

extern "C" void pc_vr_begin_eye(int eye) {
    if (!s_vr.active) return;
    pc_gx_draw_pending();
    s_vr.in_scene_pass = 1;
    s_vr.current_eye = eye;
    s_vr.ui_bound = 0;
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
    glClearDepth(depth);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Per-eye matrices differ: every uniform group must re-upload to every
     * shader program this pass, and ortho routing starts on the eye FBO. */
    pc_gx_vr_reset_routing();
    pc_gx_mark_new_pass();
}

extern "C" int pc_vr_current_eye(void) {
    return s_vr.in_scene_pass ? s_vr.current_eye : -1;
}

extern "C" int pc_vr_in_scene_pass(void) {
    return s_vr.active && s_vr.in_scene_pass;
}

extern "C" const float* pc_vr_eye_projection(void) {
    return &s_vr.eye_projection[s_vr.current_eye == 1 ? 1 : 0][0][0];
}

extern "C" const float* pc_vr_view_correction(void) {
    return &s_vr.view_correction[s_vr.current_eye == 1 ? 1 : 0][0][0];
}

extern "C" unsigned int pc_vr_bind_ui_target(int ui) {
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

extern "C" int pc_vr_skip_ui_draws(void) {
    return s_vr.active && s_vr.in_scene_pass && s_vr.current_eye == 1;
}

extern "C" float pc_vr_world_scale(void) {
    return s_vr.world_scale;
}

extern "C" void pc_vr_end_scene_passes(void) {
    if (!s_vr.active) return;
    pc_gx_draw_pending();
    s_vr.in_scene_pass = 0;
    s_vr.current_eye = -1;
    s_vr.ui_bound = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_pc_target_w = g_pc_window_w;
    g_pc_target_h = g_pc_window_h;
}

/* Draw the UI texture as a panel into one eye FBO */
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

    /* Column-major upload with transpose flag (matrix is row-major) */
    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[eye].fbo);
    glViewport(0, 0, s_vr.eye[eye].w, s_vr.eye[eye].h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s_vr.panel_prog);
    glUniformMatrix4fv(s_vr.panel_u_mvp, 1, GL_TRUE, &m44[0][0]);
    float half_w = s_vr.ui_size * 0.5f;
    float half_h = half_w * 0.75f; /* 4:3 */
    glUniform2f(s_panel_u_half, half_w, half_h);
    glUniform3f(s_panel_u_center, 0.0f, -0.05f, -s_vr.ui_distance);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_vr.ui.color);
    glUniform1i(s_vr.panel_u_tex, 0);
    glBindVertexArray(s_vr.panel_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

extern "C" void pc_vr_compose_and_submit(void) {
    if (!s_vr.active) return;

    pcvr_draw_panel(0);
    pcvr_draw_panel(1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFlush();

    vr::Texture_t texL = { (void*)(uintptr_t)s_vr.eye[0].color,
                          vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::Texture_t texR = { (void*)(uintptr_t)s_vr.eye[1].color,
                          vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
    vr::EVRCompositorError e1 = vr::VRCompositor()->Submit(vr::Eye_Left, &texL);
    vr::EVRCompositorError e2 = vr::VRCompositor()->Submit(vr::Eye_Right, &texR);
    glFlush();

    if (e1 != vr::VRCompositorError_None || e2 != vr::VRCompositorError_None) {
        if (++s_vr.submit_fail_count == 1)
            printf("[VR] compositor submit error L=%d R=%d\n", (int)e1, (int)e2);
        if (s_vr.submit_fail_count > 300)
            pcvr_drop_to_flat("compositor keeps rejecting frames");
    } else {
        s_vr.submit_fail_count = 0;
    }
    s_vr.submitted_this_frame = 1;

    /* Leave GL in the state pc_gx expects */
    pc_gx_restore_after_nes();
    glEnable(GL_DEPTH_TEST);
}

/* Frames that bypass graph_task_set00 (NES minigames, boot screens) never
 * run the eye-pass loop. The compositor must still get a frame or SteamVR
 * fades to the grid — build a panel-only frame from the UI FBO (which the
 * NES renderer draws into) over cleared eye buffers. */
extern "C" void pc_vr_ensure_submitted(void) {
    if (!s_vr.active || s_vr.submitted_this_frame) return;
    pc_gx_draw_pending();
    for (int eye = 0; eye < 2; eye++) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_vr.eye[eye].fbo);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pc_vr_compose_and_submit();
}

extern "C" void pc_vr_mirror_to_window(void) {
    if (!s_vr.active) return;
    /* Aspect-fit the left eye into the window */
    int ww = g_pc_window_w, wh = g_pc_window_h;
    int ew = s_vr.eye[0].w, eh = s_vr.eye[0].h;
    if (ww <= 0 || wh <= 0 || ew <= 0 || eh <= 0) return;

    float scale = (float)ww / (float)ew;
    if ((float)eh * scale > (float)wh) scale = (float)wh / (float)eh;
    int dw = (int)(ew * scale), dh = (int)(eh * scale);
    int dx = (ww - dw) / 2, dy = (wh - dh) / 2;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, ww, wh);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_vr.eye[0].fbo);
    glBlitFramebuffer(0, 0, ew, eh, dx, dy, dx + dw, dy + dh,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pc_gx_viewport_state_invalidate();
}

/* ---------------------------------------------------------------- */
/* Input merge (called from PADRead) */

extern "C" void pc_vr_merge_pad(unsigned short* buttons,
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

    if (l_held && (fabsf(mx) > 0.5f || fabsf(my) > 0.5f)) {
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

    if (pcvr_digital(s_vr.act_a)) *buttons |= BTN_A;
    if (pcvr_digital(s_vr.act_b)) *buttons |= BTN_B;
    if (pcvr_digital(s_vr.act_x)) *buttons |= BTN_X;
    if (pcvr_digital(s_vr.act_y)) *buttons |= BTN_Y;
    if (pcvr_digital(s_vr.act_z)) *buttons |= BTN_Z;
    if (pcvr_digital(s_vr.act_start)) *buttons |= BTN_START;
    if (l_held) { *buttons |= BTN_L; *triggerL = 255; }
    if (pcvr_digital(s_vr.act_r)) { *buttons |= BTN_R; *triggerR = 255; }
}

extern "C" void pc_vr_rumble(int on) {
    if (!s_vr.active || !s_vr.input_ready || !on) return;
    vr::VRInput()->TriggerHapticVibrationAction(s_vr.act_haptic_l, 0.0f, 0.15f, 160.0f, 0.8f,
                                                vr::k_ulInvalidInputValueHandle);
    vr::VRInput()->TriggerHapticVibrationAction(s_vr.act_haptic_r, 0.0f, 0.15f, 160.0f, 0.8f,
                                                vr::k_ulInvalidInputValueHandle);
}

/* ---------------------------------------------------------------- */
/* NES integration: draw the NES frame onto the UI panel in VR */

static GLint s_nes_saved_viewport[4];

extern "C" void pc_vr_nes_begin_draw(void) {
    if (!s_vr.active) return;
    glGetIntegerv(GL_VIEWPORT, s_nes_saved_viewport);
    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.ui.fbo);
    g_pc_target_w = s_vr.ui.w;
    g_pc_target_h = s_vr.ui.h;
}

extern "C" void pc_vr_nes_end_draw(void) {
    if (!s_vr.active) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(s_nes_saved_viewport[0], s_nes_saved_viewport[1],
               (GLsizei)s_nes_saved_viewport[2], (GLsizei)s_nes_saved_viewport[3]);
    g_pc_target_w = g_pc_window_w;
    g_pc_target_h = g_pc_window_h;
}
