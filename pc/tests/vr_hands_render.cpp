/* Actual production GL renderer checks, with no ROM or headset dependency. */
#include <SDL.h>
#include <glad/gl.h>
#include "pc_vr_hands.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static int checks, failures;
#define CHECK(c,label) do { ++checks; if (!(c)) { ++failures; printf("FAIL: %s\n",label); } } while(0)
static const int W=960, H=640;
static const GLenum caps[]={GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,
    GL_COLOR_LOGIC_OP,GL_STENCIL_TEST,GL_RASTERIZER_DISCARD,GL_POLYGON_OFFSET_FILL,
    GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_SAMPLE_MASK,GL_DEPTH_CLAMP};
static float projection[16];
static float pose[12]={1,0,0,0, 0,1,0,0, 0,0,1,-0.4f};

struct Snapshot {
    GLint program,vao,array_buffer,element_buffer,attribute_buffer;
    GLint draw_fbo,read_fbo,viewport[4],scissor[4],depth_func,polygon[2];
    GLint active_texture,texture,blend_src,blend_dst,logic_op,stencil_func,stencil_ref;
    GLboolean color_mask[4],depth_mask,enabled[sizeof(caps)/sizeof(caps[0])];
    GLdouble depth_range[2];
    GLfloat offset_factor,offset_units,sample_coverage;
    Snapshot() {
        std::memset(this,0,sizeof(*this));
        glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array_buffer); glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&element_buffer);
        glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&attribute_buffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);
        glGetIntegerv(GL_VIEWPORT,viewport); glGetIntegerv(GL_SCISSOR_BOX,scissor);
        glGetIntegerv(GL_DEPTH_FUNC,&depth_func); glGetIntegerv(GL_POLYGON_MODE,polygon);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active_texture); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_BLEND_SRC_RGB,&blend_src); glGetIntegerv(GL_BLEND_DST_RGB,&blend_dst);
        glGetIntegerv(GL_LOGIC_OP_MODE,&logic_op); glGetIntegerv(GL_STENCIL_FUNC,&stencil_func);
        glGetIntegerv(GL_STENCIL_REF,&stencil_ref); glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask); glGetDoublev(GL_DEPTH_RANGE,depth_range);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&offset_factor); glGetFloatv(GL_POLYGON_OFFSET_UNITS,&offset_units);
        glGetFloatv(GL_SAMPLE_COVERAGE_VALUE,&sample_coverage);
        for (size_t i=0;i<sizeof(caps)/sizeof(caps[0]);i++) enabled[i]=glIsEnabled(caps[i]);
    }
    bool equals(const Snapshot& other) const {
        if(std::memcmp(this,&other,sizeof(*this))==0) return true;
        const unsigned char* a=(const unsigned char*)this;
        const unsigned char* b=(const unsigned char*)&other;
        for(size_t i=0;i<sizeof(*this);i++) if(a[i]!=b[i])
            printf("State difference byte %u: %u -> %u\n",unsigned(i),unsigned(a[i]),unsigned(b[i]));
        return false;
    }
};

static GLuint make_program() {
    const char* vs="#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}";
    const char* fs="#version 330 core\nout vec4 color;void main(){color=vec4(1);}";
    GLuint v=glCreateShader(GL_VERTEX_SHADER), f=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v,1,&vs,NULL); glCompileShader(v); glShaderSource(f,1,&fs,NULL); glCompileShader(f);
    GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok); CHECK(ok,"foreign test shader linked"); return p;
}
static void clear(float r=0.13f,float g=0.24f,float b=0.36f) {
    for (size_t i=0;i<sizeof(caps)/sizeof(caps[0]);i++) glDisable(caps[i]);
    glColorMask(1,1,1,1); glDepthMask(1); glDepthRange(0,1);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glViewport(0,0,W,H);
    glClearColor(r,g,b,1); glClearDepth(1); glStencilMask(~0u);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}
static std::vector<unsigned char> pixels() {
    std::vector<unsigned char> p(W*H*4);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,p.data()); return p;
}
static std::vector<float> depths() {
    std::vector<float> p(W*H); glReadPixels(0,0,W,H,GL_DEPTH_COMPONENT,GL_FLOAT,p.data()); return p;
}
static bool hand_pixel(const std::vector<unsigned char>& p,int x,int y) {
    const int i=(y*W+x)*4; return p[i]>140 && p[i+1]>140 && p[i+2]>130;
}
static int coverage(const std::vector<unsigned char>& p) {
    int count=0; for(int y=0;y<H;y++) for(int x=0;x<W;x++) count+=hand_pixel(p,x,y); return count;
}
static double centroid(const std::vector<unsigned char>& p) {
    double total=0,count=0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) if(hand_pixel(p,x,y)) {total+=x;count++;}
    return count ? total/count : 0;
}
static void save(const char* name,const std::vector<unsigned char>& p) {
    SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_RGBA32);
    CHECK(surface!=NULL,"preview surface created"); if(!surface) return;
    for(int y=0;y<H;y++) std::memcpy((char*)surface->pixels+y*surface->pitch,p.data()+(H-y-1)*W*4,W*4);
    CHECK(SDL_SaveBMP(surface,name)==0,"native mitten preview written"); SDL_FreeSurface(surface);
}
static void unusual_state() {
    for(size_t i=0;i<sizeof(caps)/sizeof(caps[0]);i++) glEnable(caps[i]);
    glDisable(GL_DEPTH_TEST); glDepthFunc(GL_GREATER); glDepthMask(0); glDepthRange(0.2,0.8);
    glColorMask(0,1,0,0); glScissor(1,2,3,4); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_SRC_ALPHA); glLogicOp(GL_XOR);
    glStencilFunc(GL_NEVER,17,255); glPolygonOffset(2.5f,7.0f); glSampleCoverage(0.125f,GL_TRUE);
    glSampleMaski(0,0);
}
static PFNGLSHADERSOURCEPROC real_shader_source;
static int injected_sources;
static void GLAD_API_PTR failed_shader_source(GLuint shader,GLsizei,const GLchar* const*,const GLint*) {
    ++injected_sources;
    const char* invalid="#version 330 core\n#error deliberate initialization failure for renderer regression";
    real_shader_source(shader,1,&invalid,NULL);
}
static float gx_depth(float eye_z, float near_depth, float far_depth) {
    return near_depth+(far_depth-near_depth)*0.5f*
        (1.0f+(projection[10]*eye_z+projection[11])/(-eye_z));
}

int main(int argc,char** argv) {
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO)!=0) return 2;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window=SDL_CreateWindow("VR hand renderer checks",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    SDL_GLContext context=window ? SDL_GL_CreateContext(window) : NULL;
    if(!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::fprintf(stderr,"GL context unavailable: %s\n",SDL_GetError()); return 2;
    }
    printf("Renderer: %s\n",glGetString(GL_RENDERER));
    const float n=0.05f,f=10.0f;
    projection[0]=1.7320508f/(float(W)/H); projection[5]=1.7320508f;
    projection[10]=n/(n-f); projection[11]=n*f/(n-f); projection[14]=-1;
    GLuint framebuffer,texture,depthbuffer,foreign_vao,foreign_vbo,foreign_ebo;
    glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&texture); glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    glGenRenderbuffers(1,&depthbuffer); glBindRenderbuffer(GL_RENDERBUFFER,depthbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depthbuffer);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"nondefault eye framebuffer complete");
    const GLuint foreign_program=make_program(); glUseProgram(foreign_program);
    glGenVertexArrays(1,&foreign_vao); glBindVertexArray(foreign_vao);
    glGenBuffers(1,&foreign_vbo); glBindBuffer(GL_ARRAY_BUFFER,foreign_vbo);
    const float foreign_data[3]={11,22,33}; glBufferData(GL_ARRAY_BUFFER,sizeof(foreign_data),foreign_data,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,NULL);
    glGenBuffers(1,&foreign_ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,foreign_ebo);
    const unsigned short index=0; glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(index),&index,GL_STATIC_DRAW);
    clear(); unusual_state(); const Snapshot hostile;

    // Real driver failure, injected only at shader source submission. This also
    // checks that a disabled renderer does not recompile on every frame.
    real_shader_source=glad_glShaderSource; glad_glShaderSource=failed_shader_source;
    CHECK(!pc_vr_hands_draw(pose,projection,0),"shader failure returns false gracefully");
    CHECK(hostile.equals(Snapshot()),"failed lazy initialization restores all observed GL state");
    glad_glShaderSource=real_shader_source;
    CHECK(!pc_vr_hands_draw(pose,projection,0)&&injected_sources==1,"failed initialization is not retried every frame");
    pc_vr_hands_shutdown();
    CHECK(hostile.equals(Snapshot()),"shutdown after initialization failure leaves caller state intact");
    CHECK(pc_vr_hands_draw(pose,projection,0),"shutdown permits successful fresh initialization");
    CHECK(hostile.equals(Snapshot()),"first successful draw including VAO initialization restores GL state");
    auto left=pixels(); auto left_depth=depths();
    CHECK(coverage(left)>4000,"opaque cream mitten rendered despite hostile incoming state");
    CHECK(pc_vr_hands_draw(pose,projection,0),"subsequent draw succeeds");
    CHECK(hostile.equals(Snapshot()),"steady-state draw restores GL state");
    CHECK(pixels()==left,"LEQUAL permits identical redraw without state-dependent color changes");
    float restored_buffer[3]={}; glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(restored_buffer),restored_buffer);
    CHECK(std::memcmp(restored_buffer,foreign_data,sizeof(foreign_data))==0,"caller vertex buffer contents preserved");
    CHECK(glGetError()==GL_NO_ERROR,"initialization, drawing and restoration produce no GL errors");

    clear(); CHECK(pc_vr_hands_draw(pose,projection,1),"right mitten draws"); auto right=pixels();
    int mirror_difference=0; bool alpha_opaque=true; float min_depth=1,max_depth=0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) {
        mirror_difference+=hand_pixel(left,x,y)!=hand_pixel(right,W-x-1,y);
        if(hand_pixel(left,x,y)) {
            alpha_opaque=alpha_opaque&&left[(y*W+x)*4+3]==255;
            const float d=left_depth[y*W+x]; if(d<min_depth)min_depth=d; if(d>max_depth)max_depth=d;
        }
    }
    CHECK(mirror_difference<=4,"left and right thumbs have mirrored silhouettes");
    CHECK(centroid(left)>W*0.5+5 && centroid(right)<W*0.5-5,"thumbs face inward for left and right hands");
    CHECK(alpha_opaque,"mitten fragments are fully opaque");
    CHECK(min_depth>gx_depth(-0.35f,0.2f,0.8f)&&max_depth<gx_depth(-0.5f,0.2f,0.8f),
          "hand depths match the GX world using the caller's viewport range");

    clear(); pose[3]=0.1f; pc_vr_hands_draw(pose,projection,0); auto moved=pixels();
    CHECK(centroid(moved)>centroid(left)+100,"row-major grip translation moves the mesh"); pose[3]=0;
    // Foreground must occlude the hand, while its other half remains visible.
    clear(); glEnable(GL_SCISSOR_TEST); glScissor(0,0,W/2,H);
    glClearColor(1,0,0,1); glClearDepth(0.1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST); pc_vr_hands_draw(pose,projection,0);
    auto occluded=pixels(); auto occluded_depth=depths(); bool untouched=true;
    for(int y=0;y<H;y++) for(int x=0;x<W/2;x++) {
        const int i=y*W+x;
        untouched=untouched&&occluded[i*4]==255&&occluded[i*4+1]==0&&occluded[i*4+2]==0&&std::fabs(occluded_depth[i]-0.1f)<0.00001f;
    }
    CHECK(untouched,"nearer world geometry occludes the hand without color or depth damage");
    CHECK(coverage(occluded)>2000,"unoccluded hand remains visible alongside world occluder");
    save("vr-hands-occlusion.bmp",occluded);

    // Depth writes must also order overlapping hands, independent of submission.
    clear(); pose[11]=-0.48f; pc_vr_hands_draw(pose,projection,1);
    pose[11]=-0.30f; pc_vr_hands_draw(pose,projection,0); auto far_then_near=pixels(); auto ordered_depth=depths();
    clear(); pc_vr_hands_draw(pose,projection,0); pose[11]=-0.48f; pc_vr_hands_draw(pose,projection,1);
    CHECK(pixels()==far_then_near && depths()==ordered_depth,"opaque depth writes correctly order overlapping hands");
    pose[11]=-0.4f;

    // Catch GL/GX near-depth confusion: a hand entirely in front of the near
    // plane must be clipped. A normal hand farther than twice near remains.
    clear(); pose[11]=0.045f; pc_vr_hands_draw(pose,projection,0);
    CHECK(coverage(pixels())==0,"mesh wholly before the near plane is clipped");
    clear(); pose[11]=-11.0f; pc_vr_hands_draw(pose,projection,0);
    CHECK(coverage(pixels())==0,"mesh beyond the far plane is clipped"); pose[11]=-0.4f;

    clear(); unusual_state(); const Snapshot invalid_state; const auto invalid_pixels=pixels();
    CHECK(!pc_vr_hands_draw(NULL,projection,0),"null pose rejected");
    CHECK(!pc_vr_hands_draw(pose,NULL,0),"null projection rejected");
    CHECK(!pc_vr_hands_draw(pose,projection,-1)&&!pc_vr_hands_draw(pose,projection,2),"invalid hand indices rejected");
    pose[3]=std::numeric_limits<float>::quiet_NaN(); CHECK(!pc_vr_hands_draw(pose,projection,0),"nonfinite pose rejected"); pose[3]=0;
    projection[2]=std::numeric_limits<float>::infinity(); CHECK(!pc_vr_hands_draw(pose,projection,0),"nonfinite projection rejected"); projection[2]=0;
    const float p00=projection[0]; projection[0]=0; CHECK(!pc_vr_hands_draw(pose,projection,0),"degenerate projection rejected"); projection[0]=p00;
    CHECK(invalid_state.equals(Snapshot())&&pixels()==invalid_pixels,"invalid inputs preserve caller state and framebuffer");
    pc_vr_hands_shutdown(); CHECK(invalid_state.equals(Snapshot()),"normal shutdown preserves foreign program and buffer bindings");
    CHECK(pc_vr_hands_draw(pose,projection,0),"renderer restarts after normal shutdown");
    CHECK(invalid_state.equals(Snapshot()),"reinitialization preserves caller state again");

    // Real production artwork in synthetic poses; rotation belongs to tracking,
    // never to a baked renderer offset. Both show the forward mitten extension.
    clear(); const float angle=0.68f, c=std::cos(angle),s=std::sin(angle);
    float preview[12]={1,0,0,-0.11f, 0,c,-s,-0.04f, 0,s,c,-0.40f};
    pc_vr_hands_draw(preview,projection,0); preview[3]=0.11f; pc_vr_hands_draw(preview,projection,1);
    auto preview_pixels=pixels(); CHECK(coverage(preview_pixels)>12000,"two tracked-pose mittens appear in native preview");
    save("vr-hands-preview.bmp",preview_pixels);
    CHECK(glGetError()==GL_NO_ERROR,"all rendering, invalid-input and lifecycle checks leave no GL errors");
    pc_vr_hands_shutdown(); glUseProgram(0); glBindVertexArray(0); glBindBuffer(GL_ARRAY_BUFFER,0);
    glDeleteProgram(foreign_program); glDeleteBuffers(1,&foreign_vbo); glDeleteBuffers(1,&foreign_ebo); glDeleteVertexArrays(1,&foreign_vao);
    glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteFramebuffers(1,&framebuffer); glDeleteRenderbuffers(1,&depthbuffer); glDeleteTextures(1,&texture);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    printf("VR hand renderer: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
