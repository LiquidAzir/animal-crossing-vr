#include <math.h>
#include <stdio.h>

static int entered_vr, nav[6], keep_open = 1, capture;
int g_pc_running = 1;
void pc_settings_menu_enter(void) {}
void pc_settings_menu_enter_vr(void) { entered_vr++; }
int pc_settings_menu_nav_up(void) { nav[0]++; return 1; }
int pc_settings_menu_nav_down(void) { nav[1]++; return 1; }
int pc_settings_menu_nav_left(void) { nav[2]++; return 1; }
int pc_settings_menu_nav_right(void) { nav[3]++; return 1; }
int pc_settings_menu_confirm(void) { nav[4]++; return keep_open; }
int pc_settings_menu_cancel(void) { nav[5]++; return keep_open; }
int pc_settings_menu_capture_blocking(void) { return capture; }

#include "pause_source.inc"

static int checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; \
    fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); } } while (0)
static void neutral(void) { CHECK(pc_pause_menu_vr_input(0, 0, 0, 0)); }

int main(void) {
    CHECK(!pc_pause_menu_vr_input(0, 0, 0, 0));
    g_pc_title_main_menu_visible = 1;
    CHECK(!pc_pause_menu_open_vr_settings() && !g_pc_paused && !entered_vr);
    g_pc_title_main_menu_visible = 0; g_pc_nes_active = 1;
    CHECK(!pc_pause_menu_open_vr_settings() && !g_pc_paused && !entered_vr);
    g_pc_nes_active = 0;
    CHECK(pc_pause_menu_open_vr_settings() && g_pc_paused && entered_vr == 1);
    CHECK(cur_page == PAGE_SETTINGS);
    CHECK(!pc_pause_menu_open_vr_settings() && entered_vr == 1);
    CHECK(pc_pause_menu_vr_input(0, -1, 1, 1));
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0));
    CHECK(!nav[1] && !nav[4] && !nav[5]);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0));
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0)); /* duplicate PADRead */
    CHECK(nav[4] == 1);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0));
    CHECK(nav[4] == 2);
    neutral();
    for (int i = 0; i < 5; ++i) CHECK(pc_pause_menu_vr_input(0, -1, 0, 0));
    CHECK(nav[1] == 1);
    CHECK(pc_pause_menu_vr_input(0, -0.4f, 0, 0));
    CHECK(pc_pause_menu_vr_input(0, -1, 0, 0) && nav[1] == 1);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, -1, 0, 0) && nav[1] == 2);
    neutral();
    CHECK(pc_pause_menu_vr_input(-1, 1, 0, 0));
    CHECK(nav[0] == 1 && nav[2] == 0);
    neutral();
    CHECK(pc_pause_menu_vr_input(NAN, NAN, 0, 0));
    CHECK(pc_pause_menu_vr_input(INFINITY, INFINITY, 0, 0));
    CHECK(nav[0] == 1 && !nav[2] && !nav[3]);
    neutral();
    CHECK(pc_pause_menu_vr_input(NAN, 1, 1, 0));
    CHECK(nav[0] == 1 && nav[4] == 2);
    neutral();
    CHECK(pc_pause_menu_vr_input(-1, INFINITY, 0, 1));
    CHECK(!nav[2] && !nav[5] && g_pc_paused);
    neutral();
    CHECK(pc_pause_menu_vr_input(-1, 0, 0, 0) && nav[2] == 1);
    neutral();
    CHECK(pc_pause_menu_vr_input(1, 0, 1, 0));
    CHECK(pc_pause_menu_vr_input(1, 0, 0, 0));
    CHECK(nav[3] == 0); /* no queued stick action after confirm */
    neutral(); capture = 1;
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0));
    CHECK(nav[4] == 3);
    capture = 0; neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1) && g_pc_paused);
    CHECK(nav[5] == 1); /* unsaved-edits confirmation keeps pause */
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1) && nav[5] == 1);
    neutral(); keep_open = 0;
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1));
    CHECK(!g_pc_paused && g_pc_pause_input_drain && nav[5] == 2);
    CHECK(!pc_pause_menu_vr_input(0, 0, 0, 1));
    CHECK(pc_pause_menu_open_vr_settings());
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1) && g_pc_paused);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 1, 0)); /* Resume */
    CHECK(!g_pc_paused && g_pc_pause_input_drain);
    pc_pause_menu_toggle(); /* regular pause keeps its original host */
    main_sel = 1; main_activate();
    CHECK(cur_page == PAGE_SETTINGS && !s_vr_settings_host);
    CHECK(!pc_pause_menu_open_vr_settings() && cur_page == PAGE_SETTINGS);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1));
    CHECK(g_pc_paused && cur_page == PAGE_MAIN);
    neutral();
    CHECK(pc_pause_menu_vr_input(0, 0, 0, 1) && !g_pc_paused);
    printf("VR pause host: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
