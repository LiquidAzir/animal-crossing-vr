#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int window_width;
    int window_height;
    int fullscreen;       /* 0=windowed, 1=fullscreen, 2=borderless */
    int vsync;            /* 0=off, 1=on */
    int max_fps;          /* 0=uncapped, otherwise frame limiter target */
    int msaa;             /* 0=off, 2/4/8=samples */
    int texture_filtering; /* 0=force nearest-neighbor, 1=use the game's texture filtering */
    int skybox;          /* outdoor gradient/cloud sky; 0 restores stock background */
    int preload_textures; /* 0=off (load on demand), 1=on (load all at startup), 2=on + cache file */
    int disable_resetti;  /* 0=normal (Resetti appears on reset), 1=disable reset penalty */
    int disable_shop_visitor_req; /* 0=normal (Nookington's needs a foreign-town shopper), 1=skip requirement */
    int borderless_acres; /* 0=original acre transitions (faster, draws less), 1=continuous camera/movement */
    int nes_aspect;       /* NES emulator aspect: 0=fullscreen stretch, 1=4:3 pillarbox (default) */
    int master_volume;    /* Applied at the PC audio output, 0-100 (default 100) */
    int stick_deadzone;   /* Gamepad main stick deadzone, percent 0-40 (default 12) */
    int cstick_deadzone;  /* Gamepad C-stick deadzone, percent 0-40 (default 12) */
    int vr_mode;          /* 0=off, 1=auto (VR when headset present), 2=force on */
    int vr_world_scale;   /* millimeters per game unit (default 10 = 1 tile -> 40 cm) */
    int vr_ui_distance;   /* UI panel distance in cm (default 200) */
    int vr_ui_size;       /* UI panel width in cm (default 240) */
    int vr_height_offset; /* raise/lower viewpoint in cm (default 0) */
    int fp_mode;          /* start in first person: 0=no, 1=yes (F5 toggles) */
    int fp_eye_height;    /* first-person eye height above the player's feet, game units (default 52) */
    int fp_snap_degrees;  /* VR turning: 0 = smooth (default); >0 = snap angle in degrees */
    int vr_fp_world_scale;/* mm per game unit in first person (default 25 = life-size-ish) */
    int vr_solid_buildings; /* fill in the un-authored far side of buildings (default 1) */
    int vr_solid_shell;     /* legacy shell size, 50..100 (default 97); fitted structure patches ignore it */
    int vr_draw_radius;   /* VR terrain: 0 = draw the whole town (default), N = acres around the player */
    int vr_town_residency;/* VR: keep the whole town's structures/villagers spawned (default 1) */
    int vr_motion_swing;  /* VR FP: swinging the right controller triggers the tool (default 1) */
    int vr_tool_on_hand;  /* VR FP: render the held tool at the real controller pose (default 1) */
    int vr_tool_pitch;    /* tool-on-hand pitch adjustment, degrees (default 0) */
    int vr_empty_hands;   /* VR FP: floating hands at empty controllers, live toggle (default 0) */
} PCSettings;

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);
void pc_settings_cycle_resolution(int* width, int* height, int dir);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
