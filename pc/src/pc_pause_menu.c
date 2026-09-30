#include "pc_platform.h"
#include "pc_pause_menu.h"
#include "pc_settings_menu.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "m_font.h"
#include "m_rcp.h"
#include "graph.h"
#include "main.h"       /* SCREEN_WIDTH_F */

#include <stdio.h>
#include <string.h>

int g_pc_paused = 0;

/* Pausing is blocked while either is set: main menu (set by ac_animal_logo)
 * or NES running (its own pause, and pausing it crashes the game). */
int g_pc_title_main_menu_visible = 0;
int g_pc_nes_active = 0;

/* Drains the keys still held from confirming Resume so they don't leak into play. */
int g_pc_pause_input_drain = 0;

/* Pause menu pages */
typedef enum {
    PAGE_MAIN = 0,
    PAGE_SETTINGS = 1,
    PAGE_CONFIRM_QUIT = 2,
} PauseMenuPage;

#define MAIN_ITEM_COUNT     3
#define CONFIRM_ITEM_COUNT  2

static PauseMenuPage cur_page = PAGE_MAIN;
static int main_sel = 0;    /* 0=Resume, 1=Settings, 2=Quit Game */
static int confirm_sel = 0; /* 0=No (default), 1=Yes */
static int s_vr_settings_host = 0;
static int s_vr_nav_ready = 0;
static int s_vr_nav_latched = 0;
static int s_vr_confirm_prev = 0, s_vr_cancel_prev = 0;

void pc_pause_menu_toggle(void) {
    if (!g_pc_paused && (g_pc_title_main_menu_visible || g_pc_nes_active)) return;

    g_pc_paused = !g_pc_paused;
    s_vr_settings_host = 0;
    s_vr_nav_ready = 0;
    if (g_pc_paused) {
        cur_page = PAGE_MAIN;
        main_sel = 0;
    } else {
        g_pc_pause_input_drain = 1;
    }
}

int pc_pause_menu_open_vr_settings(void) {
    if (g_pc_paused || g_pc_title_main_menu_visible || g_pc_nes_active) return 0;
    pc_pause_menu_toggle();
    s_vr_settings_host = 1;
    cur_page = PAGE_SETTINGS;
    pc_settings_menu_enter_vr();
    return 1;
}

static void settings_page_closed(void) {
    cur_page = PAGE_MAIN;
    if (s_vr_settings_host) pc_pause_menu_toggle();
}

/* Input */

static void main_activate(void) {
    switch (main_sel) {
        case 0: /* Resume */
            pc_pause_menu_toggle();
            break;
        case 1: /* Settings */
            cur_page = PAGE_SETTINGS;
            pc_settings_menu_enter();
            break;
        case 2: /* Quit Game -> confirm page (default to No) */
            cur_page = PAGE_CONFIRM_QUIT;
            confirm_sel = 0;
            break;
    }
}

static void confirm_activate(void) {
    if (confirm_sel == 1) {
        printf("[PAUSE] Quit confirmed\n");
        g_pc_running = 0;
    } else {
        cur_page = PAGE_MAIN;
        main_sel = 0;
    }
}

/* Device-independent menu actions; keyboard and gamepad both map here. */
typedef enum {
    ACT_NONE = 0,
    ACT_UP,
    ACT_DOWN,
    ACT_LEFT,
    ACT_RIGHT,
    ACT_CONFIRM,
    ACT_CANCEL,
} MenuAction;

static void handle_action(MenuAction act) {
    /* Settings page is driven by pc_settings_menu. */
    if (cur_page == PAGE_SETTINGS) {
        switch (act) {
            case ACT_UP:    pc_settings_menu_nav_up();    break;
            case ACT_DOWN:  pc_settings_menu_nav_down();  break;
            case ACT_LEFT:  pc_settings_menu_nav_left();  break;
            case ACT_RIGHT: pc_settings_menu_nav_right(); break;
            case ACT_CONFIRM:
                if (!pc_settings_menu_confirm()) settings_page_closed();
                break;
            case ACT_CANCEL:
                if (!pc_settings_menu_cancel()) settings_page_closed();
                break;
            default: break;
        }
        return;
    }

    /* Pages owned here (Main, Quit-confirm). */
    {
        int item_count = (cur_page == PAGE_MAIN) ? MAIN_ITEM_COUNT : CONFIRM_ITEM_COUNT;
        int* sel       = (cur_page == PAGE_MAIN) ? &main_sel : &confirm_sel;

        switch (act) {
            case ACT_UP:
            case ACT_LEFT:
                *sel = (*sel + item_count - 1) % item_count;
                break;
            case ACT_DOWN:
            case ACT_RIGHT:
                *sel = (*sel + 1) % item_count;
                break;
            case ACT_CONFIRM:
                if (cur_page == PAGE_MAIN)              main_activate();
                else if (cur_page == PAGE_CONFIRM_QUIT) confirm_activate();
                break;
            case ACT_CANCEL:
                if (cur_page == PAGE_CONFIRM_QUIT) cur_page = PAGE_MAIN;
                else                               pc_pause_menu_toggle();
                break;
            default: break;
        }
    }
}

int pc_pause_menu_vr_input(float stick_x, float stick_y, int confirm, int cancel) {
    if (!g_pc_paused) {
        s_vr_nav_ready = 0;
        return 0;
    }
    /* Reject the entire malformed snapshot and rearm only after neutral. */
    if (!(stick_x >= -1.0f && stick_x <= 1.0f &&
          stick_y >= -1.0f && stick_y <= 1.0f)) {
        s_vr_nav_ready = 0;
        return 1;
    }
    int neutral = stick_x > -0.35f && stick_x < 0.35f &&
                  stick_y > -0.35f && stick_y < 0.35f;
    if (!s_vr_nav_ready) {
        if (neutral && !confirm && !cancel) {
            s_vr_nav_ready = 1;
            s_vr_nav_latched = 0;
            s_vr_confirm_prev = s_vr_cancel_prev = 0;
        }
        return 1;
    }
    MenuAction action = ACT_NONE;
    if (cancel && !s_vr_cancel_prev) action = ACT_CANCEL;
    else if (confirm && !s_vr_confirm_prev) action = ACT_CONFIRM;
    s_vr_confirm_prev = confirm != 0;
    s_vr_cancel_prev = cancel != 0;
    if (neutral) s_vr_nav_latched = 0;
    else if (!s_vr_nav_latched && action == ACT_NONE) {
        /* One push, one step, including diagonal pushes. */
        if (stick_y >= 0.60f && stick_y <= 1.0f) action = ACT_UP;
        else if (stick_y <= -0.60f && stick_y >= -1.0f) action = ACT_DOWN;
        else if (stick_x <= -0.60f && stick_x >= -1.0f) action = ACT_LEFT;
        else if (stick_x >= 0.60f && stick_x <= 1.0f) action = ACT_RIGHT;
        if (action != ACT_NONE) s_vr_nav_latched = 1;
    }
    /* Holding the stick while confirming must not queue a second action. */
    if ((confirm || cancel) && !neutral) s_vr_nav_latched = 1;
    if (!pc_settings_menu_capture_blocking()) handle_action(action);
    return 1;
}

static MenuAction translate_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:
        case SDLK_w:        return ACT_UP;
        case SDLK_DOWN:
        case SDLK_s:        return ACT_DOWN;
        case SDLK_LEFT:
        case SDLK_a:        return ACT_LEFT;
        case SDLK_RIGHT:
        case SDLK_d:        return ACT_RIGHT;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:    return ACT_CONFIRM;
        case SDLK_ESCAPE:   return ACT_CANCEL;
    }
    return ACT_NONE;
}

static MenuAction translate_pad_button(int button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return ACT_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return ACT_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return ACT_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return ACT_RIGHT;
        case SDL_CONTROLLER_BUTTON_A:          return ACT_CONFIRM;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_BACK:
        case SDL_CONTROLLER_BUTTON_START:      return ACT_CANCEL;
    }
    return ACT_NONE;
}

/* Left-stick nav with a per-axis latch so one push = one step. */
#define STICK_NAV_PRESS   18000
#define STICK_NAV_RELEASE 12000

static MenuAction translate_pad_axis(int axis, int value) {
    static int latch_x = 0, latch_y = 0;
    int* latch;
    MenuAction neg, pos;

    if (axis == SDL_CONTROLLER_AXIS_LEFTX)      { latch = &latch_x; neg = ACT_LEFT; pos = ACT_RIGHT; }
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY) { latch = &latch_y; neg = ACT_UP;   pos = ACT_DOWN; }
    else return ACT_NONE;

    if (*latch != 0) {
        if (value > -STICK_NAV_RELEASE && value < STICK_NAV_RELEASE) *latch = 0;
        return ACT_NONE;
    }
    if (value <= -STICK_NAV_PRESS) { *latch = -1; return neg; }
    if (value >=  STICK_NAV_PRESS) { *latch =  1; return pos; }
    return ACT_NONE;
}

int pc_pause_menu_handle_event(const SDL_Event* e) {
    if (!g_pc_paused) return 0;

    switch (e->type) {
        case SDL_KEYDOWN:
            if (e->key.repeat) return 1;
            handle_action(translate_key(e->key.keysym.sym));
            return 1; /* swallow all keys while paused */
        case SDL_CONTROLLERBUTTONDOWN:
            handle_action(translate_pad_button(e->cbutton.button));
            return 1;
        case SDL_CONTROLLERAXISMOTION:
            handle_action(translate_pad_axis(e->caxis.axis, e->caxis.value));
            return 1;
    }
    return 0;
}

/* Drawing */

static void draw_main_page(struct game_s* game) {
    static const char* items[MAIN_ITEM_COUNT] = { "Resume", "Settings", "Quit Game" };

    pc_menu_draw_centered(game, "- Paused -", 80.0f, 255, 255, 255, 255, 1.0f);

    f32 y = 110.0f;
    f32 line_h = 18.0f;
    for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
        int r, g, b, a;
        int selected = (i == main_sel);
        pc_menu_row_colors(selected, &r, &g, &b, &a);
        pc_menu_draw_centered(game, items[i], y + i * line_h, r, g, b, a,
                              selected ? PC_MENU_SCALE_SELECTED : 1.0f);
    }
}

static void draw_confirm_page(struct game_s* game) {
    pc_menu_draw_centered(game, "- Quit Game -", 80.0f, 255, 255, 255, 255, 1.0f);
    pc_menu_draw_centered(game, "Are you sure you want to quit?",
                          115.0f, 230, 230, 230, 255, 1.0f);
    pc_menu_draw_two_choice(game, "No", "Yes", confirm_sel, 150.0f);
}

void pc_pause_menu_draw(struct game_s* game) {
    if (!g_pc_paused || game == NULL || game->graph == NULL) return;

    /* Font ortho projection + identity modelview, else our screen-space quads
     * inherit whatever projection the scene ended its frame with. */
    mFont_SetMatrix(game->graph, mFont_MODE_FONT);
    pc_settings_menu_tick();

    if (cur_page == PAGE_SETTINGS) {
        /* Settings module owns its own dim backdrop. */
        pc_settings_menu_draw(game, /*with_dim_backdrop=*/1);
        /* The user may have closed it from inside (Back). */
        if (!pc_settings_menu_active()) settings_page_closed();
    } else {
        pc_menu_dim_rect(game->graph, 180);
        if (cur_page == PAGE_MAIN)              draw_main_page(game);
        else if (cur_page == PAGE_CONFIRM_QUIT) draw_confirm_page(game);
    }

    mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
}
