/* Decode the compiled production lists, with actual vertices from a local disc.
 * The Python runner checks their topology. No ROM data is checked into source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libforest/gbi_extensions.h"
#include "pc_disc.h"

#include "crossing_models.inc"
#include "src/pc_crossing_back.c_inc"
int g_pc_verbose;
static u8* rel;
void pc_load_asset(const char* name, void* dst, unsigned size, unsigned offset, int dol, int kind) {
    (void)name;
    if (dol || kind != 2) exit(3);
    memcpy(dst, rel + offset, size);
    for (unsigned i = 0; i < size; i += 16) {
        u8* p = (u8*)dst + i;
        for (int j = 0; j < 12; j += 2) {
            u8 temp = p[j]; p[j] = p[j+1]; p[j+1] = temp;
        }
    }
}

static uintptr_t ptr(u32 word) {
    uintptr_t p = pc_gbi_unpack_runtime_ptr(word);
    return p ? p : word & ~1u;
}

static int slots[32], vertex_count, first_triangle;
static void triangle(int a, int b, int c, int added, int material) {
    int ids[3] = {a,b,c};
    for (int i = 0; i < 3; i++)
        if (ids[i] >= 32 || slots[ids[i]] < 0 || slots[ids[i]] >= vertex_count) exit(4);
    printf("%s[%d,%d,%d,%d,%d]", first_triangle ? "" : ",", slots[a],slots[b],slots[c],added,material);
    first_triangle = 0;
}

static void dump(const char* name, Vtx* v, int n, Gfx* model, unsigned count) {
    printf("{\"name\":\"%s\",\"vertices\":[",name);
    for (int i = 0; i < n; i++) {
        printf("%s[%d,%d,%d,%d,%d,%d,%d,%d,%d]", i ? "," : "", v[i].v.ob[0],v[i].v.ob[1],v[i].v.ob[2],
               v[i].v.tc[0],v[i].v.tc[1],v[i].n.n[0],v[i].n.n[1],v[i].n.n[2],v[i].v.cn[3]);
    }
    printf("],\"triangles\":[");
    for (int i = 0; i < 32; i++) slots[i] = -1;
    vertex_count = n; first_triangle = 1;
    int remaining = 0, ended = 0;
    uintptr_t texture = 0, palette = 0;
    for (unsigned i = 0; i < count; i++) {
        Gfx* g = model + i;
        u32 op = g->words.w0 >> 24;
        if (remaining || op == G_TRIN_INDEPEND) {
            int first = remaining == 0;
            if (first) remaining = ((g->words.w0 >> 17) & 127) + 1;
            int ids[12] = {POLY_GET_V0_5b(g),POLY_GET_V1_5b(g),POLY_GET_V2_5b(g),
                POLY_GET_V3_5b(g),POLY_GET_V4_5b(g),POLY_GET_V5_5b(g),
                POLY_GET_V6_5b(g),POLY_GET_V7_5b(g),POLY_GET_V8_5b(g),
                POLY_GET_V9_5b(g),POLY_GET_V10_5b(g),POLY_GET_V11_5b(g)};
            if (g->words.w1 & 1) exit(5);
            int used = first ? 3 : 4;
            if (used > remaining) used = remaining;
            for (int t = 0; t < used; t++) triangle(ids[t*3],ids[t*3+1],ids[t*3+2],0,texture == (uintptr_t)bridge_1_tex_dummy ? 1 : texture == (uintptr_t)bridge_2_tex_dummy ? 2 : 0);
            remaining -= used;
        } else if (op == G_VTX) {
            int nv = (g->words.w0 >> 12) & 255;
            int start = ((g->words.w0 >> 1) & 127) - nv;
            Vtx* base = (Vtx*)ptr(g->words.w1);
            if (start < 0 || start + nv > 32 || base < v || base + nv > v + n) exit(6);
            for (int j = 0; j < nv; j++) slots[start+j] = base - v + j;
        } else if (op == G_TRI1) {
            if (texture == (uintptr_t)bridge_1_tex_dummy) {
                if (palette != (uintptr_t)bridge_1_pal_dummy) exit(7);
            } else if (texture == (uintptr_t)bridge_2_tex_dummy) {
                if (palette != (uintptr_t)bridge_2_pal_dummy) exit(7);
            } else exit(7);
            triangle((g->words.w0 >> 17)&127,(g->words.w0 >> 9)&127,(g->words.w0 >> 1)&127,1,texture == (uintptr_t)bridge_1_tex_dummy ? 1 : texture == (uintptr_t)bridge_2_tex_dummy ? 2 : 0);
        } else if (op == G_SETTIMG) texture = ptr(g->words.w1);
        else if (op == G_LOADTLUT) palette = ptr(g->words.w1);
        else if (op == G_ENDDL) { ended = 1; break; }
    }
    if (!ended || remaining) exit(8);
    puts("]}");
}

int main(void) {
    Gfx unknown[1] = {{0}};
    if (pc_crossing_back_lookup(unknown) != NULL) return 20;
    if (pc_crossing_back_lookup(NULL) != NULL) return 20;
    for (unsigned i = 0; i < ARRAY_COUNT(pc_crossing_specs); ++i)
        if (pc_crossing_back_lookup(pc_crossing_specs[i].original) != NULL) return 21;
    if (!pc_disc_init() || !(rel = pc_disc_extract_rel())) return 2;
#include "crossing_load.inc"
#include "crossing_capacity.inc"
    for (unsigned i = 0; i < ARRAY_COUNT(pc_crossing_specs); ++i) {
        pc_crossing_spec* spec = &pc_crossing_specs[i];
        Vtx before[512];
        if (spec->source_count > 512) return 22;
        memcpy(before, spec->source(), spec->source_count * sizeof(Vtx));
        int vertex_total = 0, command_total = 9;
        for (int p = 0; p < spec->patch_count; ++p) {
            int count = spec->patches[p].count;
            if (count < 3 || count > 4) return 26;
            vertex_total += count;
            command_total += count - 1; /* vertex command + n-2 triangles */
            for (int j = 0; j < spec->patches[p].count; ++j)
                if (spec->patches[p].vertices[j].source >= spec->source_count) return 23;
        }
        if (vertex_total != vertex_capacity[i] || command_total > list_capacity[i]) return 27;
        if (spec->source_count != source_capacity[i]) return 28;
        Gfx* dl = pc_crossing_back_lookup(spec->original);
        if (!dl || dl != pc_crossing_back_lookup(spec->original)) { fprintf(stderr, "cache failed %s\n",crossing_names[i]); return 24; }
        if (memcmp(before, spec->source(), spec->source_count * sizeof(Vtx))) return 25;
        int n = 0;
        for (int p = 0; p < spec->patch_count; ++p) n += spec->patches[p].count;
        dump(crossing_names[i], spec->vertices, n, dl, list_capacity[i]);
    }
    free(rel); pc_disc_shutdown();
    return 0;
}
