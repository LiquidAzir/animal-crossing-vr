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
    int msaa;             /* 0=off, 2/4/8=samples */
} PCSettings;

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
