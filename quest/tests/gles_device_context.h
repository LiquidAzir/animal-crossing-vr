/* Standalone Android GPU diagnostics: no Activity, SDL video or app storage. */
#ifndef QUEST_GLES_DEVICE_CONTEXT_H
#define QUEST_GLES_DEVICE_CONTEXT_H
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static EGLDisplay test_display = EGL_NO_DISPLAY;
static EGLSurface test_surface = EGL_NO_SURFACE;
static EGLContext test_context = EGL_NO_CONTEXT;

static int device_context_begin(int width, int height) {
    EGLint major, minor, count;
    EGLConfig config;
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE
    };
    const EGLint pbuffer_attributes[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    test_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (test_display == EGL_NO_DISPLAY || !eglInitialize(test_display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(test_display, config_attributes, &config, 1, &count) || !count) {
        fprintf(stderr, "EGL setup failed: 0x%x\n", eglGetError()); return 0;
    }
    test_surface = eglCreatePbufferSurface(test_display, config, pbuffer_attributes);
    test_context = eglCreateContext(test_display, config, EGL_NO_CONTEXT, context_attributes);
    if (test_surface == EGL_NO_SURFACE || test_context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(test_display, test_surface, test_surface, test_context)) {
        fprintf(stderr, "EGL pbuffer/context failed: 0x%x\n", eglGetError()); return 0;
    }
    printf("EGL %d.%d vendor=%s version=%s\n", major, minor,
           eglQueryString(test_display, EGL_VENDOR), eglQueryString(test_display, EGL_VERSION));
    printf("GL vendor=%s renderer=%s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER));
    printf("GL version=%s GLSL=%s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("ABI pointer-bits=%u pbuffer=%dx%d\n", (unsigned)(8*sizeof(void*)), width, height);
    return 1;
}

static void device_context_end(void) {
    if (test_display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(test_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (test_context != EGL_NO_CONTEXT) eglDestroyContext(test_display, test_context);
    if (test_surface != EGL_NO_SURFACE) eglDestroySurface(test_display, test_surface);
    eglTerminate(test_display);
    test_display = EGL_NO_DISPLAY;
}

/* RGBA glReadPixels data is bottom-up already. Write ordinary 32-bit BMP. */
static int device_save_bmp(const char* path, int width, int height, const unsigned char* rgba) {
    unsigned char header[54] = {'B','M'};
    uint32_t words[] = {(uint32_t)(54+width*height*4), 54, 40, (uint32_t)width, (uint32_t)height};
    const int offsets[] = {2,10,14,18,22};
    for (int k=0;k<5;++k) for (int b=0;b<4;++b) header[offsets[k]+b]=(unsigned char)(words[k]>>(b*8));
    header[26]=1; header[28]=32;
    FILE* file=fopen(path,"wb"); if (!file) return 0;
    int ok=fwrite(header,1,sizeof(header),file)==sizeof(header);
    for (int i=0;ok && i<width*height;++i) {
        unsigned char bgra[4]={rgba[i*4+2],rgba[i*4+1],rgba[i*4],rgba[i*4+3]};
        ok=fwrite(bgra,1,4,file)==4;
    }
    if (fclose(file)) ok=0;
    return ok;
}
#endif
