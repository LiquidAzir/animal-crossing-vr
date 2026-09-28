/* Real world shaders and production hands share one native depth attachment.
 * Surfaces are placed in physical meters, rather than clearing to a depth value
 * copied from the hand shader's own convention. No ROM or headset is required. */
#include "gles_device_context.h"
#include "pc_gl.h"
#include "pc_vr_hands.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static const int W=640,H=480;
static int checks,failures;
#define CHECK(c,label) do { ++checks; if (!(c)) { ++failures; printf("FAIL: %s\n",label); } } while (0)
static GLuint world_program,world_vao,world_vbo;
static float projection[16];
static double world_near,world_far=1;

static GLuint compile(GLenum type,const char* path) {
    std::ifstream input(path,std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    CHECK(!text.empty(),"actual world shader source loaded");
    const char* source=text.c_str();
    GLuint shader=glCreateShader(type);
    pc_gl_shader_source(shader, source); glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]={}; glGetShaderInfoLog(shader,sizeof(log),NULL,log);
        fprintf(stderr,"World shader %s: %s\n",path,log);
    }
    CHECK(ok,"actual world shader compiled");
    return shader;
}

static void make_world_program(const char* vertex,const char* fragment) {
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment);
    world_program=glCreateProgram(); glAttachShader(world_program,vs); glAttachShader(world_program,fs);
    glLinkProgram(world_program); glDeleteShader(vs); glDeleteShader(fs);
    GLint ok=0; glGetProgramiv(world_program,GL_LINK_STATUS,&ok);
    CHECK(ok,"actual default world shaders linked together");
    glGenVertexArrays(1,&world_vao); glBindVertexArray(world_vao);
    glGenBuffers(1,&world_vbo); glBindBuffer(GL_ARRAY_BUFFER,world_vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),NULL);
    glVertexAttrib3f(1,0,0,1); glVertexAttrib4f(2,1,1,1,1); glVertexAttrib2f(3,0,0);
}

static void set_projection(float near_m,float far_m,float eye_shift) {
    std::memset(projection,0,sizeof(projection));
    projection[0]=1.7320508f/(float(W)/H); projection[5]=1.7320508f;
    projection[2]=eye_shift;
    // Same GX projection contract as pcvr_update_eye_projection.
    projection[10]=near_m/(near_m-far_m);
    projection[11]=near_m*far_m/(near_m-far_m);
    projection[14]=-1;
}

static void clear() {
    glViewport(0,0,W,H); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glDisable(GL_POLYGON_OFFSET_FILL);
     glDisable(GL_RASTERIZER_DISCARD);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glDepthRangef(world_near,world_far); glColorMask(1,1,1,1);
    glClearColor(0,0,0,1); glClearDepthf(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

static void world_quad(float distance_m,float units_per_meter,int half=0) {
    // Changing game units is canceled by the world modelview scale. Both the
    // world and the controller-grip pose therefore reach the projection in m.
    const float z=-distance_m*units_per_meter,extent=distance_m*units_per_meter*3;
    const float left=half==1?0:-extent,right=half==-1?0:extent;
    const float positions[]={left,-extent,z, right,-extent,z, right,extent,z,
                             left,-extent,z, right,extent,z, left,extent,z};
    const float scale=1/units_per_meter;
    const float modelview[16]={scale,0,0,0, 0,scale,0,0, 0,0,scale,0, 0,0,0,1};
    const float normal[9]={1,0,0,0,1,0,0,0,1};
    glUseProgram(world_program); glBindVertexArray(world_vao);
    glBindBuffer(GL_ARRAY_BUFFER,world_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(positions),positions,GL_STREAM_DRAW);
    glUniformMatrix4fv(glGetUniformLocation(world_program,"u_projection"),1,GL_TRUE,projection);
    glUniformMatrix4fv(glGetUniformLocation(world_program,"u_modelview"),1,GL_TRUE,modelview);
    glUniformMatrix3fv(glGetUniformLocation(world_program,"u_normal_mtx"),1,GL_TRUE,normal);
    // A constant opaque TEV PREV result exercises the real world fragment
    // pipeline without introducing unrelated texture/lighting dependencies.
    glUniform1i(glGetUniformLocation(world_program,"u_num_tev_stages"),0);
    glUniform4f(glGetUniformLocation(world_program,"u_tev_prev"),0.03f,0.15f,0.65f,1);
    glUniform3i(glGetUniformLocation(world_program,"u_alpha_ctrl"),7,0,7);
    glUniform1i(glGetUniformLocation(world_program,"u_fog_enable"),0);
    glDrawArrays(GL_TRIANGLES,0,6);
}

static void hands() {
    for (int hand=0;hand<2;++hand) {
        const float pose[12]={1,0,0,hand==0?-0.105f:0.105f, 0,1,0,0, 0,0,1,-0.4f};
        CHECK(pc_vr_hands_draw(pose,projection,hand),"production hand draw submitted");
        pc_gl_depth_value restored[2]; pc_gl_get_depth_range(restored);
        CHECK(std::fabs(restored[0]-world_near)<0.0000001 && std::fabs(restored[1]-world_far)<0.0000001,
              "hand renderer restores the supplied world depth range");
    }
}

static std::vector<unsigned char> pixels() {
    std::vector<unsigned char> result(W*H*4);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,result.data()); return result;
}

static bool is_hand(const std::vector<unsigned char>& image,int i) {
    return image[i*4]>140 && image[i*4+1]>140 && image[i*4+2]>130;
}

static int coverage(const std::vector<unsigned char>& image,int half=0) {
    int result=0;
    const int begin=half==1?W/2:0,end=half==-1?W/2:W;
    for (int y=0;y<H;++y) for (int x=begin;x<end;++x) result+=is_hand(image,y*W+x);
    return result;
}

static void save(const char* path,const std::vector<unsigned char>& image) {
    CHECK(device_save_bmp(path,W,H,image.data()),"native world/hand preview saved");
}

static void run_case(float near_m,float far_m,float eye_shift,float units,double depth_near,double depth_far,bool preview) {
    world_near=depth_near; world_far=depth_far;
    printf("Case near=%.3fm far=%.0fm shift=%.2f units/m=%.0f depth range %.6f..%.6f\n",
           near_m,far_m,eye_shift,units,world_near,world_far);
    set_projection(near_m,far_m,eye_shift);
    clear(); hands(); const auto empty=pixels(); const int expected=coverage(empty);
    CHECK(expected>2000,"both hands visible against empty background");
    clear(); world_quad(1,units);
    hands(); const auto foreground=pixels();
    CHECK(coverage(foreground)==expected,"hands remain fully visible in front of scenery");
    if (preview) save("hands-in-front-of-world.bmp",foreground);

    clear(); world_quad(0.2f,units); hands(); const auto hidden=pixels();
    CHECK(coverage(hidden)==0,"nearer world surface correctly hides both hands");

    clear(); hands(); world_quad(1,units);
    CHECK(coverage(pixels())==expected,"later distant world draw cannot overwrite foreground hands");
    world_quad(0.2f,units);
    CHECK(coverage(pixels())==0,"later nearer world draw correctly overwrites hands");

    if (eye_shift==0) {
        clear(); world_quad(1,units); world_quad(0.2f,units,-1); hands();
        const auto partial=pixels();
        CHECK(coverage(partial,-1)==0,"left hand occluded by physical near wall");
        CHECK(coverage(partial,1)==coverage(empty,1),"right hand remains visible against farther scenery");
        if (preview) save("hands-partial-world-occlusion.bmp",partial);
    }
    CHECK(glGetError()==GL_NO_ERROR,"world/hand integration produced no GL errors");
}

int main(int argc,char** argv) {
    if (argc!=3) { fprintf(stderr,"Expected default.vert and default.frag paths\n"); return 2; }
    if (!device_context_begin(W,H)) return 2;
    GLuint framebuffer,texture,depth;
    glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    glBindTexture(GL_TEXTURE_2D,0);
    glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"shared eye framebuffer complete");
    make_world_program(argv[1],argv[2]);
    // m_view.c uses z scale/translation511; emu64 maps that to0..1022/1023.
    run_case(0.05f,10,0,1,0,1022.0/1023.0,true);
    run_case(0.05f,100,0,45,0,1,false);
    run_case(0.03f,1000,-0.14f,100,0.2,0.8,false);
    run_case(0.1f,1000,0.14f,25,0.1,0.7,false);
    printf("World/hand depth: %d checks, %d failures\n",checks,failures);
    pc_vr_hands_shutdown(); glDeleteProgram(world_program);
    glDeleteBuffers(1,&world_vbo); glDeleteVertexArrays(1,&world_vao);
    glDeleteRenderbuffers(1,&depth); glDeleteTextures(1,&texture); glDeleteFramebuffers(1,&framebuffer);
    device_context_end();
    return failures?1:0;
}
