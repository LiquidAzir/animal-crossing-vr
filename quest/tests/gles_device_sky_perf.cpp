/* Bounded offscreen production-sky timing. Not an end-to-end game benchmark. */
#include "gles_device_context.h"
#include "pc_sky.h"
#include "pc_settings.h"
#include <algorithm>
#include <chrono>
#include <stdint.h>
#include <vector>

extern "C" {
uint32_t pc_frame_counter=100;
int g_pc_paused=0;
PCSettings g_pc_settings={};
}
static const int W=1680,H=1760,N=30;
static float projection[16]={1.814529f,0,0,0, 0,1.73205f,0,0, 0,0,-1,-1, 0,0,-1,0};
static float view[12]={1,0,0,0, 0,0.968912f,0.247404f,0, 0,-0.247404f,0.968912f,0};
using Clock=std::chrono::steady_clock;
static double elapsed(Clock::time_point start) {
    return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
static void clear_target(void) {
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDepthMask(1);
    glViewport(0,0,W,H);glClearColor(0,0,0,1);glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
static void report(const char* label,std::vector<double> samples) {
    double sum=0;for(double value:samples)sum+=value;
    std::sort(samples.begin(),samples.end());
    printf("SKY_PERF %s n=%u mean_ms=%.3f min_ms=%.3f median_ms=%.3f p95_ms=%.3f max_ms=%.3f\n",
           label,(unsigned)samples.size(),sum/samples.size(),samples.front(),
           samples[samples.size()/2],samples[(samples.size()*95-1)/100],samples.back());
}
int main(void) {
    if(!device_context_begin(W,H))return 2;
    g_pc_settings.skybox=1;
    int failures=0;
    const float hours[]={43200,0,43200};
    const float rain[]={0,0,1};
    const char* labels[]={"day","night","rain"};
    std::vector<double> clear_samples;
    printf("Full target sky coverage; no world occlusion; one eye per sample; 3 warmup draws excluded.\n");
    printf("Device sleep/power/thermal frequencies uncontrolled; glFinish timings are not game frame times.\n");
    for(int mode=0;mode<3;++mode) {
        pc_sky_set_environment(1,hours[mode],rain[mode]);
        std::vector<double> times;
        for(int frame=-3;frame<N;++frame) {
            auto clear_start=Clock::now();clear_target();glFinish();
            if(frame>=0)clear_samples.push_back(elapsed(clear_start));
            pc_sky_begin_pass();pc_sky_set_view(view);
            auto start=Clock::now();
            if(!pc_sky_draw(projection,NULL))++failures;
            glFinish();
            if(frame>=0)times.push_back(elapsed(start));
        }
        report(labels[mode],times);
        GLenum error=glGetError();if(error)++failures;
        printf("SKY_PERF %s GLerror=%x\n",labels[mode],error);
    }
    report("clear_baseline",clear_samples);
    printf("SKY_PERF failures=%d\n",failures);
    pc_sky_shutdown();device_context_end();return failures?1:0;
}
