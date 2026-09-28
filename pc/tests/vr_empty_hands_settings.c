/* Run only through run_vr_empty_hands_settings_tests.py: config I/O belongs in
 * its isolated output directory, not the game or install directory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_settings.h"

#define PC_SCREEN_WIDTH 640
#define PC_SCREEN_HEIGHT 480
typedef unsigned int Uint32;
static int apply_calls;
static void apply_frame_limit_setting(void) {}
static void apply_borderless_acres_setting(void) {}
static void apply_solid_buildings_setting(void) {}
void pc_settings_apply(void) { apply_calls++; }
void pc_settings_cycle_resolution(int* w, int* h, int dir) {
    (void)w; (void)h; (void)dir;
    abort(); /* This test must never accidentally modify display settings. */
}
static Uint32 SDL_GetTicks(void) { return 1000; }

#include "vr_empty_hands_settings_source.inc"

static int checks, failures;
#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

static void write_config(const char* text) {
    FILE* f = fopen("settings.ini", "w");
    if (!f) abort();
    fputs(text, f);
    fclose(f);
}

static int saved_value_is(int value) {
    char line[256], wanted[64];
    int matches = 0;
    FILE* f = fopen("settings.ini", "r");
    if (!f) return 0;
    snprintf(wanted, sizeof(wanted), "vr_empty_hands = %d\n", value);
    while (fgets(line, sizeof(line), f)) if (strcmp(line, wanted) == 0) matches++;
    fclose(f);
    return matches == 1;
}

int main(void) {
    PCSettings defaults = g_pc_settings;
    CHECK(defaults.vr_empty_hands == 1);
    CHECK(strstr(DEFAULT_SETTINGS, "vr_empty_hands = 1\n") != NULL);
    pc_settings_load(); /* Missing config writes default-on value. */
    CHECK(g_pc_settings.vr_empty_hands == 1);
    CHECK(saved_value_is(1));

    /* Legacy configs without the key inherit the enabled default. */
    write_config("[FirstPerson]\nvr_tool_on_hand = 1\nvr_tool_pitch = 12\n");
    g_pc_settings = defaults;
    pc_settings_load();
    CHECK(g_pc_settings.vr_empty_hands == 1);
    CHECK(g_pc_settings.vr_tool_pitch == 12);
    write_config("[FirstPerson]\n\tvr_empty_hands \t= 1 \t\nvr_tool_pitch = -8\n");
    pc_settings_load();
    CHECK(g_pc_settings.vr_empty_hands == 1);
    CHECK(g_pc_settings.vr_tool_pitch == -8);
    apply_setting("vr_empty_hands", "2");
    CHECK(g_pc_settings.vr_empty_hands == 1);
    apply_setting("vr_empty_hands", "-1");
    CHECK(g_pc_settings.vr_empty_hands == 1);
    /* An explicit saved OFF preference overrides the enabled default. */
    write_config("[FirstPerson]\nvr_empty_hands = 0\n");
    g_pc_settings = defaults;
    pc_settings_load();
    CHECK(g_pc_settings.vr_empty_hands == 0);
    apply_setting("vr_empty_hands", "2");
    CHECK(g_pc_settings.vr_empty_hands == 0);

    /* Save/load retains the new preference and every neighboring setting. */
    for (int value = 0; value <= 1; value++) {
        PCSettings expected = defaults;
        expected.vr_empty_hands = value;
        expected.vr_tool_pitch = 17;
        expected.master_volume = 70;
        g_pc_settings = expected;
        pc_settings_save();
        CHECK(saved_value_is(value));
        memset(&g_pc_settings, 0x55, sizeof(g_pc_settings));
        pc_settings_load();
        CHECK(memcmp(&g_pc_settings, &expected, sizeof(expected)) == 0);
    }

    int found = 0;
    for (int t = 0; t < TAB_COUNT; t++) {
        for (int i = 0; i < s_tabs[t].count; i++) {
            const Item* item = &s_tabs[t].items[i];
            if (item->id != ITEM_VR_EMPTY_HANDS) continue;
            found++;
            CHECK(strcmp(s_tabs[t].name, "Gameplay") == 0);
            CHECK(strcmp(item->label, "VR empty hands") == 0);
            CHECK(item->restart == 0);
        }
    }
    CHECK(found == 1);
    g_pc_settings = defaults;
    s_startup = g_pc_settings;
    snapshot();
    char display[32];
    item_format(ITEM_VR_EMPTY_HANDS, display, sizeof(display));
    CHECK(strcmp(display, "< On >") == 0);
    CHECK(!s_pending_dirty && !item_changed(ITEM_VR_EMPTY_HANDS));
    item_cycle(ITEM_VR_EMPTY_HANDS, -1);
    CHECK(s_pending.vr_empty_hands == 0 && g_pc_settings.vr_empty_hands == 1);
    CHECK(s_pending_dirty && item_changed(ITEM_VR_EMPTY_HANDS));
    item_format(ITEM_VR_EMPTY_HANDS, display, sizeof(display));
    CHECK(strcmp(display, "< Off >") == 0);
    item_cycle(ITEM_VR_EMPTY_HANDS, 1);
    CHECK(s_pending.vr_empty_hands == 1 && !s_pending_dirty);
    CHECK(!item_changed(ITEM_VR_EMPTY_HANDS));
    apply_pending();
    CHECK(apply_calls == 0);

    item_cycle(ITEM_VR_EMPTY_HANDS, 1);
    apply_pending();
    PCSettings disabled = defaults;
    disabled.vr_empty_hands = 0;
    CHECK(memcmp(&g_pc_settings, &disabled, sizeof(disabled)) == 0);
    CHECK(apply_calls == 1 && !s_pending_dirty);
    CHECK(!s_pending_restart && s_sub == SUB_SETTINGS);
    CHECK(saved_value_is(0));
    CHECK(!item_changed(ITEM_VR_EMPTY_HANDS));

    item_cycle(ITEM_VR_EMPTY_HANDS, -1);
    CHECK(s_pending.vr_empty_hands == 1 && s_pending_dirty);
    snapshot(); /* Discard pending toggle, leaving the applied value intact. */
    CHECK(s_pending.vr_empty_hands == 0 && !s_pending_dirty);
    CHECK(g_pc_settings.vr_empty_hands == 0 && apply_calls == 1);
    item_cycle(ITEM_VR_EMPTY_HANDS, -1);
    apply_pending();
    CHECK(memcmp(&g_pc_settings, &defaults, sizeof(defaults)) == 0);
    CHECK(apply_calls == 2 && !s_pending_dirty && !s_pending_restart);
    CHECK(saved_value_is(1));
    printf("VR empty-hand settings/menu: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
