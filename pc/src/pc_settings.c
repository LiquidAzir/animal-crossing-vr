/* pc_settings.c - runtime settings loaded from settings.ini */
#include "pc_settings.h"
#include "pc_platform.h"
#include "pc_vr.h"
#include "m_player_lib.h"
#include "ac_birth_control.h"

PCSettings g_pc_settings = {
    .window_width  = PC_SCREEN_WIDTH,
    .window_height = PC_SCREEN_HEIGHT,
    .fullscreen    = 0,
    .vsync         = 0,
    .max_fps       = 60,
    .msaa          = 4,
    .texture_filtering = 1,
    .skybox = 1,
    .preload_textures = 0,
    .disable_resetti = 0,
    .disable_shop_visitor_req = 0,
    .borderless_acres = 1,
    .nes_aspect = 1,
    .master_volume = 100,
    .stick_deadzone = 12,
    .cstick_deadzone = 12,
    .vr_mode = 1,
    .vr_world_scale = 10,
    .vr_ui_distance = 200,
    .vr_ui_size = 240,
    .vr_height_offset = 0,
    .fp_mode = 0,
    .fp_eye_height = 52,
    .fp_snap_degrees = 0,
    .vr_fp_world_scale = 25,
    .vr_solid_buildings = 1,
    .vr_solid_shell = 97,
    .vr_draw_radius = 0,
    .vr_town_residency = 1,
    .vr_motion_swing = 1,
    .vr_tool_on_hand = 1,
    .vr_tool_pitch = 0,
    .vr_empty_hands = 0,
};

static const char* SETTINGS_FILE = "settings.ini";

static const char* DEFAULT_SETTINGS =
    "[Graphics]\n"
    "# Window size (ignored in fullscreen)\n"
    "window_width = 640\n"
    "window_height = 480\n"
    "\n"
    "# 0 = windowed, 1 = fullscreen, 2 = borderless fullscreen\n"
    "fullscreen = 0\n"
    "\n"
    "# Vertical sync: 0 = off, 1 = on\n"
    "vsync = 0\n"
    "\n"
    "# Max FPS: 60, 120, 240, or 0 for uncapped\n"
    "max_fps = 60\n"
    "\n"
    "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n"
    "msaa = 4\n"
    "\n"
    "# Texture filtering: 0 = nearest-neighbor, 1 = use the game's filtering\n"
    "texture_filtering = 1\n"
    "\n"
    "# Outdoor gradient and clouds: 0 = stock background, 1 = skybox\n"
    "skybox = 1\n"
    "\n"
    "[Enhancements]\n"
    "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n"
    "preload_textures = 0\n"
    "\n"
    "[Gameplay]\n"
    "# Disable Mr. Resetti: 0 = normal, 1 = disable\n"
    "disable_resetti = 0\n"
    "\n"
    "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n"
    "disable_shop_visitor_req = 0\n"
    "\n"
    "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n"
    "borderless_acres = 1\n"
    "\n"
    "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n"
    "nes_aspect = 1\n"
    "\n"
    "[Audio]\n"
    "# Master output volume as a percentage (0-100)\n"
    "master_volume = 100\n"
    "\n"
    "[Input]\n"
    "# Gamepad stick deadzones as a percentage (0-40)\n"
    "stick_deadzone = 12\n"
    "cstick_deadzone = 12\n"
    "\n"
    "[VR]\n"
    "# SteamVR mode: 0 = off, 1 = auto (VR when a headset is present), 2 = force on\n"
    "vr_mode = 1\n"
    "\n"
    "# World scale in millimeters per game unit (10 = one 40-unit tile is 40 cm; smaller = more miniature)\n"
    "vr_world_scale = 10\n"
    "\n"
    "# UI panel distance (cm) and width (cm)\n"
    "vr_ui_distance = 200\n"
    "vr_ui_size = 240\n"
    "\n"
    "# Raise (+) or lower (-) your viewpoint in cm\n"
    "vr_height_offset = 0\n"
    "\n"
    "[FirstPerson]\n"
    "# Start in first-person camera: 0 = normal camera, 1 = first person (F5 toggles in-game)\n"
    "fp_mode = 0\n"
    "\n"
    "# Eye height above the player's feet, in game units (one ground tile = 40)\n"
    "fp_eye_height = 52\n"
    "\n"
    "# First-person VR turning: 0 = smooth (default); set e.g. 45 for snap turns\n"
    "fp_snap_degrees = 0\n"
    "\n"
    "# VR world scale in first person, mm per game unit (25 = one tile is 1 m)\n"
    "vr_fp_world_scale = 25\n"
    "\n"
    "# Fill in the far side of buildings, which the original game never modelled\n"
    "# (you could not walk behind them). 0 = off, 1 = on. VR and first person.\n"
    "vr_solid_buildings = 1\n"
    "\n"
    "# Legacy fill-in size for remaining structures (50-100). Fitted house,\n"
    "# tailor, police, fountain, museum and post-office repairs ignore it.\n"
    "vr_solid_shell = 97\n"
    "\n"
    "# VR / first person terrain draw distance: 0 = whole town at once (default),\n"
    "# or a radius in acres (e.g. 3) if your machine needs the headroom\n"
    "vr_draw_radius = 0\n"
    "\n"
    "# VR / first person: keep every building and prop in town loaded at once (0/1)\n"
    "# (restart to apply)\n"
    "vr_town_residency = 1\n"
    "\n"
    "# VR first person: swing the right controller to use the held tool (0/1)\n"
    "vr_motion_swing = 1\n"
    "\n"
    "# VR first person: render the held tool at your real controller pose (0/1)\n"
    "vr_tool_on_hand = 1\n"
    "\n"
    "# Tool-on-hand pitch adjustment in degrees (-90..90, restart to apply)\n"
    "vr_tool_pitch = 0\n"
    "\n"
    "# VR first person: show floating hands at empty controllers (0/1, no restart)\n"
    "vr_empty_hands = 0\n";

static const char* skip_ws(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void trim_end(char* s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

static void apply_setting(const char* key, const char* value) {
    int val = atoi(value);

    if (strcmp(key, "window_width") == 0) {
        if (val >= 640) g_pc_settings.window_width = val;
    } else if (strcmp(key, "window_height") == 0) {
        if (val >= 480) g_pc_settings.window_height = val;
    } else if (strcmp(key, "fullscreen") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.fullscreen = val;
    } else if (strcmp(key, "vsync") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vsync = val;
    } else if (strcmp(key, "max_fps") == 0) {
        if (val >= 0) {
            g_pc_settings.max_fps = val;
        }
    } else if (strcmp(key, "msaa") == 0) {
        if (val == 0 || val == 2 || val == 4 || val == 8)
            g_pc_settings.msaa = val;
    } else if (strcmp(key, "texture_filtering") == 0) {
        if (val == 0 || val == 1) g_pc_settings.texture_filtering = val;
    } else if (strcmp(key, "preload_textures") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.preload_textures = val;
    } else if (strcmp(key, "disable_resetti") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_resetti = val;
    } else if (strcmp(key, "disable_shop_visitor_req") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_shop_visitor_req = val;
    } else if (strcmp(key, "borderless_acres") == 0) {
        if (val == 0 || val == 1) g_pc_settings.borderless_acres = val;
    } else if (strcmp(key, "nes_aspect") == 0) {
        if (val == 0 || val == 1) g_pc_settings.nes_aspect = val;
    } else if (strcmp(key, "master_volume") == 0) {
        if (val >= 0 && val <= 100) g_pc_settings.master_volume = val;
    } else if (strcmp(key, "stick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.stick_deadzone = val;
    } else if (strcmp(key, "cstick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.cstick_deadzone = val;
    } else if (strcmp(key, "vr_mode") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.vr_mode = val;
    } else if (strcmp(key, "vr_world_scale") == 0) {
        if (val >= 1 && val <= 1000) g_pc_settings.vr_world_scale = val;
    } else if (strcmp(key, "vr_ui_distance") == 0) {
        if (val >= 50 && val <= 1000) g_pc_settings.vr_ui_distance = val;
    } else if (strcmp(key, "vr_ui_size") == 0) {
        if (val >= 50 && val <= 1000) g_pc_settings.vr_ui_size = val;
    } else if (strcmp(key, "vr_height_offset") == 0) {
        if (val >= -300 && val <= 300) g_pc_settings.vr_height_offset = val;
    } else if (strcmp(key, "skybox") == 0) {
        if (val == 0 || val == 1) g_pc_settings.skybox = val;
    } else if (strcmp(key, "fp_mode") == 0) {
        if (val == 0 || val == 1) g_pc_settings.fp_mode = val;
    } else if (strcmp(key, "fp_eye_height") == 0) {
        if (val >= 10 && val <= 200) g_pc_settings.fp_eye_height = val;
    } else if (strcmp(key, "fp_snap_degrees") == 0) {
        if (val >= 0 && val <= 90) g_pc_settings.fp_snap_degrees = val;
    } else if (strcmp(key, "vr_fp_world_scale") == 0) {
        if (val >= 1 && val <= 1000) g_pc_settings.vr_fp_world_scale = val;
    } else if (strcmp(key, "vr_solid_buildings") == 0 || strcmp(key, "solid_buildings") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vr_solid_buildings = val;
    } else if (strcmp(key, "vr_solid_shell") == 0 || strcmp(key, "solid_shell") == 0) {
        if (val >= 50 && val <= 100) g_pc_settings.vr_solid_shell = val;
    } else if (strcmp(key, "vr_draw_radius") == 0 || strcmp(key, "draw_radius") == 0) {
        if (val >= 0 && val <= 10) g_pc_settings.vr_draw_radius = val;
    } else if (strcmp(key, "vr_town_residency") == 0 || strcmp(key, "town_residency") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vr_town_residency = val;
    } else if (strcmp(key, "vr_motion_swing") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vr_motion_swing = val;
    } else if (strcmp(key, "vr_tool_on_hand") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vr_tool_on_hand = val;
    } else if (strcmp(key, "vr_tool_pitch") == 0) {
        if (val >= -90 && val <= 90) g_pc_settings.vr_tool_pitch = val;
    } else if (strcmp(key, "vr_empty_hands") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vr_empty_hands = val;
    }
}

static void apply_frame_limit_setting(void) {
    int max_fps;

    if (g_pc_frame_limit_override >= 0) {
        g_pc_settings.max_fps = g_pc_frame_limit_override;
        g_pc_frame_limit_override = -1;
    }

    max_fps = g_pc_settings.max_fps;

    if (max_fps <= 0) {
        max_fps = 0;
    }

    /* VR paces via WaitGetPoses — the timer limiter must stay off */
    g_frame_limiter = pc_vr_active() ? 0 : (u32)max_fps;
}

static void apply_borderless_acres_setting(void) {
    int enabled = g_pc_settings.borderless_acres != 0;

    if (enabled && !g_mPlib_wade_disabled) {
        aBC_RequestNearbyRefresh();
    }
    g_mPlib_wade_disabled = enabled;
}

static void write_defaults(const char* path) {
    FILE* f = fopen(path, "w");
    if (f) {
        fputs(DEFAULT_SETTINGS, f);
        fclose(f);
    }
}

void pc_settings_save(void) {
    FILE* f = fopen(SETTINGS_FILE, "w");
    if (!f) {
        printf("[Settings] Failed to write %s\n", SETTINGS_FILE);
        return;
    }
    fprintf(f, "[Graphics]\n");
    fprintf(f, "# Window size (ignored in fullscreen)\n");
    fprintf(f, "window_width = %d\n", g_pc_settings.window_width);
    fprintf(f, "window_height = %d\n", g_pc_settings.window_height);
    fprintf(f, "\n");
    fprintf(f, "# 0 = windowed, 1 = fullscreen, 2 = borderless fullscreen\n");
    fprintf(f, "fullscreen = %d\n", g_pc_settings.fullscreen);
    fprintf(f, "\n");
    fprintf(f, "# Vertical sync: 0 = off, 1 = on\n");
    fprintf(f, "vsync = %d\n", g_pc_settings.vsync);
    fprintf(f, "\n");
    fprintf(f, "# Max FPS: 60, 120, 240, or 0 for uncapped\n");
    fprintf(f, "max_fps = %d\n", g_pc_settings.max_fps);
    fprintf(f, "\n");
    fprintf(f, "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n");
    fprintf(f, "msaa = %d\n", g_pc_settings.msaa);
    fprintf(f, "\n");
    fprintf(f, "# Texture filtering: 0 = nearest-neighbor, 1 = use the game's filtering\n");
    fprintf(f, "texture_filtering = %d\n", g_pc_settings.texture_filtering);
    fprintf(f, "\n");
    fprintf(f, "# Outdoor gradient and clouds: 0 = stock background, 1 = skybox\n");
    fprintf(f, "skybox = %d\n\n", g_pc_settings.skybox);
    fprintf(f, "[Enhancements]\n");
    fprintf(f, "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n");
    fprintf(f, "preload_textures = %d\n", g_pc_settings.preload_textures);
    fprintf(f, "\n");
    fprintf(f, "[Gameplay]\n");
    fprintf(f, "# Disable Mr. Resetti: 0 = normal, 1 = disable\n");
    fprintf(f, "disable_resetti = %d\n", g_pc_settings.disable_resetti);
    fprintf(f, "\n");
    fprintf(f, "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n");
    fprintf(f, "disable_shop_visitor_req = %d\n", g_pc_settings.disable_shop_visitor_req);
    fprintf(f, "\n");
    fprintf(f, "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n");
    fprintf(f, "borderless_acres = %d\n", g_pc_settings.borderless_acres);
    fprintf(f, "\n");
    fprintf(f, "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n");
    fprintf(f, "nes_aspect = %d\n", g_pc_settings.nes_aspect);
    fprintf(f, "\n");
    fprintf(f, "[Audio]\n");
    fprintf(f, "# Master output volume as a percentage (0-100)\n");
    fprintf(f, "master_volume = %d\n", g_pc_settings.master_volume);
    fprintf(f, "\n");
    fprintf(f, "[Input]\n");
    fprintf(f, "# Gamepad stick deadzones as a percentage (0-40)\n");
    fprintf(f, "stick_deadzone = %d\n", g_pc_settings.stick_deadzone);
    fprintf(f, "cstick_deadzone = %d\n", g_pc_settings.cstick_deadzone);
    fprintf(f, "\n");
    fprintf(f, "[VR]\n");
    fprintf(f, "# SteamVR mode: 0 = off, 1 = auto (VR when a headset is present), 2 = force on\n");
    fprintf(f, "vr_mode = %d\n", g_pc_settings.vr_mode);
    fprintf(f, "\n");
    fprintf(f, "# World scale in millimeters per game unit (10 = one 40-unit tile is 40 cm; smaller = more miniature)\n");
    fprintf(f, "vr_world_scale = %d\n", g_pc_settings.vr_world_scale);
    fprintf(f, "\n");
    fprintf(f, "# UI panel distance (cm) and width (cm)\n");
    fprintf(f, "vr_ui_distance = %d\n", g_pc_settings.vr_ui_distance);
    fprintf(f, "vr_ui_size = %d\n", g_pc_settings.vr_ui_size);
    fprintf(f, "\n");
    fprintf(f, "# Raise (+) or lower (-) your viewpoint in cm\n");
    fprintf(f, "vr_height_offset = %d\n", g_pc_settings.vr_height_offset);
    fprintf(f, "\n");
    fprintf(f, "[FirstPerson]\n");
    fprintf(f, "# Start in first-person camera: 0 = normal camera, 1 = first person (F5 toggles in-game)\n");
    fprintf(f, "fp_mode = %d\n", g_pc_settings.fp_mode);
    fprintf(f, "\n");
    fprintf(f, "# Eye height above the player's feet, in game units (one ground tile = 40)\n");
    fprintf(f, "fp_eye_height = %d\n", g_pc_settings.fp_eye_height);
    fprintf(f, "\n");
    fprintf(f, "# First-person VR turning: 0 = smooth (default); set e.g. 45 for snap turns\n");
    fprintf(f, "fp_snap_degrees = %d\n", g_pc_settings.fp_snap_degrees);
    fprintf(f, "\n");
    fprintf(f, "# VR world scale in first person, mm per game unit (25 = one tile is 1 m)\n");
    fprintf(f, "vr_fp_world_scale = %d\n", g_pc_settings.vr_fp_world_scale);
    fprintf(f, "\n");
    fprintf(f, "# Fill in the far side of buildings, which the original game never modelled\n");
    fprintf(f, "# (you could not walk behind them). 0 = off, 1 = on. VR and first person.\n");
    fprintf(f, "vr_solid_buildings = %d\n", g_pc_settings.vr_solid_buildings);
    fprintf(f, "\n");
    fprintf(f, "# Legacy fill-in size for remaining structures (50-100). Fitted house,\n");
    fprintf(f, "# tailor, police, fountain, museum and post-office repairs ignore it.\n");
    fprintf(f, "vr_solid_shell = %d\n", g_pc_settings.vr_solid_shell);
    fprintf(f, "\n");
    fprintf(f, "# VR / first person terrain draw distance: 0 = whole town at once (default),\n");
    fprintf(f, "# or a radius in acres (e.g. 3) if your machine needs the headroom\n");
    fprintf(f, "vr_draw_radius = %d\n", g_pc_settings.vr_draw_radius);
    fprintf(f, "\n");
    fprintf(f, "# VR / first person: keep every building and prop in town loaded at once (0/1)\n");
    fprintf(f, "# (restart to apply)\n");
    fprintf(f, "vr_town_residency = %d\n", g_pc_settings.vr_town_residency);
    fprintf(f, "\n");
    fprintf(f, "# VR first person: swing the right controller to use the held tool (0/1)\n");
    fprintf(f, "vr_motion_swing = %d\n", g_pc_settings.vr_motion_swing);
    fprintf(f, "\n");
    fprintf(f, "# VR first person: render the held tool at your real controller pose (0/1)\n");
    fprintf(f, "vr_tool_on_hand = %d\n", g_pc_settings.vr_tool_on_hand);
    fprintf(f, "\n");
    fprintf(f, "# Tool-on-hand pitch adjustment in degrees (-90..90, restart to apply)\n");
    fprintf(f, "vr_tool_pitch = %d\n", g_pc_settings.vr_tool_pitch);
    fprintf(f, "\n");
    fprintf(f, "# VR first person: show floating hands at empty controllers (0/1, no restart)\n");
    fprintf(f, "vr_empty_hands = %d\n", g_pc_settings.vr_empty_hands);
    fclose(f);
    printf("[Settings] Saved %s\n", SETTINGS_FILE);
}

/* Accessor for TUs that can't include pc_settings.h (pc_nes_fixnes.c). */
int pc_settings_get_nes_aspect(void) {
    return g_pc_settings.nes_aspect;
}

/* --- Resolution preset table (shared) ---
 * Ordered by width then height. The desktop's native size is injected at
 * first use (de-duplicated against the static list) so the user can snap
 * to whatever their monitor is running. Multiple presets share widths now
 * (e.g. 1280x720 vs 1280x960), so cycling is index-based rather than the
 * old width-comparison. */
#define RES_MAX 32
static int res_w_tbl[RES_MAX];
static int res_h_tbl[RES_MAX];
static int res_count = 0;

static void add_preset(int w, int h) {
    if (res_count >= RES_MAX) return;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] == w && res_h_tbl[i] == h) return; /* de-dupe */
    }
    int at = res_count;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] > w || (res_w_tbl[i] == w && res_h_tbl[i] > h)) {
            at = i;
            break;
        }
    }
    for (int i = res_count; i > at; i--) {
        res_w_tbl[i] = res_w_tbl[i - 1];
        res_h_tbl[i] = res_h_tbl[i - 1];
    }
    res_w_tbl[at] = w;
    res_h_tbl[at] = h;
    res_count++;
}

static void ensure_presets(void) {
    if (res_count > 0) return;
    /* 4:3 */
    add_preset(640,  480);
    add_preset(800,  600);
    add_preset(960,  720);
    add_preset(1024, 768);
    add_preset(1152, 864);
    add_preset(1280, 960);
    add_preset(1400, 1050);
    add_preset(1600, 1200);
    /* 16:9 */
    add_preset(1280, 720);
    add_preset(1366, 768);
    add_preset(1600, 900);
    add_preset(1920, 1080);
    add_preset(2560, 1440);
    add_preset(3840, 2160);
    /* Desktop native (inserted sorted, de-duped against the list above). */
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
        add_preset(mode.w, mode.h);
    }
}

/* Find the closest preset by total pixel count. Used when the caller's
 * current (w, h) isn't in the list (e.g. custom settings.ini value). */
static int nearest_index(int w, int h) {
    long long want = (long long)w * h;
    int best = 0;
    long long best_diff = (long long)1 << 62;
    for (int i = 0; i < res_count; i++) {
        long long diff = (long long)res_w_tbl[i] * res_h_tbl[i] - want;
        if (diff < 0) diff = -diff;
        if (diff < best_diff) { best_diff = diff; best = i; }
    }
    return best;
}

void pc_settings_cycle_resolution(int* width, int* height, int dir) {
    ensure_presets();
    int cur = -1;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] == *width && res_h_tbl[i] == *height) { cur = i; break; }
    }
    if (cur < 0) cur = nearest_index(*width, *height);

    if (dir > 0 && cur < res_count - 1) cur++;
    else if (dir < 0 && cur > 0) cur--;

    *width  = res_w_tbl[cur];
    *height = res_h_tbl[cur];
}

/* Read by the skeleton draw layer (src/c_keyframe.c). Kept here rather than
 * in pc_vr.cpp so the shell also works in flat first person. */
int g_pc_solid_buildings = 1;
int g_pc_solid_shell_pct = 97;

static void apply_solid_buildings_setting(void) {
    g_pc_solid_buildings = g_pc_settings.vr_solid_buildings != 0;
    g_pc_solid_shell_pct = g_pc_settings.vr_solid_shell;
    if (g_pc_solid_shell_pct < 50) g_pc_solid_shell_pct = 50;
    if (g_pc_solid_shell_pct > 100) g_pc_solid_shell_pct = 100;
}

void pc_settings_apply(void) {
    apply_frame_limit_setting();
    apply_borderless_acres_setting();
    apply_solid_buildings_setting();

    if (!g_pc_window) return;

    int w = g_pc_settings.window_width;
    int h = g_pc_settings.window_height;

    switch (g_pc_settings.fullscreen) {
        case 1: {
            /* Exclusive fullscreen at the user's chosen resolution. SDL
             * needs the display mode set before transitioning to
             * SDL_WINDOW_FULLSCREEN or it'll just use the desktop mode. */
            SDL_DisplayMode target = { 0 };
            target.w = w;
            target.h = h;
            int display_idx = SDL_GetWindowDisplayIndex(g_pc_window);
            if (display_idx < 0) display_idx = 0;
            SDL_DisplayMode closest;
            if (SDL_GetClosestDisplayMode(display_idx, &target, &closest)) {
                SDL_SetWindowDisplayMode(g_pc_window, &closest);
            }
            SDL_SetWindowFullscreen(g_pc_window, SDL_WINDOW_FULLSCREEN);
            break;
        }
        case 2: {
            /* Borderless window at the user's chosen size, centred. Exit
             * any fullscreen mode first (including FULLSCREEN_DESKTOP)
             * so the resize sticks. */
            SDL_SetWindowFullscreen(g_pc_window, 0);
            SDL_SetWindowBordered(g_pc_window, SDL_FALSE);
            SDL_SetWindowSize(g_pc_window, w, h);
            SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            break;
        }
        case 0:
        default: {
            SDL_SetWindowFullscreen(g_pc_window, 0);
            SDL_SetWindowBordered(g_pc_window, SDL_TRUE);
            SDL_SetWindowSize(g_pc_window, w, h);
            SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            break;
        }
    }

    /* Window vsync would fight WaitGetPoses pacing in VR */
    SDL_GL_SetSwapInterval(pc_vr_active() ? 0 : g_pc_settings.vsync);
    pc_platform_update_window_size();

    printf("[Settings] Applied: %dx%d fullscreen=%d vsync=%d max_fps=%d msaa=%d\n",
           g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.vsync, g_pc_settings.max_fps, g_pc_settings.msaa);
}

void pc_settings_load(void) {
    FILE* f = fopen(SETTINGS_FILE, "r");
    if (!f) {
        write_defaults(SETTINGS_FILE);
        apply_frame_limit_setting();
        apply_borderless_acres_setting();
        apply_solid_buildings_setting();
        printf("[Settings] Created default %s\n", SETTINGS_FILE);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        const char* p = skip_ws(line);

        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\n' || *p == '[')
            continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = (char*)skip_ws(line);
        trim_end(key);
        char* value = (char*)skip_ws(eq + 1);
        trim_end(value);

        if (*key && *value) {
            apply_setting(key, value);
        }
    }
    fclose(f);
    apply_frame_limit_setting();
    apply_borderless_acres_setting();
    apply_solid_buildings_setting();

    printf("[Settings] Loaded %s: %dx%d fullscreen=%d vsync=%d max_fps=%d msaa=%d preload_textures=%d borderless_acres=%d\n",
           SETTINGS_FILE, g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.vsync, g_pc_settings.max_fps, g_pc_settings.msaa,
           g_pc_settings.preload_textures, g_pc_settings.borderless_acres);
}
