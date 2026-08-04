/* pc_fp_camera.h - first-person camera mode (flat + VR).
 *
 * When enabled, the play scene's NORMAL/WADE camera is replaced with a view
 * from the player character's eyes. Scripted cameras (dialogue, doors,
 * events, demos) keep control and first person resumes afterwards — the
 * game camera continues to run underneath, so toggling is seamless.
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

#ifdef __cplusplus
}
#endif

#endif /* PC_FP_CAMERA_H */
