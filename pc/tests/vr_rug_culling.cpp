/* Real production cull dispatch and task lifecycle, with only GX batching and
 * unrelated command handlers stubbed. The actual compiled rug list is scanned. */
#include <cstdio>
#include <cstring>
#include "libforest/gbi_extensions.h"
#include "PR/gbi.h"
#include "dolphin/gx/GXEnum.h"
/* GX headers declare the C enum signature; the C backend implements u32.
 * Rename only this extracted test copy to avoid a C++ overload in the seam. */
#define GXSetCullMode test_GXSetCullMode

static int checks, failures, flushes, dirty_count, fp;
static int g_pc_model_viewer_no_cull;
float g_pc_vr_cull_expand;
static struct { int cull_mode; } g_gx;
static int pc_fp_view_is_active() { return fp; }
static void pc_gx_flush_if_begin_complete() { ++flushes; }
#define PC_GX_DIRTY_CULL 1
#define DIRTY(flag) (++dirty_count)
#define CHECK(c, message) do { ++checks; if (!(c)) { if(failures++<20) std::printf("FAIL: %s (%d)\n", message, __LINE__); } } while(0)

enum { AFLAGS_SET_CULLMODE, AFLAGS_WIREFRAME };
static int aflags[2];
enum { EMU64_DIRTY_FLAG_GEOMETRYMODE, EMU64_DIRTY_FLAG_LIGHTING, EMU64_DIRTY_FLAG_FOG,
       EMU64_DIRTY_FLAG_TEX_TILE0, EMU64_DIRTY_FLAG_TEX_TILE1, EMU64_DIRTY_FLAG_TEX_TILE2,
       EMU64_DIRTY_FLAG_TEX_TILE3, EMU64_DIRTY_FLAG_TEX_TILE4, EMU64_DIRTY_FLAG_TEX_TILE5,
       EMU64_DIRTY_FLAG_TEX_TILE6, EMU64_DIRTY_FLAG_TEX_TILE7 };
#define EMU64_INFO(...) ((void)0)
#define EMU64_INFOF(...) ((void)0)
#define EMU64_WARN(...) ((void)0)
#define EMU64_LOG(...) ((void)0)
#define EMU64_TIMED_SEGMENT_BEGIN() ((void)0)
#define EMU64_TIMED_SEGMENT_END(...) ((void)0)
#define OSInitFastCast() ((void)0)
#undef G_FIRST_CMD
#define G_FIRST_CMD 0xD9
#define NUM_COMMANDS 4
#define DL_HISTORY_COUNT 16
static u8 FrameCansel;
static int pc_emu64_frame_cmds, pc_emu64_frame_noop_cmds, pc_emu64_frame_vtx_cmds,
           pc_emu64_frame_tri_cmds, pc_emu64_frame_dl_cmds;
static int drawn_mode;
class emu64 {
public:
    u32 geometry_mode = G_CULL_BACK;
    bool dirty_flags[11] = {}, end_dl = false, print_commands = false;
    Gfx gfx = {}, *gfx_p = nullptr, *dl_history[DL_HISTORY_COUNT] = {};
    u32 gfx_cmd = 0, dl_history_start = 0, cmds_processed = 0, DL_stack_level = 0, err_count = 0;
    u32 command_info[NUM_COMMANDS*2] = {};
    void* work_ptr = nullptr;
    void Printf0(const char*, ...) {}
    void print_geomflags(u32) {}
    void cullmode();
    void dl_G_GEOMETRYMODE();
    u32 emu64_taskstart_r(Gfx*);
    void draw() { cullmode(); drawn_mode = g_gx.cull_mode; }
    void end() { end_dl = true; }
    void cancel() { draw(); FrameCansel = true; }
};
typedef void (emu64::*dl_func)();
static dl_func dl_func_tbl[NUM_COMMANDS] = {
    &emu64::dl_G_GEOMETRYMODE, &emu64::draw, &emu64::end, &emu64::cancel
};
#include "rug_culling_source.inc"
extern "C" { extern Gfx obj_carpetT_gfx_model[], obj_carpetT_mat_model[]; }

static Gfx command(u32 op, u32 clear = 0, u32 set = 0) {
    Gfx result = {};
    result.words.w0 = op<<24 | clear;
    result.words.w1 = set;
    return result;
}

int main() {
    const u32 bits[] = {0, G_CULL_FRONT, G_CULL_BACK, G_CULL_BOTH};
    // All ordinary world callers retain their previous behavior, including
    // viewer diagnostics, both-sided suppression and explicit draw-nothing.
    for (int vr=0; vr<2; ++vr) for (fp=0; fp<2; ++fp)
    for (g_pc_model_viewer_no_cull=0; g_pc_model_viewer_no_cull<2; ++g_pc_model_viewer_no_cull)
    for (int mode=0; mode<4; ++mode) for (int authored=0; authored<2; ++authored) {
        g_pc_vr_cull_expand = float(vr);
        int expected = mode;
        if(g_pc_model_viewer_no_cull || (!authored && (vr||fp) && (mode==1||mode==2))) expected=0;
        g_gx.cull_mode=-1; flushes=dirty_count=0;
        if(authored) pc_gx_set_authored_cull_mode(mode); else GXSetCullMode(mode);
        CHECK(g_gx.cull_mode==expected, "GX scope and ordinary override");
        CHECK(flushes==1 && dirty_count==1, "flush pending vertices before changing cull state");
        if(authored) pc_gx_set_authored_cull_mode(mode); else GXSetCullMode(mode);
        CHECK(flushes==2 && dirty_count==1, "unchanged state keeps existing dirty suppression");
        for(int wire=0; wire<2; ++wire) {
            emu64 emu;
            aflags[AFLAGS_WIREFRAME]=wire;
            emu.geometry_mode=bits[mode] | (authored ? G_PC_AUTHORED_CULL : 0);
            emu.cullmode();
            CHECK(g_gx.cull_mode==(wire?0:expected), "emu64 scope and wireframe");
        }
    }
    fp=1; g_pc_vr_cull_expand=1; g_pc_model_viewer_no_cull=0;
    aflags[AFLAGS_WIREFRAME]=0;
    for(int ending=0; ending<3; ++ending) {
        // Ending 0 skips the explicit close but exits normally; 1 aborts on
        // unknown opcode; 2 simulates FrameCansel during the marked draw.
        emu64 emu;
        Gfx list[] = { command(0xD9, 0xffffff, G_PC_AUTHORED_CULL),
                       command(ending==2?0xDC:0xDA), command(ending==1?0xFD:0xDB) };
        FrameCansel=0;
        emu.emu64_taskstart_r(list);
        CHECK(drawn_mode==GX_CULL_BACK, "marked draw preserves authored faces");
        CHECK(!(emu.geometry_mode&G_PC_AUTHORED_CULL), "normal/invalid/cancel exit closes scope");
        CHECK(emu.geometry_mode==G_CULL_BACK, "cleanup preserves every original geometry bit");
        CHECK(emu.dirty_flags[EMU64_DIRTY_FLAG_GEOMETRYMODE], "cleanup marks emulator dirty");
        CHECK(g_gx.cull_mode==GX_CULL_NONE, "cleanup restores ordinary world culling");
        FrameCansel=0;
        Gfx next[] = {command(0xDA), command(0xDB)};
        emu.emu64_taskstart_r(next);
        CHECK(drawn_mode==GX_CULL_NONE, "next task cannot inherit authored exception");
    }
    // Execute geometry commands from the real compiled rug list, without
    // replacing them with a parallel hand-maintained representation.
    emu64 emu;
    int marked=0, closed=0, vertex_commands=0, triangle_commands=0;
    for(Gfx* p=obj_carpetT_gfx_model; (u8)p->dma.cmd!=G_ENDDL; ++p) {
        if((u8)p->dma.cmd==G_GEOMETRYMODE) {
            emu.gfx=*p;
            emu.dl_G_GEOMETRYMODE();
            emu.cullmode();
            if(emu.geometry_mode&G_PC_AUTHORED_CULL) ++marked; else ++closed;
        } else {
            CHECK((emu.geometry_mode&G_PC_AUTHORED_CULL)!=0, "scope covers every rug vertex/triangle command");
            CHECK(g_gx.cull_mode==GX_CULL_BACK, "rug keeps authored cull during its complete list");
            if((u8)p->dma.cmd==G_VTX) ++vertex_commands; else ++triangle_commands;
        }
    }
    CHECK(marked==1 && closed==1, "compiled rug list has one balanced scope");
    CHECK(vertex_commands==2 && triangle_commands==13, "all original geometry commands retained");
    CHECK(emu.geometry_mode==G_CULL_BACK && g_gx.cull_mode==GX_CULL_NONE,
          "following ordinary geometry restores existing two-sided behavior");
    std::printf("Rug culling: %d checks, %d failures\n", checks, failures);
    return failures?1:0;
}
