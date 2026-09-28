/* Actual world vertex shader; compare exact color and sampled depth bits. */
#define main batch_clip_math_tests
#include "batch_clip_math.c"
#undef main
#include "gles_device_context.h"
#include "pc_gl.h"
#include <time.h>

enum {W=96,H=64,PIXELS=W*H*4};
static GLuint world, depth_program, vao,vbo,framebuffer,depth_texture,depth_framebuffer;
static unsigned rejected_runs, submitted_runs, visible_cases, depth_cases;

static GLuint compile(GLenum type,const char* source) {
    GLuint shader=glCreateShader(type);pc_gl_shader_source(shader,source);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048];glGetShaderInfoLog(shader,sizeof(log),NULL,log);printf("%s\n",log);}
    CHECK(ok);return shader;
}
static GLuint program(const char* vs,const char* fs) {
    GLuint vertex=compile(GL_VERTEX_SHADER,vs),fragment=compile(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();
    glAttachShader(p,vertex);glAttachShader(p,fragment);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);CHECK(ok);glDeleteShader(vertex);glDeleteShader(fragment);return p;
}
static char* file_text(const char* path) {
    FILE* f=fopen(path,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    char* source=(char*)malloc(size+1);if(source){size=fread(source,1,size,f);source[size]=0;}fclose(f);return source;
}
static GLuint texture(GLenum internal,GLenum format,GLenum type) {
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,internal,W,H,0,format,type,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
}
static void multiply(float* out,const float* a,const float* b) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c){out[r*4+c]=0;for(int k=0;k<4;++k)out[r*4+c]+=a[r*4+k]*b[k*4+c];}
}
static void pose(float* m,int pose_index,int eye) {
    float yaw=(pose_index%4)*1.57079632679f,pitch=((pose_index/4)-1)*.55f,roll=(pose_index%3-1)*.3f;
    float y[16]={cosf(yaw),0,sinf(yaw),0,0,1,0,0,-sinf(yaw),0,cosf(yaw),0,0,0,0,1};
    float p[16]={1,0,0,0,0,cosf(pitch),-sinf(pitch),0,0,sinf(pitch),cosf(pitch),0,0,0,0,1};
    float r[16]={cosf(roll),-sinf(roll),0,0,sinf(roll),cosf(roll),0,0,0,0,1,0,0,0,0,1};
    float tmp[16];multiply(tmp,p,y);multiply(m,r,tmp);
    m[3]=(eye?-.032f:.032f)+(pose_index%3-1)*.11f;m[7]=.07f;
}
static void projection(float* p,int eye) {
    memset(p,0,16*sizeof(float));p[0]=1.18f;p[5]=1.33f;p[2]=eye?-.19f:.16f;p[6]=.08f;
    p[10]=.03f/(.03f-100);p[11]=.03f*100/(.03f-100);p[14]=-1;
}
static void geometry(TestVertex* v,int kind,int count) {
    const float original[4][3]={{-.6f,-.4f,-2},{.6f,-.4f,-2},{.6f,.6f,-2},{-.6f,.6f,-2}};
    memset(v,0,count*sizeof(*v));for(int i=0;i<count;++i)memcpy(v[i].position,original[i],12);
    if(count==3)v[2].position[0]=0;
    for(int i=0;i<count;++i) {
        if(kind==1)v[i].position[0]-=8;if(kind==2)v[i].position[0]+=8;
        if(kind==3)v[i].position[1]-=8;if(kind==4)v[i].position[1]+=8;
        if(kind==5)v[i].position[2]=2;
        if(kind==6)v[i].position[2]=-.005f;
        if(kind==7)v[i].position[2]=i==0?1:i==1?-.01f:-3;
        if(kind==8){v[i].position[0]*=20;v[i].position[1]*=20;}
        if(kind==9){for(int a=0;a<3;++a)v[i].position[a]*=1000;}
        if(kind==10)v[i].position[0]+=1.9f;
        if(kind==11)v[i].position[1]-=1.9f;
        if(kind==12){v[i].position[0]*=.03f;v[i].position[1]*=.03f;v[i].position[2]=-.03f;}
        if(kind==13)v[i].position[2]=(i==0?-.02999f:-.03001f);
        if(kind==14)v[i].position[0]+=1e13f;
        if(kind==15){v[i].position[0]*=.0001f;v[i].position[1]*=.0001f;}
    }
}
static void read_depth(unsigned char* out) {
    glBindFramebuffer(GL_FRAMEBUFFER,depth_framebuffer);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    glUseProgram(depth_program);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,depth_texture);
    glUniform1i(glGetUniformLocation(depth_program,"depth_map"),0);
    glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,W,H,GL_RGBA_INTEGER,GL_UNSIGNED_BYTE,out);
}
static void render(const TestVertex* v,int count,const float* p,const float* m,int enable,unsigned char* color,unsigned char* depth) {
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,W,H);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glDepthRangef(0,1022.0f/1023.0f);
    glClearColor(.02f,.04f,.06f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(world);glUniformMatrix4fv(glGetUniformLocation(world,"u_projection"),1,GL_TRUE,p);
    glUniformMatrix4fv(glGetUniformLocation(world,"u_modelview"),1,GL_TRUE,m);
    glVertexAttrib4f(2,.83f,.35f,.12f,1);
    int rejected=enable&&quest_batch_clip_outside(p,m,v,sizeof(*v),count);
    if(rejected)++rejected_runs;
    else {++submitted_runs;glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,count*sizeof(*v),v,GL_STREAM_DRAW);glDrawArrays(count==4?GL_TRIANGLE_FAN:GL_TRIANGLES,0,count);}
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,color);read_depth(depth);
    CHECK(glGetError()==GL_NO_ERROR);
}
static double seconds(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void benchmark(void) {
    TestVertex v[30];float mvp[16];memcpy(mvp,identity,sizeof(mvp));
    double measurements[7];volatile unsigned result=0;
    for(int sample=0;sample<7;++sample){double start=seconds();
        for(int frame=0;frame<20;++frame)for(int run=0;run<3750;++run){
            for(int i=0;i<30;++i){v[i].position[0]=(run&1)?3.0f:0.0f;v[i].position[1]=i*.001f;v[i].position[2]=0;}
            result+=quest_batch_clip_outside(mvp,identity,v,sizeof(*v),30);
        }
        measurements[sample]=(seconds()-start)*1000/20;
    }
    for(int i=0;i<7;++i)for(int j=i+1;j<7;++j)if(measurements[j]<measurements[i]){double t=measurements[i];measurements[i]=measurements[j];measurements[j]=t;}
    printf("CPU synthetic3750runs 30vertices halfvisible ms/frame min=%.3f median=%.3f max=%.3f rejectsum=%u (includes input fill)\n",measurements[0],measurements[3],measurements[6],result);
}
int main(int argc,char**argv) {
    if(batch_clip_math_tests())return 1;
    if(argc<2||!device_context_begin(W,H))return 2;
    char* source=file_text(argv[1]);CHECK(source!=NULL);if(!source)return 2;
    world=program(source,"#version 330 core\nin vec4 v_color;out vec4 color;void main(){color=v_color;}");free(source);
    depth_program=program("#version 330 core\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}",
      "#version 330 core\nuniform highp sampler2D depth_map;layout(location=0)out highp uvec4 color;void main(){uint b=floatBitsToUint(texelFetch(depth_map,ivec2(gl_FragCoord.xy),0).r);color=uvec4(b,b>>8u,b>>16u,b>>24u)&uvec4(255u);}");
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(TestVertex),0);
    GLuint color_texture=texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE);depth_texture=texture(GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT);
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color_texture,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth_texture,0);CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    GLuint packed_depth=texture(GL_RGBA8UI,GL_RGBA_INTEGER,GL_UNSIGNED_BYTE);glGenFramebuffers(1,&depth_framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,depth_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,packed_depth,0);CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    unsigned char color_a[PIXELS],color_b[PIXELS],depth_a[PIXELS],depth_b[PIXELS];
    TestVertex vertices[4];unsigned cases=0;
    for(int count=3;count<=4;++count)for(int eye=0;eye<2;++eye)for(int orientation=0;orientation<12;++orientation)for(int kind=0;kind<16;++kind){
        float p[16],m[16];projection(p,eye);pose(m,orientation,eye);geometry(vertices,kind,count);
        render(vertices,count,p,m,0,color_a,depth_a);render(vertices,count,p,m,1,color_b,depth_b);
        int same_color=memcmp(color_a,color_b,PIXELS)==0,same_depth=memcmp(depth_a,depth_b,PIXELS)==0;
        if(!same_color||!same_depth)printf("Mismatch count=%d eye=%d pose=%d geometry=%d\n",count,eye,orientation,kind);
        CHECK(same_color);CHECK(same_depth);++cases;
        for(int i=0;i<PIXELS;i+=4)if(color_a[i]>100){++visible_cases;break;}
        for(int i=0;i<PIXELS;i+=4)if(depth_a[i]!=0||depth_a[i+1]!=0||depth_a[i+2]!=128||depth_a[i+3]!=63){++depth_cases;break;}
    }
    CHECK(visible_cases>30);CHECK(depth_cases==visible_cases);CHECK(rejected_runs>50);CHECK(rejected_runs<cases);
    /* A saved draw transform remains valid after the caller's live matrix changes. */
    float saved[16],live[16];memcpy(live,identity,sizeof(live));live[3]=4;memcpy(saved,live,sizeof(saved));memcpy(live,identity,sizeof(live));
    geometry(vertices,0,3);for(int i=0;i<3;++i)vertices[i].position[2]=0;
    CHECK(quest_batch_clip_outside(saved,identity,vertices,sizeof(*vertices),3));CHECK(!quest_batch_clip_outside(live,identity,vertices,sizeof(*vertices),3));
    glFinish();benchmark();
    printf("GLES cases=%u visible=%u depth_written=%u rejected_runs=%u submitted_runs=%u checks=%u failures=%u\n",cases,visible_cases,depth_cases,rejected_runs,submitted_runs,checks,failures);
    device_context_end();return failures!=0;
}
