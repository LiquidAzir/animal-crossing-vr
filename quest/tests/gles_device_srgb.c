/* Check the actual driver's encoded-color write contract for OpenXR targets. */
#include "gles_device_context.h"
#include <GLES2/gl2ext.h>
#include <string.h>

static GLuint shader(GLenum kind,const char* source) {
    GLuint result=glCreateShader(kind); GLint ok=0;
    glShaderSource(result,1,&source,NULL); glCompileShader(result);
    glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(result,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);return 0;}
    return result;
}
static int draw_sample(GLenum format,int conversion,unsigned char out[4]) {
    GLuint texture,fbo;
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,format,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 0;
    if(conversion)glEnable(GL_FRAMEBUFFER_SRGB_EXT);else glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,out);
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);
    return glGetError()==GL_NO_ERROR;
}
int main(void) {
    if(!device_context_begin(16,16))return 2;
    const char* extensions=(const char*)glGetString(GL_EXTENSIONS);
    int supported=extensions&&strstr(extensions,"GL_EXT_sRGB_write_control")!=NULL;
    printf("GL_EXT_sRGB_write_control=%d\n",supported);
    if(!supported){device_context_end();return 3;}
    const char* vs="#version 300 es\nprecision highp float;\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;gl_Position=vec4(p,0,1);}";
    const char* fs="#version 300 es\nprecision highp float;\nout vec4 color;\nvoid main(){color=vec4(0.25,0.5,0.75,1.0);}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),program=glCreateProgram(),vao;
    glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked)return 2;
    glUseProgram(program);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glViewport(0,0,16,16);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);
    unsigned char linear[4],encoded[4],raw[4]; int checks=0,failures=0;
#define CHECK(c) do{++checks;if(!(c))++failures;}while(0)
    CHECK(draw_sample(GL_RGBA8,0,linear));
    CHECK(draw_sample(GL_SRGB8_ALPHA8,1,encoded));
    CHECK(draw_sample(GL_SRGB8_ALPHA8,0,raw));
    printf("RGBA8 raw: %u,%u,%u,%u\n",linear[0],linear[1],linear[2],linear[3]);
    printf("SRGB8 encode enabled: %u,%u,%u,%u\n",encoded[0],encoded[1],encoded[2],encoded[3]);
    printf("SRGB8 encode disabled: %u,%u,%u,%u\n",raw[0],raw[1],raw[2],raw[3]);
    for(int i=0;i<4;++i)CHECK(raw[i]==linear[i]);
    for(int i=0;i<3;++i)CHECK(encoded[i]>raw[i]+20);
    CHECK(glGetError()==GL_NO_ERROR);
    printf("sRGB device: %d checks, %d failures\n",checks,failures);
    glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);glDeleteVertexArrays(1,&vao);
    device_context_end();return failures?1:0;
}
