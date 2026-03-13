#ifndef PC_KEYBINDINGS_H
#define PC_KEYBINDINGS_H

#include <SDL2/SDL_scancode.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* buttons */
    SDL_Scancode a;
    SDL_Scancode b;
    SDL_Scancode x;
    SDL_Scancode y;
    SDL_Scancode start;
    SDL_Scancode z;
    SDL_Scancode l;
    SDL_Scancode r;

    /* main stick */
    SDL_Scancode stick_up;
    SDL_Scancode stick_down;
    SDL_Scancode stick_left;
    SDL_Scancode stick_right;

    /* C-stick */
    SDL_Scancode cstick_up;
    SDL_Scancode cstick_down;
    SDL_Scancode cstick_left;
    SDL_Scancode cstick_right;

    /* D-pad */
    SDL_Scancode dpad_up;
    SDL_Scancode dpad_down;
    SDL_Scancode dpad_left;
    SDL_Scancode dpad_right;
} PCKeybindings;

extern PCKeybindings g_pc_keybindings;

void pc_keybindings_load(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_KEYBINDINGS_H */
