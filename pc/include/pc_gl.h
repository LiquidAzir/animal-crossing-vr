/* Small GL profile boundary. Game/GX depth and matrix conventions stay shared;
 * Android uses native GLES 3, while the existing desktop target keeps GLAD. */
#ifndef PC_GL_H
#define PC_GL_H

#include <string.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#define PC_GLSL_HEADER "#version 300 es\nprecision highp float;\nprecision highp int;\n"
typedef GLfloat pc_gl_depth_value;
#define PC_GL_UNIFORM(name) (name)
#else
#include <glad/gl.h>
#define PC_GLSL_HEADER "#version 330 core\n"
typedef GLdouble pc_gl_depth_value;
#define PC_GL_UNIFORM(name) glad_##name
#endif

static inline void pc_gl_depth_range(double near_depth, double far_depth) {
#ifdef __ANDROID__
    glDepthRangef((GLfloat)near_depth, (GLfloat)far_depth);
#else
    glDepthRange(near_depth, far_depth);
#endif
}

static inline void pc_gl_clear_depth(double depth) {
#ifdef __ANDROID__
    glClearDepthf((GLfloat)depth);
#else
    glClearDepth(depth);
#endif
}

static inline void pc_gl_get_depth_range(pc_gl_depth_value out[2]) {
#ifdef __ANDROID__
    glGetFloatv(GL_DEPTH_RANGE, out);
#else
    glGetDoublev(GL_DEPTH_RANGE, out);
#endif
}

/* External world shader files remain shared with desktop. Prefix the GLES
 * version/precision before any declarations, including generated TEV consts.
 * Embedded shaders can use PC_GLSL_HEADER directly. No per-compile allocation. */
static inline void pc_gl_shader_source(GLuint shader, const char* source) {
#ifdef __ANDROID__
    const char* body = source;
    if (strncmp(body, "#version", 8) == 0) {
        const char* newline = strchr(body, '\n');
        if (newline) body = newline + 1;
    }
    const char* parts[2] = { PC_GLSL_HEADER, body };
    glShaderSource(shader, 2, parts, NULL);
#else
    glShaderSource(shader, 1, &source, NULL);
#endif
}

#endif
