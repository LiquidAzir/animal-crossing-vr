#include "ef_effect_control.h"

#include "m_common_data.h"
#include "m_rcp.h"

#ifdef TARGET_PC
#include "pc_vr.h"
#include "m_player_lib.h"
#include "m_play.h"
#include "ac_uki.h"
#include "m_name_table.h"
#include "sys_matrix.h"
#include "m_debug.h"

/* The tiny-catch pointer belongs to the same visual presentation as the item.
 * Leave its lifetime, world position, and all non-presentation effects alone. */
static int eYajirushi_vr_presentation_matrix(const eEC_Effect_c* effect, GAME_PLAY* play, MtxF* out) {
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
    float pose[12];
    int presenting = FALSE;
    if (player == NULL || effect->item_name != RSV_NO ||
        play->submenu.menu_type != mSM_OVL_NONE || play->submenu.current_menu_type != mSM_OVL_NONE) {
        return FALSE;
    }
    if (player->now_main_index == mPlayer_INDEX_NOTICE_NET) {
        presenting = player->main_data.notice_net.end_effect_flag && player->item_net_catch_label != 0 &&
                     player->item_net_catch_type == mPlayer_NET_CATCH_TYPE_INSECT;
    } else if (player->now_main_index == mPlayer_INDEX_NOTICE_ROD) {
        UKI_ACTOR* rod = (UKI_ACTOR*)player->fishing_rod_actor_p;
        presenting = player->main_data.notice_rod.end_effect_flag && rod != NULL && rod->child_actor != NULL;
    }
    if (!presenting || !pc_vr_item_presentation_mtx(pose)) return FALSE;
    out->xx = pose[0]; out->xy = pose[1]; out->xz = pose[2]; out->xw = pose[3];
    out->yx = pose[4]; out->yy = pose[5]; out->yz = pose[6]; out->yw = pose[7];
    out->zx = pose[8]; out->zy = pose[9]; out->zz = pose[10]; out->zw = pose[11];
    out->wx = out->wy = out->wz = 0.0f; out->ww = 1.0f;
    return TRUE;
}
#endif

static void eYajirushi_init(xyz_t pos, int prio, s16 angle, GAME* game, u16 item_name, s16 arg0, s16 arg1);
static void eYajirushi_ct(eEC_Effect_c* effect, GAME* game, void* ct_arg);
static void eYajirushi_mv(eEC_Effect_c* effect, GAME* game);
static void eYajirushi_dw(eEC_Effect_c* effect, GAME* game);

eEC_PROFILE_c iam_ef_yajirushi = {
    // clang-format off
    &eYajirushi_init,
    &eYajirushi_ct,
    &eYajirushi_mv,
    &eYajirushi_dw,
    6,
    eEC_NO_CHILD_ID,
    eEC_DEFAULT_DEATH_DIST,
    // clang-format on
};

#define eYajirushi_ALPHA effect->effect_specific[0]

static void eYajirushi_init(xyz_t pos, int prio, s16 angle, GAME* game, u16 item_name, s16 arg0, s16 arg1) {
    eEC_CLIP->make_effect_proc(eEC_EFFECT_YAJIRUSHI, pos, NULL, game, NULL, item_name, prio, 0, 0);
}

static void eYajirushi_ct(eEC_Effect_c* effect, GAME* game, void* ct_arg) {
    eYajirushi_ALPHA = 0;
    effect->timer = 4;
}

static void eYajirushi_mv(eEC_Effect_c* effect, GAME* game) {
    eEC_CLIP->set_continious_env_proc(effect, 4, 60);

    if (effect->state == eEC_STATE_NORMAL) {
        eYajirushi_ALPHA = (s16)eEL_CalcAdjust_F(4.0f - effect->lifetime, 0.0f, 3.0f, 0.0f, 255.0f);
    } else if (effect->state == eEC_STATE_CONTINUOUS) {
        eYajirushi_ALPHA = 255;
    } else if (effect->state == eEC_STATE_FINISHED) {
        eYajirushi_ALPHA = (s16)eEL_CalcAdjust_F(6.0f - effect->lifetime, 0.0f, 6.0f, 255.0f, 0.0f);
    }
}

extern Gfx ef_kore_modelT[];

static void eYajirushi_dw(eEC_Effect_c* effect, GAME* game) {
    OPEN_DISP(game->graph);

    _texture_z_light_fog_prim_xlu(game->graph);
#ifdef TARGET_PC
    {
        MtxF presentation_matrix;
        if (eYajirushi_vr_presentation_matrix(effect, (GAME_PLAY*)game, &presentation_matrix)) {
            f32 adj_scale = 1.0f + ((f32)(int)GETREG(MYKREG, 27)) * 0.01f;
            Matrix_push();
            Matrix_put(&presentation_matrix);
            Matrix_scale(effect->scale.x * adj_scale, effect->scale.y * adj_scale,
                         effect->scale.z * adj_scale, MTX_MULT);
            gSPMatrix(NEXT_POLY_XLU_DISP, _Matrix_to_Mtx_new(game->graph),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            Matrix_pull();
        } else {
            eEC_CLIP->auto_matrix_xlu_proc(game, &effect->position, &effect->scale);
        }
    }
#else
    eEC_CLIP->auto_matrix_xlu_proc(game, &effect->position, &effect->scale);
#endif
    gDPSetPrimColor(NEXT_POLY_XLU_DISP, 0, 255, 255, 80, 30, eYajirushi_ALPHA);
    gSPDisplayList(NEXT_POLY_XLU_DISP, ef_kore_modelT);

    CLOSE_DISP(game->graph);
}
