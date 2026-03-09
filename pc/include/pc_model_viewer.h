/* pc_model_viewer.h - Debug skeleton model viewer scene */
#ifndef PC_MODEL_VIEWER_H
#define PC_MODEL_VIEWER_H

#include "game.h"
#include "m_view.h"
#include "c_keyframe.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_pc_model_viewer;

extern void pc_model_viewer_init(GAME* game);
extern void pc_model_viewer_cleanup(GAME* game);

/* Must fit inside alloc_size declared in game_dlftbls */
typedef struct {
    GAME game;                          /* must be first (base class) */
    View view;
    cKF_SkeletonInfo_R_c skeleton_info;
    s_xyz joint_work[64];
    s_xyz joint_target[64];
    int current_model;
    int initialized_model;              /* -1 = none */
    f32 cam_angle_y;
    f32 cam_angle_x;
    f32 cam_distance;
    f32 cam_pan_x;
    f32 cam_pan_z;
    int anim_paused;
} GAME_MODEL_VIEWER;

#ifdef __cplusplus
}
#endif

#endif /* PC_MODEL_VIEWER_H */
