/* pc_vr.h - SteamVR (OpenVR) stereo rendering, tracking, and input.
 *
 * The VR mod re-interprets each frame's display list once per eye
 * (see pc/VR_ARCHITECTURE.md). This header is the C boundary; the
 * implementation (pc_vr.cpp) is C++ because openvr.h is a C++ header.
 *
 * Mode is resolved at pc_vr_init():
 *   settings [vr] vr=off  -> never touch OpenVR
 *   vr=auto (default)     -> VR when a runtime + HMD are present, else flat
 *   vr=on / --vr          -> VR, message box on failure, then flat
 */
#ifndef PC_VR_H
#define PC_VR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PADStatus PADStatus;

/* --- Lifecycle --- */
void pc_vr_init(void);            /* after pc_platform_init (needs GL context) */
void pc_vr_shutdown(void);
int  pc_vr_active(void);          /* 1 = VR session live this frame */

/* --- Frame flow (graph.c / jsyswrap / pc_vi) --- */
void pc_vr_frame_begin(void);     /* poll events, update actions, WaitGetPoses */
void pc_vr_begin_eye(int eye);    /* bind + clear eye FBO, set target dims (0=L,1=R) */
void pc_vr_end_scene_passes(void);/* unbind to default framebuffer */
void pc_vr_compose_and_submit(void); /* UI panel into both eyes, Submit L+R */
void pc_vr_ensure_submitted(void);/* panel-only frame if nothing submitted yet
                                     (NES minigames / boot bypass the eye loop) */
void pc_vr_mirror_to_window(void);/* aspect-fit blit of left eye to backbuffer */
int  pc_vr_current_eye(void);     /* eye index during scene passes, -1 outside */

/* --- Render-state queries used by pc_gx --- */
/* Nonzero while a scene (eye) pass is active: perspective draws use VR
 * matrices; ortho draws are routed to the UI FBO. */
int  pc_vr_in_scene_pass(void);
/* Row-major 4x4 projection for the current eye, GX z convention ([-1,0]). */
const float* pc_vr_eye_projection(void);
/* Row-major 3x4: transforms the game's combined view*model position matrix
 * into VR eye space (X = V_vr * V_game^-1). */
const float* pc_vr_view_correction(void);
/* Ortho draw routing: pc_gx calls with 1 to target the UI FBO, 0 to return
 * to the current eye FBO. Returns the FBO name it bound. */
unsigned int pc_vr_bind_ui_target(int ui);
/* During the right-eye pass UI draws are dropped (already drawn in L pass). */
int  pc_vr_skip_ui_draws(void);
/* Fog ranges are in view units; VR view space is meters. */
float pc_vr_world_scale(void);
/* Switch between the diorama scale and the first-person life-size scale. */
void pc_vr_set_fp_scale(int fp_active);

/* Motion tools: game-world anchor matrix (row-major 3x4, MtxF top rows) for
 * the held tool at the real right-controller pose. Returns 0 when the
 * normal animated hand matrix should be used. */
int pc_vr_hand_tool_mtx(float out[12]);

/* emu64 hook: called whenever the game's view (lookAt) matrix is loaded
 * into the projection stack. mtx34 is row-major 3x4, game units. */
void pc_vr_notify_game_view(const float* mtx34);

/* emu64 CPU-cull widening: 0 = inactive, else expanded NDC bound. */
extern float g_pc_vr_cull_expand;
/* Extra slack for the near-side Z cull test when VR is active. */
extern float g_pc_vr_cull_znear_slack;

/* --- Input --- */
/* Merge VR controller state into the pad. Called from PADRead. */
void pc_vr_merge_pad(unsigned short* buttons,
                     signed char* stickX, signed char* stickY,
                     signed char* cstickX, signed char* cstickY,
                     unsigned char* triggerL, unsigned char* triggerR);
void pc_vr_rumble(int on);

/* --- NES / model viewer integration --- */
/* When VR is active the NES frame is drawn into the UI FBO. */
void pc_vr_nes_begin_draw(void);
void pc_vr_nes_end_draw(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_VR_H */
