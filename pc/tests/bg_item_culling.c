/* Real production matrix/culling functions, real game types, no rendered scene.
 * Counters wrap the matrix routines without changing their calculations. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bg_item.h"
#include "m_play.h"
#include "sys_matrix.h"
#include "m_skin_matrix.h"

#ifdef TEST_ORIGINAL_CULLING
#undef TARGET_PC
#endif

int g_pc_full_world;
int g_pc_window_w = 640, g_pc_window_h = 480;
#define PC_GC_WIDTH 640
#define PC_GC_HEIGHT 480

static MtxF matrix_storage;
static MtxF* Matrix_now = &matrix_storage;
xyz_t ZeroVec;
static int projections, matrix_puts, matrix_positions, area_queries, talk_queries;
static float last_area_x, last_area_z, last_area_radius;
static int checks, failures;
#define CHECK(test, label) do { ++checks; if (!(test)) { ++failures; \
    printf("FAIL %s (line %d)\n", label, __LINE__); } } while (0)

#define Skin_Matrix_PrjMulVector production_projection
#define Matrix_put production_matrix_put
#define Matrix_Position production_matrix_position
#include "culling_matrices_source.inc"
#undef Skin_Matrix_PrjMulVector
#undef Matrix_put
#undef Matrix_Position

void Skin_Matrix_PrjMulVector(MtxF* mf, xyz_t* src, xyz_t* dst, f32* w) {
    ++projections;
    production_projection(mf, src, dst, w);
}
void Matrix_put(MtxF* matrix) {
    ++matrix_puts;
    production_matrix_put(matrix);
}
void Matrix_Position(xyz_t* src, xyz_t* dst) {
    ++matrix_positions;
    production_matrix_position(src, dst);
}

/* Camera dialogue-area geometry is independent of the optimization. The seam
 * masks the origin and records the unchanged world coordinates/radius. */
int Camera2_CheckEnterCullingArea(f32 x, f32 z, f32 radius) {
    ++area_queries;
    last_area_x = x; last_area_z = z; last_area_radius = radius;
    return x == 0.0f && z == 0.0f;
}

#include "bg_culling_source.inc"

static GAME_PLAY play;
static ACTOR actor;
static bg_item_common_c common;
static bg_item_draw_table_c table;

static MtxF identity(void) {
    MtxF result;
    memset(&result, 0, sizeof(result));
    result.xx = result.yy = result.zz = result.ww = 1.0f;
    return result;
}

static int full_world_active(void) {
#ifdef TARGET_PC
    return g_pc_full_world != 0;
#else
    return 0;
#endif
}

static void clear_counts(void) {
    projections = matrix_puts = matrix_positions = area_queries = talk_queries = 0;
}

static void setup(void) {
    memset(&play, 0, sizeof(play));
    memset(&actor, 0, sizeof(actor));
    memset(&common, 0, sizeof(common));
    memset(&table, 0, sizeof(table));
    play.projection_matrix = identity();
    actor.cull_radius = 0.5f;
    actor.cull_distance = 10.0f;
    actor.cull_width = 0.25f;
    actor.cull_height = 0.5f;
    matrix_storage = identity();
    g_pc_window_w = 640; g_pc_window_h = 480;
    g_pc_full_world = 0;
    clear_counts();
}

static void check_position(xyz_t pos, int ordinary_visible) {
    MtxF saved_projection = play.projection_matrix;
    xyz_t saved_pos = pos;
    ACTOR saved_actor = actor;
    clear_counts();
    int actual = bg_item_common_culling_check(&play, &actor, &pos);
    CHECK(actual == (full_world_active() || ordinary_visible), "visibility unchanged");
    CHECK(projections == !full_world_active(), "only full-world mode removes projection");
    CHECK(!memcmp(&play.projection_matrix, &saved_projection, sizeof(saved_projection)), "projection matrix untouched");
    CHECK(!memcmp(&pos, &saved_pos, sizeof(pos)), "world position untouched");
    CHECK(!memcmp(&actor, &saved_actor, sizeof(actor)), "actor untouched");
    CHECK(!matrix_puts && !matrix_positions && !area_queries, "simple check has no extra state effects");
}

static void test_point_culling(void) {
    static const xyz_t points[] = {
        {0, 0, 0}, {0, 0, -0.5f}, {0, 0, 10.5f},
        {1.25f, 0, 0}, {-1.25f, 0, 0}, {0, -1.5f, 0}, {0, 1.5f, 0},
        {1.249f, 1.499f, 10.499f}, {-1.249f, -1.499f, -0.499f},
    };
    static const int expected[] = {1, 0, 0, 0, 0, 0, 0, 1, 1};
    for (int mode = 0; mode < 3; ++mode) {
        setup();
        g_pc_full_world = mode == 2 ? -1 : mode;
        for (unsigned i = 0; i < sizeof(points) / sizeof(points[0]); ++i)
            check_position(points[i], expected[i]);
        play.projection_matrix.ww = 2.0f;
        check_position((xyz_t){2, 2, 0}, 1);
        check_position((xyz_t){3, 0, 0}, 0);
        play.projection_matrix.ww = 0.5f;
        check_position((xyz_t){1, 0, 0}, 1);
        check_position((xyz_t){1.25f, 0, 0}, 0);
        play.projection_matrix = identity();
        play.projection_matrix.xw = 4;
        check_position((xyz_t){-4, 0, 0}, 1);
        check_position((xyz_t){0, 0, 0}, 0);
        play.projection_matrix = identity();
        g_pc_window_w = 1920; g_pc_window_h = 1080;
#ifdef PC_ENHANCEMENTS
        check_position((xyz_t){1.5f, 0, 0}, 1);
#else
        check_position((xyz_t){1.5f, 0, 0}, 0);
#endif
    }
}

static void test_talk_mask(void) {
    for (int mode = 0; mode <= 1; ++mode) {
        setup();
        g_pc_full_world = mode;
        xyz_t pos = {0, 0, 0};
        CHECK(!bg_item_common_culling_check_talk(&play, &actor, &pos), "dialogue origin stays masked");
        CHECK(area_queries == 1 && last_area_radius == 65, "same dialogue-area radius");
        CHECK(!last_area_x && !last_area_z, "dialogue receives original world coordinates");
        CHECK(projections == !full_world_active(), "dialogue also avoids unused full-world projection");
        clear_counts();
        pos = (xyz_t){0.5f, 0, 0};
        CHECK(bg_item_common_culling_check_talk(&play, &actor, &pos), "visible scenery outside dialogue mask survives");
        CHECK(area_queries == 1 && last_area_x == pos.x && last_area_z == pos.z, "outside mask query retained");
        clear_counts();
        pos = (xyz_t){50, 0, 0};
        CHECK(bg_item_common_culling_check_talk(&play, &actor, &pos) == full_world_active(), "ordinary out-of-frustum scenery remains culled");
        CHECK(area_queries == full_world_active(), "ordinary rejection still short-circuits dialogue query");
    }
}

static void init_linked_positions(bg_item_draw_pos_c* nodes) {
    memset(nodes, 0, sizeof(*nodes) * 8);
    for (int i = 0; i < 8; ++i) {
        nodes[i].mtxf = identity();
        nodes[i].mtxf.xw = 100 + i;
        nodes[i].next_add_cnt = 256;
        nodes[i].cull_flag = 99;
    }
    /* Nonadjacent list: 0 -> 2 -> 5 -> 7 (sentinel). */
    nodes[0].next_add_cnt = 2; nodes[0].mtxf.xw = 0;
    nodes[2].next_add_cnt = 3; nodes[2].mtxf.xw = 0.5f;
    nodes[5].next_add_cnt = 2; nodes[5].mtxf.xw = 50;
}

static void test_linked_loops(void) {
    bg_item_draw_pos_c nodes[8], saved[8];
    for (int mode = 0; mode <= 1; ++mode) for (int talk = 0; talk <= 1; ++talk) {
        setup(); g_pc_full_world = mode;
        init_linked_positions(nodes);
        memcpy(saved, nodes, sizeof(nodes));
        if (talk) bg_item_common_culling_check_talk_loop(&play, &actor, nodes);
        else bg_item_common_culling_check_loop(&play, &actor, nodes);
        CHECK(nodes[0].cull_flag == talk, "dialogue loop alone masks origin");
        CHECK(nodes[2].cull_flag == FALSE, "middle linked scenery visible");
        CHECK(nodes[5].cull_flag == !full_world_active(), "far linked scenery follows active mode");
        CHECK(projections == (full_world_active() ? 0 : 3), "full-world linked loop projects no positions");
        CHECK(matrix_puts == 3 && matrix_positions == 3, "all matrix side effects preserved");
        CHECK(!memcmp(&matrix_storage, &saved[5].mtxf, sizeof(matrix_storage)), "global matrix remains last visited scenery matrix");
        CHECK(area_queries == (talk ? (full_world_active() ? 3 : 2) : 0), "dialogue query ordering preserved");
        for (int i = 0; i < 8; ++i) {
            CHECK(!memcmp(&nodes[i].mtxf, &saved[i].mtxf, sizeof(MtxF)), "source scenery transforms untouched");
            CHECK(nodes[i].next_add_cnt == saved[i].next_add_cnt, "linked offsets untouched");
            if (i != 0 && i != 2 && i != 5)
                CHECK(!memcmp(&nodes[i], &saved[i], sizeof(nodes[i])), "skipped nodes and sentinel untouched");
        }
        clear_counts();
        bg_item_common_culling_check_loop(&play, &actor, &nodes[7]);
        bg_item_common_culling_check_talk_loop(&play, &actor, &nodes[7]);
        CHECK(!projections && !matrix_puts && !matrix_positions && !area_queries, "empty sentinel loops do no work");
    }
}

static int talk_tree(int index) { ++talk_queries; return index == 1; }

static void test_draw_table_routing(void) {
    static u16 indices[] = {0, 1, 9};
    for (int mode = 0; mode <= 1; ++mode) for (int talking = 0; talking <= 1; ++talking) {
        setup(); g_pc_full_world = mode;
        common.flags = talking;
        common.talk_display_limit_check_proc = talk_tree;
        table.draw_data.idx_p = indices;
        table.draw_data.val = 3;
        init_linked_positions(&table.draw_data.draw_pos[1]);
        init_linked_positions(&table.draw_data.draw_pos[9]);
        bg_item_common_draw_check(&play, &common, &actor, &table);
        CHECK(table.draw_data.draw_pos[1].cull_flag == talking, "designated tree group uses dialogue mask");
        CHECK(!table.draw_data.draw_pos[9].cull_flag, "other tree group keeps ordinary culling");
        CHECK(table.draw_data.draw_pos[6].cull_flag == !full_world_active() &&
              table.draw_data.draw_pos[14].cull_flag == !full_world_active(), "both groups retain mode-specific distant culling");
        CHECK(matrix_puts == 6 && matrix_positions == 6, "draw-table routing visits both linked lists");
        CHECK(projections == (full_world_active() ? 0 : 6), "draw-table projection calls eliminated only in full-world mode");
        CHECK(talk_queries == (talking ? 2 : 0), "zero index and disabled dialogue skip tree callback");
        CHECK(area_queries == (talking ? (full_world_active() ? 3 : 2) : 0), "area mask limited to selected tree group");
    }
}

int main(void) {
    test_point_culling();
    test_talk_mask();
    test_linked_loops();
    test_draw_table_routing();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
