/* pc_vr.cpp - SteamVR (OpenVR) backend: stereo FBOs, tracking, compositor
 * submit, UI panel composite, and Touch-controller input.
 *
 * Uses OpenVR's flat "FnTable" C interface (openvr_capi.h) instead of the
 * C++ interfaces: openvr_api.dll is MSVC-built, and MSVC<->MinGW C++ member
 * calls (thiscall vtables, by-value aggregate returns) are not a contract we
 * can rely on from a MinGW i686 build. FnTable methods are plain __stdcall
 * function pointers — compiler-agnostic by design. The exported entry points
 * (VR_InitInternal etc.) are plain C and safe either way.
 *
 * All calls run on the main/render thread.
 *
 * Spaces and conventions (see pc/VR_ARCHITECTURE.md):
 *   - Game view matrices are row-major 3x4, right-handed, -Z forward,
 *     game units (1 tile = 40 units).
 *   - OpenVR poses are row-major 3x4 (HmdMatrix34_t), meters, -Z forward.
 *   - The seated tracking origin is equated with a *leveled* (yaw-only)
 *     frame at the game camera: W = T_height * Scale * Anchor maps game
 *     world -> seated meters. Per eye: V_vr = (H*E)^-1 * W, and the
 *     correction applied to the game's combined view*model position matrices
 *     is X = V_vr * V_game^-1.
 *   - Projections are kept in the GX depth convention (near -> NDC -1,
 *     far -> NDC 0) to match the rest of pc_gx.
 */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_diag.h"
#include "pc_vr.h"
#include "pc_fp_camera.h"

#include "openvr_capi.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern "C" {
void pc_gx_draw_pending(void);
void pc_gx_flush_if_begin_complete(void);
void pc_gx_restore_after_nes(void);
void pc_gx_viewport_state_invalidate(void);
void pc_gx_mark_new_pass(void);
void pc_gx_vr_reset_routing(void);
void pc_gx_dirty_colormask(void);
void pc_gx_get_clear(float* rgba, float* depth);
extern int g_pc_target_w, g_pc_target_h;   /* pc_gx.c: current render target dims */
}

/* openvr_capi.h ships its global entry-point declarations disabled (#if 0);
 * consumers declare them. These are plain C exports of openvr_api.dll. */
extern "C" {
S_API intptr_t VR_InitInternal(EVRInitError* peError, EVRApplicationType eType);
S_API void VR_ShutdownInternal(void);
S_API bool VR_IsHmdPresent(void);
S_API intptr_t VR_GetGenericInterface(const char* pchInterfaceVersion, EVRInitError* peError);
S_API bool VR_IsRuntimeInstalled(void);
S_API const char* VR_GetVRInitErrorAsSymbol(EVRInitError error);
S_API const char* VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);
}

/* ---------------------------------------------------------------- */
/* Init log: always written next to the exe (stdout is NUL'd in the
 * default GUI build, which made the first field failure undiagnosable). */

static FILE* s_vr_logf = NULL;

static void pcvr_log(const char* fmt, ...) {
    va_list ap;
    if (s_vr_logf) {
        va_start(ap, fmt);
        vfprintf(s_vr_logf, fmt, ap);
        va_end(ap);
        fputc('\n', s_vr_logf);
        fflush(s_vr_logf);
    }
    va_start(ap, fmt);
    printf("[VR] ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
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

/* Inverse of rotation + UNIFORM scale + translation: M = [sR | t]. */
static void m34_invert_rs(const M34 m, M34 out) {
    float s2 = m[0][0] * m[0][0] + m[1][0] * m[1][0] + m[2][0] * m[2][0];
    float k = (s2 > 1e-12f) ? 1.0f / s2 : 1.0f;   /* (sR)^T / s^2 = R^T / s */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = m[j][i] * k;
    for (int i = 0; i < 3; i++)
        out[i][3] = -(out[i][0] * m[0][3] + out[i][1] * m[1][3] + out[i][2] * m[2][3]);
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

static void m34_from_hmd(const HmdMatrix34_t* src, M34 out) {
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
    int runtime_up;             /* VR_InitInternal succeeded */

    /* FnTable interfaces (flat C, __stdcall) */
    struct VR_IVRSystem_FnTable*     sys;
    struct VR_IVRCompositor_FnTable* comp;
    struct VR_IVRInput_FnTable*      input;
    struct VR_IVRChaperone_FnTable*  chap;

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
    float ui_dist_k;            /* smoothed panel pull-in (1.0 <-> 0.6 in FP) */

    /* UI composite GL objects */
    GLuint panel_prog;
    GLuint panel_vao, panel_vbo;
    GLint  panel_u_mvp, panel_u_tex;

    /* Input */
    VRActionSetHandle_t action_set;
    VRActionHandle_t act_move, act_camera;
    VRActionHandle_t act_a, act_b, act_x, act_y, act_l, act_r, act_z, act_start;
    VRActionHandle_t act_recenter;
    VRActionHandle_t act_hand_r;
    VRActionHandle_t act_haptic_l, act_haptic_r;
    int input_ready;
    int recenter_latch;

    /* Motion tools: right-controller pose in seated space + derived state */
    M34 world_from_seated;      /* W^-1, updated with the view correction */
    int hand_valid;             /* pose valid this frame */
    M34 hand_world;             /* tool anchor in game-world units */
    M34 tool_local;             /* controller-tip -> tool grip orientation */
    Uint32 swing_hi_since;      /* ticks when speed first exceeded threshold (0 = below) */
    Uint32 swing_block_until;   /* refractory deadline, ms ticks */
    int swing_pulse;            /* inject A this frame */
    int hand_ever_valid;        /* pose has been valid at least once */
    int hand_missing_logged;    /* one-shot diagnostic for unbound pose */
    Uint32 active_since_ticks;  /* when the session went active */
    u32 flat_scene_stamp;       /* pc_frame_counter when flat-scene last stamped */

    /* Frame-timing telemetry (vr_log.txt every ~30s) */
    uint32_t t_last_frame_index;
    unsigned int t_frames;
    float t_gpu_sum, t_gpu_max;
    unsigned int t_drops, t_mispresent;
    Uint32 t_last_log_ticks;

    int submit_fail_count;
    int submitted_this_frame;
    int logged_first_submit;
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

static void pcvr_update_eye_projection(int eye);
static void pcvr_update_view_correction(void);

/* First person uses a life-size scale; the diorama scale returns with the
 * normal camera. Rebuilds projections (far plane derives from scale). */
extern "C" void pc_vr_set_fp_scale(int fp_active) {
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

/* ---------------------------------------------------------------- */
/* FnTable acquisition */

static void* pcvr_get_fntable(const char* version) {
    char name[128];
    snprintf(name, sizeof(name), "FnTable:%s", version);
    EVRInitError err = EVRInitError_VRInitError_None;
    intptr_t p = VR_GetGenericInterface(name, &err);
    if (err != EVRInitError_VRInitError_None || p == 0) {
        pcvr_log("GetGenericInterface(%s) failed: %d", name, (int)err);
        return NULL;
    }
    return (void*)p;
}

/* ---------------------------------------------------------------- */
/* Input actions */

static void pcvr_input_init(void) {
    s_vr.input_ready = 0;
    if (!s_vr.input) return;

    char manifest[MAX_PATH];
    if (!GetFullPathNameA("vr_actions\\actionmanifest.json", sizeof(manifest), manifest, NULL))
        return;
    FILE* f = fopen(manifest, "rb");
    if (!f) {
        pcvr_log("no action manifest at %s - VR controllers disabled", manifest);
        return;
    }
    fclose(f);

    EVRInputError err = s_vr.input->SetActionManifestPath((char*)manifest);
    if (err != EVRInputError_VRInputError_None) {
        pcvr_log("SetActionManifestPath failed: %d", (int)err);
        return;
    }

    s_vr.input->GetActionSetHandle((char*)"/actions/main", &s_vr.action_set);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/move",     &s_vr.act_move);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/camera",   &s_vr.act_camera);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/a",        &s_vr.act_a);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/b",        &s_vr.act_b);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/x",        &s_vr.act_x);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/y",        &s_vr.act_y);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/l",        &s_vr.act_l);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/r",        &s_vr.act_r);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/z",        &s_vr.act_z);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/start",    &s_vr.act_start);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/recenter", &s_vr.act_recenter);
    s_vr.input->GetActionHandle((char*)"/actions/main/in/hand_right", &s_vr.act_hand_r);
    s_vr.input->GetActionHandle((char*)"/actions/main/out/haptic_left",  &s_vr.act_haptic_l);
    s_vr.input->GetActionHandle((char*)"/actions/main/out/haptic_right", &s_vr.act_haptic_r);
    s_vr.input_ready = 1;
    pcvr_log("input actions ready (%s)", manifest);
}

static int pcvr_digital(VRActionHandle_t h) {
    if (!s_vr.input_ready || h == k_ulInvalidActionHandle) return 0;
    InputDigitalActionData_t d;
    if (s_vr.input->GetDigitalActionData(h, &d, sizeof(d), k_ulInvalidInputValueHandle)
            != EVRInputError_VRInputError_None)
        return 0;
    return d.bActive && d.bState;
}

static void pcvr_analog(VRActionHandle_t h, float* x, float* y) {
    *x = 0.0f; *y = 0.0f;
    if (!s_vr.input_ready || h == k_ulInvalidActionHandle) return;
    InputAnalogActionData_t d;
    if (s_vr.input->GetAnalogActionData(h, &d, sizeof(d), k_ulInvalidInputValueHandle)
            != EVRInputError_VRInputError_None)
        return;
    if (!d.bActive) return;
    *x = d.x; *y = d.y;
}

/* ---------------------------------------------------------------- */
/* Matrix pipeline */

static void pcvr_update_eye_projection(int eye) {
    HmdMatrix44_t p = s_vr.sys->GetProjectionMatrix(
        eye == 0 ? EVREye_Eye_Left : EVREye_Eye_Right, PC_VR_NEAR_M, pcvr_far_m());

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

    /* Motion tools need seated-meters -> game-world (W^-1) */
    m34_invert_rs(W, s_vr.world_from_seated);
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

    s_vr_logf = fopen("vr_log.txt", "w");
    pcvr_log("init: vr_mode=%d scale=%.4f (openvr FnTable backend)",
             s_vr.requested, s_vr.world_scale);

    if (!VR_IsRuntimeInstalled()) {
        pcvr_log("SteamVR is not installed - running flat");
        if (s_vr.requested == 2)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Animal Crossing VR",
                "SteamVR is not installed.\nInstall SteamVR from Steam, then relaunch.", g_pc_window);
        return;
    }
    if (s_vr.requested == 1 && !VR_IsHmdPresent()) {
        pcvr_log("no HMD detected - running flat (use --vr to force)");
        return;
    }

    EVRInitError err = EVRInitError_VRInitError_None;
    VR_InitInternal(&err, EVRApplicationType_VRApplication_Scene);
    if (err != EVRInitError_VRInitError_None) {
        pcvr_log("VR_InitInternal failed: %s",
                 VR_GetVRInitErrorAsEnglishDescription(err));
        if (s_vr.requested == 2)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Animal Crossing VR",
                "Could not start SteamVR.\nIs the headset connected?", g_pc_window);
        return;
    }
    s_vr.runtime_up = 1;
    pcvr_log("runtime initialized");

    s_vr.sys   = (struct VR_IVRSystem_FnTable*)pcvr_get_fntable(IVRSystem_Version);
    s_vr.comp  = (struct VR_IVRCompositor_FnTable*)pcvr_get_fntable(IVRCompositor_Version);
    s_vr.input = (struct VR_IVRInput_FnTable*)pcvr_get_fntable(IVRInput_Version);
    s_vr.chap  = (struct VR_IVRChaperone_FnTable*)pcvr_get_fntable(IVRChaperone_Version);
    if (!s_vr.sys || !s_vr.comp) {
        pcvr_log("required interfaces unavailable (sys=%p comp=%p) - running flat",
                 (void*)s_vr.sys, (void*)s_vr.comp);
        VR_ShutdownInternal();
        s_vr.runtime_up = 0;
        return;
    }
    pcvr_log("interfaces: sys/comp ok, input=%s, chaperone=%s",
             s_vr.input ? "ok" : "missing", s_vr.chap ? "ok" : "missing");

    s_vr.comp->SetTrackingSpace(ETrackingUniverseOrigin_TrackingUniverseSeated);

    uint32_t rw = 0, rh = 0;
    s_vr.sys->GetRecommendedRenderTargetSize(&rw, &rh);
    pcvr_log("recommended render target: %ux%u", rw, rh);
    if (rw < 640 || rw > 8192) rw = 1440;
    if (rh < 480 || rh > 8192) rh = 1584;

    if (!pcvr_create_target(&s_vr.eye[0], (int)rw, (int)rh) ||
        !pcvr_create_target(&s_vr.eye[1], (int)rw, (int)rh) ||
        !pcvr_create_target(&s_vr.ui, 1280, 960) ||
        !pcvr_create_panel_gl()) {
        pcvr_log("GL resource creation failed - running flat");
        pcvr_destroy_target(&s_vr.eye[0]);
        pcvr_destroy_target(&s_vr.eye[1]);
        pcvr_destroy_target(&s_vr.ui);
        VR_ShutdownInternal();
        s_vr.runtime_up = 0;
        return;
    }

    for (int e = 0; e < 2; e++) {
        HmdMatrix34_t eth = s_vr.sys->GetEyeToHeadTransform(e == 0 ? EVREye_Eye_Left : EVREye_Eye_Right);
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

    /* Controller-tip -> tool-grip orientation: tools extend along the hand
     * matrix's +Z, the SteamVR tip pose points along -Z, so flip about Y,
     * then apply the user's pitch trim about X. */
    {
        /* Negated so positive vr_tool_pitch tilts the tool tip upward
         * (the flip about Y inverts the local X rotation sense) */
        float p = -(float)g_pc_settings.vr_tool_pitch * (float)(PC_PI / 180.0);
        float cp2 = cosf(p), sp2 = sinf(p);
        M34 flip, pitch;
        m34_identity(flip);
        flip[0][0] = -1.0f;
        flip[2][2] = -1.0f;
        m34_identity(pitch);
        pitch[1][1] = cp2;  pitch[1][2] = -sp2;
        pitch[2][1] = sp2;  pitch[2][2] = cp2;
        m34_mul(flip, pitch, s_vr.tool_local);
    }
    m34_identity(s_vr.world_from_seated);
    s_vr.t_last_log_ticks = SDL_GetTicks();
    s_vr.active_since_ticks = s_vr.t_last_log_ticks;
    s_vr.flat_scene_stamp = (u32)-1000;

    s_vr.active = 1;
    s_vr.current_eye = -1;
    pcvr_log("ACTIVE: eye %ux%u, world scale %.4f m/unit", rw, rh, s_vr.world_scale);

    /* Starting in first person: switch to the life-size scale now */
    if (g_pc_fp_mode) {
        pc_vr_set_fp_scale(1);
    }
}

extern "C" void pc_vr_shutdown(void) {
    if (s_vr.runtime_up) {
        pcvr_destroy_target(&s_vr.eye[0]);
        pcvr_destroy_target(&s_vr.eye[1]);
        pcvr_destroy_target(&s_vr.ui);
        if (s_vr.panel_prog) glDeleteProgram(s_vr.panel_prog);
        if (s_vr.panel_vao) glDeleteVertexArrays(1, &s_vr.panel_vao);
        if (s_vr.panel_vbo) glDeleteBuffers(1, &s_vr.panel_vbo);
        VR_ShutdownInternal();
        s_vr.runtime_up = 0;
        s_vr.sys = NULL; s_vr.comp = NULL; s_vr.input = NULL; s_vr.chap = NULL;
    }
    s_vr.active = 0;
    if (s_vr_logf) {
        fclose(s_vr_logf);
        s_vr_logf = NULL;
    }
}

static void pcvr_drop_to_flat(const char* why) {
    pcvr_log("%s - dropping to flat mode", why);
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
    struct VREvent_t ev;
    while (s_vr.sys && s_vr.sys->PollNextEvent(&ev, sizeof(ev))) {
        switch (ev.eventType) {
        case EVREventType_VREvent_Quit:
        case EVREventType_VREvent_DriverRequestedQuit:
            s_vr.sys->AcknowledgeQuit_Exiting();
            pcvr_drop_to_flat("SteamVR quit requested");
            return;
        case EVREventType_VREvent_IpdChanged:
            for (int e = 0; e < 2; e++) {
                HmdMatrix34_t eth = s_vr.sys->GetEyeToHeadTransform(e == 0 ? EVREye_Eye_Left : EVREye_Eye_Right);
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
        VRActiveActionSet_t as;
        memset(&as, 0, sizeof(as));
        as.ulActionSet = s_vr.action_set;
        s_vr.input->UpdateActionState(&as, sizeof(as), 1);

        /* X+Y chord (or a user-bound recenter action): recenter seated origin */
        int chord = pcvr_digital(s_vr.act_x) && pcvr_digital(s_vr.act_y);
        int recenter_held = chord || pcvr_digital(s_vr.act_recenter);
        if (recenter_held && !s_vr.recenter_latch) {
            if (s_vr.chap)
                s_vr.chap->ResetZeroPose(ETrackingUniverseOrigin_TrackingUniverseSeated);
            s_vr.recenter_latch = 1;
        } else if (!recenter_held) {
            s_vr.recenter_latch = 0;
        }
    }

    /* Blocks until ~3ms before vsync; paces the whole loop at HMD rate */
    struct TrackedDevicePose_t poses[64]; /* k_unMaxTrackedDeviceCount */
    EVRCompositorError cerr =
        s_vr.comp->WaitGetPoses(poses, k_unMaxTrackedDeviceCount, NULL, 0);
    if (cerr == EVRCompositorError_VRCompositorError_None &&
        poses[k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
        m34_from_hmd(&poses[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking,
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

    /* --- Motion tools: right-controller pose + swing gesture --- */
    s_vr.hand_valid = 0;
    if (s_vr.swing_pulse > 0) s_vr.swing_pulse--;
    if (s_vr.input_ready && s_vr.act_hand_r != k_ulInvalidActionHandle) {
        InputPoseActionData_t pd;
        if (s_vr.input->GetPoseActionDataForNextFrame(
                s_vr.act_hand_r, ETrackingUniverseOrigin_TrackingUniverseSeated,
                &pd, sizeof(pd), k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None &&
            pd.bActive && pd.pose.bPoseIsValid) {

            M34 hand_seated, t1;
            m34_from_hmd(&pd.pose.mDeviceToAbsoluteTracking, hand_seated);
            m34_mul(s_vr.world_from_seated, hand_seated, t1); /* seated -> game world */
            m34_mul(t1, s_vr.tool_local, s_vr.hand_world);    /* grip orientation */
            /* W^-1 left a 1/world_scale in the basis; the matrix this
             * replaces (right_hand_mtx) carries the player's 0.01 actor
             * scale that tool models are authored for. Rebase the 3x3 to
             * 0.01 (translation stays in world units). */
            {
                float k = 0.01f * s_vr.world_scale;
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++)
                        s_vr.hand_world[r][c] *= k;
            }
            s_vr.hand_valid = 1;
            s_vr.hand_ever_valid = 1;

            /* Unified swing gesture: sustained fast controller motion fires
             * one A press (tool use). Time-based thresholds so behavior is
             * identical at 72/90/120 Hz: ~22ms sustained rejects tracking
             * spikes, 350ms refractory stops re-triggers within one swing
             * arc. Only with a swingable tool out, in FP, not during
             * conversations (A would advance dialogue). */
            if (g_pc_settings.vr_motion_swing && pc_fp_view_is_active() &&
                pc_fp_swingable_equipped() && !pc_fp_in_talk()) {
                float vx = pd.pose.vVelocity.v[0];
                float vy = pd.pose.vVelocity.v[1];
                float vz = pd.pose.vVelocity.v[2];
                float speed = sqrtf(vx * vx + vy * vy + vz * vz);
                Uint32 tnow = SDL_GetTicks();
                if (speed > 2.2f) {
                    if (s_vr.swing_hi_since == 0) s_vr.swing_hi_since = tnow;
                    if (tnow - s_vr.swing_hi_since >= 22 && tnow >= s_vr.swing_block_until) {
                        s_vr.swing_pulse = 2;      /* consumed by next PADRead */
                        s_vr.swing_block_until = tnow + 350;
                        s_vr.swing_hi_since = 0;
                    }
                } else {
                    s_vr.swing_hi_since = 0;
                }
            } else {
                s_vr.swing_hi_since = 0;
                /* A pulse must not carry into a conversation a villager
                 * just started — it would skip the opening dialogue page */
                if (pc_fp_in_talk()) s_vr.swing_pulse = 0;
            }
        }
    }

    /* One-shot setup diagnostic: motion tools enabled but the pose action
     * never bound (stale vr_actions folder or a custom binding without the
     * pose). Points straight at the fix instead of failing silently. */
    if (!s_vr.hand_ever_valid && !s_vr.hand_missing_logged && s_vr.input_ready &&
        (g_pc_settings.vr_tool_on_hand || g_pc_settings.vr_motion_swing) &&
        SDL_GetTicks() - s_vr.active_since_ticks > 10000) {
        s_vr.hand_missing_logged = 1;
        pcvr_log("right-hand pose not delivering yet (10s) - motion tools waiting. "
                 "Normal if the controllers are asleep or you were in the dashboard. "
                 "If tools never follow your hand: update the vr_actions folder next "
                 "to the exe, or re-select the default binding once in SteamVR.");
    }

    /* --- Frame-timing telemetry: one vr_log line every ~30s --- */
    {
        Compositor_FrameTiming ft;
        ft.m_nSize = sizeof(ft);
        if (s_vr.comp->GetFrameTiming(&ft, 1) && ft.m_nFrameIndex != s_vr.t_last_frame_index) {
            s_vr.t_last_frame_index = ft.m_nFrameIndex;
            s_vr.t_frames++;
            s_vr.t_gpu_sum += ft.m_flTotalRenderGpuMs;
            if (ft.m_flTotalRenderGpuMs > s_vr.t_gpu_max) s_vr.t_gpu_max = ft.m_flTotalRenderGpuMs;
            s_vr.t_drops += ft.m_nNumDroppedFrames;
            s_vr.t_mispresent += ft.m_nNumMisPresented;
        }
        Uint32 now_ticks = SDL_GetTicks();
        if (now_ticks - s_vr.t_last_log_ticks >= 30000 && s_vr.t_frames > 0) {
            pcvr_log("timing: %.0fs frames=%u fps=%.1f gpu_avg=%.2fms gpu_max=%.2fms drops=%u mispresent=%u",
                     (now_ticks - s_vr.t_last_log_ticks) / 1000.0f, s_vr.t_frames,
                     s_vr.t_frames * 1000.0f / (float)(now_ticks - s_vr.t_last_log_ticks),
                     s_vr.t_gpu_sum / (float)s_vr.t_frames, s_vr.t_gpu_max,
                     s_vr.t_drops, s_vr.t_mispresent);
            s_vr.t_frames = 0;
            s_vr.t_gpu_sum = 0.0f;
            s_vr.t_gpu_max = 0.0f;
            s_vr.t_drops = 0;
            s_vr.t_mispresent = 0;
            s_vr.t_last_log_ticks = now_ticks;
        }
    }

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

/* Yaw of the headset gaze relative to the FP anchor, in binary-angle units.
 * Positive = turned left, matching the game's yaw sense. Lets movement (and
 * A-targeting via getCamera2AngleY) follow where you're LOOKING, not just
 * where the stick-turned anchor points. Head turned left by d: RotY(+d),
 * so H[0][2]=sin d, H[2][2]=cos d -> atan2 recovers d. */
extern "C" float pc_vr_head_yaw_offset_bang(void) {
    if (!s_vr.active || !s_vr.have_pose) return 0.0f;
    return atan2f(s_vr.head_pose[0][2], s_vr.head_pose[2][2])
         * (65536.0f / (float)(2.0 * PC_PI));
}

/* Flat-scene: frame-stamped like the FP active flag so scene teardown can
 * never leave it latched (m_play stamps it every play frame). */
extern u32 pc_frame_counter;

extern "C" void pc_vr_set_flat_scene(int on) {
    s_vr.flat_scene_stamp = on ? pc_frame_counter : (u32)(pc_frame_counter - 1000u);
}

extern "C" int pc_vr_flat_scene_active(void) {
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
    glUniformMatrix4fv(s_vr.panel_u_mvp, 1, GL_TRUE, &m44[0][0]);
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

extern "C" void pc_vr_compose_and_submit(void) {
    if (!s_vr.active) return;

    /* Smooth the FP panel pull-in once per frame (shared by both eyes) so
     * conversation-phase changes ease instead of popping */
    {
        float target = pc_fp_view_is_active() ? 0.6f : 1.0f;
        if (s_vr.ui_dist_k <= 0.0f) s_vr.ui_dist_k = 1.0f;
        s_vr.ui_dist_k += (target - s_vr.ui_dist_k) * 0.2f;
        if (fabsf(target - s_vr.ui_dist_k) < 0.01f) s_vr.ui_dist_k = target;
    }

    pcvr_draw_panel(0);
    pcvr_draw_panel(1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFlush();

    struct Texture_t texL;
    texL.handle = (void*)(uintptr_t)s_vr.eye[0].color;
    texL.eType = ETextureType_TextureType_OpenGL;
    texL.eColorSpace = EColorSpace_ColorSpace_Gamma;
    struct Texture_t texR = texL;
    texR.handle = (void*)(uintptr_t)s_vr.eye[1].color;

    EVRCompositorError e1 = s_vr.comp->Submit(EVREye_Eye_Left, &texL, NULL,
                                              EVRSubmitFlags_Submit_Default);
    EVRCompositorError e2 = s_vr.comp->Submit(EVREye_Eye_Right, &texR, NULL,
                                              EVRSubmitFlags_Submit_Default);
    glFlush();

    if (!s_vr.logged_first_submit) {
        s_vr.logged_first_submit = 1;
        pcvr_log("first frame submitted: L=%d R=%d", (int)e1, (int)e2);
    }

    if (e1 != EVRCompositorError_VRCompositorError_None ||
        e2 != EVRCompositorError_VRCompositorError_None) {
        if (++s_vr.submit_fail_count == 1)
            pcvr_log("compositor submit error L=%d R=%d", (int)e1, (int)e2);
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
    if (s_vr.swing_pulse > 0) *buttons |= BTN_A;

    if (pcvr_digital(s_vr.act_a)) *buttons |= BTN_A;
    if (pcvr_digital(s_vr.act_b)) *buttons |= BTN_B;
    if (pcvr_digital(s_vr.act_x)) *buttons |= BTN_X;
    if (y_now && !l_held) *buttons |= BTN_Y;
    if (pcvr_digital(s_vr.act_z)) *buttons |= BTN_Z;
    if (pcvr_digital(s_vr.act_start)) *buttons |= BTN_START;
    if (l_held) { *buttons |= BTN_L; *triggerL = 255; }
    if (pcvr_digital(s_vr.act_r)) { *buttons |= BTN_R; *triggerR = 255; }
}

/* Tool anchor for the item draw: row-major 3x4 (MtxF top rows), game-world
 * units. Returns 0 when the animated hand should be used instead. */
extern "C" int pc_vr_hand_tool_mtx(float out[12]) {
    if (!s_vr.active || !s_vr.hand_valid || !g_pc_settings.vr_tool_on_hand)
        return 0;
    if (!pc_fp_view_is_active())
        return 0;
    memcpy(out, s_vr.hand_world, sizeof(float) * 12);
    return 1;
}

extern "C" void pc_vr_rumble(int on) {
    if (!s_vr.active || !s_vr.input_ready || !on) return;
    s_vr.input->TriggerHapticVibrationAction(s_vr.act_haptic_l, 0.0f, 0.15f, 160.0f, 0.8f,
                                             k_ulInvalidInputValueHandle);
    s_vr.input->TriggerHapticVibrationAction(s_vr.act_haptic_r, 0.0f, 0.15f, 160.0f, 0.8f,
                                             k_ulInvalidInputValueHandle);
}

/* ---------------------------------------------------------------- */
/* NES integration: draw the NES frame onto the UI panel in VR */

static GLint s_nes_saved_viewport[4];

extern "C" void pc_vr_nes_begin_draw(void) {
    if (!s_vr.active) return;
    glGetIntegerv(GL_VIEWPORT, s_nes_saved_viewport);
    glBindFramebuffer(GL_FRAMEBUFFER, s_vr.ui.fbo);
    /* The game DL path may have left alpha writes masked off; the panel
     * needs coverage alpha from the NES quad. */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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
    /* We touched the color mask behind the dirty system */
    pc_gx_dirty_colormask();
}
