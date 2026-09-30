/* Run via run_vr_settings_menu_tests.py; the generated config is isolated. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_settings.h"
#include "pc_keybindings.h"

#define PC_SCREEN_WIDTH 640
#define PC_SCREEN_HEIGHT 480
static int apply_calls, binding_calls;
PCKeybindings g_pc_keybindings;
PCPadBindings g_pc_padbindings;
static void apply_frame_limit_setting(void) {}
static void apply_borderless_acres_setting(void) {}
static void apply_solid_buildings_setting(void) {}
void pc_settings_apply(void) { apply_calls++; }
void pc_settings_cycle_resolution(int* w, int* h, int dir) {
    (void)w; (void)h; (void)dir;
    abort(); /* No display settings should be reachable from the VR page. */
}
void pc_keybindings_save(void) { binding_calls++; }
void pc_keybindings_reset_defaults(void) { binding_calls++; }
static Uint32 SDL_GetTicks(void) { return 1000; }

#include "vr_settings_menu_source.inc"

static int checks, failures;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

static void select_row(int row) {
    for (int i = 0; i < 10; i++) pc_settings_menu_nav_up();
    for (int i = 0; i < row; i++) pc_settings_menu_nav_down();
    CHECK(s_sel == row);
}

static void verify_saved(const PCSettings* expected) {
    /* Loading an actual saved file also checks the settings parser retains
     * unrelated fields; compare the whole structure, not just menu fields. */
    memset(&g_pc_settings, 0x55, sizeof(g_pc_settings));
    pc_settings_load();
    CHECK(memcmp(&g_pc_settings, expected, sizeof(*expected)) == 0);
}

int main(void) {
    PCSettings initial = g_pc_settings;
    initial.fp_snap_degrees = 23; /* Supported custom setting, no preset. */
    initial.master_volume = 70;
    initial.vr_empty_hands = 0;
    initial.vr_motion_swing = 1;
    g_pc_settings = initial;
    pc_settings_save();
    pc_settings_menu_enter_vr();
    CHECK(pc_settings_menu_active() && s_vr_only && s_sel == 0);
    CHECK(s_sub == SUB_SETTINGS && cur_item_count() == 4);
    CHECK(!s_pending_dirty && memcmp(&s_pending, &initial, sizeof(initial)) == 0);
    CHECK(cur_tab()->items[0].id == ITEM_VR_EMPTY_HANDS);
    CHECK(cur_tab()->items[1].id == ITEM_VR_TURNING);
    CHECK(cur_tab()->items[2].id == ITEM_VR_MOTION_SWINGS);
    CHECK(cur_tab()->items[3].id == ITEM_MASTER_VOLUME);
    char buf[64];
    item_format(ITEM_VR_TURNING, buf, sizeof(buf));
    CHECK(strcmp(buf, "< Snap 23 >") == 0);
    for (int i = 0; i < 10; i++) pc_settings_menu_nav_up();
    CHECK(s_sel == 0); /* There is no desktop tab row above this page. */
    for (int i = 0; i < 10; i++) pc_settings_menu_nav_down();
    CHECK(s_sel == 5 && idx_apply() == 4 && idx_back() == 5);
    pc_settings_menu_nav_left();
    pc_settings_menu_nav_right();
    CHECK(!s_pending_dirty && s_tab == 0);
    CHECK(pc_settings_menu_confirm() == 0 && !pc_settings_menu_active());

    /* Editing another row and applying must preserve a custom turn angle. */
    pc_settings_menu_enter_vr();
    pc_settings_menu_confirm();
    CHECK(s_pending.vr_empty_hands == 1 && g_pc_settings.vr_empty_hands == 0);
    CHECK(s_pending_dirty && item_changed(ITEM_VR_EMPTY_HANDS));
    select_row(4);
    CHECK(pc_settings_menu_confirm() == 1 && !s_pending_dirty);
    PCSettings expected = initial;
    expected.vr_empty_hands = 1;
    CHECK(memcmp(&g_pc_settings, &expected, sizeof(expected)) == 0);
    CHECK(apply_calls == 0 && s_sub == SUB_SETTINGS && s_res_deadline == 0);
    verify_saved(&expected);
    CHECK(pc_settings_menu_cancel() == 0);

    /* Every configured angle is preserved until the Turning row is edited. */
    for (int angle = 0; angle <= 90; angle++) {
        g_pc_settings.fp_snap_degrees = angle;
        pc_settings_menu_enter_vr();
        CHECK(s_pending.fp_snap_degrees == angle && !s_pending_dirty);
        select_row(1);
        pc_settings_menu_nav_right();
        int right = angle < 30 ? 30 : angle < 45 ? 45 : angle < 90 ? 90 : 0;
        CHECK(s_pending.fp_snap_degrees == right);
        CHECK(g_pc_settings.fp_snap_degrees == angle);
        pc_settings_menu_enter_vr();
        select_row(1);
        pc_settings_menu_nav_left();
        int left = angle == 0 ? 90 : angle <= 30 ? 0 : angle <= 45 ? 30 : 45;
        CHECK(s_pending.fp_snap_degrees == left);
    }

    g_pc_settings = initial;
    pc_settings_menu_enter_vr();
    select_row(1);
    pc_settings_menu_nav_left();
    item_format(ITEM_VR_TURNING, buf, sizeof(buf));
    CHECK(strcmp(buf, "< Smooth >") == 0);
    select_row(2);
    pc_settings_menu_confirm();
    CHECK(s_pending.vr_motion_swing == 0 && g_pc_settings.vr_motion_swing == 1);
    item_format(ITEM_VR_MOTION_SWINGS, buf, sizeof(buf));
    CHECK(strcmp(buf, "< Off >") == 0);
    select_row(3);
    for (int i = 0; i < 20; i++) pc_settings_menu_nav_right();
    CHECK(s_pending.master_volume == 100);
    for (int i = 0; i < 20; i++) pc_settings_menu_nav_left();
    CHECK(s_pending.master_volume == 0);
    pc_settings_menu_nav_right();
    CHECK(g_pc_settings.master_volume == 70 && s_pending.master_volume == 10);
    /* An unrelated live update after opening must not be replaced by Apply. */
    g_pc_settings.window_width = 1280;
    g_pc_settings.window_height = 720;
    g_pc_settings.vsync = !initial.vsync;
    g_pc_settings.vr_tool_pitch = 17;
    expected = g_pc_settings;
    expected.fp_snap_degrees = 0;
    expected.vr_motion_swing = 0;
    expected.master_volume = 10;
    select_row(4);
    CHECK(pc_settings_menu_confirm() == 1);
    CHECK(memcmp(&g_pc_settings, &expected, sizeof(expected)) == 0);
    CHECK(memcmp(&s_pending, &expected, sizeof(expected)) == 0);
    CHECK(!s_pending_dirty && apply_calls == 0 && binding_calls == 0);
    verify_saved(&expected);

    /* B and Resume share Keep editing / Discard, with the safe default. */
    select_row(0);
    pc_settings_menu_confirm();
    CHECK(pc_settings_menu_cancel() == 1 && s_sub == SUB_CONFIRM_BACK && s_back_sel == 0);
    CHECK(pc_settings_menu_confirm() == 1 && s_sub == SUB_SETTINGS && s_pending_dirty);
    select_row(5);
    CHECK(pc_settings_menu_confirm() == 1 && s_sub == SUB_CONFIRM_BACK);
    CHECK(pc_settings_menu_cancel() == 1 && s_sub == SUB_SETTINGS && s_pending_dirty);
    CHECK(pc_settings_menu_cancel() == 1 && s_sub == SUB_CONFIRM_BACK);
    pc_settings_menu_nav_right();
    CHECK(pc_settings_menu_confirm() == 0 && !pc_settings_menu_active());
    CHECK(memcmp(&g_pc_settings, &expected, sizeof(expected)) == 0);
    verify_saved(&expected);
    pc_settings_menu_enter_vr();
    CHECK(!s_pending_dirty && s_pending.vr_empty_hands == expected.vr_empty_hands);
    CHECK(pc_settings_menu_cancel() == 0);

    /* Returning to the ordinary menu restores its tabs and standard Apply. */
    pc_settings_menu_enter();
    CHECK(!s_vr_only && s_sel == -1 && cur_tab() == &s_tabs[0]);
    pc_settings_menu_nav_right();
    CHECK(s_tab == 1 && strcmp(cur_tab()->name, "Audio") == 0);
    pc_settings_menu_confirm();
    pc_settings_menu_confirm();
    CHECK(s_pending_dirty);
    pc_settings_menu_nav_down();
    CHECK(pc_settings_menu_confirm() == 1 && apply_calls == 1 && !s_pending_dirty);
    CHECK(pc_settings_menu_cancel() == 0 && binding_calls == 0);
    printf("VR compact settings menu: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
