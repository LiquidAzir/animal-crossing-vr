/* Actual-source GX upload parity and bounded mobile driver benchmark. */
#include "gles_device_context.h"
#include "pc_gx_internal.h"
#include <stddef.h>
static double upload_ns;
static size_t upload_bytes;
static unsigned allocations, uploads;
static Uint64 ticks(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (Uint64)t.tv_sec*1000000000ull+t.tv_nsec; }
static double map_ns,unmap_ns,attrib_ns,copy_ns,data_ns;
static int fail_next_map,fail_next_unmap;
static Uint64 pc_profiler_begin_timer(void){return ticks();}
enum {PC_PROF_TIMER_BUFFER_UPLOAD};
static void pc_profiler_add_time(int timer,Uint64 start){(void)timer;upload_ns+=(double)(ticks()-start);}
static void pc_profiler_add_count_buffer_upload(size_t bytes){++uploads;upload_bytes+=bytes;}
static void tracked_buffer_data(GLenum target,GLsizeiptr size,const void* data,GLenum usage){Uint64 t=ticks();++allocations;glBufferData(target,size,data,usage);data_ns+=ticks()-t;}
static void* tracked_map(GLenum target,GLintptr offset,GLsizeiptr length,GLbitfield access){if(fail_next_map){fail_next_map=0;return NULL;}Uint64 t=ticks();void* p=glMapBufferRange(target,offset,length,access);map_ns+=ticks()-t;return p;}
static GLboolean tracked_unmap(GLenum target){Uint64 t=ticks();GLboolean r=glUnmapBuffer(target);unmap_ns+=ticks()-t;if(fail_next_unmap){fail_next_unmap=0;return GL_FALSE;}return r;}
static void tracked_attrib(GLuint index,GLint size,GLenum type,GLboolean normalized,GLsizei stride,const void* pointer){Uint64 t=ticks();glVertexAttribPointer(index,size,type,normalized,stride,pointer);attrib_ns+=ticks()-t;}
static void* tracked_copy(void* dst,const void* src,size_t length){Uint64 t=ticks();void* r=memcpy(dst,src,length);copy_ns+=ticks()-t;return r;}
#define glBufferData tracked_buffer_data
#define glMapBufferRange tracked_map
#define glUnmapBuffer tracked_unmap
#define glVertexAttribPointer tracked_attrib
#define memcpy tracked_copy
#include "gles_device_stream_functions.inc"
#undef glBufferData
#undef glMapBufferRange
#undef glUnmapBuffer
#undef glVertexAttribPointer
#undef memcpy

static int checks,failures;
#define CHECK(c,label) do{++checks;if(!(c)){++failures;printf("FAIL: %s\n",label);}}while(0)
static GLuint vao,vbo,ebo,program;
static GLuint shader(GLenum type,const char* source){
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,NULL);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);CHECK(ok,"shader compile");return s;
}
static void begin(int legacy){
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    if(legacy)pc_gx_vertex_stream_base(0);else pc_gx_vertex_stream_new_frame();
    upload_ns=0;upload_bytes=0;allocations=uploads=0;
    map_ns=unmap_ns=attrib_ns=copy_ns=data_ns=0;
}
static void upload(int legacy,const PCGXVertex* vertices,int count){
    if(legacy){
        Uint64 t=ticks();tracked_buffer_data(GL_ARRAY_BUFFER,count*sizeof(*vertices),vertices,GL_STREAM_DRAW);
        pc_profiler_add_time(0,t);pc_profiler_add_count_buffer_upload(count*sizeof(*vertices));
    }else pc_gx_buffer_data_profiled(GL_ARRAY_BUFFER,count*sizeof(*vertices),vertices,GL_STREAM_DRAW);
}
static int vertices_for(PCGXVertex* vertices,int kind,int cell){
    static const int index[4][6]={{0,1,2,0,2,3},{0,1,3,2,0,0},{0,1,2,3,0,0},{0,1,2,3,0,0}};
    const float corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    int count=kind==0?6:4;
    memset(vertices,0,count*sizeof(*vertices));
    for(int i=0;i<count;++i){
        int corner=index[kind][i];
        vertices[i].position[0]=-0.75f+(cell%4)*0.5f+corners[corner][0]*0.20f;
        vertices[i].position[1]=cell<4?-0.5f:0.5f;vertices[i].position[1]+=corners[corner][1]*0.4f;
        vertices[i].normal[0]=0.1f*(cell+1);vertices[i].normal[1]=0.25f;
        vertices[i].color0[0]=20+cell*20;vertices[i].color0[1]=150-cell*12;
        vertices[i].color0[2]=80+cell*10;vertices[i].color0[3]=255;
        vertices[i].texcoord[0][0]=0.15f*corner;vertices[i].texcoord[0][1]=0.1f*cell;
    }
    return count;
}
static void draw(int kind,int count){
    if(kind==3)glDrawElements(GL_TRIANGLES,(count/4)*6,GL_UNSIGNED_SHORT,NULL);
    else glDrawArrays(kind==0?GL_TRIANGLES:kind==1?GL_TRIANGLE_STRIP:GL_TRIANGLE_FAN,0,count);
}
static void parity(int legacy,int wrap,unsigned char* pixels){
    begin(legacy);glViewport(0,0,256,128);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    for(int cell=0;cell<8;++cell){
        PCGXVertex vertices[6];int count=vertices_for(vertices,cell%4,cell);
        if(!legacy&&wrap&&cell==2)s_pc_vertex_stream_cursor=PC_GX_VERTEX_STREAM_BYTES-count*sizeof(*vertices);
        upload(legacy,vertices,count);draw(cell%4,count);
        if(!legacy&&wrap&&cell==2)CHECK(s_pc_vertex_stream_cursor==PC_GX_VERTEX_STREAM_BYTES,"exact end-of-store upload");
        if(!legacy&&wrap&&cell==3)CHECK(s_pc_vertex_stream_cursor==count*sizeof(*vertices),"wrapped batch starts at zero");
    }
    glReadPixels(0,0,256,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    CHECK(glGetError()==GL_NO_ERROR,"all primitive uploads/draws/readback valid");
    CHECK(uploads==8,"all eight batches uploaded");
    CHECK(allocations==(legacy?8:wrap?2:1),"bounded allocations including wrap");
}
static int compare_double(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}
static void benchmark(int legacy){
    enum{N=12,BATCHES=3750};double frame[N],times[N];
    PCGXVertex vertices[30];memset(vertices,0,sizeof(vertices));
    for(int i=0;i<30;++i){
        vertices[i].position[0]=(i%3==0?-0.012f:0.012f);
        vertices[i].position[1]=(i%3==2?0.012f:-0.012f);
        vertices[i].normal[1]=1;vertices[i].color0[0]=50;vertices[i].color0[1]=120;vertices[i].color0[2]=180;vertices[i].color0[3]=255;
    }
    glViewport(0,0,256,128);
    for(int f=-2;f<N;++f){
        begin(legacy);glClear(GL_COLOR_BUFFER_BIT);glFinish();Uint64 start=ticks();
        for(int batch=0;batch<BATCHES;++batch){int kind=batch%4,count=kind==3?28:30;upload(legacy,vertices,count);draw(kind,count);}
        glFinish();
        if(f>=0){frame[f]=(ticks()-start)/1000000.0;times[f]=upload_ns/1000000.0;}
        CHECK(allocations==(legacy?BATCHES:1),"benchmark allocation count");
        CHECK(uploads==BATCHES,"benchmark upload count");
        CHECK(glGetError()==GL_NO_ERROR,"benchmark no GL error");
    }
    double sum=0,up=0;for(int i=0;i<N;++i){sum+=frame[i];up+=times[i];}
    qsort(frame,N,sizeof(double),compare_double);qsort(times,N,sizeof(double),compare_double);
    printf("STREAM_PERF %s frames=%d draws=%d bytes=%zu allocs=%u upload_mean_ms=%.3f upload_median_ms=%.3f total_mean_ms=%.3f total_median_ms=%.3f\n",
           legacy?"legacy":"append",N,BATCHES,upload_bytes,allocations,up/N,times[N/2],sum/N,frame[N/2]);
    printf("STREAM_BREAKDOWN last_frame map_ms=%.3f unmap_ms=%.3f attrib_ms=%.3f memcpy_ms=%.3f data_ms=%.3f\n",map_ns/1e6,unmap_ns/1e6,attrib_ns/1e6,copy_ns/1e6,data_ns/1e6);
}
int main(void){
    if(!device_context_begin(256,128))return 2;
    const char* vs="#version 300 es\nprecision highp float;layout(location=0) in vec3 pos;layout(location=1) in vec3 normal;layout(location=2) in vec4 col;layout(location=3) in vec2 uv;out vec4 color;void main(){gl_Position=vec4(pos,1);color=col+vec4(normal*0.1,0)+vec4(uv*0.1,0,0);}";
    const char* fs="#version 300 es\nprecision highp float;in vec4 color;out vec4 frag;void main(){frag=color;}";
    GLuint vert=shader(GL_VERTEX_SHADER,vs),frag=shader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,vert);glAttachShader(program,frag);glLinkProgram(program);glUseProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);CHECK(linked,"program link");
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    for(int a=0;a<4;++a)glEnableVertexAttribArray(a);
    GLushort indices[42];for(int i=0;i<7;++i){int b=i*4;GLushort q[6]={b,b+1,b+2,b,b+2,b+3};memcpy(indices+i*6,q,sizeof(q));}
    glGenBuffers(1,&ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);
    unsigned char old[256*128*4],fresh[sizeof(old)],wrapped[sizeof(old)];
    parity(1,0,old);parity(0,0,fresh);parity(0,1,wrapped);
    CHECK(memcmp(old,fresh,sizeof(old))==0,"append output pixel-identical to old uploads");
    CHECK(memcmp(old,wrapped,sizeof(old))==0,"wrap preserves prior draws and quad indices");
    int visible=0;for(size_t i=0;i<sizeof(old);i+=4)visible+=old[i]>0;
    CHECK(visible>20000,"all colored geometry was actually drawn");
    CHECK(device_save_bmp("stream-parity.bmp",256,128,fresh),"parity image saved");
    /* Consecutive frames reusing the same GL object must orphan before writes. */
    parity(0,0,fresh);CHECK(memcmp(old,fresh,sizeof(old))==0,"new-frame orphan preserves output");
    for(int repeat=0;repeat<20;++repeat){
        parity(0,1,wrapped);CHECK(memcmp(old,wrapped,sizeof(old))==0,"repeated wrap preserves queued earlier draws");
    }
    fail_next_map=1;parity(0,1,fresh);CHECK(memcmp(old,fresh,sizeof(old))==0,"mapping failure fallback preserves output");
    fail_next_unmap=1;parity(0,1,fresh);CHECK(memcmp(old,fresh,sizeof(old))==0,"unmap failure fallback preserves output");
    benchmark(1);benchmark(0);
    printf("STREAM_TEST %d checks, %d failures, vertex_stride=%zu\n",checks,failures,sizeof(PCGXVertex));
    glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ebo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(vert);glDeleteShader(frag);
    device_context_end();return failures?1:0;
}
