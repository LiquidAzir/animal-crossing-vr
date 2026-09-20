/* A direction-only sky: no eye translation, finite dome, or stereo disparity.
 * Procedural rounded clouds keep the low-detail, painted look of the village.
 * Embedded shaders keep the executable self-contained and rollback simple. */
#include "pc_sky.h"
#include "pc_settings.h"
#include <glad/gl.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdint.h>

extern "C" { extern uint32_t pc_frame_counter; extern int g_pc_paused; }

static GLuint program, vao;
static int failed, outdoors, have_environment, have_view, drawn, environment_active;
static uint32_t environment_frame;
static float seconds, overcast, view[12];
static GLint u_projection, u_rotation, u_environment;

static const char* vertex_source = R"GLSL(#version 330 core
out vec2 screen;
void main() {
    screen = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    gl_Position = vec4(screen, 1.0, 1.0);
}
)GLSL";

static const char* fragment_source = R"GLSL(#version 330 core
in vec2 screen;
out vec4 color;
uniform vec4 projection; // P00, P11, P02, P12 (asymmetric per-eye frusta)
uniform mat3 eye_to_world;
uniform vec2 environment; // game seconds, overcast blend
const float PI = 3.14159265359;
float hash(float n) { return fract(sin(n * 127.1 + 311.7) * 43758.5453); }
float oval(vec2 p, vec2 center, vec2 size) {
    return length((p-center)/size);
}
// Smooth-edged clusters, with a broad base and irregular rounded lobes.
float cloud(vec2 p, float seed) {
    p.x += 0.018*sin(p.y*6.0+seed*9.0);
    p.y += 0.008*sin(p.x*9.0+seed*15.0);
    float d = oval(p, vec2(0.0,-0.10), vec2(1.0,0.30));
    d = min(d, oval(p, vec2(-0.49,0.04), vec2(0.43,0.39)));
    d = min(d, oval(p, vec2(-0.10,0.23), vec2(0.48,0.57)));
    d = min(d, oval(p, vec2(0.36,0.13+0.10*seed), vec2(0.46,0.43)));
    d = min(d, oval(p, vec2(0.73,-0.04), vec2(0.35,0.26)));
    return 1.0-smoothstep(0.86,1.08,d);
}
void main() {
    vec3 eye = vec3((screen + projection.zw)/projection.xy, -1.0);
    vec3 ray = normalize(eye_to_world * eye);
    float h = ray.y;
    float azimuth = atan(ray.x,ray.z)/(2.0*PI)+0.5;
    float hour = environment.x/3600.0;
    float wet = clamp(environment.y,0.0,1.0);
    float day = smoothstep(5.0,7.5,hour)*(1.0-smoothstep(17.0,20.0,hour));
    float dusk = max(1.0-abs(hour-6.0)/1.5,1.0-abs(hour-18.5)/1.5);
    dusk = max(dusk,0.0)*(1.0-wet);
    vec3 zenith = mix(vec3(0.018,0.032,0.095),vec3(0.065,0.29,0.84),day);
    vec3 horizon = mix(vec3(0.065,0.10,0.19),vec3(0.65,0.85,0.96),day);
    horizon = mix(horizon,vec3(0.96,0.56,0.37),dusk*0.72);
    zenith = mix(zenith,mix(vec3(0.045,0.055,0.075),vec3(0.36,0.43,0.51),day),wet);
    horizon = mix(horizon,mix(vec3(0.10,0.12,0.15),vec3(0.65,0.70,0.74),day),wet);
    vec3 sky = mix(horizon,zenith,pow(clamp(h,0.0,1.0),0.48));
    // Fade below the horizon to a quiet blue haze behind the actual terrain.
    sky = mix(sky,horizon*mix(0.55,0.77,day),smoothstep(0.0,0.20,-h));

    // Sparse distant island silhouettes, entirely behind the playable world.
    float ridge = 0.006 + 0.015*pow(max(0.0,sin(azimuth*2.0*PI*7.0+1.2)),4.0)
                        + 0.009*pow(max(0.0,sin(azimuth*2.0*PI*19.0)),6.0);
    float islands = smoothstep(0.52,0.75,sin(azimuth*2.0*PI*5.0));
    float land = (1.0-smoothstep(ridge-0.002,ridge+0.002,h))*smoothstep(-0.012,0.0,h)*islands;
    sky = mix(sky,horizon*0.76,land*(1.0-wet*0.5));

    // Periodic longitude, bounded drift (same time for both eyes), no cube seams.
    if (h > 0.015 && h < 0.87) {
        float latitude = asin(h);
        for (int row=0; row<3; ++row) {
            float r = float(row);
            float count = 18.0-r*5.0;
            // One full revolution per game day joins continuously at midnight.
            float x = (azimuth+environment.x/86400.0)*count + r*2.71;
            float cell = floor(x);
            for (int neighbor=-1; neighbor<=1; ++neighbor) {
                float id = mod(cell+float(neighbor),count);
                float seed = hash(id+r*39.0);
                float center = 0.5+(seed-0.5)*0.32;
                float elevation = 0.055+r*0.22+hash(id+93.0+r*17.0)*(0.045+r*0.11);
                vec2 p = vec2((x-cell-float(neighbor)-center)/(0.22+seed*0.14),
                              (latitude-elevation)/(0.025+r*0.065));
                if (abs(p.x)<1.4 && abs(p.y)<1.2 && (row==0 || seed>0.15)) {
                    float cover = cloud(p,seed)*mix(0.78,0.96,r/2.0);
                    vec3 shade = mix(vec3(0.12,0.16,0.25),vec3(0.64,0.76,0.93),day);
                    vec3 lit = mix(vec3(0.23,0.28,0.38),vec3(0.98,0.98,0.94),day);
                    lit = mix(lit,vec3(0.98,0.72,0.57),dusk*0.65);
                    vec3 puff = mix(shade,lit,smoothstep(-0.16,0.42,p.y));
                    puff = mix(puff,horizon*0.94,wet*0.85);
                    sky = mix(sky,puff,cover);
                }
            }
        }
    }
    color = vec4(sky,1.0);
}
)GLSL";

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader,1,&source,NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(shader,sizeof(log),NULL,log);
        fprintf(stderr,"[Sky] Shader failed: %s\n",log);
        glDeleteShader(shader); return 0;
    }
    return shader;
}

static int init(void) {
    if (program) return 1;
    if (failed) return 0;
    failed = 1; // A failure falls back to the stock clear color; never spam retries.
    GLuint vs = compile(GL_VERTEX_SHADER,vertex_source);
    GLuint fs = compile(GL_FRAGMENT_SHADER,fragment_source);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint candidate = glCreateProgram();
    glAttachShader(candidate,vs); glAttachShader(candidate,fs); glLinkProgram(candidate);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(candidate,GL_LINK_STATUS,&ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(candidate,sizeof(log),NULL,log);
        fprintf(stderr,"[Sky] Link failed: %s\n",log);
        glDeleteProgram(candidate); return 0;
    }
    program = candidate;
    glGenVertexArrays(1,&vao);
    u_projection = glGetUniformLocation(program,"projection");
    u_rotation = glGetUniformLocation(program,"eye_to_world");
    u_environment = glGetUniformLocation(program,"environment");
    fprintf(stdout,"[Sky] Outdoor sky renderer initialized\n");
    return 1;
}

extern "C" void pc_sky_set_environment(int enabled, float time, float cloudiness) {
    outdoors = enabled;
    seconds = time;
    overcast = cloudiness;
    environment_frame = pc_frame_counter;
    have_environment = 1;
    environment_active = enabled;
}

static void update_environment_active(void) {
    // Freeze the last live answer while paused; bypassing age checks outright
    // would revive an outdoor sky that already expired on a scene transition.
    if (!g_pc_paused)
        environment_active = have_environment && outdoors &&
            (uint32_t)(pc_frame_counter-environment_frame)<=1u;
}

extern "C" void pc_sky_begin_pass(void) {
    drawn = have_view = 0;
    update_environment_active(); // expire even when a scene has no perspective draws
}
extern "C" void pc_sky_set_view(const float* matrix) {
    memcpy(view,matrix,sizeof(view));
    have_view = 1;
}

extern "C" int pc_sky_draw(const float* projection, const float* correction) {
    update_environment_active();
    if (drawn || !have_view || !environment_active || !g_pc_settings.skybox) return 0;
    if (fabsf(projection[0])<0.00001f || fabsf(projection[5])<0.00001f) return 0;
    // Compose X*V for VR, then transpose its normalized rotation. Translation is
    // deliberately discarded, keeping clouds at infinity for both eyes and scales.
    float rotation[9];
    for (int r=0;r<3;r++) {
        float row[3];
        for (int c=0;c<3;c++) {
            row[c] = correction ? correction[r*4]*view[c] + correction[r*4+1]*view[4+c]
                                + correction[r*4+2]*view[8+c] : view[r*4+c];
        }
        float length = sqrtf(row[0]*row[0]+row[1]*row[1]+row[2]*row[2]);
        if (!(length>0.000001f) || !std::isfinite(length)) return 0;
        for (int c=0;c<3;c++) rotation[c*3+r]=row[c]/length;
    }
    if (!init()) return 0;
    GLint previous_program, previous_vao, depth_func;
    GLdouble depth_range[2];
    GLboolean depth_mask, color_mask[4];
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean logic = glIsEnabled(GL_COLOR_LOGIC_OP);
    glGetIntegerv(GL_CURRENT_PROGRAM,&previous_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previous_vao);
    glGetIntegerv(GL_DEPTH_FUNC,&depth_func);
    glGetDoublev(GL_DEPTH_RANGE,depth_range);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    glDepthRange(0.0,1.0);
    glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_COLOR_LOGIC_OP); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glUseProgram(program); glBindVertexArray(vao);
    glUniform4f(u_projection,projection[0],projection[5],projection[2],projection[6]);
    glUniformMatrix3fv(u_rotation,1,GL_TRUE,rotation);
    glUniform2f(u_environment,seconds,overcast);
    glDrawArrays(GL_TRIANGLES,0,3);
    // Exact restoration also preserves the GX renderer's cached state.
    glUseProgram(previous_program); glBindVertexArray(previous_vao);
    glDepthFunc(depth_func); glDepthMask(depth_mask);
    glDepthRange(depth_range[0],depth_range[1]);
    glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);
    if (!depth) glDisable(GL_DEPTH_TEST);
    if (blend) glEnable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (logic) glEnable(GL_COLOR_LOGIC_OP);
    drawn = 1;
    return 1;
}

extern "C" void pc_sky_shutdown(void) {
    if (program) glDeleteProgram(program);
    if (vao) glDeleteVertexArrays(1,&vao);
    program=vao=0; failed=have_environment=have_view=drawn=environment_active=0;
}
