/* The runner extracts the named real NES functions into the generated include. */
#include "gles_device_context.h"
#include "pc_gl.h"
#include <stdint.h>
int g_pc_window_w=256,g_pc_window_h=224,g_pc_target_w=256,g_pc_target_h=224,g_pc_profile_enabled;
void pc_gx_draw_pending(void){} void pc_gx_restore_after_nes(void){}
void pc_vr_nes_begin_draw(void){} void pc_vr_nes_end_draw(void){}
int pc_settings_get_nes_aspect(void){return 0;}
void pc_profiler_add_count_texture_bind_slow(void){}
static GLuint fixnes_shader,fixnes_vao,fixnes_vbo,fixnes_texture;
static GLint fixnes_tex_uniform=-1;
#include "gles_device_nes_functions.inc"
int main(void) {
    if (!device_context_begin(256,224)) return 2;
    static uint16_t fb[256*240];
    for(int y=0;y<240;++y) for(int x=0;x<256;++x)
        fb[y*256+x]=x<85?0x001f:x<170?0x07e0:0xf800;
    pc_fixnes_render_frame(fb);
    int failures=0;
    for(int c=0;c<3;++c) {
        unsigned char px[4]; glReadPixels(42+85*c,112,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
        printf("NES channel%d: %u,%u,%u,%u\n",c,px[0],px[1],px[2],px[3]);
        for(int k=0;k<3;++k) if((k==c&&px[k]<250)||(k!=c&&px[k]>5))++failures;
    }
    GLint linked=0; glGetProgramiv(fixnes_shader,GL_LINK_STATUS,&linked);
    if (!linked) ++failures;
    GLenum error=glGetError(); if(error) ++failures;
    printf("NES device: 11 checks, %d failures, GLerror=%x\n",failures,error);
    device_context_end(); return failures?1:0;
}
