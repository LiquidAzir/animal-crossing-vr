/* Controller-only Start/Z chord and pause input isolation. No runtime SDKs. */
#ifndef PC_VR_MENU_INPUT_H
#define PC_VR_MENU_INPUT_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t tick;
    unsigned int mask;
} PCVRMenuClickEdge;

typedef struct {
    int ready, drain, y_prev;
    unsigned int raw_clicks, output_clicks;
    int chord_latched;
    uint32_t down_since[2], sampled_frame, consumed_frame;
    int consumed;
    int sampled, edge_head, edge_count;
    PCVRMenuClickEdge edges[16];
} PCVRMenuInput;

typedef struct {
    float mx, my, cx, cy;
    int a, b, x, y, l, r, start, z;
    int available, active, neutral;
    uint32_t tick, frame;
} PCVRMenuSample;

static inline void pc_vr_menu_reset(PCVRMenuInput* s) { memset(s, 0, sizeof(*s)); }

/* Delay both edges of a lone stick click by 180 ms. At most one queued edge
 * is exposed per game frame: even a long frame cannot swallow a quick tap.
 * PADRead runs twice per frame, so repeated reads return the same pulse. */
static inline unsigned int pc_vr_menu_clicks(PCVRMenuInput* s, unsigned int raw,
                                             uint32_t tick, uint32_t frame, int* open) {
    *open = 0;
    if (s->sampled && s->sampled_frame == frame) return s->output_clicks;
    s->sampled = 1;
    s->sampled_frame = frame;
    const unsigned int rising = raw & ~s->raw_clicks;
    for (int i = 0; i < 2; ++i) if (rising & (1u << i)) s->down_since[i] = tick;
    if (s->chord_latched) {
        if (!raw) s->chord_latched = 0;
        s->raw_clicks = raw;
        return 0;
    }
    const uint32_t separation = s->down_since[0] - s->down_since[1];
    if (raw == 3 && rising && (separation <= 180u || (uint32_t)(0u - separation) <= 180u)) {
        s->chord_latched = 1;
        s->raw_clicks = raw;
        s->output_clicks = 0;
        s->edge_head = s->edge_count = 0;
        *open = 1;
        return 0;
    }
    if (raw != s->raw_clicks) {
        if (s->edge_count == 16) {
            /* Abnormal button chatter: fail closed until both clicks release. */
            s->chord_latched = 1;
            s->output_clicks = 0;
            s->edge_head = s->edge_count = 0;
            s->raw_clicks = raw;
            return 0;
        }
        PCVRMenuClickEdge* edge = &s->edges[(s->edge_head + s->edge_count) % 16];
        edge->tick = tick;
        edge->mask = raw;
        ++s->edge_count;
        s->raw_clicks = raw;
    }
    if (s->edge_count && (uint32_t)(tick - s->edges[s->edge_head].tick) >= 180u) {
        s->output_clicks = s->edges[s->edge_head].mask;
        s->edge_head = (s->edge_head + 1) % 16;
        --s->edge_count;
    }
    return s->output_clicks;
}

/* A consumed close frame stays consumed for every subsequent PADRead. All
 * physical controls must settle before game input resumes, including input
 * from a keyboard/gamepad that was already present in the merged PAD state. */
static inline int pc_vr_menu_filter(PCVRMenuInput* s, const PCVRMenuSample* in,
                                    int paused, int (*open_menu)(void),
                                    int (*navigate)(float, float, int, int),
                                    unsigned int* clicks) {
    *clicks = 0;
    if (!in->available) {
        pc_vr_menu_reset(s);
        return in->active || paused;
    }
    if (s->consumed && s->consumed_frame == in->frame) return 1;
    if (paused) {
        s->drain = 1;
        /* A focus/tracking interruption resets ready even if the menu stayed
         * open. Do not let a stale held confirm reach its previous edge latch. */
        if (!s->ready && in->neutral) s->ready = 1;
        if (s->ready) navigate(in->mx, in->my, in->a, in->b);
        s->consumed = 1; s->consumed_frame = in->frame;
        return 1;
    }
    if (!s->ready || s->drain) {
        if (in->neutral) {
            pc_vr_menu_reset(s);
            s->ready = 1;
        }
        s->consumed = 1; s->consumed_frame = in->frame;
        return 1;
    }
    int open = 0;
    *clicks = pc_vr_menu_clicks(s, (in->start ? 1u : 0u) | (in->z ? 2u : 0u),
                               in->tick, in->frame, &open);
    if (open && open_menu()) {
        s->drain = 1;
        *clicks = 0;
        s->consumed = 1; s->consumed_frame = in->frame;
        return 1;
    }
    return 0;
}
#endif
