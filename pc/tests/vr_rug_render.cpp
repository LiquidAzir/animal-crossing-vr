/* Hidden native reproduction: original rug geometry, UVs and atlas, with a
 * simple diagnostic-lighting shader. The routing test separately executes the
 * production GX/emu64 functions; this isolates their GL cull-state consequence.
 * No game save, headset, image edits, depth bias or filtering changes. */
#include <SDL.h>
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const int W = 1200, H = 900;
static int checks, failures;
#define CHECK(c, msg) do { ++checks; if(!(c)) { ++failures; std::printf("FAIL: %s\n", msg); } } while(0)

static std::vector<unsigned char> load(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if(!file) std::exit(2);
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> data(size);
    if(std::fread(data.data(), 1, size, file) != size_t(size)) std::exit(2);
    std::fclose(file);
    return data;
}

static void normalize(float* vector) {
    const float length = std::sqrt(vector[0]*vector[0]+vector[1]*vector[1]+vector[2]*vector[2]);
    for(int i=0; i<3; ++i) vector[i] /= length;
}

static void cross(const float* a, const float* b, float* out) {
    out[0] = a[1]*b[2]-a[2]*b[1];
    out[1] = a[2]*b[0]-a[0]*b[2];
    out[2] = a[0]*b[1]-a[1]*b[0];
}

static void camera(float* matrix, float yaw) {
    const float angle = yaw*3.14159265f/180;
    const float eye[] = {7000*std::sin(angle), 3300, 7000*std::cos(angle)};
    const float target[] = {0, 750, 0};
    float forward[3], right[3], up[] = {0, 1, 0};
    for(int i=0; i<3; ++i) forward[i] = target[i]-eye[i];
    normalize(forward);
    cross(forward, up, right);
    normalize(right);
    cross(right, forward, up);
    float view[16] = {};
    for(int j=0; j<3; ++j) {
        view[j] = right[j]; view[4+j] = up[j]; view[8+j] = -forward[j];
        view[3] -= right[j]*eye[j]; view[7] -= up[j]*eye[j]; view[11] += forward[j]*eye[j];
    }
    view[15] = 1;
    const float near_z=50, far_z=20000, q=1/std::tan(38*3.14159265f/360);
    // Same GX clip-depth convention consumed by the existing world shader.
    const float projection[16] = {q/(float(W)/H),0,0,0, 0,q,0,0,
                                 0,0,near_z/(near_z-far_z),near_z*far_z/(near_z-far_z), 0,0,-1,0};
    for(int i=0; i<4; ++i) for(int j=0; j<4; ++j) {
        matrix[i*4+j] = 0;
        for(int k=0; k<4; ++k) matrix[i*4+j] += projection[i*4+k]*view[k*4+j];
    }
}

static std::vector<unsigned char> shot(const char* path) {
    std::vector<unsigned char> pixels(W*H*4), flipped(pixels.size());
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for(int y=0; y<H; ++y)
        std::copy_n(pixels.data()+y*W*4, W*4, flipped.data()+(H-y-1)*W*4);
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(flipped.data(), W, H, 32, W*4,
                                                   0xff, 0xff00, 0xff0000, 0xff000000);
    CHECK(surface && SDL_SaveBMP(surface, path)==0, "save actual native pixels");
    SDL_FreeSurface(surface);
    return pixels;
}

static GLuint shader(GLenum kind, const char* text) {
    GLuint result = glCreateShader(kind);
    glShaderSource(result, 1, &text, nullptr);
    glCompileShader(result);
    GLint ok;
    glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
    if(!ok) std::exit(4);
    return result;
}

int main(int, char**) {
    if(SDL_Init(SDL_INIT_VIDEO) != 0) return 3;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("Rug culling regression", 0, 0, W, H,
                                          SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if(!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) return 3;
    const char* vertex_text = "#version 330 core\n"
        "layout(location=0)in vec3 p;layout(location=1)in vec2 uv;"
        "layout(location=2)in float shade;uniform mat4 m;out vec2 t;out float c;"
        "void main(){gl_Position=m*vec4(p,1);t=uv;c=shade;}";
    const char* fragment_text = "#version 330 core\n"
        "in vec2 t;in float c;uniform sampler2D tex;out vec4 color;"
        "void main(){vec4 s=texture(tex,t);if(s.a<.5)discard;color=vec4(s.rgb*c,1);}";
    GLuint vertex=shader(GL_VERTEX_SHADER, vertex_text), fragment=shader(GL_FRAGMENT_SHADER, fragment_text);
    GLuint program=glCreateProgram();
    glAttachShader(program, vertex); glAttachShader(program, fragment); glLinkProgram(program);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if(!ok) return 4;
    glUseProgram(program);
    auto vertices=load("rug-vertices.bin"), texture=load("rug-texture.rgba");
    CHECK(vertices.size()==48*3*24 && texture.size()==32*32*4, "original rug asset dimensions");
    GLuint vao, buffer, tex, fbo, color, depth;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    glGenBuffers(1, &buffer); glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size(), vertices.data(), GL_STATIC_DRAW);
    for(int i=0; i<3; ++i) glEnableVertexAttribArray(i);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(void*)12);
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,24,(void*)20);
    glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_MIRRORED_REPEAT);
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &color); glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) return 5;
    glViewport(0,0,W,H); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glCullFace(GL_BACK); glClearColor(.24f,.18f,.18f,0); glUniform1i(glGetUniformLocation(program,"tex"),0);
    int total_different=0;
    for(int yaw: {0,30,60,180,210}) {
        std::vector<unsigned char> previous;
        for(int cull=0; cull<2; ++cull) {
            if(cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            float matrix[16]; camera(matrix,float(yaw));
            glUniformMatrix4fv(glGetUniformLocation(program,"m"),1,GL_TRUE,matrix);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES,0,vertices.size()/24);
            char path[80];
            std::snprintf(path,sizeof(path),"rug-%d-%s.bmp",yaw,cull?"authored-cull":"two-sided");
            auto pixels=shot(path);
            int filled=0;
            for(int i=0; i<W*H; ++i) if(pixels[i*4+3]) ++filled;
            CHECK(filled>10000,"rug remains visible from front and back");
            if(cull) {
                int different=0, coverage_changes=0;
                for(int i=0; i<W*H; ++i) {
                    if(pixels[i*4+3]!=previous[i*4+3]) ++coverage_changes;
                    if(!std::equal(pixels.begin()+i*4,pixels.begin()+i*4+3,previous.begin()+i*4)) ++different;
                }
                // Opaque original front/back artwork shares the same silhouette.
                CHECK(coverage_changes<40,"authored culling retains complete silhouette");
                total_different+=different;
                std::printf("angle %d: %d changed color pixels, %d silhouette pixels\n",yaw,different,coverage_changes);
            }
            previous.swap(pixels);
        }
    }
    CHECK(total_different>1000,"original coincident UV sheets reproduce visible interference");
    CHECK(glGetError()==GL_NO_ERROR,"native GL calls complete without error");
    glDeleteRenderbuffers(1,&depth); glDeleteRenderbuffers(1,&color); glDeleteFramebuffers(1,&fbo);
    glDeleteTextures(1,&tex); glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao);
    glDeleteProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    std::printf("Rug native render: %d checks, %d failures; 10 original-mesh images\n",checks,failures);
    return failures?1:0;
}
