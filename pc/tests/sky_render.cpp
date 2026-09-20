/* Native OpenGL checks of the production sky renderer, no ROM or headset.
 * Compiles shaders on the actual driver and captures the actual draw output. */
#include <SDL.h>
#include <glad/gl.h>
#include "pc_sky.h"
#include "pc_settings.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <stdint.h>

extern "C" {
uint32_t pc_frame_counter = 100;
int g_pc_paused = 0;
PCSettings g_pc_settings = {};
}
static int checks, failures;
#define CHECK(c,label) do { ++checks; if (!(c)) { ++failures; printf("FAIL: %s\n",label); } } while(0)
const int W=960,H=540;
static float projection[16] = {0.974279f,0,0,0, 0,1.73205f,0,0, 0,0,-1,-1, 0,0,-1,0};
static float view[12] = {1,0,0,0, 0,0.968912f,0.247404f,0, 0,-0.247404f,0.968912f,0};
static void clear(void) {
    glDisable(GL_SCISSOR_TEST); glColorMask(1,1,1,1); glDepthMask(1);
    glClearColor(0.9f,0.0f,0.6f,1); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glViewport(0,0,W,H);
    pc_sky_begin_pass(); pc_sky_set_view(view);
}
static std::vector<unsigned char> pixels(void) {
    std::vector<unsigned char> out(W*H*4);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,out.data());
    return out;
}
static void save(const char* name, const std::vector<unsigned char>& p) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_RGBA32);
    for (int y=0;y<H;y++) memcpy((char*)surface->pixels+y*surface->pitch,p.data()+(H-1-y)*W*4,W*4);
    CHECK(SDL_SaveBMP(surface,name)==0,"preview written");
    SDL_FreeSurface(surface);
}
static double brightness(const std::vector<unsigned char>& p) {
    double sum=0; for (int i=0;i<W*H;i++) sum+=p[i*4]+p[i*4+1]+p[i*4+2];
    return sum/(W*H*3);
}
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO)!=0) return 2;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_Window* window=SDL_CreateWindow("Sky renderer checks",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    SDL_GLContext context=window ? SDL_GL_CreateContext(window) : NULL;
    if (!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        fprintf(stderr,"GL context unavailable: %s\n",SDL_GetError()); return 2;
    }
    printf("Renderer: %s\n",glGetString(GL_RENDERER));
    g_pc_settings.skybox=1;
    clear(); CHECK(!pc_sky_draw(projection,NULL),"no environment means no sky in boot/menu scenes");
    pc_sky_set_environment(0,43200,0);
    CHECK(!pc_sky_draw(projection,NULL),"indoors/submenu excluded");
    pc_sky_set_environment(1,43200,0);
    pc_sky_begin_pass(); CHECK(!pc_sky_draw(projection,NULL),"fresh world view required each pass");
    pc_sky_set_view(view); g_pc_settings.skybox=0;
    CHECK(!pc_sky_draw(projection,NULL),"stock-background setting disables sky");
    g_pc_settings.skybox=1; pc_frame_counter+=2;
    CHECK(!pc_sky_draw(projection,NULL),"stale outdoor state cannot leak into another scene");
    g_pc_paused=1;
    CHECK(!pc_sky_draw(projection,NULL),"pausing cannot resurrect an expired outdoor sky");
    g_pc_paused=0; pc_sky_set_environment(1,43200,0); clear();
    CHECK(pc_sky_draw(projection,NULL),"outdoor scene active before pause");
    g_pc_paused=1; pc_frame_counter+=100; clear();
    CHECK(pc_sky_draw(projection,NULL),"paused outdoor scene retains sky");
    g_pc_paused=0;
    // Leaving gameplay must expire even if the intervening scene is all 2D.
    pc_frame_counter+=2; pc_sky_begin_pass();
    g_pc_paused=1; clear();
    CHECK(!pc_sky_draw(projection,NULL),"an all-2D scene expires the sky before a later pause");
    g_pc_paused=0;
    pc_sky_set_environment(1,43200,0); clear();
    CHECK(pc_sky_draw(projection,NULL),"day sky compiles and draws");
    CHECK(!pc_sky_draw(projection,NULL),"at most one draw per pass");
    auto day=pixels(); save("sky-day.bmp",day);
    CHECK(day[(H-1)*W*4+W*2+2]>day[(H-1)*W*4+W*2],"upper daytime sky is blue");
    clear(); pc_sky_set_environment(1,0,0); pc_sky_draw(projection,NULL);
    auto night=pixels(); save("sky-night.bmp",night);
    CHECK(brightness(night)<brightness(day)*0.4,"night stays dark");
    clear(); pc_sky_set_environment(1,86400,0); pc_sky_draw(projection,NULL);
    auto midnight=pixels();
    int midnight_diff=0;
    for(size_t i=0;i<night.size();i++) {
        int d=abs(int(night[i])-int(midnight[i])); if(d>midnight_diff)midnight_diff=d;
    }
    CHECK(midnight_diff<=1,"clouds join continuously across midnight");
    clear(); pc_sky_set_environment(1,66600,0); pc_sky_draw(projection,NULL);
    save("sky-sunset.bmp",pixels());
    clear(); pc_sky_set_environment(1,43200,1); pc_sky_draw(projection,NULL);
    auto rain=pixels(); save("sky-rain.bmp",rain);
    int top=(H-20)*W*4+W*2;
    CHECK(abs(rain[top+2]-rain[top])<abs(day[top+2]-day[top]),"overcast desaturates sky");
    // Translation and VR's world scale must never introduce sky parallax.
    float correction[12]={0.025f,0,0,800, 0,0.025f,0,-300, 0,0,0.025f,100};
    clear(); pc_sky_set_environment(1,43200,0); pc_sky_draw(projection,correction);
    auto translated=pixels();
    int maxdiff=0;
    for (size_t i=0;i<day.size();i++) { int d=abs(int(day[i])-int(translated[i])); if(d>maxdiff)maxdiff=d; }
    CHECK(maxdiff<=1,"VR scale/eye translation leaves infinite sky unchanged");
    // Asymmetric eye frusta see matching world directions in their overlap.
    projection[2]=0.1f; clear(); pc_sky_draw(projection,NULL); auto left=pixels();
    projection[2]=-0.1f; clear(); pc_sky_draw(projection,NULL); auto right=pixels();
    maxdiff=0;
    for (int y=0;y<H;y++) for (int x=0;x<W-96;x++) for(int c=0;c<3;c++) {
        int d=abs(int(left[(y*W+x)*4+c])-int(right[(y*W+x+96)*4+c]));
        if(d>maxdiff)maxdiff=d;
    }
    CHECK(maxdiff<=2,"asymmetric stereo rays match at infinity"); projection[2]=0;
    // Existing nearer geometry must survive, with no sky writes to depth.
    clear(); glEnable(GL_SCISSOR_TEST); glScissor(0,0,W/2,H);
    glClearColor(1,0,0,1); glClearDepth(0.25); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glScissor(5,6,7,8); glEnable(GL_BLEND); glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_GREATER); glDepthMask(GL_TRUE);
    glDepthRange(0.2,0.8);
    glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
    CHECK(pc_sky_draw(projection,NULL),"sky draws with nondefault game GL state");
    GLboolean mask[4], depth_mask; GLint func, scissor[4];
    glGetBooleanv(GL_COLOR_WRITEMASK,mask); glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);
    glGetIntegerv(GL_DEPTH_FUNC,&func); glGetIntegerv(GL_SCISSOR_BOX,scissor);
    CHECK(mask[0]&&!mask[1]&&mask[2]&&!mask[3]&&depth_mask&&func==GL_GREATER,"write masks and depth comparison restored");
    CHECK(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST)&&scissor[0]==5,"blend cull and scissor restored");
    GLdouble depth_range[2]; glGetDoublev(GL_DEPTH_RANGE,depth_range);
    CHECK(fabs(depth_range[0]-0.2)<0.0001&&fabs(depth_range[1]-0.8)<0.0001,"nondefault depth range restored");
    auto foreground=pixels(); CHECK(foreground[0]==255&&foreground[1]==0&&foreground[2]==0,"foreground is never painted over");
    float near_depth,far_depth;
    glReadPixels(1,1,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&near_depth);
    glReadPixels(W-2,1,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&far_depth);
    CHECK(fabsf(near_depth-0.25f)<0.0001f&&far_depth==1.0f,"sky leaves the depth buffer intact");
    // Measure the real shader at a representative eye-buffer size. This is
    // sky-only GPU time, not a claim about whole-game/headset performance.
    GLuint fbo,texture,depth_buffer,timer;
    glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2048,2048,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    glGenRenderbuffers(1,&depth_buffer); glBindRenderbuffer(GL_RENDERBUFFER,depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,2048,2048);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_buffer);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"eye-sized test framebuffer complete");
    clear(); glViewport(0,0,2048,2048); glFinish();
    glGenQueries(1,&timer); glBeginQuery(GL_TIME_ELAPSED,timer);
    for(int i=0;i<32;i++) { pc_sky_begin_pass(); pc_sky_set_view(view); pc_sky_draw(projection,NULL); }
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 elapsed=0; glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&elapsed);
    printf("Sky-only GPU: %.3f ms per 2048x2048 pass (32-pass average)\n",elapsed/32000000.0);
    glDeleteQueries(1,&timer); glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteRenderbuffers(1,&depth_buffer); glDeleteTextures(1,&texture); glDeleteFramebuffers(1,&fbo);
    CHECK(glGetError()==GL_NO_ERROR,"no OpenGL errors");
    pc_sky_shutdown(); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    printf("Sky renderer: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
