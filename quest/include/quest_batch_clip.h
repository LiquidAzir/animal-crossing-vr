/* Conservative render-only rejection. No renderer state or GL calls. */
#ifndef QUEST_BATCH_CLIP_H
#define QUEST_BATCH_CLIP_H
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Matrices are the exact row-major uniforms for this draw; modelview is
 * affine, as in the GX vertex shader. Each strided
 * element starts with three float positions. Caller owns valid array storage
 * and limits use to triangle/quad world batches. Zero means keep the draw.
 * The six planes match GL's -w..w clip volume, including the world's unusual
 * GX projection whose nominal far distance maps to z=0 rather than z=w. */
static inline int quest_batch_clip_outside(const float projection[16], const float modelview[16], const void* positions,
                                         size_t stride, size_t count) {
    unsigned common = 63;
    size_t i;
    if (!projection || !modelview || !positions || !count || stride < 3 * sizeof(float) ||
        count - 1 > (SIZE_MAX - 3 * sizeof(float)) / stride) return 0;
    if ((uintptr_t)positions > UINTPTR_MAX - ((count - 1) * stride + 3 * sizeof(float))) return 0;
    for (i = 0; i < 16; ++i)
        if (!isfinite(projection[i]) || fabsf(projection[i]) > 1.0e12f ||
            !isfinite(modelview[i]) || fabsf(modelview[i]) > 1.0e12f) return 0;
    if (modelview[12] != 0 || modelview[13] != 0 || modelview[14] != 0 || modelview[15] != 1) return 0;
    for (i = 0; i < count; ++i) {
        float p[3], eye[3], eye_error[3], clip[4], error[4];
        unsigned outside = 0;
        int row;
        memcpy(p, (const unsigned char*)positions + i * stride, sizeof(p));
        for (row = 0; row < 3; ++row)
            if (!isfinite(p[row]) || fabsf(p[row]) > 1.0e12f) return 0;
        for (row = 0; row < 3; ++row) {
            const float* m = modelview + row * 4;
            const float x = m[0] * p[0], y = m[1] * p[1], z = m[2] * p[2];
            eye[row] = ((x + y) + z) + m[3];
            eye_error[row] = (fabsf(x) + fabsf(y) + fabsf(z) + fabsf(m[3])) *
                         (64.0f * FLT_EPSILON) + 1.0e-5f;
            if (!isfinite(eye[row]) || !isfinite(eye_error[row])) return 0;
        }
        for (row = 0; row < 4; ++row) {
            const float* m = projection + row * 4;
            const float x = m[0] * eye[0], y = m[1] * eye[1], z = m[2] * eye[2];
            clip[row] = ((x + y) + z) + m[3];
            /* Keep the shader's two stages, propagating modelview error.
             * Precomposing P*MV can hide cancellation in either stage. */
            error[row] = fabsf(m[0]) * eye_error[0] + fabsf(m[1]) * eye_error[1] +
                         fabsf(m[2]) * eye_error[2] +
                         (fabsf(x) + fabsf(y) + fabsf(z) + fabsf(m[3])) *
                         (64.0f * FLT_EPSILON) + 1.0e-5f;
            if (!isfinite(clip[row]) || !isfinite(error[row])) return 0;
        }
        for (row = 0; row < 3; ++row) {
            const float margin = error[row] + error[3];
            if (clip[row] < -clip[3] - margin) outside |= 1u << (row * 2);
            if (clip[row] >  clip[3] + margin) outside |= 2u << (row * 2);
        }
        common &= outside;
        if (!common) return 0; /* Remaining vertices cannot make a common plane. */
    }
    return common != 0;
}
#endif
