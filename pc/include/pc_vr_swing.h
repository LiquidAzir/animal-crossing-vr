/* Small, clock-injected gesture state machine shared with the regression test.
 * No game or VR SDK dependencies; all time arithmetic is wrap-safe. */
#ifndef PC_VR_SWING_H
#define PC_VR_SWING_H
#include <stdint.h>
#include <float.h>

typedef struct {
    uint32_t high_since;
    uint32_t last_fire;
    int high;
    int fired;
    int latched;
    int pulse;
} PCVRSwing;

static inline void pc_vr_swing_cancel(PCVRSwing* swing) {
    swing->high = 0;
    swing->pulse = 0;
    /* Preserve cooldown/latch across a menu or tracking interruption. A
     * previously fired arc must settle before it can become a new swing. */
}

static inline void pc_vr_swing_update(PCVRSwing* swing, uint32_t now,
                                    float speed, int enabled) {
    if (!enabled || !(speed >= 0.0f && speed <= FLT_MAX)) {
        /* A bad velocity is a tracking interruption too. Clear the pending
         * pulse as well as qualification; do not treat it as a slow hand. */
        pc_vr_swing_cancel(swing);
        return;
    }
    if (swing->pulse > 0) swing->pulse--;
    if (speed <= 2.2f) {
        swing->high = 0;
        if (speed >= 0.0f && speed <= 1.1f) swing->latched = 0;
        return;
    }
    if (swing->latched) return;
    if (!swing->high) {
        swing->high = 1;
        swing->high_since = now;
    }
    if ((uint32_t)(now - swing->high_since) >= 22u &&
        (!swing->fired || (uint32_t)(now - swing->last_fire) >= 350u)) {
        swing->pulse = 2; /* Preserve the existing two-frame virtual A press. */
        swing->last_fire = now;
        swing->fired = 1;
        swing->latched = 1;
        swing->high = 0;
    }
}
#endif
