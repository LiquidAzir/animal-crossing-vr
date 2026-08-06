/* pc_fp_camera.h - first-person camera mode (flat + VR).
 *
 * When enabled, the play scene's NORMAL/WADE/TALK cameras are replaced with
 * a view from the player character's eyes — regular villager conversations
 * stay first person (the view snaps to face the partner as the chat
 * starts). Scripted cameras (doors, demos, events, item, staff roll) keep
 * control and first person resumes afterwards — the game camera continues
 * to run underneath, so toggling is seamless.
 *
 * Look: C-stick / right stick (smooth yaw+pitch flat; snap yaw in VR, where
 * pitch comes from the headset). Movement stays stick-relative-to-view by
 * redirecting the camera yaw the player movement code reads
 * (getCamera2AngleY). The player model is hidden while the first-person
 * view is live (its shadow is kept).
 *
 * Toggle: F5, left-grip + Y (VR), fp_mode in settings.ini.
 */
#ifndef PC_FP_CAMERA_H
#define PC_FP_CAMERA_H

#include "pc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* First-person projection while the override is live */
#define PC_FP_FOV_DEG 60.0f
#define PC_FP_NEAR    8.0f
/* Flat FP only: far plane covering the 4480x6400-unit town diagonal so the
 * whole-town draw is visible. VR keeps the stock game far — emu64 inverts
 * the game matrix for fog, and VR's render projection is replaced
 * separately, so changing it under VR would shift VR fog. */
#define PC_FP_FAR     8000.0f

/* Mode flag: user wants first person (the view engages whenever the camera
 * is in a follow mode). */
extern int g_pc_fp_mode;

void pc_fp_init(void);      /* pull settings (after pc_settings_load) */
void pc_fp_toggle(void);

/* --- called from the camera hook (m_camera2.c Camera2_SetView) --- */

/* Advance look yaw/pitch from the merged C-stick. dt in 60Hz frames. */
void pc_fp_frame(float dt);

/* Compute the first-person view from the player's feet position. */
void pc_fp_view(const float player_pos[3], float eye[3], float at[3], float up[3]);

/* Snap the look direction to face a world point (used when a conversation
 * starts, so the villager is in front of you). Pitch aims at target_y in
 * flat mode; VR pitch stays with the headset. */
void pc_fp_face_point(const float player_pos[3], float target_x, float target_y, float target_z);

/* Toggled on mid-conversation: 1 exactly once, so the hook re-snaps. */
int pc_fp_consume_resnap(void);

/* Conversation ended: ease flat-mode pitch back to level. */
void pc_fp_talk_exit(void);

/* Whether the override actually ran this frame (vs a scripted camera). */
void pc_fp_set_active(int active);
int  pc_fp_view_is_active(void);

/* Last real camera yaw, so toggling on starts facing the same way. */
void pc_fp_notify_camera_yaw(s16 yaw);

/* Camera yaw the movement code should use while the FP view is live —
 * same convention as Camera2.direction.y (forward = (sin yaw, 0, cos yaw),
 * per atans_table's reversed-atan2 argument order). */
s16 pc_fp_camera_yaw(void);

/* 1 while the player model should not be rendered (shadow still draws). */
int pc_fp_hide_player(void);

/* Motion tools: current item main index (written by the item draw
 * dispatcher) and whether it's a swingable tool (axe/net/rod/shovel). */
extern int g_pc_item_main_index_now;
int pc_fp_swingable_equipped(void);

/* TALK-camera state (set by the m_camera2 hook): gates the swing gesture
 * so a controller swing can't advance dialogue. */
void pc_fp_set_in_talk(int in_talk);
int  pc_fp_in_talk(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_FP_CAMERA_H */
