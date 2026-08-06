#include "c_keyframe.h"

#include "MSL_C/w_math.h"
#include "graph.h"
#include "libultra/libultra.h"
#include "sys_math3d.h"
#include "sys_matrix.h"

/**
 * Resets and initializes a frame control structure with default parameters.
 * Sets all numerical properties to 1.0f and mode to cKF_FRAMECONTROL_STOP.
 *
 * @param frame_control Pointer to the frame control structure.
 */
static void cKF_FrameControl_zeroClera(cKF_FrameControl_c* frame_control) {
    bzero(frame_control, sizeof(cKF_FrameControl_c));
    frame_control->max_frames = 1.0f;
    frame_control->current_frame = 1.0f;
    frame_control->previous_frame = 1.0f;
    frame_control->speed = 1.0f;
    frame_control->end_frame = 1.0f;
    frame_control->start_frame = 1.0f;
    frame_control->mode = cKF_FRAMECONTROL_STOP;
}

/**
 * Initializes a frame control structure.
 * Wrapper for cKF_FrameControl_zeroClera to provide a clear constructor interface.
 *
 * @param frame_control Pointer to the frame control structure to initialize.
 */
static void cKF_FrameControl_ct(cKF_FrameControl_c* frame_control) {
    cKF_FrameControl_zeroClera(frame_control);
}

/**
 * Sets the frame control parameters for an animation sequence.
 *
 * @param frame_control Pointer to the frame control structure.
 * @param start_frame Starting frame of the animation sequence.
 * @param end_frame Ending frame of the animation sequence; if less than 1.0f, max_frames is used.
 * @param max_frames Maximum number of frames in the animation sequence.
 * @param current_frame The current frame number in the animation sequence.
 * @param speed The speed at which the animation should play.
 * @param mode The mode of the animation (e.g., stop, repeat).
 */
static void cKF_FrameControl_setFrame(cKF_FrameControl_c* frame_control, f32 start_frame, f32 end_frame, f32 max_frames,
                                      f32 current_frame, f32 speed, int mode) {
    frame_control->start_frame = start_frame;

    if (end_frame < 1.0f) {
        frame_control->end_frame = max_frames;
    } else {
        frame_control->end_frame = end_frame;
    }

    frame_control->max_frames = max_frames;
    frame_control->speed = speed;
    frame_control->current_frame = current_frame;
    frame_control->previous_frame = current_frame;
    frame_control->mode = mode;
}

/**
 * Checks if the current frame is within a certain range and calculates the overshoot.
 *
 * @param fc Pointer to the frame control structure.
 * @param current The target frame to compare against the current frame.
 * @param out Pointer to a float where the overshoot amount will be stored.
 * @return Returns 1 if within the range and adjustments were made, 0 otherwise.
 */
static int cKF_FrameControl_passCheck(cKF_FrameControl_c* fc, f32 current, f32* out) {
    f32 cur;
    f32 speed;

    *out = 0.0f;
    cur = fc->current_frame;
    if (cur == current) {
        return FALSE;
    }

    speed = (fc->start_frame < fc->end_frame) ? fc->speed : -fc->speed;
#ifdef TARGET_PC
    speed *= (f32)gamePT->graph->dt_num_60fps_frames;
#endif

    // Check if current frame within target range considering speed
    if ((speed >= 0.0f && cur < current && cur + speed >= current) ||
        (speed < 0.0f && cur > current && cur + speed <= current)) {
        *out = cur + speed - current; // Calculate overshoot
        return TRUE;
    }
    return FALSE;
}

extern int cKF_FrameControl_passCheck_now(cKF_FrameControl_c* fc, const float current) {
    const float cur = fc->current_frame;

    if (cur == current) {
        return TRUE;
    } else {
        const float prev = fc->previous_frame;
        const float start = fc->start_frame;
        const float end = fc->end_frame;
        float speed = fc->speed;

        if (start >= end) {
            speed = -speed;
        }

        if (speed >= 0.0f) {
            if (prev <= cur) {
                // no wrap this update
                if (prev < current && cur >= current) {
                    return TRUE;
                }
            } else {
                // wrapped from end -> start
                if ((prev < end && current > prev && current <= end) ||
                    (cur >= start && current >= start && current <= cur)) {
                    return TRUE;
                }
            }
        } else { // playing backwards
            if (prev >= cur) {
                // no wrap
                if (prev > current && cur <= current) {
                    return TRUE;
                }
            } else {
                // wrapped from start -> end
                if ((prev > start && current >= start && current < prev) ||
                    (cur <= end && current >= cur && current <= end)) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

extern int cKF_FrameControl_stop_proc(cKF_FrameControl_c* fc) {
    f32 out;

    if (fc->current_frame == fc->end_frame) {
        return cKF_STATE_STOPPED;
    }
    if (cKF_FrameControl_passCheck(fc, fc->end_frame, &out)) {
        fc->current_frame = fc->end_frame;
        return cKF_STATE_STOPPED;
    }
    if (cKF_FrameControl_passCheck(fc, fc->start_frame, &out)) {
        fc->current_frame = fc->end_frame;
        return cKF_STATE_STOPPED;
    }
    return cKF_STATE_NONE;
}

/**
 * Repeats animation by looping from start to end frame.
 *
 * Loops the current frame back to the start or end, based on the animation's progression,
 * allowing for continuous playback.
 *
 * @param fc Pointer to the frame control structure.
 * @return cKF_STATE_CONTINUE if animation continues; cKF_STATE_NONE otherwise.
 */
static int cKF_FrameControl_repeat_proc(cKF_FrameControl_c* fc) {
    f32 out;

    if (cKF_FrameControl_passCheck(fc, fc->end_frame, &out)) {
        fc->current_frame = fc->start_frame + out;
        return cKF_STATE_CONTINUE;
    }
    if (cKF_FrameControl_passCheck(fc, fc->start_frame, &out)) {
        fc->current_frame = fc->end_frame + out;
        return cKF_STATE_CONTINUE;
    }
    return cKF_STATE_NONE;
}

/**
 * Plays animation based on current mode.
 *
 * Updates current frame according to animation speed and mode, ensuring playback within
 * the animation's frame range.
 *
 * @param fc Pointer to the frame control structure.
 * @return Animation state after update.
 */
 static int cKF_FrameControl_play(cKF_FrameControl_c* fc) {
    int rec;
    float speed;

    // Store the previous frame BEFORE any modifications (stop_proc may clamp current_frame)
    fc->previous_frame = fc->current_frame;

    if (fc->mode == cKF_FRAMECONTROL_STOP) {
        rec = cKF_FrameControl_stop_proc(fc);
    } else {
        rec = cKF_FrameControl_repeat_proc(fc);
    }

    if (rec == cKF_STATE_NONE) {
        speed = fc->speed;
        if (fc->start_frame >= fc->end_frame) {
            speed = -speed;
        }

        speed *= gamePT->graph->dt_num_60fps_frames;
        // if (frame > 0.0f) {
        //     OSReport("cKF_FrameControl_play: dt: %f, frame: %f, fc->current_frame: %f, fc->max_frames: %f\n", GAME_DELTATIME, frame, fc->current_frame, fc->max_frames);
        // }
        fc->current_frame += speed; // TODO: we should probably pass in graph somehow
    }
    if (fc->current_frame < 1.0f) {
        fc->current_frame = (fc->current_frame - 1.0f) + fc->max_frames;
    } else if (fc->current_frame > fc->max_frames) {
        fc->current_frame = (fc->current_frame - fc->max_frames) + 1.0f;
    }
    return rec;
}

extern f32 cKF_HermitCalc(f32 time, f32 tension, f32 startPos, f32 endPos, f32 startTangent, f32 endTangent) {
    f32 position;
    f32 timeSquared;
    f32 timeCubed;
    f32 basisH10;
    f32 basisH11;

    timeSquared = time * time;
    timeCubed = timeSquared * time;
    position = -(timeCubed * 2.0f) + (3.0f * timeSquared);
    basisH10 = time + (timeCubed - (timeSquared * 2.0f));
    basisH11 = timeCubed - timeSquared;

    return (((1.0f - position) * startPos) + (position * endPos)) +
           (tension * ((basisH10 * startTangent) + (basisH11 * endTangent)));
}

typedef struct {
    s16 frame;
    s16 value;
    s16 tangent;
} cKF_AnimKey_c;

/**
 * Calculates keyframe value based on frame position within keyframe data.
 *
 * Interpolates or directly retrieves the y-component of a keyframe based on the given frame number.
 * Uses linear or Hermite interpolation depending on the proximity of frame to keyframe positions.
 *
 * @param start_idx Starting index of the keyframe data.
 * @param n_frames Number of frames in the keyframe sequence.
 * @param data_src Pointer to the keyframe data source.
 * @param frame Current frame number for which to calculate the keyframe value.
 * @return Interpolated or directly retrieved keyframe value.
 */
static s16 cKF_KeyCalc(s16 start_idx, s16 n_frames, s16* data_src, f32 frame) {
    int now;
    int next;
    cKF_AnimKey_c* key_p = (cKF_AnimKey_c*)&data_src[start_idx * 3];

    /* If the first frame is greater than the current frame then the first value is held */
    if (key_p[0].frame >= frame) {
        return key_p[0].value;
    }

    /* If the current frame is greater than the last frame then the last value is held */
    if (key_p[n_frames - 1].frame <= frame) {
        return key_p[n_frames - 1].value;
    }

    // Search through all frames
    now = 0;
    next = 1;

    while (TRUE) {
        if (key_p[next].frame > frame) {
            f32 delta_frame = key_p[next].frame - key_p[now].frame;

            if (!(F32_IS_ZERO(delta_frame))) {
                f32 t = (frame - key_p[now].frame) / delta_frame; // progress towards the next frame
                f32 tension = delta_frame * (1.0f / 30.0f);
                f32 calc = cKF_HermitCalc(t, tension, key_p[now].value, key_p[next].value, key_p[now].tangent,
                                          key_p[next].tangent);
                int key = calc + 0.5; // Always round up

                return key;
            } else {
                return key_p[now].value;
            }
        }

        now++;
        next++;
    }
}

extern void cKF_SkeletonInfo_subRotInterpolation(f32 t, s16* out, s16 rot1, s16 rot2) {
    u16 urot1 = rot1;
    s32 pad;
    u16 urot2 = rot2;
    f32 f1 = rot1;
    f32 signedDiff = rot2 - f1;
    f32 f2 = urot1;
    f32 unsignedDiff = urot2 - f2;

    if (fabsf(signedDiff) < fabsf(unsignedDiff)) {
        *out = f1 + signedDiff * t;
    } else {
        *out = f2 + unsignedDiff * t;
    }
}

/**
 * Morphs current state towards a target state with a given step size.
 *
 * Adjusts 'now' values towards 'target' values based on 'step', performing this operation
 * for three consecutive s16 values (x->y->z) starting from 'now' and 'target'.
 *
 * @param now Pointer to the current values.
 * @param target Pointer to the target values to morph towards.
 * @param step Fractional step size for morphing.
 */
static void cKF_SkeletonInfo_morphST(s16* now, s16* target, f32 step) {
    int i;

    for (i = 0; i < 3; i++) {
        if (*now != *target) {
            f32 now_f = (f32)*now;
            f32 target_f = (f32)*target;
            f32 diff = (target_f - now_f);
            *now = (diff * step) + now_f;
        }

        now++;
        target++;
    }
}

/**
 * Zeroes out and initializes a skeleton info structure.
 *
 * Clears all data within cKF_SkeletonInfo_R_c structure, effectively resetting it.
 *
 * @param keyframe Pointer to the skeleton info structure to be cleared.
 */
static void cKF_SkeletonInfo_R_zeroClear(cKF_SkeletonInfo_R_c* keyframe) {
    bzero(keyframe, sizeof(cKF_SkeletonInfo_R_c));
}

extern void cKF_SkeletonInfo_R_ct(cKF_SkeletonInfo_R_c* keyframe, cKF_Skeleton_R_c* skeleton,
                                  cKF_Animation_R_c* animation, s_xyz* work_table, s_xyz* target_table) {
    cKF_SkeletonInfo_R_zeroClear(keyframe);
    cKF_FrameControl_ct(&keyframe->frame_control);

    keyframe->skeleton = skeleton;
    keyframe->animation = animation;
    keyframe->current_joint = work_table;
    keyframe->target_joint = target_table;
}

extern void cKF_SkeletonInfo_R_dt(cKF_SkeletonInfo_R_c* keyframe) {
}

extern void cKF_SkeletonInfo_R_init_standard_stop(cKF_SkeletonInfo_R_c* keyframe, cKF_Animation_R_c* animation,
                                                  s_xyz* rotation_diff_table) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, 1.0f, 0.5f, 0.0f,
                            cKF_FRAMECONTROL_STOP, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_standard_stop_morph(cKF_SkeletonInfo_R_c* keyframe, cKF_Animation_R_c* animation,
                                                        s_xyz* rotation_diff_table, f32 morph) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, 1.0f, 0.5f, morph,
                            cKF_FRAMECONTROL_STOP, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_standard_repeat(cKF_SkeletonInfo_R_c* keyframe, cKF_Animation_R_c* animation,
                                                    s_xyz* rotation_diff_table) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, 1.0f, 0.5f, 0.0f,
                            cKF_FRAMECONTROL_REPEAT, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_standard_repeat_morph(cKF_SkeletonInfo_R_c* keyframe, cKF_Animation_R_c* animation,
                                                          s_xyz* rotation_diff_table, f32 morph) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, 1.0f, 0.5f, morph,
                            cKF_FRAMECONTROL_REPEAT, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init(cKF_SkeletonInfo_R_c* keyframe, cKF_Skeleton_R_c* skeleton,
                                    cKF_Animation_R_c* animation, f32 start_frame, f32 end_frame, f32 current_frame,
                                    f32 frame_speed, f32 morph_counter, int mode, s_xyz* rotation_diff_table) {
    keyframe->morph_counter = morph_counter;
    keyframe->skeleton = skeleton;
    keyframe->animation = animation;

    cKF_FrameControl_setFrame(&keyframe->frame_control, start_frame, end_frame, keyframe->animation->frames,
                              current_frame, frame_speed, mode);
    keyframe->rotation_diff_table = rotation_diff_table;
}

/**
 * Morphs joint positions towards target positions based on the morph counter.
 *
 * Adjusts each joint's position in the skeleton towards its target position using interpolation,
 * based on a calculated step size derived from the morph counter. If the morph counter is zero,
 * no morphing occurs.
 *
 * @param keyframe Pointer to the skeleton info structure containing joint and target positions.
 */
static void cKF_SkeletonInfo_R_morphJoint(cKF_SkeletonInfo_R_c* keyframe) {
    int i;
    s_xyz* current_joint = keyframe->current_joint;
    s_xyz* target_joint = keyframe->target_joint;
    f32 step;
    s16 next_joint_x;
    s16 next_joint_y;
    s16 next_joint_z;
    s16 next_target_x;
    s16 next_target_y;
    s16 next_target_z;

    if (!(F32_IS_ZERO(keyframe->morph_counter))) {
        step = (0.5f * (f32)gamePT->graph->dt_num_60fps_frames) / fabsf(keyframe->morph_counter);
        if (step > 1.0f) {
            step = 1.0f;
        }
    } else {
        step = 0.0f;
    }

    cKF_SkeletonInfo_morphST(&current_joint->x, &target_joint->x, step);

    current_joint++;
    target_joint++;

    for (i = 0; i < keyframe->skeleton->num_joints; i++) {
        next_joint_x = current_joint->x;
        next_target_x = target_joint->x;

        next_joint_y = current_joint->y;
        next_joint_z = current_joint->z;

        next_target_y = target_joint->y;
        next_target_z = target_joint->z;

        if (next_joint_x != next_target_x || next_joint_y != next_target_y || next_joint_z != next_target_z) {
            f32 difxyz = fabsf((f32)next_target_x - (f32)next_joint_x) + fabsf((f32)next_target_y - (f32)next_joint_y) +
                         fabsf((f32)next_target_z - (f32)next_joint_z);

            s16 temp_vec_x = 0x7FFF + next_joint_x;
            s16 temp_vec_y = 0x7FFF - next_joint_y;
            s16 temp_vec_z = 0x7FFF + next_joint_z;

            f32 dif_xyz2 = fabsf((f32)next_target_x - (f32)temp_vec_x) + fabsf((f32)next_target_y - (f32)temp_vec_y) +
                           fabsf((f32)next_target_z - (f32)temp_vec_z);

            if (difxyz < dif_xyz2) {
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->x, next_joint_x, next_target_x);
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->y, next_joint_y, next_target_y);
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->z, next_joint_z, next_target_z);
            } else {
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->x, temp_vec_x, next_target_x);
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->y, temp_vec_y, next_target_y);
                cKF_SkeletonInfo_subRotInterpolation(step, &current_joint->z, temp_vec_z, next_target_z);
            }
        }
        target_joint++;
        current_joint++;
    }
}

// TODO: There's probably no difference between this and US.
// The inlines used to match US are fake.
#if VERSION >= VER_GAFU01_00
extern int cKF_SkeletonInfo_R_play(cKF_SkeletonInfo_R_c* keyframe) {
    int i;
    int j;
    u8* flagTable;
    int keyTableIndex = 0;
    int fixedTableIndex = 0;
    int dataIndex = 0;
    s16* jointValuePtr;
    s16* fixedTable;
    s16* dataTable;
    s16* keyTable;
    u32 jointFlag; // Check translation (xyz)

    // Choose between current and target joint based on morph counter
    if (F32_IS_ZERO(keyframe->morph_counter)) {
        jointValuePtr = &keyframe->current_joint->x;
    } else {
        jointValuePtr = &keyframe->target_joint->x;
    }

    jointFlag = cKF_ANIMATION_BIT_TRANS_X;

    // Retrieve animation tables
    fixedTable = keyframe->animation->fixed_table;
    keyTable = keyframe->animation->key_table;
    dataTable = keyframe->animation->data_table;
    flagTable = keyframe->animation->flag_table;

    

    // Process root translation x -> y -> z
    for (j = 0; j < 3; j++) {
        if (*flagTable & jointFlag) {
            // Apply joint translation
            *jointValuePtr =
                cKF_KeyCalc(dataIndex, keyTable[keyTableIndex], dataTable, keyframe->frame_control.current_frame);
            dataIndex += keyTable[keyTableIndex];
            keyTableIndex++;
        } else {
            // Use fixed value if not flagged for keyframe animation
            *jointValuePtr = fixedTable[fixedTableIndex];
            fixedTableIndex++;
        }

        jointFlag >>= 1; // Shift x -> y -> z
        jointValuePtr++; // Move to next joint
    }

    // Process remaining joint rotations
    for (i = 0; i < keyframe->skeleton->num_joints; i++) {
        jointFlag = cKF_ANIMATION_BIT_ROT_X; // Reset flag for new joint

        // Process each joint x -> y -> z
        for (j = 0; j < 3; j++) {
            f32 adjustedJointValue;
            f32 mod;

            // Similar logic to above, but for each joint in the skeleton
            if (jointFlag & flagTable[i]) {
                *jointValuePtr =
                    cKF_KeyCalc(dataIndex, keyTable[keyTableIndex], dataTable, keyframe->frame_control.current_frame);
                dataIndex += keyTable[keyTableIndex];
                keyTableIndex++;
            } else {
                *jointValuePtr = fixedTable[fixedTableIndex];
                fixedTableIndex++;
            }

            // Reduce the value by 90% and clamp to [0, 360) degrees converted back to binangle (s16)
            // This effectively limits any joint's maximum rotation to be in the range of [-36.8, 36.7] degrees
            adjustedJointValue = *jointValuePtr * 0.1f;
            mod = MOD_F(adjustedJointValue, 360.0f);
            *jointValuePtr = DEG2SHORT_ANGLE(mod);
            jointValuePtr++;


            jointFlag >>= 1; // Shift flag for next component x -> y -> z
        }

        // flagTable++;
    }

    // Apply rotation differences if available
    if (keyframe->rotation_diff_table != NULL) {
        s_xyz* currentJointPtr = (F32_IS_ZERO(keyframe->morph_counter)) ? keyframe->current_joint : keyframe->target_joint;

        currentJointPtr++; // Skip first joint, usually root, which is handled separately
        for (j = 0; j < keyframe->skeleton->num_joints; j++) {
            // Apply rotation differences to each joint
            currentJointPtr->x += keyframe->rotation_diff_table[j].x;
            currentJointPtr->y += keyframe->rotation_diff_table[j].y;
            currentJointPtr->z += keyframe->rotation_diff_table[j].z;

            currentJointPtr++; // Move to next joint
        }
    }

    // Handle morphing and play control based on morph counter
    if (F32_IS_ZERO(keyframe->morph_counter)) {
        // Play normally if no morphing is needed
        return cKF_FrameControl_play(&keyframe->frame_control);
    } else {
        f32 morph_step = 0.5f * (f32)gamePT->graph->dt_num_60fps_frames;
        if (keyframe->morph_counter > 0.0f) {
            cKF_SkeletonInfo_R_morphJoint(keyframe);
            keyframe->morph_counter -= morph_step;
            if (keyframe->morph_counter <= 0.0f) {
                keyframe->morph_counter = 0.0f;
            }
            return cKF_STATE_NONE;
        } else {
            cKF_SkeletonInfo_R_morphJoint(keyframe);
            keyframe->morph_counter += morph_step;
            if (keyframe->morph_counter >= 0.0f) {
                keyframe->morph_counter = 0.0f;
            }
            return cKF_FrameControl_play(&keyframe->frame_control);
        }
    }
}
#else
/**
 * Retrieves the flag table from an animation structure.
 *
 * Provides direct access to the flag table indicating the state or properties of each frame
 * or keyframe in the animation sequence.
 *
 * @param keyframe Pointer to the animation structure.
 * @return Pointer to the flag table.
 */
inline u8* cKF_Animation_R_getFlagTable(cKF_Animation_R_c* keyframe) {
    return keyframe->flag_table;
}

/**
 * Retrieves the fixed table from an animation structure.
 *
 * Provides direct access to the fixed table containing fixed values used in the animation,
 * possibly for static or non-interpolated properties.
 *
 * @param keyframe Pointer to the animation structure.
 * @return Pointer to the fixed table.
 */
inline s16* cKF_Animation_R_getFixedTable(cKF_Animation_R_c* keyframe) {
    return keyframe->fixed_table;
}

/**
 * Retrieves the key table from an animation structure.
 *
 * Provides direct access to the key table containing key points used for interpolating
 * animation values.
 *
 * @param keyframe Pointer to the animation structure.
 * @return Pointer to the key table.
 */
inline s16* cKF_Animation_R_getKeyTable(cKF_Animation_R_c* keyframe) {
    return keyframe->key_table;
}

/**
 * Retrieves the data table from an animation structure.
 *
 * Provides direct access to the data table containing all numerical data points used for
 * animating properties over time.
 *
 * @param keyframe Pointer to the animation structure.
 * @return Pointer to the data table.
 */
inline s16* cKF_Animation_R_getDataTable(cKF_Animation_R_c* keyframe) {
    return keyframe->data_table;
}

extern int cKF_SkeletonInfo_R_play(cKF_SkeletonInfo_R_c* keyframe) {
    int state;
    int jointIndex;
    s_xyz* currentJointPtr;
    s16 fixedJointValue;
    s16* currentJointValue;
    cKF_Animation_R_c* animationData;
    int component;
    f32 adjustedJointValue;

    int keyTableIndex = 0;
    int fixedTableIndex = 0;

    int dataIndex = 0;

#ifdef TARGET_PC
    /* Validate keyframe data to prevent crash from corrupted animation state.
       Note: data_table and key_table can legitimately be NULL for static poses
       (all flags zero, all values from fixed_table). Only reject truly invalid state. */
    if (keyframe == NULL || keyframe->skeleton == NULL || keyframe->animation == NULL ||
        keyframe->current_joint == NULL || keyframe->skeleton->num_joints > 200 ||
        keyframe->animation->flag_table == NULL || keyframe->animation->fixed_table == NULL) {
        return cKF_STATE_STOPPED;
    }
#endif

    // Choose between current and target joint based on morph counter
    s16* jointValuePtr =
        (F32_IS_ZERO(keyframe->morph_counter)) ? &keyframe->current_joint->x : &keyframe->target_joint->x;
    u32 jointFlag = cKF_ANIMATION_BIT_TRANS_X; // Check translation (xyz)

    // Retrieve animation tables - direct struct access (bypass inline functions)
    s16* fixedTable = keyframe->animation->fixed_table;
    s16* keyTable = keyframe->animation->key_table;
    s16* dataTable = keyframe->animation->data_table;
    u8* flagTable = keyframe->animation->flag_table;

    // Process root translation x -> y -> z
    for (component = 0; component < 3; component++) {
        if (flagTable[0] & jointFlag) {
            // Apply joint translation
            *jointValuePtr =
                cKF_KeyCalc(dataIndex, keyTable[keyTableIndex], dataTable, keyframe->frame_control.current_frame);
            dataIndex += keyTable[keyTableIndex++];
        } else {
            // Use fixed value if not flagged for keyframe animation
            *jointValuePtr = fixedTable[fixedTableIndex++];
        }

        jointFlag >>= 1; // Shift x -> y -> z
        jointValuePtr++; // Move to next joint
    }

    // Process remaining joint rotations
    for (jointIndex = 0; jointIndex < keyframe->skeleton->num_joints; jointIndex++) {
        jointFlag = cKF_ANIMATION_BIT_ROT_X; // Reset flag for new joint

        // Process each joint x -> y -> z
        for (component = 0; component < 3; component++) {
            // Similar logic to above, but for each joint in the skeleton
            if (jointFlag & flagTable[jointIndex]) {
                *jointValuePtr =
                    cKF_KeyCalc(dataIndex, keyTable[keyTableIndex], dataTable, keyframe->frame_control.current_frame);
                dataIndex += keyTable[keyTableIndex++];
            } else {
                *jointValuePtr = fixedTable[fixedTableIndex++];
            }

            // Convert joint value to fixed-point format after scaling
            jointFlag >>= 1; // Shift flag for next component x -> y -> z

            // Reduce the value by 90% and clamp to [0, 360) degrees converted back to binangle (s16)
            // This effectively limits any joint's maximum rotation to be in the range of [-36.8, 36.7] degrees
            adjustedJointValue = *jointValuePtr * 0.1f;
            *jointValuePtr++ = DEG2SHORT_ANGLE(MOD_F(adjustedJointValue, 360.0f));
        }
    }

    // Apply rotation differences if available
    if (keyframe->rotation_diff_table != NULL) {
        currentJointPtr = (F32_IS_ZERO(keyframe->morph_counter)) ? keyframe->current_joint : keyframe->target_joint;

        currentJointPtr++; // Skip first joint, usually root, which is handled separately
        for (component = 0; component < keyframe->skeleton->num_joints; component++) {
            // Apply rotation differences to each joint
            currentJointPtr->x += keyframe->rotation_diff_table[component].x;
            currentJointPtr->y += keyframe->rotation_diff_table[component].y;
            currentJointPtr->z += keyframe->rotation_diff_table[component].z;

            currentJointPtr++; // Move to next joint
        }
    }

    // Handle morphing and play control based on morph counter
    if (F32_IS_ZERO(keyframe->morph_counter)) {
        state = cKF_FrameControl_play(&keyframe->frame_control);
    } else {
        f32 morph_step = 0.5f * (f32)gamePT->graph->dt_num_60fps_frames;
        if (keyframe->morph_counter > 0.0f) {
            cKF_SkeletonInfo_R_morphJoint(keyframe);
            keyframe->morph_counter -= morph_step;
            if (keyframe->morph_counter <= 0.0f) {
                keyframe->morph_counter = 0.0f;
            }
            state = cKF_STATE_NONE;
        } else {
            cKF_SkeletonInfo_R_morphJoint(keyframe);
            keyframe->morph_counter += morph_step;
            if (keyframe->morph_counter >= 0.0f) {
                keyframe->morph_counter = 0.0f;
            }
            state = cKF_FrameControl_play(&keyframe->frame_control);
        }
    }

    return state;
}
#endif

#ifdef TARGET_PC
/* --- VR/FP "solid buildings" shell pass ---
 * Every town camera in the stock game looks from a fixed direction, so the
 * far side of buildings was never authored — with a free camera you see
 * straight through them. We re-draw a structure's skeleton spun 180
 * degrees and shrunk a few percent, so the front wall's geometry lands
 * where the missing back wall belongs while the copy stays inside the
 * original's hull everywhere real geometry already exists (the real pass
 * is drawn second and wins depth ties).
 *
 * THE PIVOT MUST BE THE BUILDING'S TRUE CENTRE. Root joints are NOT the
 * centre (house1's root sits at {2000,0,0}; shop2's at {17213,0,56808}) —
 * pivoting there displaces the copy by twice the offset, which rendered
 * as a visible second house. Since vertex data only exists at runtime
 * (loaded from the user's disc), the centre is MEASURED on first draw by
 * walking the skeleton's display lists and computing the vertex AABB in
 * the parent frame. Skeletons whose display lists can't be parsed simply
 * skip the shell (and say so once in the log). */
#include "libforest/gbi_extensions.h"
#include "pc_platform.h"

int   g_ckf_shell_pass  = 0;
float g_ckf_shell_scale = 0.97f;
/* AABB mid minus root translation (parent frame). Stored relative so an
 * animated root translation can never stale it; the live root trans is
 * re-added at draw time. */
static f32 g_ckf_shell_rel_x, g_ckf_shell_rel_y, g_ckf_shell_rel_z;

extern int pc_vr_active(void);
extern int pc_fp_view_is_active(void);
extern int g_pc_solid_buildings;
extern int g_pc_solid_shell_pct;
extern int g_pc_verbose;

int cKF_shell_wanted(void) {
    return g_pc_solid_buildings && (pc_vr_active() || pc_fp_view_is_active());
}

/* --- skeleton AABB measurement --- */

typedef struct {
    f32 min_x, min_y, min_z;
    f32 max_x, max_y, max_z;
    f32 root_px, root_py, root_pz;
    int verts;
    int fail;
    int packets;
} ckf_measure_box_t;

/* Static-data pointer resolution, mirroring emu64::seg2k0 minus the live
 * segment table: a LIVE segment address means we cannot know what the DL
 * references at measure time, so the measurement fails (never guess). */
static u32 ckf_measure_resolve(u32 adr) {
    uintptr_t p = pc_gbi_unpack_runtime_ptr(adr);
    if (p != 0) {
        return (u32)p;
    }
    if (adr & 1) {
        return adr & ~1u;
    }
    if ((adr >> 28) != 0 || adr < 0x03000000) {
        return adr;
    }
    if (adr >= pc_image_base && adr < pc_image_end) {
        return adr;
    }
    return 0;
}

static void ckf_measure_dl(Gfx* g, ckf_measure_box_t* box, int depth) {
    MtxF* m = get_Matrix_now();

    if (depth > 4) {
        box->fail = 1;
        return;
    }

    for (;;) {
        u32 w0;
        u32 w1;
        u8 cmd;

        if (box->fail || ++box->packets > 8192) {
            box->fail = 1;
            return;
        }

        w0 = g->words.w0;
        w1 = g->words.w1;
        cmd = (u8)(w0 >> 24);

        if (cmd == G_ENDDL) {
            return;
        }

        if (cmd == G_VTX) {
            Gvtx* gv = (Gvtx*)g;
            u32 n = gv->n;
            u32 a = ckf_measure_resolve(gv->addr);
            Vtx* v = (Vtx*)(uintptr_t)a;
            u32 i;

            if (a == 0 || n == 0 || n > 128) {
                box->fail = 1;
                return;
            }
            for (i = 0; i < n; i++) {
                f32 x = v[i].n.ob[0];
                f32 y = v[i].n.ob[1];
                f32 z = v[i].n.ob[2];
                f32 wx = m->xx * x + m->xy * y + m->xz * z + m->xw;
                f32 wy = m->yx * x + m->yy * y + m->yz * z + m->yw;
                f32 wz = m->zx * x + m->zy * y + m->zz * z + m->zw;

                if (box->verts == 0) {
                    box->min_x = box->max_x = wx;
                    box->min_y = box->max_y = wy;
                    box->min_z = box->max_z = wz;
                } else {
                    if (wx < box->min_x) box->min_x = wx;
                    if (wx > box->max_x) box->max_x = wx;
                    if (wy < box->min_y) box->min_y = wy;
                    if (wy > box->max_y) box->max_y = wy;
                    if (wz < box->min_z) box->min_z = wz;
                    if (wz > box->max_z) box->max_z = wz;
                }
                box->verts++;
            }
        } else if (cmd == G_TRIN || cmd == G_TRIN_INDEPEND) {
            /* Packed triangles: the continuation words carry NO opcode byte
             * and must be skipped by face count, exactly as emu64::dl_G_TRIN
             * consumes them (5-bit words: 3 faces on the first word then 4;
             * 7-bit words: 2 then 3). */
            int n_faces = (int)((w0 >> 17) & 0x7F) + 1;
            int first = 1;
            Gfx* w = g;

            while (n_faces > 0) {
                int is5 = ((w->words.w1 & POLY_BITMASK) == POLY_5b);

                n_faces -= is5 ? (first ? 3 : 4) : (first ? 2 : 3);
                first = 0;
                w++;
                if (++box->packets > 8192) {
                    box->fail = 1;
                    return;
                }
            }
            g = w;
            continue;
        } else if (cmd == G_DL) {
            u32 par = (w0 >> 16) & 0xFF;
            u32 a = ckf_measure_resolve(w1);

            /* Only plain push/nopush calls are parseable; GXDL payloads are
             * raw GX data and anything else is unknown territory. */
            if (a == 0 || (par != G_DL_PUSH && par != G_DL_NOPUSH)) {
                box->fail = 1;
                return;
            }
            if (par == G_DL_NOPUSH) {
                g = (Gfx*)(uintptr_t)a;
                continue;
            }
            ckf_measure_dl((Gfx*)(uintptr_t)a, box, depth + 1);
            if (box->fail) {
                return;
            }
        } else if (cmd == G_MTX) {
            /* A matrix load inside the model (yamishop's sliding door does
             * this) means later vertices land somewhere we cannot compute —
             * measuring them under the wrong matrix would poison the centre.
             * Fail honestly; that model just gets no shell. */
            box->fail = 1;
            return;
        } else if (cmd == 0x0D) {
            /* G_QUADN: packed quads whose continuation words carry no opcode
             * byte — we do not parse them, and single-packet skipping would
             * misinterpret the stream. No building model uses them today. */
            box->fail = 1;
            return;
        }
        /* Everything else (DP state, Dolphin extensions) is a single 8-byte
         * packet — skip it without dereferencing anything. */
        g++;
    }
}

/* Mirrors cKF_Si3_draw_SV_R_child's transform selection exactly, minus the
 * drawing, so the AABB lands where the geometry really renders. */
static void ckf_measure_joint(cKF_SkeletonInfo_R_c* keyframe, int* joint_num, ckf_measure_box_t* box) {
    cKF_Joint_R_c* skel_c_joint = keyframe->skeleton->joint_table + *joint_num;
    s_xyz* cur_joint = &keyframe->current_joint[*joint_num];
    xyz_t trans;
    s_xyz joint1;
    int an_flag;
    int i;

    if (*joint_num != 0) {
        trans.x = skel_c_joint->translation.x;
        trans.y = skel_c_joint->translation.y;
        trans.z = skel_c_joint->translation.z;
    } else {
        an_flag = keyframe->animation_enabled;
        if (an_flag & cKF_ANIMATION_TRANS_XZ) {
            trans.x = keyframe->base_model_translation.x;
            trans.z = keyframe->base_model_translation.z;
        } else {
            trans.x = cur_joint->x;
            trans.z = cur_joint->z;
        }
        if (an_flag & cKF_ANIMATION_TRANS_Y) {
            trans.y = keyframe->base_model_translation.y;
        } else {
            trans.y = cur_joint->y;
        }
        box->root_px = trans.x;
        box->root_py = trans.y;
        box->root_pz = trans.z;
    }

    joint1 = cur_joint[1];
    if ((joint_num[0] == 0) && (keyframe->animation_enabled & cKF_ANIMATION_ROT_Y)) {
        joint1.x = keyframe->base_model_rotation.x;
        joint1.y = keyframe->updated_base_model_rotation.y;
        joint1.z = keyframe->updated_base_model_rotation.z;
    }

    Matrix_push();
    Matrix_softcv3_mult(&trans, &joint1);

    if (skel_c_joint->model != NULL && !box->fail) {
        ckf_measure_dl(skel_c_joint->model, box, 0);
    }

    joint_num[0]++;
    for (i = 0; i < skel_c_joint->child; i++) {
        ckf_measure_joint(keyframe, joint_num, box);
    }

    Matrix_pull();
}

/* Per-skeleton measurement cache. state: 0 empty, 1 ok, -1 failed. */
typedef struct {
    cKF_Skeleton_R_c* skel;
    int state;
    f32 rel_x, rel_y, rel_z;
} ckf_shell_cache_t;

#define CKF_SHELL_CACHE_MAX 48
static ckf_shell_cache_t s_shell_cache[CKF_SHELL_CACHE_MAX];
static int s_shell_cache_count;

static ckf_shell_cache_t* ckf_shell_measure(cKF_SkeletonInfo_R_c* keyframe) {
    ckf_shell_cache_t* slot;
    ckf_measure_box_t box;
    MtxF ident;
    int jn;
    int i;

    for (i = 0; i < s_shell_cache_count; i++) {
        if (s_shell_cache[i].skel == keyframe->skeleton) {
            return &s_shell_cache[i];
        }
    }

    /* Never bake a mid-animation pose into the cache: a save resumed at
     * the house-exit animation boots with the door OPEN, and a centre
     * measured then would be skewed for the whole session. Frames 1 and
     * end are the shut poses in every door animation; anything between is
     * mid-swing, so DEFER (no cache write, retry next frame). NOTE: do not
     * gate on speed — the island cottage idles at speed 0.5 forever and a
     * speed test would starve its shell permanently. */
    {
        f32 cf = keyframe->frame_control.current_frame;

        if (cf != 1.0f && cf != keyframe->frame_control.end_frame) {
            return NULL;
        }
    }

    if (s_shell_cache_count >= CKF_SHELL_CACHE_MAX) {
        static int warned;

        if (!warned) {
            warned = 1;
            printf("[SolidBuildings] shell cache full (%d); later models get no shell\n",
                   CKF_SHELL_CACHE_MAX);
        }
        return NULL; /* treat as failure; never evict */
    }

    slot = &s_shell_cache[s_shell_cache_count++];
    slot->skel = keyframe->skeleton;
    slot->state = -1;
    slot->rel_x = slot->rel_y = slot->rel_z = 0.0f;

    bzero(&box, sizeof(box));

    bzero(&ident, sizeof(ident));
    ident.xx = 1.0f;
    ident.yy = 1.0f;
    ident.zz = 1.0f;
    ident.ww = 1.0f;

    /* Measure in the PARENT frame: seed an identity so the island houses'
     * rotated roots ((-90,0,+90)) are handled by the walk itself. */
    Matrix_push();
    Matrix_put(&ident);
    jn = 0;
    ckf_measure_joint(keyframe, &jn, &box);
    Matrix_pull();

    if (!box.fail && box.verts > 0) {
        slot->state = 1;
        slot->rel_x = (box.min_x + box.max_x) * 0.5f - box.root_px;
        slot->rel_y = (box.min_y + box.max_y) * 0.5f - box.root_py;
        slot->rel_z = (box.min_z + box.max_z) * 0.5f - box.root_pz;
    }

    if (g_pc_verbose || slot->state != 1) {
        printf("[SolidBuildings] skel=%p joints=%d verts=%d root=(%.0f,%.0f,%.0f) centre_rel=(%.0f,%.0f,%.0f) ext=(%.0f,%.0f,%.0f)%s\n",
               (void*)keyframe->skeleton, keyframe->skeleton->num_joints, box.verts,
               box.root_px, box.root_py, box.root_pz,
               slot->rel_x, slot->rel_y, slot->rel_z,
               (box.max_x - box.min_x) * 0.5f, (box.max_y - box.min_y) * 0.5f,
               (box.max_z - box.min_z) * 0.5f,
               slot->state == 1 ? "" : "  MEASUREMENT FAILED - shell disabled for this model");
    }

    return slot;
}
#endif

extern void cKF_Si3_draw_SV_R_child(GAME* game, cKF_SkeletonInfo_R_c* keyframe, int* joint_num,
                                    cKF_draw_callback prerender_callback, cKF_draw_callback postrender_callback,
                                    void* arg, Mtx** mtxpp) {
    int i;
    int an_flag;
    Gfx* joint_m;
    Gfx* mjoint_m;
    u8 joint_f;
    xyz_t trans;
    s_xyz joint1;
    cKF_Joint_R_c* skel_c_joint;
    s_xyz* cur_joint;
    GRAPH* graph;

    skel_c_joint = keyframe->skeleton->joint_table;
    skel_c_joint += *joint_num;
    cur_joint = &keyframe->current_joint[*joint_num];

    if (*joint_num != 0) {
        // Since we have joints, take the current joint's translation
        trans.x = skel_c_joint->translation.x;
        trans.y = skel_c_joint->translation.y;
        trans.z = skel_c_joint->translation.z;
    } else {
        // No joints, take the base model translation w/ respect to flags
        an_flag = keyframe->animation_enabled;
        if (an_flag & cKF_ANIMATION_TRANS_XZ) {
            trans.x = keyframe->base_model_translation.x;
            trans.z = keyframe->base_model_translation.z;
        } else {
            trans.x = cur_joint->x;
            trans.z = cur_joint->z;
        }
        if (an_flag & cKF_ANIMATION_TRANS_Y) {
            trans.y = keyframe->base_model_translation.y;
        } else {
            trans.y = cur_joint->y;
        }
    }

    joint1 = cur_joint[1];

    if ((joint_num[0] == 0) && (keyframe->animation_enabled & cKF_ANIMATION_ROT_Y)) {
        joint1.x = keyframe->base_model_rotation.x;
        joint1.y = keyframe->updated_base_model_rotation.y;
        joint1.z = keyframe->updated_base_model_rotation.z;
    }

    graph = game->graph;
    OPEN_DISP(graph);
    Matrix_push();

    joint_m = skel_c_joint->model;
    mjoint_m = joint_m;
    joint_f = skel_c_joint->flags;

    // Render joint if prerender callback wasn't supplied or the callback does not return FALSE
    {
    if ((prerender_callback == NULL) ||
        ((prerender_callback != NULL) &&
         (prerender_callback(game, keyframe, *joint_num, &mjoint_m, &joint_f, arg, &joint1, &trans)) != FALSE)) {
#ifdef TARGET_PC
        if (g_ckf_shell_pass && *joint_num == 0) {
            /* Spin and shrink about the MEASURED building centre, in the
             * PARENT frame: M · T(c) · RotY(180) · S · T(-c) · <normal
             * chain>. The fixed point is c, so the copy lands exactly on
             * the original's footprint (the old root-pivot version was
             * displaced by twice the pivot-to-centre offset and read as a
             * second house). Parent-frame rotation also keeps the island
             * houses upright — their roots are rotated (-90,0,+90). */
            f32 cx = g_ckf_shell_rel_x + trans.x;
            f32 cy = g_ckf_shell_rel_y + trans.y;
            f32 cz = g_ckf_shell_rel_z + trans.z;

            Matrix_translate(cx, cy, cz, MTX_MULT);
            Matrix_RotateY((s16)0x8000, MTX_MULT);
            Matrix_scale(g_ckf_shell_scale, g_ckf_shell_scale, g_ckf_shell_scale, MTX_MULT);
            Matrix_translate(-cx, -cy, -cz, MTX_MULT);
            /* trans deliberately NOT zeroed: the unmodified normal chain
             * (softcv3_mult with the real trans and rotation) follows. */
        }
#endif
        Matrix_softcv3_mult(&trans, &joint1);
#ifdef TARGET_PC
        if (g_ckf_shell_pass && (joint_f & cKF_JOINT_FLAG_DISP_XLU)) {
            /* Translucent parts (glass, glow) would double-blend */
            mjoint_m = NULL;
        }
#endif
        if (mjoint_m != NULL) {
            _Matrix_to_Mtx(*mtxpp);
            if (joint_f & cKF_JOINT_FLAG_DISP_XLU) {
                // Joint translated & drawn in XLU display list
                gSPMatrix(NOW_POLY_XLU_DISP++, *mtxpp, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(NOW_POLY_XLU_DISP++, mjoint_m);
            } else {
                // Joint translated & drawin OPA display list
                gSPMatrix(NOW_POLY_OPA_DISP++, *mtxpp, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(NOW_POLY_OPA_DISP++, mjoint_m);
            }
            mtxpp[0]++;
        } else if (joint_m != NULL) {
            // Joint has a rendered model but the prerender callback chose not to render it
            // so we apply the translation still
            _Matrix_to_Mtx(*mtxpp);
            gSPMatrix(NOW_POLY_OPA_DISP++, *mtxpp, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            mtxpp[0]++;
        }
    }
    }

    CLOSE_DISP(graph);

    // Call postrender callback if supplied
    if (postrender_callback != NULL) {
        postrender_callback(game, keyframe, *joint_num, &mjoint_m, &joint_f, arg, &joint1, &trans);
    }

    joint_num[0]++; // Move onto next joint

    // Render all children
    for (i = 0; i < skel_c_joint->child; i++) {
        cKF_Si3_draw_SV_R_child(game, keyframe, joint_num, prerender_callback, postrender_callback, arg, mtxpp);
    }

    // Remove the effect of this joint's translation & rotatation
    Matrix_pull();
}

extern void cKF_Si3_draw_R_SV(GAME* game, cKF_SkeletonInfo_R_c* keyframe, Mtx* mtxp,
                              cKF_draw_callback prerender_callback, cKF_draw_callback postrender_callback, void* arg) {
    int joint_num;
    Mtx* mtx_p = mtxp;

    if (mtxp != NULL) {
        GRAPH* graph = game->graph;
        OPEN_DISP(graph);

        /* TODO: these should probably be made into a custom macro somewhere */
        gSPSegment(NOW_POLY_OPA_DISP++, G_MWO_SEGMENT_D,
                   mtx_p); // Load matrix (opaque)
        gSPSegment(NOW_POLY_XLU_DISP++, G_MWO_SEGMENT_D,
                   mtx_p); // Load matrix (translucent)

        CLOSE_DISP(graph);
        joint_num = 0;

        cKF_Si3_draw_SV_R_child(game, keyframe, &joint_num, prerender_callback, postrender_callback, arg, &mtx_p);
    }
}

#ifdef TARGET_PC
/* Structure draw with the VR solid-shell pass (see cKF_shell_wanted above).
 * Opt-in: only the building draw procs call this — the base function has
 * ~175 callers (every villager, fish and insect) that must not double. */
extern void cKF_Si3_draw_R_SV_solid(GAME* game, cKF_SkeletonInfo_R_c* keyframe, Mtx* mtxp,
                                    cKF_draw_callback prerender_callback,
                                    cKF_draw_callback postrender_callback, void* arg,
                                    cKF_pipeline_reset_proc pipeline_reset) {
    if (mtxp != NULL && keyframe != NULL && keyframe->skeleton != NULL && cKF_shell_wanted()) {
        /* Measure (or look up) the building's true centre BEFORE any
         * allocation or state change: a skeleton whose display lists can't
         * be parsed skips the shell entirely with zero side effects. */
        ckf_shell_cache_t* cache = ckf_shell_measure(keyframe);

        if (cache != NULL && cache->state == 1) {
            /* The shell needs its own Mtx pool: the caller sized its array
             * to exactly num_shown_joints, and the display list is replayed
             * once per eye, so the matrices must stay live for the whole
             * frame. Note GRAPH_ALLOC today can NOT return NULL (it is an
             * unchecked bump-down; exhaustion is caught at frame end by
             * THA_GA_isCrash) — the check only guards a future bounded
             * allocator. */
            Mtx* shell_mtx = GRAPH_ALLOC_TYPE(game->graph, Mtx, (u32)keyframe->skeleton->num_shown_joints);

            if (shell_mtx != NULL) {
                int pct = g_pc_solid_shell_pct;

                if (pct < 50) pct = 50;
                if (pct > 100) pct = 100;
                g_ckf_shell_scale = pct * 0.01f;
                g_ckf_shell_rel_x = cache->rel_x;
                g_ckf_shell_rel_y = cache->rel_y;
                g_ckf_shell_rel_z = cache->rel_z;
                g_ckf_shell_pass = 1;
                /* Prerender only: postrender callbacks emit extra display
                 * lists (window glow, attachment matrices) that must not be
                 * doubled. */
                cKF_Si3_draw_R_SV(game, keyframe, shell_mtx, prerender_callback, NULL, arg);
                g_ckf_shell_pass = 0;

                /* Some models' joints set combiner / render mode / texture
                 * state that the following joints inherit (Nook's "light"
                 * part is the last joint drawn and leaves an ENVIRONMENT
                 * combiner behind). Without re-running the caller's pipeline
                 * setup, the REAL pass inherits the shell's leftover state
                 * and the building renders as a flat black silhouette in
                 * daylight. */
                if (pipeline_reset != NULL) {
                    pipeline_reset(game->graph);
                }
            }
        }
    }

    cKF_Si3_draw_R_SV(game, keyframe, mtxp, prerender_callback, postrender_callback, arg);
}
#endif

extern void cKF_SkeletonInfo_R_init_standard_repeat_speedsetandmorph(cKF_SkeletonInfo_R_c* keyframe,
                                                                     cKF_Animation_R_c* animation,
                                                                     s_xyz* rotation_diff_table, f32 frame_speed,
                                                                     f32 morph_counter) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, 1.0f, frame_speed,
                            morph_counter, cKF_FRAMECONTROL_REPEAT, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_standard_repeat_setframeandspeedandmorph(cKF_SkeletonInfo_R_c* keyframe,
                                                                             cKF_Animation_R_c* animation,
                                                                             s_xyz* rotation_diff_table, f32 frame,
                                                                             f32 frame_speed, f32 morph_counter) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, frame, frame_speed,
                            morph_counter, cKF_FRAMECONTROL_REPEAT, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_standard_setframeandspeedandmorphandmode(cKF_SkeletonInfo_R_c* keyframe,
                                                                             cKF_Animation_R_c* animation,
                                                                             s_xyz* rotation_diff_table, f32 frame,
                                                                             f32 frame_speed, f32 morph_counter,
                                                                             int mode) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, 1.0f, animation->frames, frame, frame_speed,
                            morph_counter, mode, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_init_reverse_setspeedandmorphandmode(cKF_SkeletonInfo_R_c* keyframe,
                                                                    cKF_Animation_R_c* animation,
                                                                    s_xyz* rotation_diff_table, f32 frame_speed,
                                                                    f32 morph_counter, int mode) {
    cKF_SkeletonInfo_R_init(keyframe, keyframe->skeleton, animation, animation->frames, 1.0f, animation->frames,
                            frame_speed, morph_counter, mode, rotation_diff_table);
}

extern void cKF_SkeletonInfo_R_combine_work_set(cKF_SkeletonInfo_R_combine_work_c* combine,
                                                cKF_SkeletonInfo_R_c* keyframe) {
    combine->keyframe = keyframe;
    combine->anm_const_val_tbl = keyframe->animation->fixed_table;
    combine->anm_key_num = keyframe->animation->key_table;
    combine->anm_data_src = keyframe->animation->data_table;
    combine->anm_check_bit_tbl = keyframe->animation->flag_table;
    combine->anm_key_num_idx = 0;
    combine->anm_const_val_tbl_idx = 0;
    combine->anm_data_src_idx = 0;
}

extern void cKF_SkeletonInfo_R_combine_translation(s16** joint, int* flag, cKF_SkeletonInfo_R_combine_work_c* combine,
                                                   s8* part_table) {
    int i;

    for (i = 0; i < 3; i++) {
        /* Determine which animation we should pull from for the joint */
        switch (*part_table) {
            case 0:
                if (*combine[0].anm_check_bit_tbl & *flag) {
                    (**joint) =
                        cKF_KeyCalc(combine[0].anm_data_src_idx, combine[0].anm_key_num[combine[0].anm_key_num_idx],
                                    combine[0].anm_data_src, combine[0].keyframe->frame_control.current_frame);
                } else {
                    (**joint) = combine[0].anm_const_val_tbl[combine[0].anm_const_val_tbl_idx];
                }

                break;
            case 1:
                if (*combine[1].anm_check_bit_tbl & *flag) {
                    (**joint) =
                        cKF_KeyCalc(combine[1].anm_data_src_idx, combine[1].anm_key_num[combine[1].anm_key_num_idx],
                                    combine[1].anm_data_src, combine[1].keyframe->frame_control.current_frame);
                } else {
                    (**joint) = combine[1].anm_const_val_tbl[combine[1].anm_const_val_tbl_idx];
                }

                break;
            case 2:
                if (*combine[2].anm_check_bit_tbl & *flag) {
                    (**joint) =
                        cKF_KeyCalc(combine[2].anm_data_src_idx, combine[2].anm_key_num[combine[2].anm_key_num_idx],
                                    combine[2].anm_data_src, combine[2].keyframe->frame_control.current_frame);
                } else {
                    (**joint) = combine[2].anm_const_val_tbl[combine[2].anm_const_val_tbl_idx];
                }

                break;
        }

        if (*combine[0].anm_check_bit_tbl & *flag) {
            combine[0].anm_data_src_idx += combine[0].anm_key_num[combine[0].anm_key_num_idx];
            combine[0].anm_key_num_idx++;
        } else {
            combine[0].anm_const_val_tbl_idx++;
        }

        if (*combine[1].anm_check_bit_tbl & *flag) {
            combine[1].anm_data_src_idx += combine[1].anm_key_num[combine[1].anm_key_num_idx];
            combine[1].anm_key_num_idx++;
        } else {
            combine[1].anm_const_val_tbl_idx++;
        }

        if (*combine[2].anm_check_bit_tbl & *flag) {
            combine[2].anm_data_src_idx += combine[2].anm_key_num[combine[2].anm_key_num_idx];
            combine[2].anm_key_num_idx++;
        } else {
            combine[2].anm_const_val_tbl_idx++;
        }

        *flag = (u32)*flag >> 1;
        *joint += 1;
    }
}

extern void cKF_SkeletonInfo_R_combine_rotation(s16** joint, int* flag, cKF_SkeletonInfo_R_combine_work_c* combine,
                                                s8* part_table) {
    int i;
    int j;
    f32 calc_joint;

    for (i = 0; i < combine->keyframe->skeleton->num_joints; i++) {
        *flag = 4;

        for (j = 0; j < 3; j++) {
            /* Determine which animation we should pull from for the joint */
            switch (part_table[i + 1]) {
                case 0:
                    if (*flag & combine[0].anm_check_bit_tbl[i]) {
                        (**joint) =
                            cKF_KeyCalc(combine[0].anm_data_src_idx, combine[0].anm_key_num[combine[0].anm_key_num_idx],
                                        combine[0].anm_data_src, combine[0].keyframe->frame_control.current_frame);
                    } else {
                        (**joint) = combine[0].anm_const_val_tbl[combine[0].anm_const_val_tbl_idx];
                    }
                    break;

                case 1:
                    if (*flag & combine[1].anm_check_bit_tbl[i]) {
                        (**joint) =
                            cKF_KeyCalc(combine[1].anm_data_src_idx, combine[1].anm_key_num[combine[1].anm_key_num_idx],
                                        combine[1].anm_data_src, combine[1].keyframe->frame_control.current_frame);
                    } else {
                        (**joint) = combine[1].anm_const_val_tbl[combine[1].anm_const_val_tbl_idx];
                    }
                    break;

                case 2:
                    if (*flag & combine[2].anm_check_bit_tbl[i]) {
                        (**joint) =
                            cKF_KeyCalc(combine[2].anm_data_src_idx, combine[2].anm_key_num[combine[2].anm_key_num_idx],
                                        combine[2].anm_data_src, combine[2].keyframe->frame_control.current_frame);
                    } else {
                        (**joint) = combine[2].anm_const_val_tbl[combine[2].anm_const_val_tbl_idx];
                    }
                    break;
            }
            if (*flag & combine[0].anm_check_bit_tbl[i]) {
                combine[0].anm_data_src_idx += combine[0].anm_key_num[combine[0].anm_key_num_idx];
                combine[0].anm_key_num_idx++;
            } else {
                combine[0].anm_const_val_tbl_idx++;
            }
            if (*flag & combine[1].anm_check_bit_tbl[i]) {
                combine[1].anm_data_src_idx += combine[1].anm_key_num[combine[1].anm_key_num_idx];
                combine[1].anm_key_num_idx++;
            } else {
                combine[1].anm_const_val_tbl_idx++;
            }
            if (*flag & combine[2].anm_check_bit_tbl[i]) {
                combine[2].anm_data_src_idx += combine[2].anm_key_num[combine[2].anm_key_num_idx];
                combine[2].anm_key_num_idx++;
            } else {
                combine[2].anm_const_val_tbl_idx++;
            }

            /*
             * At this point, rotation values in joint are encoded in [degree.x] format.
             * This gives one decimal of rotational precision. We then convert from degrees to
             * s16 binangle.
             */

            /* s16 degree -> float degree with 1 decimal place precision */
            calc_joint = 0.1f * (**joint);
            /* degree (unbound) -> [0.0f, 360.0f) -> binangle [-32768, 32767] */
            **joint = (s16)DEG2SHORT_ANGLE2(MOD_F(calc_joint, 360.0f));
            *flag = (u32)*flag >> 1;
            *joint += 1;
        }
    }
}

extern int cKF_SkeletonInfo_R_combine_play(cKF_SkeletonInfo_R_c* info1, cKF_SkeletonInfo_R_c* info2, s8* part_table) {
    int combinet;
    s16* joint;
    cKF_SkeletonInfo_R_combine_work_c combine[3];
    int i;
    s_xyz* joint2;
    s_xyz* applyjoint;

    if ((info1 == NULL) || (info2 == NULL) || (part_table == NULL)) {
        return cKF_STATE_NONE;
    }
    joint = (F32_IS_ZERO(info1->morph_counter)) ? &info1->current_joint->x : &info1->target_joint->x;

    if (info1 != NULL) {
        cKF_SkeletonInfo_R_combine_work_set(&combine[0], info1);
    }
    if (info2 != NULL) {
        cKF_SkeletonInfo_R_combine_work_set(&combine[1], info2);
        cKF_SkeletonInfo_R_combine_work_set(&combine[2], info2);
    }
    combinet = 0x20;
    cKF_SkeletonInfo_R_combine_translation(&joint, &combinet, &combine[0], part_table);
    cKF_SkeletonInfo_R_combine_rotation(&joint, &combinet, &combine[0], part_table);

    if (info1->rotation_diff_table != NULL) {
        if (F32_IS_ZERO(info1->morph_counter)) {
            applyjoint = info1->current_joint;
        } else {
            applyjoint = info1->target_joint;
        }

        applyjoint += 1;
        for (i = 0; i < info1->skeleton->num_joints; i++) {
            applyjoint->x += info1->rotation_diff_table[i].x;
            applyjoint->y += info1->rotation_diff_table[i].y;
            applyjoint->z += info1->rotation_diff_table[i].z;

            applyjoint++;
        }
    }
    if (F32_IS_ZERO(info1->morph_counter)) {
        cKF_FrameControl_play(&info2->frame_control);
        return cKF_FrameControl_play(&info1->frame_control);
    } else {
        f32 morph_step = 0.5f * (f32)gamePT->graph->dt_num_60fps_frames;
        if (info1->morph_counter > 0.0f) {
            cKF_SkeletonInfo_R_morphJoint(info1);
            info1->morph_counter -= morph_step;
            if (info1->morph_counter <= 0.0f) {
                info1->morph_counter = 0.0f;
            }
            return cKF_STATE_NONE;
        } else {
            cKF_SkeletonInfo_R_morphJoint(info1);
            info1->morph_counter += morph_step;
            if (info1->morph_counter >= 0.0f) {
                info1->morph_counter = 0.0f;
            }
            cKF_FrameControl_play(&info2->frame_control);
            return cKF_FrameControl_play(&info1->frame_control);
        }
    }
}

extern void cKF_SkeletonInfo_R_T_combine_play(int* state1, int* state2, int* state3, cKF_SkeletonInfo_R_c* info1,
                                              cKF_SkeletonInfo_R_c* info2, cKF_SkeletonInfo_R_c* info3,
                                              s8* part_table) {
    int combinet;
    s16* joint;
    cKF_SkeletonInfo_R_combine_work_c combine[3];
    int i;
    s_xyz* joint2;
    s_xyz* applyjoint;

    if ((info1 == NULL) || (info2 == NULL) || (info3 == NULL) || (part_table == NULL)) {
        return;
    }

    joint = (F32_IS_ZERO(info1->morph_counter)) ? &info1->current_joint->x : &info1->target_joint->x;

    if (info1 != NULL) {
        cKF_SkeletonInfo_R_combine_work_set(&combine[0], info1);
    }
    if (info2 != NULL) {
        cKF_SkeletonInfo_R_combine_work_set(&combine[1], info2);
    }
    if (info3 != NULL) {
        cKF_SkeletonInfo_R_combine_work_set(&combine[2], info3);
    }

    combinet = 0x20;
    cKF_SkeletonInfo_R_combine_translation(&joint, &combinet, &combine[0], part_table);
    cKF_SkeletonInfo_R_combine_rotation(&joint, &combinet, &combine[0], part_table);

    if (info1->rotation_diff_table != NULL) {
        if (F32_IS_ZERO(info1->morph_counter)) {
            applyjoint = info1->current_joint;
        } else {
            applyjoint = info1->target_joint;
        }

        applyjoint += 1;
        for (i = 0; i < info1->skeleton->num_joints; i++) {
            applyjoint->x += info1->rotation_diff_table[i].x;
            applyjoint->y += info1->rotation_diff_table[i].y;
            applyjoint->z += info1->rotation_diff_table[i].z;

            applyjoint++;
        }
    }
    if (F32_IS_ZERO(info1->morph_counter)) {
        *state1 = cKF_FrameControl_play(&info1->frame_control);
        *state2 = cKF_FrameControl_play(&info2->frame_control);
        *state3 = cKF_FrameControl_play(&info3->frame_control);
    } else {
        f32 morph_step = 0.5f * (f32)gamePT->graph->dt_num_60fps_frames;
        if (info1->morph_counter > 0.0f) {
            cKF_SkeletonInfo_R_morphJoint(info1);
            info1->morph_counter -= morph_step;
            if (info1->morph_counter <= 0.0f) {
                info1->morph_counter = 0.0f;
            }
            *state1 = cKF_STATE_NONE;
            *state2 = cKF_STATE_NONE;
            *state3 = cKF_STATE_NONE;
        } else {
            cKF_SkeletonInfo_R_morphJoint(info1);
            info1->morph_counter += morph_step;
            if (info1->morph_counter >= 0.0f) {
                info1->morph_counter = 0.0f;
            }
            *state1 = cKF_FrameControl_play(&info1->frame_control);
            *state2 = cKF_FrameControl_play(&info2->frame_control);
            *state3 = cKF_FrameControl_play(&info3->frame_control);
        }
    }
}

extern void cKF_SkeletonInfo_R_Animation_Set_base_shape_trs(cKF_SkeletonInfo_R_c* keyframe, f32 transx, f32 transy,
                                                            f32 transz, s16 anglex, s16 angley, s16 anglez) {
    keyframe->base_model_translation.x = transx;
    keyframe->base_model_translation.y = transy;
    keyframe->base_model_translation.z = transz;

    keyframe->base_model_rotation.x = anglex;
    keyframe->updated_base_model_rotation.x = anglex;

    keyframe->base_model_rotation.y = angley;
    keyframe->updated_base_model_rotation.y = angley;

    keyframe->base_model_rotation.z = anglez;
    keyframe->updated_base_model_rotation.z = anglez;
}

extern void cKF_SkeletonInfo_R_AnimationMove_ct_base(xyz_t* basepos, xyz_t* correctpos, s16 ybase, s16 yidle,
                                                     f32 counter, cKF_SkeletonInfo_R_c* keyframe, int an_flag) {
    keyframe->animation_enabled = an_flag;
    keyframe->fixed_counter = (counter >= 0.0f) ? counter : -counter;
    keyframe->base_world_position = ZeroVec;
    keyframe->model_world_position_correction = ZeroVec;

    if (basepos != NULL) {
        if (correctpos == NULL) {
            correctpos = basepos;
        }
        if (an_flag & cKF_ANIMATION_TRANS_XZ) {
            keyframe->base_world_position.x = correctpos->x;
            keyframe->base_world_position.z = correctpos->z;
            keyframe->model_world_position_correction.x = basepos->x - correctpos->x;
            keyframe->model_world_position_correction.z = basepos->z - correctpos->z;
        }
        if (an_flag & cKF_ANIMATION_TRANS_Y) {
            keyframe->base_world_position.y = correctpos->y;
            keyframe->model_world_position_correction.y = basepos->y - correctpos->y;
        }
    }
    keyframe->base_angle_y = yidle;
    keyframe->model_angle_correction = 0;

    if (an_flag & cKF_ANIMATION_ROT_Y) {
        int sub = ybase - yidle;

        if (sub > DEG2SHORT_ANGLE2(180.0f)) {
            sub = -(DEG2SHORT_ANGLE2(360.0f) - sub);
        } else if (sub < DEG2SHORT_ANGLE2(-180.0f)) {
            sub += DEG2SHORT_ANGLE2(360.0f);
        }
        keyframe->base_angle_y = yidle;
        keyframe->model_angle_correction = sub;
    }
}

extern void cKF_SkeletonInfo_R_AnimationMove_dt(cKF_SkeletonInfo_R_c* keyframe) {
    int an_flag = keyframe->animation_enabled;
    s_xyz* cur_joint = keyframe->current_joint;

    if (an_flag & cKF_ANIMATION_TRANS_XZ) {
        cur_joint->x = keyframe->base_model_translation.x;
        cur_joint->z = keyframe->base_model_translation.z;
    }
    if (an_flag & cKF_ANIMATION_TRANS_Y) {
        cur_joint->y = keyframe->base_model_translation.y;
    }
    if (an_flag & cKF_ANIMATION_ROT_Y) {
        cur_joint = keyframe->current_joint;
        cur_joint[1].x = keyframe->base_model_rotation.x;
        cur_joint[1].y = keyframe->base_model_rotation.y;
        cur_joint[1].z = keyframe->base_model_rotation.z;
    }
    keyframe->animation_enabled = 0;
}

extern void cKF_SkeletonInfo_R_AnimationMove_base(xyz_t* base, s16* sbase, xyz_t* scale, s16 yidle,
                                                  cKF_SkeletonInfo_R_c* keyframe) {
    f32 fc = keyframe->fixed_counter;
    f32 move_step = 0.5f * (f32)gamePT->graph->dt_num_60fps_frames;
    f32 count = fc + (move_step * 2.0f);
    int an_flag = keyframe->animation_enabled;
    f32 correct_y;
    f32 mangle_y;
    s16 angley;
    s16 angle_c;
    s_xyz* update_base;
    s16 base_x;
    s_xyz* cur_joint;
    s16 sub;
    f32 trans_x;
    f32 trans_z;
    f32 move_x;
    f32 move_z;
    f32 temp;
    f32 sin, cos;

    if (count > 0.0f) {
        correct_y = move_step / count;
    } else {
        correct_y = 0.0f;
    }

    if (an_flag & cKF_ANIMATION_ROT_Y) {
        mangle_y = keyframe->model_angle_correction;
        if (count > 0.0f) {
            keyframe->model_angle_correction -= (s16)(int)(mangle_y * correct_y);
        } else {
            keyframe->model_angle_correction = 0;
        }
    }

    if (count > 0.0f) {
        if (an_flag & cKF_ANIMATION_TRANS_XZ) {
            f32 cx, cz;

            cx = keyframe->model_world_position_correction.x;
            cx *= correct_y;

            cz = keyframe->model_world_position_correction.z;
            cz *= correct_y;

            keyframe->model_world_position_correction.x -= cx;
            keyframe->model_world_position_correction.z -= cz;
        }
        if (an_flag & cKF_ANIMATION_TRANS_Y) {
            f32 cy;

            cy = keyframe->model_world_position_correction.y;
            cy *= correct_y;

            keyframe->model_world_position_correction.y -= cy;
        }
    } else {
        keyframe->model_world_position_correction.x = 0.0f;
        keyframe->model_world_position_correction.y = 0.0f;
        keyframe->model_world_position_correction.z = 0.0f;
    }

    if ((sbase != NULL) && (an_flag & cKF_ANIMATION_ROT_Y)) {
        angley = keyframe->base_angle_y;
        angle_c = keyframe->model_angle_correction;
        base_x = keyframe->base_model_rotation.x;
        update_base = &keyframe->updated_base_model_rotation;
        Matrix_push();
        Matrix_rotateXYZ(keyframe->current_joint[1].x, keyframe->current_joint[1].y, keyframe->current_joint[1].z, MTX_LOAD);
        Matrix_to_rotate2_new(get_Matrix_now(), update_base, MTX_LOAD);
        Matrix_pull();
        *sbase = angley + angle_c + (update_base->x - base_x);
    }

    if (base != NULL) {
        cur_joint = keyframe->current_joint;
        sub = 0;
        if (sbase != NULL) {
            sub = *sbase - yidle;
        }
        if (an_flag & cKF_ANIMATION_TRANS_XZ) {
            f32 move_x, move_z;
            f32 base_x, base_z;
            f32 temp2;
            f32 temp1;

            trans_x = keyframe->base_model_translation.x;
            trans_z = keyframe->base_model_translation.z;

            sin = sin_s(sub);
            cos = cos_s(sub);

            temp1 = (trans_x * cos) + (trans_z * sin);
            move_x = scale->x * (cur_joint->x - temp1);
            temp1 = (-trans_x * sin) + (trans_z * cos);
            move_z = scale->z * (cur_joint->z - temp1);

            sin = sin_s(yidle);
            cos = cos_s(yidle);

            base_x = keyframe->model_world_position_correction.x;
            base_z = keyframe->model_world_position_correction.z;
            temp2 = (move_x * cos) + (move_z * sin);
            base->x = temp2 + (keyframe->base_world_position.x + base_x);
            temp2 = (-move_x * sin) + (move_z * cos);
            base->z = temp2 + (keyframe->base_world_position.z + base_z);
        }
        if (an_flag & cKF_ANIMATION_TRANS_Y) {
            base->y = scale->y * (cur_joint->y - keyframe->base_model_translation.y) +
                      (keyframe->base_world_position.y + keyframe->model_world_position_correction.y);
        }
    }
    count = fc - move_step;
    if (count < 0.0f) {
        count = 0.0f;
    }
    keyframe->fixed_counter = count;
}

extern void cKF_SkeletonInfo_R_AnimationMove_CulcTransToWorld(xyz_t* calc_pos, const xyz_t* base_pos, f32 trans_x,
                                                              f32 trans_y, f32 trans_z, s16 angle_y, const xyz_t* scale,
                                                              cKF_SkeletonInfo_R_c* keyframe, int trans_flag) {
    f32 sin, cos;
    f32 j_x, j_z;
    s_xyz* cur_joint = keyframe->current_joint;

    if (trans_flag & cKF_ANIMATION_TRANS_XZ) {
        j_x = cur_joint->x - trans_x;
        j_z = cur_joint->z - trans_z;

        sin = sin_s(angle_y);
        cos = cos_s(angle_y);

        calc_pos->x = base_pos->x + scale->x * ((j_x * cos) + (j_z * sin));
        calc_pos->z = base_pos->z + scale->z * ((-j_x * sin) + (j_z * cos));
    }

    if (trans_flag & cKF_ANIMATION_TRANS_Y) {
        calc_pos->y = base_pos->y + scale->y * (cur_joint->y - trans_y);
    }
}
