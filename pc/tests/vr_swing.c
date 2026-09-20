#include <stdio.h>
#include <math.h>
#include "pc_vr_swing.h"

static int checks, failures;
#define CHECK(c, label) do { checks++; if (!(c)) { failures++; printf("FAIL: %s\n", label); } } while (0)

int main(void) {
    const int rates[] = {72, 90, 120};
    for (int r = 0; r < 3; r++) {
        PCVRSwing s = {0};
        int presses = 0, previous = 0;
        /* A long continuous arc must fire once, even beyond the cooldown. */
        for (int frame = 0; frame < rates[r] * 2; frame++) {
            uint32_t ms = (uint32_t)(frame * 1000 / rates[r]);
            pc_vr_swing_update(&s, ms, 3.0f, 1);
            if (s.pulse && !previous) presses++;
            previous = s.pulse > 0;
        }
        CHECK(presses == 1, "one continuous arc = one press at every headset rate");
        CHECK(s.pulse == 0, "virtual A releases for net swing");
        pc_vr_swing_update(&s, 2100, 0.5f, 1);
        pc_vr_swing_update(&s, 2200, 3.0f, 1);
        pc_vr_swing_update(&s, 2222, 3.0f, 1);
        CHECK(s.pulse == 2, "a separate swing still works");
    }
    PCVRSwing s = {0};
    pc_vr_swing_update(&s, 0, 3.0f, 1);
    pc_vr_swing_update(&s, 21, 3.0f, 1);
    CHECK(!s.pulse, "short tracking spike cannot fire");
    pc_vr_swing_update(&s, 22, 3.0f, 1);
    CHECK(s.pulse == 2, "22 ms qualification including timer zero");
    pc_vr_swing_update(&s, 23, 0.0f, 1);
    pc_vr_swing_update(&s, 24, 3.0f, 1);
    pc_vr_swing_update(&s, 371, 3.0f, 1);
    CHECK(!s.pulse, "cooldown prevents rapid repeated use");
    pc_vr_swing_update(&s, 372, 3.0f, 1);
    CHECK(s.pulse == 2, "350 ms cooldown permits next separate use");

    s = (PCVRSwing){0};
    pc_vr_swing_update(&s, 100, 3, 1);
    pc_vr_swing_update(&s, 110, 0, 0);
    pc_vr_swing_update(&s, 900, 3, 1);
    CHECK(!s.pulse, "tracking/menu interruption discards partial gesture");
    pc_vr_swing_update(&s, 921, 3, 1);
    CHECK(!s.pulse, "recovered tracking must qualify afresh");
    pc_vr_swing_update(&s, 922, 3, 1);
    CHECK(s.pulse == 2, "fresh valid gesture after tracking recovery");
    pc_vr_swing_update(&s, 923, 3, 0);
    CHECK(!s.pulse, "pending press cancelled immediately on lost tracking/menu");
    pc_vr_swing_update(&s, 2000, 3, 1);
    pc_vr_swing_update(&s, 2022, 3, 1);
    CHECK(!s.pulse, "interruption cannot split a fired arc into a second use");
    pc_vr_swing_update(&s, 2023, 0, 1);
    pc_vr_swing_update(&s, 2030, 3, 1);
    pc_vr_swing_update(&s, 2052, 3, 1);
    CHECK(s.pulse == 2, "settling hand rearms after interruption");

    s = (PCVRSwing){0};
    pc_vr_swing_update(&s, UINT32_MAX - 10u, 3, 1);
    pc_vr_swing_update(&s, 11u, 3, 1);
    CHECK(s.pulse == 2, "qualification crosses millisecond wrap");
    pc_vr_swing_update(&s, 12u, 0, 1);
    pc_vr_swing_update(&s, 20u, 3, 1);
    pc_vr_swing_update(&s, 360u, 3, 1);
    CHECK(!s.pulse, "cooldown after timer wrap");
    pc_vr_swing_update(&s, 361u, 3, 1);
    CHECK(s.pulse == 2, "cooldown expires after timer wrap");
    s = (PCVRSwing){0};
    pc_vr_swing_update(&s, UINT32_MAX - 100u, 3, 1);
    pc_vr_swing_update(&s, UINT32_MAX - 78u, 3, 1);
    pc_vr_swing_update(&s, UINT32_MAX - 60u, 0, 1);
    pc_vr_swing_update(&s, 0u, 3, 1);
    pc_vr_swing_update(&s, 270u, 3, 1);
    CHECK(!s.pulse, "cooldown straddles timer wrap");
    pc_vr_swing_update(&s, 271u, 3, 1);
    CHECK(s.pulse == 2, "wrapped cooldown expires at correct duration");

    s = (PCVRSwing){0};
    pc_vr_swing_update(&s, 100, 3, 1);
    pc_vr_swing_update(&s, 121, NAN, 1);
    pc_vr_swing_update(&s, 122, 3, 1);
    CHECK(!s.pulse, "invalid velocity breaks sustained gesture");
    pc_vr_swing_update(&s, 130, 0, 1);
    for (int i = 0; i < 20; i++) pc_vr_swing_update(&s, 200 + i * 100, 2, 1);
    CHECK(!s.pulse, "ordinary hand movement does not activate a tool");
    s = (PCVRSwing){0};
    pc_vr_swing_update(&s, 100, INFINITY, 1);
    pc_vr_swing_update(&s, 200, INFINITY, 1);
    CHECK(!s.pulse, "infinite tracking velocity cannot activate a tool");
    const float invalid_speeds[] = {NAN, INFINITY, -INFINITY, -1.0f};
    for (int i = 0; i < 4; i++) {
        s = (PCVRSwing){0};
        pc_vr_swing_update(&s, 100, 3, 1);
        pc_vr_swing_update(&s, 122, 3, 1);
        pc_vr_swing_update(&s, 123, invalid_speeds[i], 1);
        CHECK(!s.pulse && !s.high, "bad velocity cancels the remaining synthetic press immediately");
        pc_vr_swing_update(&s, 600, 3, 1);
        pc_vr_swing_update(&s, 622, 3, 1);
        CHECK(!s.pulse, "bad velocity must not rearm a previously fired arc");
        pc_vr_swing_update(&s, 623, 0, 1);
        pc_vr_swing_update(&s, 624, 3, 1);
        pc_vr_swing_update(&s, 646, 3, 1);
        CHECK(s.pulse == 2, "valid settling and a fresh gesture recover after bad velocity");
    }
    printf("Motion gesture: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
