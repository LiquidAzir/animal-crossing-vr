/* Execute the actual acre submission function. The lookup is the seam tested
 * separately by the disc-backed geometry suite. No game save or GPU required. */
#include <stdio.h>
#include <string.h>
#include "ac_field_draw.h"

static GAME game;
static GRAPH graph;
static Gfx output[32], unrelated[16], original[1], other[1], completion[1];
static Gfx aFD_cull_set_model[1];
static int vr, fp, ready, lookups, checks, failures;
int pc_vr_active(void) { return vr; }
int pc_fp_view_is_active(void) { return fp; }
Gfx* pc_crossing_back_lookup(Gfx* list) {
    ++lookups;
    return list == original && ready ? completion : NULL;
}
#define CHECK(c, message) do { ++checks; if (!(c)) { ++failures; printf("FAIL %s (%d)\n",message,__LINE__); } } while(0)
#include "crossing_draw_source.inc"

static uintptr_t pointer(u32 word) {
    uintptr_t p = pc_gbi_unpack_runtime_ptr(word);
    return p ? p : word & ~1u;
}

static void exercise(int head, int first_person, int loaded, Gfx* list) {
    int before = checks;
    vr = head; fp = first_person; ready = loaded; lookups = 0;
    memset(output, 0xa5, sizeof(output));
    memset(unrelated, 0x3c, sizeof(unrelated));
    memset(&graph, 0, sizeof(graph));
    graph.bg_opaque_thaga.thaGfx.head_p = output;
    graph.polygon_opaque_thaga.thaGfx.head_p = unrelated;
    graph.polygon_translucent_thaga.thaGfx.head_p = unrelated + 4;
    game.graph = &graph;
    aFD_DrawBg(list, 1, &game);
    int capped = list == original && loaded && (vr || fp);
    int count = list ? 2 + capped : 0;
    CHECK(graph.bg_opaque_thaga.thaGfx.head_p == output + count, "original submission retained; exactly one cap if enabled");
    CHECK(lookups == !!(list && (vr || fp)), "flat/null draws never request supplemental geometry");
    CHECK(graph.polygon_opaque_thaga.thaGfx.head_p == unrelated, "other opaque stream untouched");
    CHECK(graph.polygon_translucent_thaga.thaGfx.head_p == unrelated + 4, "translucent stream untouched");
    if (list) {
        CHECK(output[0].words.w0 >> 24 == G_MOVEWORD && pointer(output[0].words.w1) == (uintptr_t)list,
              "original acre selected before cull wrapper");
        CHECK(output[1].words.w0 >> 24 == G_DL && pointer(output[1].words.w1) == (uintptr_t)aFD_cull_set_model,
              "original culling display list unchanged");
    }
    if (capped) CHECK(output[2].words.w0 >> 24 == G_DL && pointer(output[2].words.w1) == (uintptr_t)completion,
                      "same-matrix completion follows original acre");
    for (int i = 0; i < count; ++i)
        CHECK(output[i].words.w0 >> 24 != G_MTX, "no matrix replacement, push, or scale");
    CHECK(output[count].words.w0 == 0xa5a5a5a5 && output[count].words.w1 == 0xa5a5a5a5,
          "display-list write stays within returned cursor");
    CHECK(checks > before, "case executed");
}

int main(void) {
    for (int vr_mode = 0; vr_mode < 2; ++vr_mode)
        for (int fp_mode = 0; fp_mode < 2; ++fp_mode)
            for (int loaded = 0; loaded < 2; ++loaded) {
                exercise(vr_mode, fp_mode, loaded, original);
                exercise(vr_mode, fp_mode, loaded, other);
                exercise(vr_mode, fp_mode, loaded, NULL);
            }
    printf("Crossing draw routing: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
