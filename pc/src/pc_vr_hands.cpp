/* Small controller-grip mittens, drawn after world geometry and before UI.
 * The only persistent resources are one program, one VAO and one static VBO.
 * Mesh creation happens once; drawing neither allocates nor touches game state. */
#include "pc_vr_hands.h"
#include "pc_gl.h"
#include <cmath>
#include <cstdio>
#include <cstddef>

namespace {
GLuint program, vao, vbo;
GLint u_pose, u_projection, u_mirror;
GLsizei vertex_count;
bool failed;

const char* vertex_source = PC_GLSL_HEADER R"GLSL(
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
uniform mat4 eye_from_grip;
uniform mat4 gx_projection;
uniform float mirror_hand;
out vec3 eye_normal;
void main() {
    vec3 p = position * vec3(mirror_hand, 1.0, 1.0);
    vec3 n = normal * vec3(mirror_hand, 1.0, 1.0);
    vec4 clip = gx_projection * eye_from_grip * vec4(p, 1.0);
    // Remap GX -1/0 to GL -1/+1 for clipping. The half viewport depth
    // range below keeps the written depths identical to the GX world.
    clip.z = 2.0 * clip.z + clip.w;
    gl_Position = clip;
    eye_normal = mat3(eye_from_grip) * n;
}
)GLSL";

const char* fragment_source = PC_GLSL_HEADER R"GLSL(
in vec3 eye_normal;
out vec4 color;
void main() {
    vec3 light = normalize(vec3(-0.35, 0.75, 0.60));
    float shade = 0.68 + 0.32 * max(dot(normalize(eye_normal), light), 0.0);
    color = vec4(vec3(0.98, 0.96, 0.88) * shade, 1.0);
}
)GLSL";

// GX and other native overlays may leave any of these enabled. Save before
// lazy initialization too: creating the VAO/VBO changes buffer bindings.
const GLenum capabilities[] = {
    GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_SCISSOR_TEST,
    GL_STENCIL_TEST, GL_RASTERIZER_DISCARD,
    GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE,
#ifndef __ANDROID__
    GL_COLOR_LOGIC_OP, GL_SAMPLE_MASK, GL_DEPTH_CLAMP
#endif
};
struct SavedState {
    GLint old_program, old_vao, array_buffer, depth_func, polygon_mode[2];
    GLboolean depth_mask, color_mask[4];
    pc_gl_depth_value depth_range[2];
    GLboolean enabled[sizeof(capabilities)/sizeof(capabilities[0])];
    SavedState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
        glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
        // Core contexts have one mode; compatibility contexts can return a
        // separate back-face mode. Some core drivers write only the first.
#ifndef __ANDROID__
        polygon_mode[0]=polygon_mode[1]=-1;
        glGetIntegerv(GL_POLYGON_MODE, polygon_mode);
        if (polygon_mode[1]==-1) polygon_mode[1]=polygon_mode[0];
#endif
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
        pc_gl_get_depth_range(depth_range);
        for (size_t i=0; i<sizeof(capabilities)/sizeof(capabilities[0]); ++i)
            enabled[i] = glIsEnabled(capabilities[i]);
    }
    ~SavedState() {
        glUseProgram((GLuint)old_program);
        glBindVertexArray((GLuint)old_vao);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)array_buffer);
        glDepthFunc((GLenum)depth_func);
        glDepthMask(depth_mask);
        pc_gl_depth_range(depth_range[0], depth_range[1]);
        glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
#ifndef __ANDROID__
        if (polygon_mode[0] == polygon_mode[1]) {
            glPolygonMode(GL_FRONT_AND_BACK, (GLenum)polygon_mode[0]);
        } else {
            // Separate modes are possible only in a compatibility context.
            glPolygonMode(GL_FRONT, (GLenum)polygon_mode[0]);
            glPolygonMode(GL_BACK, (GLenum)polygon_mode[1]);
        }
#endif
        for (size_t i=0; i<sizeof(capabilities)/sizeof(capabilities[0]); ++i) {
            if (enabled[i]) glEnable(capabilities[i]);
            else glDisable(capabilities[i]);
        }
    }
};

struct Vertex { float position[3], normal[3]; };
const int latitudes=8, longitudes=16;
const int max_vertices=2 * (latitudes-1) * longitudes * 6;

Vertex ellipsoid_vertex(int latitude, int longitude, const float center[3], const float radius[3]) {
    const float pi=3.14159265358979323846f;
    const float theta=pi * float(latitude)/latitudes;
    const float phi=2.0f*pi * float(longitude)/longitudes;
    // Exact poles and a closed seam avoid tiny gaps between adjacent triangles.
    const float ring=(latitude==0 || latitude==latitudes) ? 0.0f : std::sin(theta);
    const float unit[3]={ring*std::cos(phi), std::cos(theta), ring*std::sin(phi)};
    Vertex v;
    float length2=0;
    for (int i=0; i<3; ++i) {
        v.position[i]=center[i]+radius[i]*unit[i];
        v.normal[i]=unit[i]/radius[i];
        length2+=v.normal[i]*v.normal[i];
    }
    const float inverse_length=1.0f/std::sqrt(length2);
    for (int i=0; i<3; ++i) v.normal[i]*=inverse_length;
    return v;
}

void append_ellipsoid(Vertex* vertices, int& count, const float center[3], const float radius[3]) {
    for (int latitude=0; latitude<latitudes; ++latitude) {
        for (int longitude=0; longitude<longitudes; ++longitude) {
            const int next=(longitude+1)%longitudes;
            const Vertex a=ellipsoid_vertex(latitude,longitude,center,radius);
            const Vertex b=ellipsoid_vertex(latitude+1,longitude,center,radius);
            const Vertex c=ellipsoid_vertex(latitude+1,next,center,radius);
            const Vertex d=ellipsoid_vertex(latitude,next,center,radius);
            if (latitude!=latitudes-1) {
                vertices[count++]=a; vertices[count++]=b; vertices[count++]=c;
            }
            if (latitude!=0) {
                vertices[count++]=a; vertices[count++]=c; vertices[count++]=d;
            }
        }
    }
}

GLuint compile(GLenum type, const char* source) {
    GLuint shader=glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader,1,&source,NULL);
    glCompileShader(shader);
    GLint ok=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[1024]={};
        glGetShaderInfoLog(shader,sizeof(log),NULL,log);
        std::fprintf(stderr,"[VR hands] Shader failed: %s\n",log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void release_resources() {
    if (vbo) glDeleteBuffers(1,&vbo);
    if (vao) glDeleteVertexArrays(1,&vao);
    if (program) glDeleteProgram(program);
    program=vao=vbo=0;
    vertex_count=0;
}

bool initialize() {
    if (program) return true;
    if (failed) return false;
    // A failed attempt is retried only after shutdown, avoiding per-frame spam.
    failed=true;
    GLuint vertex=compile(GL_VERTEX_SHADER,vertex_source);
    GLuint fragment=vertex ? compile(GL_FRAGMENT_SHADER,fragment_source) : 0;
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        return false;
    }
    program=glCreateProgram();
    if (program) {
        glAttachShader(program,vertex);
        glAttachShader(program,fragment);
        glLinkProgram(program);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked=0;
    if (program) glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if (!linked) {
        char log[1024]={};
        if (program) glGetProgramInfoLog(program,sizeof(log),NULL,log);
        std::fprintf(stderr,"[VR hands] Program failed: %s\n",log);
        release_resources();
        return false;
    }
    u_pose=glGetUniformLocation(program,"eye_from_grip");
    u_projection=glGetUniformLocation(program,"gx_projection");
    u_mirror=glGetUniformLocation(program,"mirror_hand");
    if (u_pose<0 || u_projection<0 || u_mirror<0) {
        release_resources();
        return false;
    }
    Vertex vertices[max_vertices];
    int count=0;
    const float palm_center[3]={0.0f,0.0f,-0.024f};
    const float palm_radius[3]={0.043f,0.032f,0.067f};
    const float thumb_center[3]={0.043f,0.004f,-0.018f};
    const float thumb_radius[3]={0.022f,0.026f,0.031f};
    append_ellipsoid(vertices,count,palm_center,palm_radius);
    append_ellipsoid(vertices,count,thumb_center,thumb_radius);
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    if (!vao || !vbo) {
        release_resources();
        return false;
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,count*sizeof(Vertex),vertices,GL_STATIC_DRAW);
    GLint bytes=0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
    if (bytes!=int(count*sizeof(Vertex))) {
        release_resources();
        return false;
    }
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,normal));
    vertex_count=count;
    failed=false;
    return true;
}
} // namespace

extern "C" int pc_vr_hands_draw(const float pose[12], const float projection[16], int hand) {
    if (!pose || !projection || hand<0 || hand>1) return 0;
    for (int i=0; i<12; ++i) if (!std::isfinite(pose[i])) return 0;
    for (int i=0; i<16; ++i) if (!std::isfinite(projection[i])) return 0;
    if (std::fabs(projection[0])<0.000001f || std::fabs(projection[5])<0.000001f) return 0;
    SavedState saved;
    if (!initialize()) return 0;
    const float matrix[16]={pose[0],pose[1],pose[2],pose[3],
                            pose[4],pose[5],pose[6],pose[7],
                            pose[8],pose[9],pose[10],pose[11],
                            0,0,0,1};
    for (size_t i=0; i<sizeof(capabilities)/sizeof(capabilities[0]); ++i)
        glDisable(capabilities[i]);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    pc_gl_depth_range(saved.depth_range[0],
                 (saved.depth_range[0]+saved.depth_range[1])*0.5);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
#ifndef __ANDROID__
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
#endif
    glUseProgram(program);
    glBindVertexArray(vao);
    glUniformMatrix4fv(u_pose,1,GL_TRUE,matrix);
    glUniformMatrix4fv(u_projection,1,GL_TRUE,projection);
    glUniform1f(u_mirror,hand==0 ? 1.0f : -1.0f);
    glDrawArrays(GL_TRIANGLES,0,vertex_count);
    return 1;
}

extern "C" void pc_vr_hands_shutdown(void) {
    release_resources();
    failed=false;
}
