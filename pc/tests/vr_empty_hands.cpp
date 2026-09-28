/* The source under test is extracted verbatim; OpenVR/GL are narrow seams. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "openvr_capi.h"
#include "pc_settings.h"

typedef uint32_t u32;
typedef int GLint;
typedef unsigned int GLuint;
typedef float M34[3][4];
enum { GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING, GL_VIEWPORT,
       GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER };
static struct {
    int active, head_pose_valid, input_ready;
    VR_IVRInput_FnTable* input;
    VRActionHandle_t act_empty_hand[2];
    M34 empty_hand_pose[2], inv_eye_pose[2];
    int empty_hand_valid[2], empty_hands_available, empty_hands_render_failed;
    int empty_hand_seen[2], empty_hands_missing_logged;
    u32 empty_hands_stamp, active_since_ticks;
    struct { GLuint fbo; int w, h; } eye[2];
    float eye_projection[2][4][4];
} s_vr;
PCSettings g_pc_settings;
u32 pc_frame_counter;
int g_pc_paused;
static int fp_active, flat_scene, in_talk;
static u32 now_ticks;
static u32 SDL_GetTicks(void) { return now_ticks; }
static int checks, failures;
#define CHECK(test, label) do { ++checks; if (!(test)) { ++failures; \
    printf("FAIL %s (line %d)\n", label, __LINE__); } } while (0)
static int pc_fp_view_is_active(void) { return fp_active; }
static int pc_vr_flat_scene_active(void) { return flat_scene; }
static int pc_fp_in_talk(void) { return in_talk; }

static InputPoseActionData_t poses[2];
static EVRInputError pose_error[2];
static int pose_calls[2];
static VR_IVRInput_FnTable input_table;
static EVRInputError OPENVR_FNTABLE_CALLTYPE get_pose(
    VRActionHandle_t action, ETrackingUniverseOrigin origin,
    InputPoseActionData_t* output, uint32_t size, VRInputValueHandle_t device) {
    int hand = action == 101 ? 0 : 1;
    CHECK(action == 101 || action == 102, "pose request uses independently resolved handle");
    CHECK(origin == ETrackingUniverseOrigin_TrackingUniverseSeated, "pose uses seated reference space");
    CHECK(size == sizeof(*output) && device == k_ulInvalidInputValueHandle, "pose API contract");
    ++pose_calls[hand];
    *output = poses[hand];
    return pose_error[hand];
}

static GLint draw_fbo, read_fbo, viewport[4];
static int gl_changes, draw_calls, fail_draw_call, logs;
struct DrawRecord { int hand; GLint fbo, viewport[4]; float model[12], projection[16]; };
static DrawRecord draws[8];
static void glGetIntegerv(int name, GLint* out) {
    if (name == GL_VIEWPORT) memcpy(out, viewport, sizeof(viewport));
    else *out = name == GL_DRAW_FRAMEBUFFER_BINDING ? draw_fbo : read_fbo;
}
static void glBindFramebuffer(int target, GLuint fbo) {
    ++gl_changes;
    if (target == GL_DRAW_FRAMEBUFFER) draw_fbo = (GLint)fbo;
    else if (target == GL_READ_FRAMEBUFFER) read_fbo = (GLint)fbo;
    else CHECK(0, "known framebuffer target");
}
static void glViewport(int x, int y, int w, int h) {
    ++gl_changes;
    viewport[0] = x; viewport[1] = y; viewport[2] = w; viewport[3] = h;
}
static int pc_vr_hands_draw(const float* model, const float* projection, int hand) {
    DrawRecord* rec = &draws[draw_calls++];
    rec->hand = hand; rec->fbo = draw_fbo;
    memcpy(rec->viewport, viewport, sizeof(viewport));
    memcpy(rec->model, model, sizeof(rec->model));
    memcpy(rec->projection, projection, sizeof(rec->projection));
    return draw_calls != fail_draw_call;
}
static void pcvr_log(const char*) { ++logs; }

#include "runtime_source.inc"

static void reset(void) {
    memset(&s_vr, 0, sizeof(s_vr));
    memset(&g_pc_settings, 0, sizeof(g_pc_settings));
    memset(poses, 0, sizeof(poses));
    memset(pose_error, 0, sizeof(pose_error));
    memset(pose_calls, 0, sizeof(pose_calls));
    memset(draws, 0, sizeof(draws));
    memset(&input_table, 0, sizeof(input_table));
    input_table.GetPoseActionDataForNextFrame = get_pose;
    s_vr.input = &input_table;
    s_vr.active = s_vr.head_pose_valid = s_vr.input_ready = 1;
    s_vr.active_since_ticks = 1000;
    now_ticks = 2000;
    g_pc_settings.vr_empty_hands = fp_active = 1;
    g_pc_paused = flat_scene = in_talk = 0;
    pc_frame_counter = 42;
    pc_vr_set_empty_hands_available(1);
    for (int hand = 0; hand < 2; ++hand) {
        s_vr.act_empty_hand[hand] = 101 + hand;
        poses[hand].bActive = poses[hand].pose.bDeviceIsConnected = poses[hand].pose.bPoseIsValid = true;
        m34_identity(poses[hand].pose.mDeviceToAbsoluteTracking.m);
        poses[hand].pose.mDeviceToAbsoluteTracking.m[0][3] = hand ? .3f : -.3f;
        poses[hand].pose.mDeviceToAbsoluteTracking.m[1][3] = -.25f;
        poses[hand].pose.mDeviceToAbsoluteTracking.m[2][3] = -.7f;
        m34_identity(s_vr.inv_eye_pose[hand]);
        s_vr.inv_eye_pose[hand][0][3] = hand ? -.032f : .032f;
        s_vr.eye[hand].fbo = 80 + hand;
        s_vr.eye[hand].w = 1200 + hand;
        s_vr.eye[hand].h = 1300 + hand;
        for (int i = 0; i < 16; ++i) (&s_vr.eye_projection[hand][0][0])[i] = (float)(hand * 20 + i);
    }
    draw_fbo = 30; read_fbo = 31;
    viewport[0] = 3; viewport[1] = 4; viewport[2] = 640; viewport[3] = 480;
    gl_changes = draw_calls = fail_draw_call = logs = 0;
}

static void test_visibility(void) {
    reset();
    CHECK(pcvr_empty_hands_visible(), "fresh eligible player visible");
    pc_frame_counter++;
    CHECK(!pcvr_empty_hands_visible(), "missing player draw/loading/scene teardown cannot retain hands");
    pc_vr_set_empty_hands_available(1);
    CHECK(pcvr_empty_hands_visible(), "fresh player draw restores hands");
    pc_vr_set_empty_hands_available(0);
    CHECK(!pcvr_empty_hands_visible(), "current item/presentation/menu rejection overrides prior availability");
    pc_vr_set_empty_hands_available(-4);
    CHECK(pcvr_empty_hands_visible() && s_vr.empty_hands_available == 1, "bridge normalizes eligibility");
    int* gates[] = { &g_pc_settings.vr_empty_hands, &s_vr.active, &s_vr.head_pose_valid, &fp_active };
    for (unsigned i = 0; i < sizeof(gates)/sizeof(gates[0]); ++i) {
        *gates[i] = 0;
        CHECK(!pcvr_empty_hands_visible(), "disabled/session/headset/third-person gate hides hands");
        *gates[i] = 1;
    }
    int* blocks[] = { &g_pc_paused, &flat_scene, &in_talk };
    for (unsigned i = 0; i < sizeof(blocks)/sizeof(blocks[0]); ++i) {
        *blocks[i] = 1;
        CHECK(!pcvr_empty_hands_visible(), "pause/flat-menu-NES/dialogue gate hides hands");
        *blocks[i] = 0;
    }
    pc_frame_counter = UINT32_MAX;
    pc_vr_set_empty_hands_available(1);
    CHECK(pcvr_empty_hands_visible(), "unsigned frame wrap last frame valid");
    pc_frame_counter = 0;
    CHECK(!pcvr_empty_hands_visible(), "unsigned frame wrap still rejects old eligibility");
    pc_vr_set_empty_hands_available(1);
    CHECK(pcvr_empty_hands_visible(), "unsigned frame wrap fresh eligibility valid");
}

static void test_poses(void) {
    reset();
    pcvr_poll_empty_hands(1);
    CHECK(s_vr.empty_hand_valid[0] && s_vr.empty_hand_valid[1], "both current grip poses accepted");
    for (int hand = 0; hand < 2; ++hand)
        CHECK(memcmp(s_vr.empty_hand_pose[hand], poses[hand].pose.mDeviceToAbsoluteTracking.m, sizeof(M34)) == 0,
              "OpenVR row-major transform copied without tool orientation or scale");
    pcvr_poll_empty_hands(0);
    CHECK(!s_vr.empty_hand_valid[0] && !s_vr.empty_hand_valid[1], "failed action update invalidates both old poses");
    for (int hand = 0; hand < 2; ++hand) {
        for (int defect = 0; defect < 5; ++defect) {
            reset();
            pcvr_poll_empty_hands(1);
            if (defect == 0) poses[hand].bActive = false;
            if (defect == 1) poses[hand].pose.bDeviceIsConnected = false;
            if (defect == 2) poses[hand].pose.bPoseIsValid = false;
            if (defect == 3) pose_error[hand] = EVRInputError_VRInputError_InvalidHandle;
            if (defect == 4) s_vr.act_empty_hand[hand] = k_ulInvalidActionHandle;
            pcvr_poll_empty_hands(1);
            CHECK(!s_vr.empty_hand_valid[hand] && s_vr.empty_hand_valid[1-hand],
                  "invalid/unbound/disconnected hand hides independently without stale fallback");
        }
        for (int index = 0; index < 12; ++index) {
            reset();
            (&poses[hand].pose.mDeviceToAbsoluteTracking.m[0][0])[index] = index & 1 ? INFINITY : NAN;
            pcvr_poll_empty_hands(1);
            CHECK(!s_vr.empty_hand_valid[hand] && s_vr.empty_hand_valid[1-hand],
                  "all nonfinite pose matrix elements rejected independently");
        }
    }
    for (int gate = 0; gate < 3; ++gate) {
        reset();
        pcvr_poll_empty_hands(1);
        memset(pose_calls, 0, sizeof(pose_calls));
        if (gate == 0) s_vr.head_pose_valid = 0;
        if (gate == 1) s_vr.input_ready = 0;
        if (gate == 2) g_pc_settings.vr_empty_hands = 0;
        pcvr_poll_empty_hands(1);
        CHECK(!s_vr.empty_hand_valid[0] && !s_vr.empty_hand_valid[1], "disabled tracking gate clears both old poses");
        CHECK(!pose_calls[0] && !pose_calls[1], "disabled tracking gate makes no optional pose API call");
    }
    reset();
    pcvr_poll_empty_hands(1);
    poses[0].pose.bPoseIsValid = poses[1].pose.bPoseIsValid = false;
    pcvr_poll_empty_hands(1);
    CHECK(!s_vr.empty_hand_valid[0] && !s_vr.empty_hand_valid[1], "simultaneous controller tracking loss clears both poses");
    poses[0].pose.bPoseIsValid = poses[1].pose.bPoseIsValid = true;
    pcvr_poll_empty_hands(1);
    CHECK(s_vr.empty_hand_valid[0] && s_vr.empty_hand_valid[1], "both controller poses recover on next valid snapshot");
}

static void check_restored(void) {
    CHECK(draw_fbo == 30 && read_fbo == 31, "separate draw/read FBOs restored");
    CHECK(viewport[0] == 3 && viewport[1] == 4 && viewport[2] == 640 && viewport[3] == 480,
          "full viewport restored for UI/world renderer");
}

static void test_missing_pose_diagnostic(void) {
    reset();
    now_ticks = s_vr.active_since_ticks + 10000;
    pcvr_draw_empty_hands();
    CHECK(logs == 0 && !s_vr.empty_hands_missing_logged,
          "missing pose startup grace period includes exact ten-second boundary");
    now_ticks++;
    pcvr_draw_empty_hands();
    CHECK(logs == 1 && s_vr.empty_hands_missing_logged,
          "eligible player with poses never received gets one missing-binding diagnostic");
    CHECK(!draw_calls && !gl_changes, "missing-pose warning performs no renderer or GL work");
    now_ticks += 30000;
    pcvr_draw_empty_hands();
    CHECK(logs == 1, "missing pose diagnostic does not repeat across frames");

    for (int missing = 0; missing < 2; ++missing) {
        reset();
        poses[missing].bActive = false;
        pcvr_poll_empty_hands(1);
        CHECK(!s_vr.empty_hand_seen[missing] && s_vr.empty_hand_seen[1-missing],
              "poll records seen only for the independently valid pose");
        now_ticks = s_vr.active_since_ticks + 10001;
        pcvr_draw_empty_hands();
        CHECK(logs == 1 && draw_calls == 2 && !s_vr.empty_hands_render_failed,
              "one never-bound hand warns without hiding the working hand");
    }

    reset();
    pcvr_poll_empty_hands(1);
    CHECK(s_vr.empty_hand_seen[0] && s_vr.empty_hand_seen[1], "valid polling remembers both delivered poses");
    poses[0].bActive = poses[1].bActive = false;
    pcvr_poll_empty_hands(1);
    now_ticks = s_vr.active_since_ticks + 30000;
    pcvr_draw_empty_hands();
    CHECK(!logs && !s_vr.empty_hands_missing_logged && !draw_calls,
          "temporary tracking loss after both poses were seen does not report missing bindings");

    for (int block = 0; block < 5; ++block) {
        reset();
        now_ticks = s_vr.active_since_ticks + 30000;
        if (block == 0) g_pc_settings.vr_empty_hands = 0;
        if (block == 1) flat_scene = 1;
        if (block == 2) pc_vr_set_empty_hands_available(0);
        if (block == 3) pc_frame_counter++;
        if (block == 4) g_pc_paused = 1;
        pcvr_draw_empty_hands();
        CHECK(!logs && !s_vr.empty_hands_missing_logged,
              "disabled/menu/item/stale-player/pause state cannot produce missing-hand warning");
    }
}

static void test_draw(void) {
    reset();
    pcvr_poll_empty_hands(1);
    /* Rotate view in the second eye to check rotation AND translated basis. */
    s_vr.inv_eye_pose[1][0][0] = 0; s_vr.inv_eye_pose[1][0][2] = 1;
    s_vr.inv_eye_pose[1][2][0] = -1; s_vr.inv_eye_pose[1][2][2] = 0;
    pcvr_draw_empty_hands();
    CHECK(draw_calls == 4, "both hands drawn once in both eyes");
    check_restored();
    for (int eye = 0; eye < 2; ++eye) for (int hand = 0; hand < 2; ++hand) {
        DrawRecord* rec = &draws[eye*2 + hand];
        CHECK(rec->hand == hand, "left/right labels preserved for thumb mirroring");
        CHECK(rec->fbo == 80 + eye, "hand draw targets corresponding world eye FBO");
        CHECK(rec->viewport[0] == 0 && rec->viewport[1] == 0 &&
              rec->viewport[2] == 1200 + eye && rec->viewport[3] == 1300 + eye, "correct eye viewport");
        CHECK(memcmp(rec->projection, s_vr.eye_projection[eye], sizeof(rec->projection)) == 0,
              "uses corresponding world eye projection");
        float x = hand ? .3f : -.3f;
        CHECK(fabsf(rec->model[3] - (eye ? -.7f-.032f : x+.032f)) < 1e-6f &&
              fabsf(rec->model[7] + .25f) < 1e-6f &&
              fabsf(rec->model[11] - (eye ? -x : -.7f)) < 1e-6f, "eye-from-grip rigid composition in meters");
        CHECK(rec->model[eye ? 2 : 0] == 1 && rec->model[5] == 1 && rec->model[eye ? 8 : 10] == (eye ? -1 : 1),
              "hand orientation follows inverse-eye rotation, without tool local rotation");
    }
    for (int mask = 0; mask < 4; ++mask) {
        reset();
        pcvr_poll_empty_hands(1);
        s_vr.empty_hand_valid[0] = mask & 1;
        s_vr.empty_hand_valid[1] = mask & 2;
        pcvr_draw_empty_hands();
        CHECK(draw_calls == ((mask&1 ? 1 : 0) + (mask&2 ? 1 : 0))*2, "each available hand rendered independently");
        check_restored();
        if (!mask) CHECK(gl_changes == 0, "no tracked hands leaves GL untouched");
    }
    for (int block = 0; block < 4; ++block) {
        reset(); pcvr_poll_empty_hands(1);
        if (block == 0) pc_frame_counter++;
        if (block == 1) g_pc_paused = 1;
        if (block == 2) pc_vr_set_empty_hands_available(0);
        if (block == 3) s_vr.empty_hands_render_failed = 1;
        pcvr_draw_empty_hands();
        CHECK(!draw_calls && !gl_changes, "blocked hand draw makes no GL changes");
    }
    for (int eye = 0; eye < 2; ++eye) for (int index = 0; index < 12; ++index) {
        reset(); pcvr_poll_empty_hands(1);
        float* element = &s_vr.inv_eye_pose[eye][0][0] + index;
        float original = *element;
        *element = index & 1 ? INFINITY : NAN;
        pcvr_draw_empty_hands();
        CHECK(draw_calls == 2 && draws[0].fbo == 80 + (1-eye) && draws[1].fbo == 80 + (1-eye),
              "nonfinite inverse-eye transform never reaches renderer; other eye remains independent");
        CHECK(!s_vr.empty_hands_render_failed && logs == 0, "temporary malformed tracking never latches renderer failure");
        check_restored();
        *element = original;
        draw_calls = 0;
        pcvr_draw_empty_hands();
        CHECK(draw_calls == 4 && !s_vr.empty_hands_render_failed, "valid inverse-eye tracking immediately restores both eyes");
        check_restored();
    }
    reset(); pcvr_poll_empty_hands(1);
    s_vr.inv_eye_pose[0][0][0] = s_vr.inv_eye_pose[1][0][0] = NAN;
    pcvr_draw_empty_hands();
    CHECK(draw_calls == 0 && !s_vr.empty_hands_render_failed, "bad headset transform in both eyes hides hands for this frame only");
    check_restored();
    for (int fail_at = 1; fail_at <= 4; ++fail_at) {
        reset(); pcvr_poll_empty_hands(1); fail_draw_call = fail_at;
        pcvr_draw_empty_hands();
        CHECK(draw_calls == fail_at && s_vr.empty_hands_render_failed && logs == 1,
              "optional renderer failure stops both-eye drawing and logs once");
        check_restored();
        pcvr_draw_empty_hands();
        CHECK(draw_calls == fail_at && logs == 1, "renderer failure remains optional, no repeated GL/error work");
    }
}

int main(void) {
    test_visibility(); test_poses(); test_draw(); test_missing_pose_diagnostic();
    printf("Empty-hand runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
