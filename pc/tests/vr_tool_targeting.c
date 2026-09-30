/* Regression harness: real fixture structs and extracted production functions.
 * Only runtime state, world/collision services, and request acceptance are faked.
 * Collision reach and asset-dependent catch behavior need the headset playtest. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "m_player.h"
#include "m_play.h"
#include "m_name_table.h"
#include <stdlib.h>
#include "pc_vr_menu_input.h"
#undef mEv_IsTitleDemo
#ifdef TEST_FIXED
#include "pc_vr_swing.h"
#endif

#define PC_PI 3.14159265358979323846
static PLAYER_ACTOR fixture;
static GAME game;
static GAME_PLAY collision_game;
GAME* gamePT = &collision_game.game;
static ClObjTrisElem_c axe_triangle, net_triangle;
#define GET_PLAYER_ACTOR_GAME(g) (&fixture)
#define GET_PLAYER_ACTOR_GAME_ACTOR(g) (&fixture.actor_class)
static struct {
    int active, have_pose, head_pose_valid;
    float head_pose[3][4];
    int input_ready, act_move, act_camera;
    int menu_input_available;
    PCVRMenuInput menu_input;
    int act_a, act_b, act_x, act_y, act_l, act_r, act_z, act_start;
#ifdef TEST_FIXED
    PCVRSwing swing;
#else
    int swing_pulse;
#endif
} s_vr;
static float s_yaw;
static int g_pc_paused, fp_active, flat_scene, in_talk, title_demo;
static u32 pc_frame_counter;
static u32 SDL_GetTicks(void) { return pc_frame_counter * 16u; }
static int pc_pause_menu_open_vr_settings(void) { return 0; }
static int pc_pause_menu_vr_input(float x, float y, int a, int b) { return g_pc_paused; }
static int trigger_a, held_a, accept_request, checked_yaw, target_queries;
static int axe_result, scoop_result, damage_result, valid_unit;
static xyz_t cast_target;
static int casts;
static int action_held[12], toggles;
static float move_x, move_y;
static mPlayer_Controller_Data_c demo_input;
static int failures, checks;
#define CHECK(condition, label) do { checks++; if (!(condition)) { \
    printf("FAIL: %s (line %d)\n", label, __LINE__); failures++; } } while (0)

static int pc_fp_view_is_active(void) { return fp_active; }
static int pc_vr_flat_scene_active(void) { return flat_scene; }
static int pc_fp_in_talk(void) { return in_talk; }
static void pc_fp_toggle(void) { toggles++; }
static int pcvr_digital(int action) { return action_held[action]; }
static void pcvr_analog(int action, float* x, float* y) {
    *x = action == s_vr.act_move ? move_x : 0;
    *y = action == s_vr.act_move ? move_y : 0;
}
static int mEv_IsTitleDemo(void) { return title_demo; }
static mPlayer_Controller_Data_c* mPlib_Get_controller_data_for_title_demo_p(void) { return &demo_input; }
int chkTrigger(u16 button) { return trigger_a; }
int chkButton(u16 button) { return held_a; }
static s8 Player_actor_Get_ItemKind(ACTOR* actor, int state) { return fixture.item_kind; }
static int Player_actor_check_request_main_able(GAME* g, int state, int priority) { return accept_request; }
static void Player_actor_request_main_index(GAME* g, int state, int priority) {
    fixture.requested_main_index = state;
    fixture.requested_main_index_priority = priority;
    fixture.requested_main_index_changed = TRUE;
}
static int Player_actor_Check_axe_after(ACTOR* a, xyz_t* p, mActor_name_t* item, ACTOR** reflect) {
    checked_yaw = a->shape_info.rotation.y;
    target_queries++;
    *p = a->world.position;
    *item = 1;
    *reflect = NULL;
    return axe_result;
}
int mFI_Wpos2UtNum(int* x, int* z, xyz_t p) { *x = *z = 0; return valid_unit; }
static mActor_name_t Player_actor_Get_ItemNoSubmenu(void) { return 1; }
static mActor_name_t Player_actor_GetitemNo_forDamageAxe(mActor_name_t item, int reflected) { return damage_result; }
static int Player_actor_request_main_broken_axe_type_swing(GAME* g, xyz_t* p, mActor_name_t i, int x, int z, int prio) { return accept_request; }
static int Player_actor_request_main_swing_axe_all(GAME* g, xyz_t* p, mActor_name_t i, u16 d, int x, int z, int prio) { return accept_request; }
static int Player_actor_request_main_broken_axe_type_reflect(GAME* g, xyz_t* p, mActor_name_t i, ACTOR* a, int prio) { return accept_request; }
static int Player_actor_request_main_reflect_axe_all(GAME* g, xyz_t* p, mActor_name_t i, u16 d, ACTOR* a, int prio) { return accept_request; }
static int Player_actor_request_main_air_axe_all(GAME* g, int prio) { return accept_request; }
static int Player_actor_Check_scoop_after(GAME* g, xyz_t* p, mActor_name_t* i, ACTOR** a, int golden) {
    checked_yaw = fixture.actor_class.shape_info.rotation.y;
    target_queries++;
    *p = fixture.actor_class.world.position;
    *i = 1; *a = NULL;
    return scoop_result;
}
static int Player_actor_request_main_dig_scoop_all(GAME* g, xyz_t* p, mActor_name_t i, int prio) { return accept_request; }
static int Player_actor_request_main_fill_scoop_all(GAME* g, xyz_t* p, int prio) { return accept_request; }
static int Player_actor_request_main_reflect_scoop_all(GAME* g, xyz_t* p, mActor_name_t i, ACTOR* a, int prio) { return accept_request; }
static int Player_actor_request_main_air_scoop_all(GAME* g, int prio) { return accept_request; }
static int Player_actor_request_main_get_scoop_all(GAME* g, xyz_t* p, mActor_name_t i, int prio) { return accept_request; }
/* Known water everywhere: record the real rod destination without needing town assets. */
f32 sin_s(s16 a) { return sinf(a * (float)(2.0 * PC_PI / 65536.0)); }
f32 cos_s(s16 a) { return cosf(a * (float)(2.0 * PC_PI / 65536.0)); }
u32 mCoBG_Wpos2BgAttribute_Original(xyz_t p) { return 1; }
int mCoBG_GetMoveBgHeight(f32* y, xyz_t* p) { return -1; }
int mCoBG_CheckWaterAttribute(u32 a) { return TRUE; }
f32 mCoBG_GetWaterHeight_File(xyz_t p, char* file, int line) { return 0; }
static int Player_actor_request_main_cast_rod(GAME* g, const xyz_t* p, int prio) { cast_target = *p; casts++; return TRUE; }
static int Player_actor_request_main_air_rod(GAME* g, int prio) { return TRUE; }
int mPlib_get_player_actor_main_index(GAME* g) { return fixture.now_main_index; }
int CollisionCheck_setOCC(GAME* g, CollisionCheck_c* c, ClObj_c* o) { return 0; }

#include "tool_source.inc"

static void reset(int kind, int expected_yaw) {
    memset(&fixture, 0, sizeof(fixture));
    memset(&axe_triangle, 0, sizeof(axe_triangle));
    memset(&net_triangle, 0, sizeof(net_triangle));
    fixture.item_axe_tris.elements = &axe_triangle;
    fixture.item_axe_tris.count = 1;
    fixture.item_net_tris.elements = &net_triangle;
    fixture.item_net_tris.count = 1;
    memset(&s_vr, 0, sizeof(s_vr));
    memset(&demo_input, 0, sizeof(demo_input));
    memset(action_held, 0, sizeof(action_held));
    move_x = move_y = 0;
    s_vr.input_ready = 1;
    s_vr.menu_input_available = s_vr.menu_input.ready = 1; /* input has already settled */
    s_vr.act_move = 0; s_vr.act_camera = 1;
    s_vr.act_a = 2; s_vr.act_b = 3; s_vr.act_x = 4; s_vr.act_y = 5;
    s_vr.act_l = 6; s_vr.act_r = 7; s_vr.act_z = 8; s_vr.act_start = 9;
    s_vr.active = s_vr.have_pose = s_vr.head_pose_valid = fp_active = 1;
    g_pc_paused = flat_scene = in_talk = title_demo = 0;
    trigger_a = held_a = accept_request = 1;
    axe_result = mPlayer_AXE_HIT_TREE; scoop_result = mPlayer_INDEX_DIG_SCOOP;
    damage_result = 1; valid_unit = 1;
    checked_yaw = 1234; target_queries = casts = 0;
    fixture.item_kind = kind;
    fixture.now_main_index = mPlayer_INDEX_WAIT;
    fixture.requested_main_index = mPlayer_INDEX_WAIT;
    fixture.actor_class.shape_info.rotation.y = -1234;
    fixture.actor_class.world.angle.y = -1234;
    fixture.actor_class.world.position.x = 240;
    fixture.actor_class.world.position.z = 360;
    /* Exercise the actual sum of a turned camera anchor and tracked head yaw. */
    s_yaw = 8192;
    float offset = ((s16)expected_yaw - s_yaw) * (float)(2.0 * PC_PI / 65536.0);
    s_vr.head_pose[0][2] = sinf(offset);
    s_vr.head_pose[2][2] = cosf(offset);
}

static int near_angle(int actual, int expected) { return abs((s16)(actual - expected)) <= 1; }
static void check_facing(int expected) {
    CHECK(near_angle(fixture.actor_class.shape_info.rotation.y, expected), "visible facing matches gaze");
    CHECK(near_angle(fixture.actor_class.world.angle.y, expected), "logical facing matches gaze");
}

static unsigned short merge(int pending_swing, int* movement) {
    ++pc_frame_counter;
    unsigned short buttons = 0;
    signed char x = 0, y = 0, cx = 0, cy = 0;
    unsigned char l = 0, r = 0;
#ifdef TEST_FIXED
    s_vr.swing.pulse = pending_swing;
#else
    s_vr.swing_pulse = pending_swing;
#endif
    pc_vr_merge_pad(&buttons, &x, &y, &cx, &cy, &l, &r);
    *movement = x;
    return buttons;
}

int main(void) {
    const int directions[] = {0, 8192, 16384, 24576, -32768, -24576, -16384, -8192, 32767, -32767};
    const int tools[] = {mPlayer_ITEM_KIND_AXE, mPlayer_ITEM_KIND_GOLD_AXE,
        mPlayer_ITEM_KIND_SHOVEL, mPlayer_ITEM_KIND_GOLD_SHOVEL};
    for (unsigned d = 0; d < sizeof(directions)/sizeof(directions[0]); d++) {
        reset(mPlayer_ITEM_KIND_AXE, directions[d]);
        Player_actor_SetPosition_OBJtoLine_forItem(&fixture.actor_class, (GAME*)&collision_game);
        xyz_t* v = axe_triangle.attribute.tri.vtx;
        float x = (v[1].x + v[2].x) * 0.5f - v[0].x;
        float z = (v[1].z + v[2].z) * 0.5f - v[0].z;
        int yaw = (int)(atan2f(x, z) * (float)(65536.0 / (2.0 * PC_PI)));
        /* Two integer-angle round trips (pose -> yaw -> triangle -> yaw). */
        CHECK(abs((s16)(yaw - directions[d])) <= 2, "axe collision probe follows gaze before the action");
        CHECK(fabsf(v[0].y - 31.0f) < 0.001f && fabsf(sqrtf(x*x+z*z) - 34.6572f) < 0.02f,
            "axe probe retains original height, width and reach");
        check_facing(-1234); /* Probe refresh must not steer the body mid-action. */
    }
    reset(mPlayer_ITEM_KIND_AXE, 16384); s_vr.active = 0;
    Player_actor_SetPosition_OBJtoLine_forItem(&fixture.actor_class, (GAME*)&collision_game);
    xyz_t* flat_v = axe_triangle.attribute.tri.vtx;
    int flat_yaw = (int)(atan2f((flat_v[1].x+flat_v[2].x)*0.5f-flat_v[0].x,
        (flat_v[1].z+flat_v[2].z)*0.5f-flat_v[0].z) * (float)(65536.0/(2.0*PC_PI)));
    CHECK(near_angle(flat_yaw, -1234), "flat axe probe retains body-relative collision");
    for (unsigned t = 0; t < sizeof(tools)/sizeof(tools[0]); t++) {
        for (unsigned d = 0; d < sizeof(directions)/sizeof(directions[0]); d++) {
            reset(tools[t], directions[d]);
            if (mPlayer_ITEM_IS_AXE(tools[t])) Player_actor_CheckAndRequest_main_axe_all(&game, 4);
            else Player_actor_CheckAndRequest_main_scoop_all(&game, 4);
            CHECK(target_queries == 1 && near_angle(checked_yaw, directions[d]), "gaze applied BEFORE axe/shovel world query");
            check_facing(directions[d]);
        }
    }
    for (unsigned d = 0; d < sizeof(directions)/sizeof(directions[0]); d++) {
        reset(mPlayer_ITEM_KIND_ROD, directions[d]);
        CHECK(Player_actor_request_main_ready_rod(&game, 4), "rod request accepted");
        check_facing(directions[d]);
        /* Looking elsewhere during wind-up must not redirect the committed cast. */
        s_yaw += 10000;
        fixture.keyframe0.frame_control.current_frame = 10;
        Player_actor_request_proc_index_fromReady_rod(&fixture.actor_class, &game);
        float angle = directions[d] * (float)(2 * PC_PI / 65536.0);
        CHECK(casts == 1 && fabsf(cast_target.x - (240 + 100 * sinf(angle))) < 0.02f &&
            fabsf(cast_target.z - (360 + 100 * cosf(angle))) < 0.02f, "real rod target stays along chosen gaze");
    }
    reset(mPlayer_ITEM_KIND_NET, 16384);
    CHECK(Player_actor_request_main_ready_net(&game, 4), "net ready accepted");
    check_facing(16384);
    s_yaw += 16384;
    fixture.now_main_index = mPlayer_INDEX_READY_NET;
    CHECK(Player_actor_request_main_swing_net(&game, 22), "net release accepted");
    check_facing(-32768);

    /* Physical/virtual A share the above controller checks. No A = no aim/query. */
    reset(mPlayer_ITEM_KIND_AXE, 16384); trigger_a = held_a = 0;
    CHECK(!Player_actor_CheckAndRequest_main_axe_all(&game, 4) && !target_queries, "no input has no effect");
    check_facing(-1234);
    reset(mPlayer_ITEM_KIND_SHOVEL, 16384); trigger_a = held_a = 0;
    CHECK(!Player_actor_CheckAndRequest_main_scoop_all(&game, 4) && !target_queries, "shovel no input has no effect");
    check_facing(-1234);

    for (int reason = 0; reason < 8; reason++) {
        reset(mPlayer_ITEM_KIND_AXE, 16384);
        if (reason == 0) s_vr.active = 0;              /* flat, including flat FP */
        if (reason == 1) fp_active = 0;               /* diorama/scripted camera */
        if (reason == 2) g_pc_paused = 1;
        if (reason == 3) flat_scene = 1;
        if (reason == 4) in_talk = 1;
        if (reason == 5) s_vr.head_pose_valid = 0;
        if (reason == 6) { title_demo = 1; demo_input.trigger_btn_a = 1; }
        if (reason == 7) { fixture.requested_main_index_changed = TRUE; fixture.requested_main_index = mPlayer_INDEX_TALK; }
        Player_actor_CheckAndRequest_main_axe_all(&game, 4);
        check_facing(-1234);
    }
    reset(mPlayer_ITEM_KIND_ROD, 16384); accept_request = 0;
    CHECK(!Player_actor_request_main_ready_rod(&game, 4), "rejected cast stays rejected"); check_facing(-1234);
    reset(mPlayer_ITEM_KIND_NET, 16384); accept_request = 0;
    CHECK(!Player_actor_request_main_ready_net(&game, 4), "rejected net ready stays rejected");
    CHECK(!Player_actor_request_main_swing_net(&game, 22), "rejected net swing stays rejected"); check_facing(-1234);

    /* Rejected requests must not turn an ongoing action or higher-priority state. */
    for (int branch = 0; branch < 6; branch++) {
        reset(mPlayer_ITEM_KIND_AXE, 16384); accept_request = 0;
        fixture.actor_class.world.angle.y = 4321; // preserve distinct logical/visual yaw
        if (branch == 1 || branch == 3) damage_result = EMPTY_NO;
        if (branch == 2 || branch == 3) axe_result = mPlayer_AXE_HIT_REFLECT;
        if (branch == 4) axe_result = 0;
        if (branch == 5) valid_unit = 0;
        CHECK(!Player_actor_CheckAndRequest_main_axe_all(&game, 4), "rejected axe request remains rejected");
        CHECK(fixture.actor_class.shape_info.rotation.y == -1234 && fixture.actor_class.world.angle.y == 4321,
              "rejected axe request restores both original angles");
        if (valid_unit) {
            accept_request = 1;
            CHECK(Player_actor_CheckAndRequest_main_axe_all(&game, 4), "accepted axe branch keeps its request");
            check_facing(16384);
        }
    }
    const int scoop_states[] = {mPlayer_INDEX_DIG_SCOOP, mPlayer_INDEX_FILL_SCOOP,
        mPlayer_INDEX_REFLECT_SCOOP, mPlayer_INDEX_AIR_SCOOP, mPlayer_INDEX_GET_SCOOP, -1};
    for (int branch = 0; branch < 6; branch++) {
        reset(mPlayer_ITEM_KIND_SHOVEL, 16384); accept_request = 0; scoop_result = scoop_states[branch];
        fixture.actor_class.world.angle.y = 4321;
        CHECK(!Player_actor_CheckAndRequest_main_scoop_all(&game, 4), "rejected shovel request remains rejected");
        CHECK(fixture.actor_class.shape_info.rotation.y == -1234 && fixture.actor_class.world.angle.y == 4321,
              "rejected shovel request restores both original angles");
        if (scoop_result != -1) {
            accept_request = 1;
            CHECK(Player_actor_CheckAndRequest_main_scoop_all(&game, 4), "accepted shovel branch keeps its request");
            check_facing(16384);
        }
    }

    /* Execute the actual pad merger: the VR settings pause consumes physical
     * input too; ordinary gameplay dialogue/inventory keep physical A/B. */
    int movement;
    for (int reason = 0; reason < 5; reason++) {
        reset(mPlayer_ITEM_KIND_NET, 0);
        if (reason == 0) g_pc_paused = 1;
        if (reason == 1) flat_scene = 1;
        if (reason == 2) in_talk = 1;
        if (reason == 3) fp_active = 0;
        if (reason == 4) s_vr.head_pose_valid = 0;
        CHECK(!(merge(2, &movement) & 0x100), "pending motion cannot activate menu/dialogue/invalid context");
        action_held[s_vr.act_a] = action_held[s_vr.act_b] = 1;
        unsigned short merged = merge(2, &movement) & 0x300;
        CHECK(merged == ((reason == 0 || reason == 4) ? 0 : 0x300),
              "pause/tracking loss consume physical A/B; normal game menus retain them");
    }
    reset(mPlayer_ITEM_KIND_NET, 0);
    CHECK(merge(2, &movement) & 0x100, "allowed motion still injects A");
    action_held[s_vr.act_a] = 1;
    CHECK(merge(0, &movement) & 0x100, "physical A works without motion");
    action_held[s_vr.act_l] = 1; move_x = 0.3f;
    CHECK((merge(0, &movement) & 0x40) && movement == 0, "tool-selection grip suppresses partial stick movement");
    move_x = 0.8f;
    CHECK((merge(0, &movement) & 0x2) && movement == 0, "grip and full stick retain D-pad tool selection");
    action_held[s_vr.act_l] = 0;
    merge(0, &movement);
    CHECK(movement == 80, "normal left-stick movement is unchanged");
    reset(mPlayer_ITEM_KIND_AXE, 0);
    action_held[s_vr.act_l] = 1; move_x = 0.8f;
    unsigned short selection = merge(2, &movement);
    CHECK(!(selection & 0x100) && (selection & 0x2), "tool selection cancels pending motion but keeps D-pad");
    action_held[s_vr.act_a] = 1;
    CHECK(merge(2, &movement) & 0x100, "physical A is preserved while selecting tools");
    /* The actual movement heading and sprint turn check must agree over a
     * complete gaze rotation, strafing/backward input, and angle wrap. */
    for (int gaze = 0; gaze < 65536; gaze += 2048) {
        for (int stick = -32768; stick < 32768; stick += 8192) {
            reset(mPlayer_ITEM_KIND_NET, gaze);
            collision_game.game.mcon.move_angle = (s16)stick;
            s16 move = Player_actor_Get_ControllerAngle(gamePT);
            fixture.actor_class.world.angle.y = move;
            CHECK(Player_actor_Get_DiffWorldAngleToControllerAngle(&fixture.actor_class) == 0,
                  "running straight never requests a skid at any gaze/stick heading");
            fixture.actor_class.world.angle.y = move + 20000;
            CHECK(Player_actor_Get_DiffWorldAngleToControllerAngle(&fixture.actor_class) >= 18204,
                  "real sharp reversals still permit the original skid");
            fixture.actor_class.world.angle.y = move + 8000;
            CHECK(Player_actor_Get_DiffWorldAngleToControllerAngle(&fixture.actor_class) == 8000,
                  "gentle turns keep running without the skid");
        }
    }
    reset(mPlayer_ITEM_KIND_NET, 0); fp_active = 0;
    collision_game.camera.direction.y = (s16)0x8000;
    collision_game.game.mcon.move_angle = 16384;
    fixture.actor_class.world.angle.y = (s16)0x8000;
    CHECK(Player_actor_Get_DiffWorldAngleToControllerAngle(&fixture.actor_class) == 0,
          "original fixed-camera sprint remains unchanged");
    title_demo = fp_active = 1; demo_input.mcon.move_angle = 16384;
    CHECK(Player_actor_Get_DiffWorldAngleToControllerAngle(&fixture.actor_class) == 0,
          "title demo retains original sprint turn logic");
    printf("Tool targeting: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
