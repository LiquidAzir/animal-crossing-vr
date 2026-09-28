/* Production Camera2_SetView and mMsg_Draw_Window with render/runtime seams.
 * Covers continuity, fallback modes, gesture suppression and temporary layout.
 * This does not claim to render a catch animation or a headset image. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "m_play.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "m_msg.h"
#include "m_font.h"
#include "pc_fp_camera.h"
#include "pc_vr.h"

#define PC_PI 3.14159265358979323846
static int failures, checks;
#define CHECK(c, label) do { checks++; if (!(c)) { \
    if (failures < 20) printf("FAIL: %s (line %d)\n", label, __LINE__); \
    failures++; } } while (0)
#define CLOSE(a, b) (fabsf((a) - (b)) < 0.001f)
static GAME_PLAY play;
static GRAPH graph;
static PLAYER_ACTOR player;
static PLAYER_ACTOR* player_ptr = &player;
static struct { int active, head_pose_valid; } s_vr = {1, 1};
static struct { int fp_eye_height; } g_pc_settings = {52};
int g_pc_fp_mode = 1;
static int g_pc_paused, title_demo, flat_scene, face_calls, exit_calls, frame_calls;
static int s_in_talk, s_last_active;
static float s_yaw, s_pitch;
static u32 s_active_stamp, pc_frame_counter = 1000;
static xyz_t rendered_eye, rendered_at;
static float rendered_near, rendered_far;

PLAYER_ACTOR* get_player_actor_withoutCheck(GAME_PLAY* p) { return player_ptr; }
int mEv_CheckTitleDemo(void) { return title_demo; }
int pc_vr_active(void) { return s_vr.active; }
int pc_vr_flat_scene_active(void) { return flat_scene; }
/* Indoor stock near/far calculation avoids unrelated field setup. */
static int Camera2_CheckInDoorNearFar(GAME_PLAY* p) { return 1; }
f32 Math3DLength(const xyz_t* a, const xyz_t* b) {
    float x = a->x-b->x, y = a->y-b->y, z = a->z-b->z;
    return sqrtf(x*x + y*y + z*z);
}
int chase_f(f32* const value, const f32 target, f32 step) { *value = target; return 1; }
void xyz_t_add(const xyz_t* const a, const xyz_t* const b, xyz_t* const c) {
    c->x = a->x+b->x; c->y = a->y+b->y; c->z = a->z+b->z;
}
void setScaleView(View* v, f32 scale) { }
void setPerspectiveView(View* v, f32 fov, f32 near, f32 far) {
    rendered_near = near; rendered_far = far;
}
void setLookAtView(View* v, xyz_t* eye, xyz_t* at, xyz_t* up) {
    rendered_eye = *eye; rendered_at = *at;
}
void pc_fp_frame(float dt) { frame_calls++; }
int pc_fp_consume_resnap(void) { return 0; }
void pc_fp_face_point(const float p[3], float x, float y, float z) { face_calls++; }
void pc_fp_talk_exit(void) { exit_calls++; }
void pc_fp_notify_camera_yaw(s16 yaw) { }

static int matrix_draws, body_draws, font_draws, choice_draws;
static float matrix_y, body_y, font_y, choice_y;
void mFont_SetMatrix(GRAPH* g, int mode) { }
void mFont_UnSetMatrix(GRAPH* g, int mode) { }
static void mMsg_SetMatrix(mMsg_Window_c* m, GAME* g, int mode) {
    matrix_draws++; matrix_y = m->center_y;
}
static void mMsg_UnSetMatrix(void) { }
static void mMsg_DrawWindowBody(mMsg_Window_c* m, GAME* g, int mode) {
    body_draws++; body_y = m->center_y;
}
static void mMsg_draw_font(mMsg_Window_c* m, GAME* g) {
    font_draws++; font_y = m->center_y;
    /* Real font drawing consumes voice flags; these side effects must survive. */
    m->status_flags &= ~mMsg_STATUS_FLAG_VOICE_ENTRY;
}
void mChoice_Draw(mChoice_c* c, GAME* g, int mode) {
    choice_draws++; choice_y = c->center_y;
}

#include "presentation_source.inc"

static void camera_frame(int mode) {
    play.camera.now_main_index = mode;
    pc_frame_counter++;
    Camera2_SetView(&play);
}

static void test_camera(void) {
    const int follow[] = {CAMERA2_PROCESS_NORMAL, CAMERA2_PROCESS_WADE,
        CAMERA2_PROCESS_TALK, CAMERA2_PROCESS_DOOR, CAMERA2_PROCESS_ITEM};
    const int scripted[] = {CAMERA2_PROCESS_STOP, CAMERA2_PROCESS_DEMO,
        CAMERA2_PROCESS_LOCK, CAMERA2_PROCESS_SIMPLE, CAMERA2_PROCESS_CUST_TALK,
        CAMERA2_PROCESS_INTER, CAMERA2_PROCESS_STAFF_ROLL, CAMERA2_PROCESS_INTER2};
    play.game.graph = &graph;
    graph.dt_num_60fps_frames = 1.0f;
    play.camera.lookat.eye = (xyz_t){500, 500, 500};
    play.camera.lookat.center = (xyz_t){0, 30, 0};
    play.camera.perspective.fov_y = 45;
    play.camera.perspective.scale = 1;
    player.actor_class.world.position = (xyz_t){100, 12, -200};
    for (int vr = 0; vr <= 1; vr++) {
        s_vr.active = vr;
        for (int heading = 0; heading < 16; heading++) {
            s_yaw = heading * 4096.0f;
            s_pitch = vr ? 0 : 0.2f;
            face_calls = 0;
            camera_frame(CAMERA2_PROCESS_NORMAL);
            xyz_t before_eye = rendered_eye, before_at = rendered_at;
            for (int frame = 0; frame < 120; frame++) {
                camera_frame(CAMERA2_PROCESS_ITEM);
                CHECK(pc_fp_view_is_active() && pc_fp_hide_player(), "presentation remains first person");
                CHECK(CLOSE(rendered_eye.x, before_eye.x) && CLOSE(rendered_eye.y, before_eye.y) &&
                      CLOSE(rendered_eye.z, before_eye.z), "presentation preserves eye position");
                CHECK(CLOSE(rendered_at.x, before_at.x) && CLOSE(rendered_at.y, before_at.y) &&
                      CLOSE(rendered_at.z, before_at.z), "presentation preserves gaze");
                CHECK(CLOSE(rendered_near, PC_FP_NEAR) &&
                      CLOSE(rendered_far, vr ? 1600 : PC_FP_FAR), "presentation retains FP projection");
                CHECK(pc_fp_in_talk() && !pc_vr_tool_input_allowed(), "motion gestures cannot skip catch text");
            }
            camera_frame(CAMERA2_PROCESS_NORMAL);
            CHECK(pc_fp_view_is_active() && !pc_fp_in_talk(), "normal camera resumes without FP dropout");
            CHECK(face_calls == 0, "catch never snaps gaze to trophy pose");
            CHECK(pc_vr_tool_input_allowed() == vr, "motion tools resume after message");
        }
        for (unsigned i = 0; i < sizeof(follow)/sizeof(*follow); i++) {
            camera_frame(follow[i]);
            CHECK(pc_fp_view_is_active(), "supported follow cameras retain FP");
        }
        for (unsigned i = 0; i < sizeof(scripted)/sizeof(*scripted); i++) {
            camera_frame(scripted[i]);
            CHECK(!pc_fp_view_is_active() && !pc_fp_hide_player(), "scripted cameras keep stock view/body");
            CHECK(CLOSE(rendered_eye.x, 500), "scripted eye stays stock");
        }
        for (int mode = 0; mode < CAMERA2_PROCESS_NUM; mode++) {
            g_pc_fp_mode = 0;
            camera_frame(mode);
            CHECK(!pc_fp_view_is_active(), "FP disabled preserves all stock cameras");
            g_pc_fp_mode = 1;
            title_demo = 1;
            camera_frame(mode);
            CHECK(!pc_fp_view_is_active(), "title attract cameras remain stock");
            title_demo = 0;
        }
    }
    player_ptr = NULL;
    camera_frame(CAMERA2_PROCESS_ITEM);
    CHECK(!pc_fp_view_is_active(), "missing player does not enable FP");
    player_ptr = &player;
    s_vr.active = 1;
    camera_frame(CAMERA2_PROCESS_ITEM);
    player.actor_class.world.position.x += 7;
    camera_frame(CAMERA2_PROCESS_ITEM);
    CHECK(CLOSE(rendered_eye.x, player.actor_class.world.position.x), "FP follows player during item motion");
    CHECK(frame_calls > 0, "look input continues during presentations");
}

static void test_dialogue(void) {
    mMsg_Window_c msg = {0};
    mMsg_Data_c data = {0};
    msg.msg_data = &data;
    msg.draw_flag = msg.data_loaded = data.data_loaded = 1;
    msg.center_y = 185.4f;
    msg.choice_window.center_y = 177.114f;
    msg.choice_window.center_y_begin = 169;
    msg.choice_window.center_y_target = 177.114f;
    msg.choice_window.text_y = 28;
    msg.window_scale = msg.text_scale = 1;
    for (int vr = 0; vr <= 1; vr++) for (int fp = 0; fp <= 1; fp++)
    for (int flat = 0; flat <= 1; flat++) {
        s_vr.active = vr; flat_scene = flat; g_pc_fp_mode = fp;
        pc_fp_set_active(fp);
        for (int draw = 0; draw < 120; draw++) {
            msg.status_flags |= mMsg_STATUS_FLAG_VOICE_ENTRY;
            mMsg_Window_c expected = msg;
            expected.status_flags &= ~mMsg_STATUS_FLAG_VOICE_ENTRY;
            float shift = vr && fp && !flat ? -32 : 0;
            mMsg_Draw_Window(&msg, &play.game);
            CHECK(CLOSE(matrix_y, 185.4f + shift) && CLOSE(body_y, matrix_y) &&
                  CLOSE(font_y, matrix_y), "dialogue box, nameplate matrix and text share placement");
            CHECK(CLOSE(choice_y, 177.114f + shift), "choices move with dialogue");
            CHECK(memcmp(&expected, &msg, sizeof(msg)) == 0,
                  "draw restores animated layout exactly and preserves font side effects");
        }
    }
    int before = body_draws;
    msg.draw_flag = 0;
    mMsg_Draw_Window(&msg, &play.game);
    msg.draw_flag = 1; msg.data_loaded = 0;
    mMsg_Draw_Window(&msg, &play.game);
    CHECK(body_draws == before, "hidden/unloaded messages remain hidden");
    msg.data_loaded = 1; data.data_loaded = 0;
    before = font_draws;
    mMsg_Draw_Window(&msg, &play.game);
    CHECK(font_draws == before, "unloaded text is not drawn");
    CHECK(CLOSE(msg.center_y, 185.4f) && CLOSE(msg.choice_window.center_y, 177.114f),
          "unloaded text still restores message and choice centers");
}

int main(void) {
    test_camera();
    test_dialogue();
    printf("%d presentation checks, %d failures\n", checks, failures);
    return failures != 0;
}
