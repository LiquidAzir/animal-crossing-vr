/* pc_pad.c - GC controller input via SDL gamepad + keyboard */
#include "pc_platform.h"

typedef struct {
    u16 button;
    s8  stickX;
    s8  stickY;
    s8  substickX;
    s8  substickY;
    u8  triggerL;
    u8  triggerR;
    u8  analogA;
    u8  analogB;
    s8  err;
} PADStatus;

#define PAD_CHAN0_BIT 0x80000000
#define PAD_CHAN1_BIT 0x40000000
#define PAD_CHAN2_BIT 0x20000000
#define PAD_CHAN3_BIT 0x10000000

#define PAD_BUTTON_LEFT   0x0001
#define PAD_BUTTON_RIGHT  0x0002
#define PAD_BUTTON_DOWN   0x0004
#define PAD_BUTTON_UP     0x0008
#define PAD_TRIGGER_Z     0x0010
#define PAD_TRIGGER_R     0x0020
#define PAD_TRIGGER_L     0x0040
#define PAD_BUTTON_A      0x0100
#define PAD_BUTTON_B      0x0200
#define PAD_BUTTON_X      0x0400
#define PAD_BUTTON_Y      0x0800
#define PAD_BUTTON_START  0x1000

/* analog stick constants */
#define STICK_MAGNITUDE     80
#define AXIS_DEADZONE       500
#define AXIS_CLAMP_MIN      (-32767) /* prevent s16 overflow on negate */
#define TRIGGER_THRESHOLD   100
#define RUMBLE_DURATION_MS  200

static SDL_GameController* g_controller = NULL;

void PADInit(void) {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                break;
            }
        }
    }
}

u32 PADRead(PADStatus* status) {
    memset(status, 0, sizeof(PADStatus) * 4);

    const u8* keys = SDL_GetKeyboardState(NULL);
    u16 buttons = 0;
    s8 stickX = 0, stickY = 0;
    s8 cstickX = 0, cstickY = 0;

    if (keys[SDL_SCANCODE_SPACE])   buttons |= PAD_BUTTON_A;
    if (keys[SDL_SCANCODE_LSHIFT])  buttons |= PAD_BUTTON_B;
    if (keys[SDL_SCANCODE_X])       buttons |= PAD_BUTTON_X;
    if (keys[SDL_SCANCODE_Y])       buttons |= PAD_BUTTON_Y;
    if (keys[SDL_SCANCODE_RETURN])  buttons |= PAD_BUTTON_START;
    if (keys[SDL_SCANCODE_Z])       buttons |= PAD_TRIGGER_Z;
    if (keys[SDL_SCANCODE_Q])       buttons |= PAD_TRIGGER_L;
    if (keys[SDL_SCANCODE_E])       buttons |= PAD_TRIGGER_R;

    /* WASD = main stick */
    if (keys[SDL_SCANCODE_W]) stickY += STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_S]) stickY -= STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_A]) stickX -= STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_D]) stickX += STICK_MAGNITUDE;

    /* arrows = C-stick */
    if (keys[SDL_SCANCODE_UP])    cstickY += STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_DOWN])  cstickY -= STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_LEFT])  cstickX -= STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_RIGHT]) cstickX += STICK_MAGNITUDE;

    /* IJKL = D-pad */
    if (keys[SDL_SCANCODE_I]) buttons |= PAD_BUTTON_UP;
    if (keys[SDL_SCANCODE_K]) buttons |= PAD_BUTTON_DOWN;
    if (keys[SDL_SCANCODE_J]) buttons |= PAD_BUTTON_LEFT;
    if (keys[SDL_SCANCODE_L]) buttons |= PAD_BUTTON_RIGHT;

    /* hotplug */
    if (!g_controller) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                g_controller = SDL_GameControllerOpen(i);
                if (g_controller) break;
            }
        }
    }

    if (g_controller) {
        if (!SDL_GameControllerGetAttached(g_controller)) {
            SDL_GameControllerClose(g_controller);
            g_controller = NULL;
        }
    }
    if (g_controller) {
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_A)) buttons |= PAD_BUTTON_A;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_B)) buttons |= PAD_BUTTON_B;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_X)) buttons |= PAD_BUTTON_X;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_Y)) buttons |= PAD_BUTTON_Y;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START)) buttons |= PAD_BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    buttons |= PAD_BUTTON_UP;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  buttons |= PAD_BUTTON_DOWN;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  buttons |= PAD_BUTTON_LEFT;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= PAD_BUTTON_RIGHT;

        s16 lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        s16 ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (lx < AXIS_CLAMP_MIN) lx = AXIS_CLAMP_MIN;
        if (ly < AXIS_CLAMP_MIN) ly = AXIS_CLAMP_MIN;
        if (abs(lx) > AXIS_DEADZONE) stickX = (s8)(lx >> 8);
        if (abs(ly) > AXIS_DEADZONE) stickY = (s8)(-(ly >> 8));

        s16 rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        s16 ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        if (rx < AXIS_CLAMP_MIN) rx = AXIS_CLAMP_MIN;
        if (ry < AXIS_CLAMP_MIN) ry = AXIS_CLAMP_MIN;
        if (abs(rx) > AXIS_DEADZONE) cstickX = (s8)(rx >> 8);
        if (abs(ry) > AXIS_DEADZONE) cstickY = (s8)(-(ry >> 8));

        u8 lt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
        u8 rt = (u8)(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);
        if (lt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_L;
        if (rt > TRIGGER_THRESHOLD) buttons |= PAD_TRIGGER_R;
        status[0].triggerL = lt;
        status[0].triggerR = rt;
    }

    status[0].button = buttons;
    status[0].stickX = stickX;
    status[0].stickY = stickY;
    status[0].substickX = cstickX;
    status[0].substickY = cstickY;
    status[0].err = 0; /* PAD_ERR_NONE */

    return PAD_CHAN0_BIT; /* Controller 1 connected */
}

void PADControlMotor(int chan, u32 command) {
    if (g_controller && chan == 0) {
        u16 intensity = (command == 1) ? 0xFFFF : 0;
        SDL_GameControllerRumble(g_controller, intensity, intensity, RUMBLE_DURATION_MS);
    }
}

void PADControlAllMotors(const u32* commands) {
    PADControlMotor(0, commands[0]);
}

void PADCleanup(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}

void PADReset(u32 mask) { (void)mask; }
void PADRecalibrate(u32 mask) { (void)mask; }
u32  PADSync(void) { return 0; }
void PADSetSpec(u32 spec) { (void)spec; }
BOOL PADSetAnalogMode(u32 mode) { return TRUE; }
void PADClamp(void* status) { (void)status; }
u32  PADGetType(s32 chan, u32* type) { if (type) *type = 0x09000000; return 0; }
