/* Decode the compiled production lists, with actual vertices from a local disc.
 * The Python runner checks their topology. No ROM data is checked into source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libforest/gbi_extensions.h"
#include "pc_disc.h"

u8 bush_pal_dummy[32], cliff_pal_dummy[32], earth_pal_dummy[32];
u8 grass_tex_dummy[512], bush_b_tex_dummy[1024], bush_a_tex_dummy[2048];
u8 cliff_tex_dummy[2048], earth_tex_dummy[2048];
int g_pc_verbose;
static u8* rel;
#include "cliff_models.inc"

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
static void triangle(int a, int b, int c, int added, int cliff) {
    int ids[3] = {a,b,c};
    for (int i = 0; i < 3; i++)
        if (ids[i] >= 32 || slots[ids[i]] < 0 || slots[ids[i]] >= vertex_count) exit(4);
    printf("%s[%d,%d,%d,%d,%d]", first_triangle ? "" : ",", slots[a],slots[b],slots[c],added,cliff);
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
            for (int t = 0; t < used; t++) triangle(ids[t*3],ids[t*3+1],ids[t*3+2],0,texture == (uintptr_t)cliff_tex_dummy);
            remaining -= used;
        } else if (op == G_VTX) {
            int nv = (g->words.w0 >> 12) & 255;
            int start = ((g->words.w0 >> 1) & 127) - nv;
            Vtx* base = (Vtx*)ptr(g->words.w1);
            if (start < 0 || start + nv > 32 || base < v || base + nv > v + n) exit(6);
            for (int j = 0; j < nv; j++) slots[start+j] = base - v + j;
        } else if (op == G_TRI1) {
            if (texture != (uintptr_t)cliff_tex_dummy || palette != (uintptr_t)cliff_pal_dummy) exit(7);
            triangle((g->words.w0 >> 17)&127,(g->words.w0 >> 9)&127,(g->words.w0 >> 1)&127,1,1);
        } else if (op == G_SETTIMG) texture = ptr(g->words.w1);
        else if (op == G_LOADTLUT) palette = ptr(g->words.w1);
        else if (op == G_ENDDL) { ended = 1; break; }
    }
    if (!ended || remaining) exit(8);
    puts("]}");
}

int main(void) {
    if (!pc_disc_init() || !(rel = pc_disc_extract_rel())) return 2;
    _pc_load_src_data_field_bg_acre_grd_s_c4_s_1_grd_s_c4_s_1_c();
    _pc_load_src_data_field_bg_acre_grd_s_c4_s_2_grd_s_c4_s_2_c();
    dump("grd_s_c4_s_1",grd_s_c4_s_1_v,ARRAY_COUNT(grd_s_c4_s_1_v),grd_s_c4_s_1_model,ARRAY_COUNT(grd_s_c4_s_1_model));
    dump("grd_s_c4_s_2",grd_s_c4_s_2_v,ARRAY_COUNT(grd_s_c4_s_2_v),grd_s_c4_s_2_model,ARRAY_COUNT(grd_s_c4_s_2_model));
    free(rel); pc_disc_shutdown();
    return 0;
}
