#ifndef AC_FIELD_DRAW_H
#define AC_FIELD_DRAW_H

#include "types.h"
#include "m_actor.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TARGET_PC
/* PC/VR draws the WHOLE town (7x10 = 70 acres) — no moving window, so
 * terrain can never stream in or out. Flat mode keeps the classic 3x3 by
 * only filling the first 9 slots (the rest carry exist=FALSE). */
#define aFD_BLOCK_DRAW_NUM 70
#else
#define aFD_BLOCK_DRAW_NUM 9
#endif

typedef struct field_draw_s FIELD_DRAW_ACTOR;

typedef struct field_draw_block_s {
  xyz_t wpos;
  int bx;
  int bz;
  int exist;
} aFD_block_c;

typedef struct field_draw_marin_info_s {
  u32 tile0_scroll;
  u32 tile1_scroll;
  rgba_t beach_env_color;
  u32 frame;
  f32 anim_frame;
} aFD_marin_info_c;

struct field_draw_s {
  ACTOR actor_class;
  aFD_block_c block[aFD_BLOCK_DRAW_NUM];
  aFD_marin_info_c marin_info;
};

extern ACTOR_PROFILE Field_Draw_Profile;

#ifdef __cplusplus
}
#endif

#endif

