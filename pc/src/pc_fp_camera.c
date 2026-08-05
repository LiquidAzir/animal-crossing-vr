/* pc_fp_camera.c - first-person camera mode (flat + VR).
 *
 * Angle convention: atans_table(a, b) returns theta with a = r*cos(theta),
 * b = r*sin(theta) — the REVERSE argument order of C's atan2 (see
 * sys_math_atan.c and m_controller.c's cos_s/sin_s reconstruction).
 * Camera2_DirectionCalc does direction.y = atans_table(back.z, back.x)
 * + 180 deg, and movement integrates x += sin_s(angle), z += cos_s(angle)
 * (m_actor.c:60). So the FORWARD vector for yaw theta is
 * (sin theta, 0, cos theta); yaw initializes directly from direction.y at
 * toggle time and getCamera2AngleY can return our yaw verbatim, keeping the
 * stick->walk-direction formula (m_player_common.c_inc:2261) exact.
 */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_fp_camera.h"
#include "pc_vr.h"
#include <math.h>

/* merged C-stick (keyboard + gamepad + VR), set by PADRead in pc_pad.c */
extern int g_pc_substick_x;
extern int g_pc_substick_y;

int g_pc_fp_mode = 0;

/* Written by Player_actor_Item_draw each frame: the player's current item
 * main index (mPlayer_ITEM_MAIN_*), -1 when nothing is out. */
int g_pc_item_main_index_now = -1;

/* Swingable ranges from the mPlayer_ITEM_MAIN_* enum (m_player.h:445-471):
 * axe 1, net 2-7 + 9, rod 11-18, shovel 20. The putaway states — NET (8)
 * and ROD (19) — are excluded so a swing can't re-trigger mid-stow. */
int pc_fp_swingable_equipped(void) {
    int i = g_pc_item_main_index_now;
    return (i >= 1 && i <= 7) || (i == 9) || (i >= 11 && i <= 18) || (i == 20);
}

/* Camera is in TALK mode (set by the m_camera2 hook): the swing gesture
 * must not fire — A would advance dialogue. */
static int s_in_talk;

void pc_fp_set_in_talk(int in_talk) {
    s_in_talk = in_talk;
}

int pc_fp_in_talk(void) {
    return s_in_talk;
}

#define FP_BANG_PER_DEG   (65536.0f / 360.0f)
#define FP_TURN_RATE_BANG 380.0f   /* per 60Hz frame at full stick (~2.1 deg) */
#define FP_PITCH_RATE     0.020f   /* radians per 60Hz frame at full stick */
#define FP_PITCH_CLAMP    1.15f    /* ~66 deg up/down */
#define FP_SNAP_ON        0.60f    /* stick thresholds for VR snap turning */
#define FP_SNAP_OFF       0.35f

static float s_yaw;          /* forward angle, binary-angle units [0, 65536) */
static float s_pitch;        /* radians, positive = up (flat mode only) */
static u32   s_active_stamp; /* pc_frame_counter when the override last ran */
static s16   s_last_cam_yaw; /* live game camera yaw (for toggle-on init) */
static int   s_snap_latch;
static int   s_resnap;       /* toggle-on mid-talk: face the partner again */
static int   s_pitch_decay;  /* ease pitch back to level after a talk (flat) */
static int   s_last_active;  /* frozen while the PC pause menu is open */

extern int g_pc_paused;      /* pc_pause_menu.c */

/* Frame counter from pc_vi.c. The active flag is frame-stamped rather than
 * latched: some camera modes (STOP's empty main, a TALK edge case) never
 * reach Camera2_SetView, and a stale latch would keep the player hidden and
 * the movement yaw redirected under a camera we no longer control. */
extern u32 pc_frame_counter;

void pc_fp_init(void) {
    g_pc_fp_mode = g_pc_settings.fp_mode != 0;
    s_yaw = 0.0f;
    s_pitch = 0.0f;
    s_active_stamp = (u32)-1000;
    s_snap_latch = 0;
    if (g_pc_fp_mode) {
        printf("[FP] starting in first person (F5 toggles)\n");
    }
}

void pc_fp_toggle(void) {
    g_pc_fp_mode = !g_pc_fp_mode;
    if (g_pc_fp_mode) {
        /* Face wherever the game camera was facing */
        s_yaw = (float)(u16)s_last_cam_yaw;
        s_pitch = 0.0f;
        s_snap_latch = 1; /* don't snap off a stick that's already deflected */
        s_resnap = 1;     /* if we're mid-conversation, face the partner */
    }
    g_pc_settings.fp_mode = g_pc_fp_mode;
    pc_vr_set_fp_scale(g_pc_fp_mode);
    printf("[FP] first person %s\n", g_pc_fp_mode ? "ON" : "OFF");
}

int pc_fp_consume_resnap(void) {
    int r = s_resnap;
    s_resnap = 0;
    return r;
}

void pc_fp_talk_exit(void) {
    s_pitch_decay = 1;
}

void pc_fp_notify_camera_yaw(s16 yaw) {
    s_last_cam_yaw = yaw;
}

void pc_fp_set_active(int active) {
    s_active_stamp = active ? pc_frame_counter : (u32)(pc_frame_counter - 1000u);
}

int pc_fp_view_is_active(void) {
    /* The PC pause menu freezes game logic (no Camera2_SetView stamping) but
     * not drawing — freeze the answer too, or the player model pops visible
     * inside the frozen first-person view two frames after pausing. */
    if (!g_pc_paused) {
        s_last_active = (u32)(pc_frame_counter - s_active_stamp) <= 1u;
    }
    return g_pc_fp_mode && s_last_active;
}

int pc_fp_hide_player(void) {
    return pc_fp_view_is_active();
}

s16 pc_fp_camera_yaw(void) {
    /* In VR, movement follows the headset gaze (anchor yaw + head yaw), so
     * stick-forward walks where you're looking even mid-head-turn. */
    float y = s_yaw + pc_vr_head_yaw_offset_bang();
    return (s16)(u16)((int)y & 0xFFFF);
}

void pc_fp_frame(float dt) {
    float sx = (float)g_pc_substick_x / 100.0f;
    float sy = (float)g_pc_substick_y / 100.0f;
    if (sx > 1.0f) sx = 1.0f;
    if (sx < -1.0f) sx = -1.0f;
    if (sy > 1.0f) sy = 1.0f;
    if (sy < -1.0f) sy = -1.0f;
    if (dt > 4.0f) dt = 4.0f;

    if (pc_vr_active() && g_pc_settings.fp_snap_degrees > 0) {
        /* VR: snap turning (comfort); pitch comes from the headset */
        if (!s_snap_latch && fabsf(sx) > FP_SNAP_ON) {
            float step = (float)g_pc_settings.fp_snap_degrees * FP_BANG_PER_DEG;
            s_yaw -= (sx > 0.0f ? step : -step);
            s_snap_latch = 1;
        } else if (s_snap_latch && fabsf(sx) < FP_SNAP_OFF) {
            s_snap_latch = 0;
        }
        s_pitch = 0.0f;
    } else {
        /* Smooth look (flat, or VR with fp_snap_degrees = 0) */
        s_yaw -= sx * FP_TURN_RATE_BANG * dt;
        if (!pc_vr_active()) {
            s_pitch += sy * FP_PITCH_RATE * dt;
            if (s_pitch > FP_PITCH_CLAMP) s_pitch = FP_PITCH_CLAMP;
            if (s_pitch < -FP_PITCH_CLAMP) s_pitch = -FP_PITCH_CLAMP;
        } else {
            s_pitch = 0.0f;
        }
    }

    while (s_yaw < 0.0f) s_yaw += 65536.0f;
    while (s_yaw >= 65536.0f) s_yaw -= 65536.0f;

    /* After a conversation, ease the flat-mode pitch back to level until
     * the player touches the look stick again */
    if (s_pitch_decay) {
        if (sy != 0.0f || fabsf(s_pitch) < 0.02f) {
            s_pitch_decay = 0;
        } else if (!pc_vr_active()) {
            s_pitch -= s_pitch * 0.10f * dt;
        }
    }
}

void pc_fp_face_point(const float player_pos[3], float target_x, float target_y, float target_z) {
    float eye_y = player_pos[1] + (float)g_pc_settings.fp_eye_height;
    float dx = target_x - player_pos[0];
    float dz = target_z - player_pos[2];
    float hd = sqrtf(dx * dx + dz * dz);

    if (hd < 1.0f) {
        return;
    }

    /* forward = (sin yaw, 0, cos yaw) -> yaw = atan2(dx, dz) */
    s_yaw = atan2f(dx, dz) * (65536.0f / (float)(2.0 * PC_PI));
    while (s_yaw < 0.0f) s_yaw += 65536.0f;
    while (s_yaw >= 65536.0f) s_yaw -= 65536.0f;

    if (!pc_vr_active()) {
        s_pitch = atan2f(target_y - eye_y, hd);
        if (s_pitch > FP_PITCH_CLAMP) s_pitch = FP_PITCH_CLAMP;
        if (s_pitch < -FP_PITCH_CLAMP) s_pitch = -FP_PITCH_CLAMP;
    }
}

void pc_fp_view(const float player_pos[3], float eye[3], float at[3], float up[3]) {
    const float th = s_yaw * (float)(2.0 * PC_PI / 65536.0);
    const float cp = cosf(s_pitch);
    const float fx = sinf(th) * cp;   /* game convention: x = sin, z = cos */
    const float fy = sinf(s_pitch);
    const float fz = cosf(th) * cp;

    eye[0] = player_pos[0];
    eye[1] = player_pos[1] + (float)g_pc_settings.fp_eye_height;
    eye[2] = player_pos[2];

    at[0] = eye[0] + fx * 100.0f;
    at[1] = eye[1] + fy * 100.0f;
    at[2] = eye[2] + fz * 100.0f;

    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;
}
