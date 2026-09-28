/* Compile and inspect every production preseeded GX program on device. */
#include "gles_device_context.h"
#include "pc_gx_internal.h"
PCGXState g_gx;
int g_pc_verbose = 1;
Uint32 SDL_GetTicks(void) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (Uint32)(now.tv_sec*1000u+now.tv_nsec/1000000u);
}
void pc_gx_cache_uniform_locations(GLuint shader, PCGXUloc* out) {
    (void)shader; memset(out,-1,sizeof(*out));
}
/* Include the real implementation so failure cannot hide behind its fallback. */
#include "../../pc/src/pc_gx_tev.c"
int main(void) {
    if (!device_context_begin(128,128)) return 2;
    pc_gx_tev_init();
    int failures=0, checks=0, linked=0;
    ++checks; if (s_variant_count!=PC_SHADER_SEED_COUNT) ++failures;
    ++checks; if (!default_program) ++failures;
    for (int i=0;i<s_variant_count;++i) {
        GLint ok=0; glGetProgramiv(s_variants[i].prog,GL_LINK_STATUS,&ok);
        ++checks; if (!ok || s_variants[i].prog==default_program) ++failures; else ++linked;
    }
    GLenum error=glGetError(); ++checks; if (error) ++failures;
    printf("TEV device: %d checks, %d failures, %d/%d specialized programs linked, GLerror=%x\n",
           checks,failures,linked,PC_SHADER_SEED_COUNT,error);
    pc_gx_tev_shutdown(); device_context_end();
    return failures?1:0;
}
