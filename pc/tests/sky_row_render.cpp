/* Compare the production sky against the same source with its conservative
 * row rejection removed. Uses a native GL context, without ROM or headset. */
#include <SDL.h>
#include <glad/gl.h>
#include "pc_sky.h"
#include "pc_settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
uint32_t pc_frame_counter=100;
int g_pc_paused=0;
PCSettings g_pc_settings={};
void baseline_pc_sky_set_environment(int,float,float);
void baseline_pc_sky_begin_pass(void);
void baseline_pc_sky_set_view(const float*);
int baseline_pc_sky_draw(const float*,const float*);
void baseline_pc_sky_shutdown(void);
}

static float projection[16]={1,0,0,0, 0,1,0,0, 0,0,-1,-1, 0,0,-1,0};
static float view[12];
static GLuint fbo,color,depth,query;
static int errors;

static void setup(int dimension) {
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,dimension,dimension,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,dimension,dimension);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) ++errors;
    glViewport(0,0,dimension,dimension);
    glColorMask(1,1,1,1); glDepthMask(1); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

static void set_view(float pitch,float yaw) {
    const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
    const float values[12]={cy,0,-sy,0, sp*sy,cp,sp*cy,0, cp*sy,-sp,cp*cy,0};
    std::copy(values,values+12,view);
}

static void draw(int production) {
    if (production) {
        pc_sky_begin_pass(); pc_sky_set_view(view);
        if (!pc_sky_draw(projection,NULL)) ++errors;
    } else {
        baseline_pc_sky_begin_pass(); baseline_pc_sky_set_view(view);
        if (!baseline_pc_sky_draw(projection,NULL)) ++errors;
    }
}

static void environment(float time,float wet) {
    pc_sky_set_environment(1,time,wet);
    baseline_pc_sky_set_environment(1,time,wet);
}

static void benchmark(int dimension) {
    printf("Benchmark: %d square; seven alternating batches of 256 calls; warmed programs.\n",dimension);
    setup(dimension); environment(43200,0);
    const float pitches[]={0,0.25f,0.65f};
    for (float pitch:pitches) {
        set_view(pitch,0);
        for (int i=0;i<64;++i) { draw(0); draw(1); }
        double milliseconds[2][7];
        for (int trial=0;trial<7;++trial) for (int j=0;j<2;++j) {
            const int variant=(trial+j)%2;
            glFinish(); glBeginQuery(GL_TIME_ELAPSED,query);
            for (int i=0;i<256;++i) draw(variant);
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 elapsed=0;
            glGetQueryObjectui64v(query,GL_QUERY_RESULT,&elapsed);
            milliseconds[variant][trial]=double(elapsed)/1e6/256;
        }
        for (int variant=0;variant<2;++variant)
            std::sort(milliseconds[variant],milliseconds[variant]+7);
        printf("pitch %.2f: baseline median %.6f ms [%.6f, %.6f]; production median %.6f ms [%.6f, %.6f]\n",
               pitch,milliseconds[0][3],milliseconds[0][0],milliseconds[0][6],
               milliseconds[1][3],milliseconds[1][0],milliseconds[1][6]);
    }
}

int main(int argc,char** argv) {
    const bool measure=argc>1 && std::strcmp(argv[1],"--benchmark")==0;
    if (SDL_Init(SDL_INIT_VIDEO)!=0) return 2;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window=SDL_CreateWindow("Sky row regression",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    SDL_GLContext context=window ? SDL_GL_CreateContext(window) : NULL;
    if (!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        fprintf(stderr,"GL context unavailable: %s\n",SDL_GetError()); return 2;
    }
    printf("Renderer: %s; %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    glGenFramebuffers(1,&fbo); glGenTextures(1,&color);
    glGenRenderbuffers(1,&depth); glGenQueries(1,&query);
    g_pc_settings.skybox=1;
    const int dimension=384;
    setup(dimension);
    std::vector<unsigned char> before(dimension*dimension*4),after(before.size());
    const float times[]={0,19800,23400,43200,66600,72000,86399};
    const float wetness[]={0,0.5f,1};
    const float pitches[]={-1.2f,-0.65f,-0.15f,0,0.25f,0.65f,1.2f};
    int comparisons=0,differing_frames=0,max_difference=0;
    size_t differing_values=0;
    for (float time:times) for (float wet:wetness) for (float pitch:pitches) for (int yaw=0;yaw<8;++yaw) {
        environment(time,wet); set_view(pitch,yaw*0.785398163f);
        draw(0); glReadPixels(0,0,dimension,dimension,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
        draw(1); glReadPixels(0,0,dimension,dimension,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
        bool differs=false;
        for (size_t i=0;i<before.size();++i) if (before[i]!=after[i]) {
            differs=true; ++differing_values;
            max_difference=std::max(max_difference,std::abs(int(before[i])-int(after[i])));
        }
        if (differs) ++differing_frames;
        ++comparisons;
        if (glGetError()!=GL_NO_ERROR) ++errors;
    }
    printf("Compared %d views: %d differing frames, %llu differing RGBA values, maximum byte difference %d\n",
           comparisons,differing_frames,(unsigned long long)differing_values,max_difference);
    if (measure) { benchmark(2048); benchmark(3072); }
    if (glGetError()!=GL_NO_ERROR) ++errors;
    printf("Errors: %d\n",errors);
    pc_sky_shutdown(); baseline_pc_sky_shutdown();
    glDeleteQueries(1,&query); glDeleteFramebuffers(1,&fbo);
    glDeleteTextures(1,&color); glDeleteRenderbuffers(1,&depth);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    return errors || differing_frames ? 1 : 0;
}
