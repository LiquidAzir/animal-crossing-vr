#pragma once

#include <stdint.h>
#include <limits.h>

/* App-local ceiling, independent of runtime/global headset resolution settings.
 * Both the game and the small session probe use this bounded target size. */
#define QUEST_MAX_EYE_DIMENSION 1760u

/* Uniformly downscale the runtime recommendation within all three bounds.
 * Round the other axis to its nearest pixel (minimum one pixel); this retains
 * the recommended aspect to pixel precision without ever enlarging either
 * dimension. uint64_t products keep extreme uint32_t runtime values safe.
 * A zero recommendation, runtime limit, or app limit is invalid. */
static inline int quest_select_eye_size(uint32_t recommended_width,
                                       uint32_t recommended_height,
                                       uint32_t runtime_max_width,
                                       uint32_t runtime_max_height,
                                       uint32_t app_max_edge,
                                       int* width, int* height) {
    if (width) *width = 0;
    if (height) *height = 0;
    if (!width || !height || !recommended_width || !recommended_height ||
        !runtime_max_width || !runtime_max_height || !app_max_edge) return 0;

    uint32_t bound_width = recommended_width, bound_height = recommended_height;
    if (bound_width > runtime_max_width) bound_width = runtime_max_width;
    if (bound_height > runtime_max_height) bound_height = runtime_max_height;
    if (bound_width > app_max_edge) bound_width = app_max_edge;
    if (bound_height > app_max_edge) bound_height = app_max_edge;
    /* Eye sizes also become GLsizei and XrExtent2Di members. */
    if (bound_width > INT_MAX) bound_width = INT_MAX;
    if (bound_height > INT_MAX) bound_height = INT_MAX;

    uint32_t selected_width, selected_height;
    if ((uint64_t)bound_width * recommended_height <=
        (uint64_t)bound_height * recommended_width) {
        selected_width = bound_width;
        selected_height = (uint32_t)(((uint64_t)recommended_height * bound_width +
                                      recommended_width / 2u) / recommended_width);
        if (!selected_height) selected_height = 1;
    } else {
        selected_height = bound_height;
        selected_width = (uint32_t)(((uint64_t)recommended_width * bound_height +
                                     recommended_height / 2u) / recommended_height);
        if (!selected_width) selected_width = 1;
    }
    *width = (int)selected_width;
    *height = (int)selected_height;
    return 1;
}
