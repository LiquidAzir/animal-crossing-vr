#ifndef PC_VR_HANDS_H
#define PC_VR_HANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draw one opaque white oval hand marker into the caller's eye framebuffer/viewport.
 * eye_from_grip is a row-major rigid 3x4 transform in meters (-Z forward).
 * gx_projection is row-major 4x4, with GX near/far NDC depth -1/0.
 * The caller sets GL_DEPTH_RANGE to the world viewport's near/far values.
 * Hands write into its lower half, matching the unremapped GX world shader.
 * hand is 0 for left or 1 for right. Returns 0 on invalid input/init failure.
 * Requires the renderer's current OpenGL context; preserves its GL state.
 * No tracking, gameplay, or tool offsets are applied here. */
int pc_vr_hands_draw(const float eye_from_grip[12],
                     const float gx_projection[16], int hand);

/* Call before destroying the GL context. A later draw can initialize again. */
void pc_vr_hands_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
