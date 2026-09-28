/* Experimental only: EXT_buffer_storage upload comparison. Not game code.
 * Reuses the production append uploader extracted by gles_device_run.py.
 * Spec: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_buffer_storage.txt
 * Coherent writes are visible to subsequent commands; page reuse is fenced. */
#define main append_test_main
#include "gles_device_stream.c"
#undef main
#include <GLES2/gl2ext.h>

enum { PAGE_COUNT = 3 };
static GLuint persistent_vbo;
static unsigned char* persistent_map;
static GLsync page_fence[PAGE_COUNT];
static int page = -1;
static size_t page_cursor;
static unsigned page_advances, page_waits;
static double fence_wait_ns;

static void persistent_next_page(void) {
    if (page >= 0) {
        CHECK(page_fence[page] == NULL, "used page has no previous live fence");
        page_fence[page] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        CHECK(page_fence[page] != NULL, "submitted page receives a fence");
    }
    page = (page + 1) % PAGE_COUNT;
    if (page_fence[page]) {
        Uint64 start = ticks();
        GLenum result = GL_TIMEOUT_EXPIRED;
        for (int tries = 0; tries < 5 && result == GL_TIMEOUT_EXPIRED; ++tries)
            result = glClientWaitSync(page_fence[page], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
        fence_wait_ns += ticks() - start;
        CHECK(result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED,
              "page reuse waits for all prior GPU reads");
        if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) exit(3);
        glDeleteSync(page_fence[page]); page_fence[page] = NULL; ++page_waits;
    }
    page_cursor = 0; ++page_advances;
}

static void persistent_begin(void) {
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, persistent_vbo);
    upload_ns = 0; upload_bytes = 0; uploads = allocations = 0;
    fence_wait_ns = 0;
    persistent_next_page();
}

static void persistent_upload(const PCGXVertex* vertices, int count) {
    Uint64 start = ticks();
    size_t bytes = count * sizeof(*vertices);
    CHECK(bytes <= PC_GX_VERTEX_STREAM_BYTES, "batch fits a page");
    if (bytes > PC_GX_VERTEX_STREAM_BYTES - page_cursor) persistent_next_page();
    size_t offset = (size_t)page * PC_GX_VERTEX_STREAM_BYTES + page_cursor;
    memcpy(persistent_map + offset, vertices, bytes);
    pc_gx_vertex_stream_base(offset);
    page_cursor += bytes;
    upload_ns += ticks() - start; upload_bytes += bytes; ++uploads;
}

static void persistent_parity(int wrap, unsigned char* pixels) {
    persistent_begin(); glViewport(0,0,256,128); glClear(GL_COLOR_BUFFER_BIT);
    for (int cell = 0; cell < 8; ++cell) {
        PCGXVertex vertices[6]; int count = vertices_for(vertices,cell%4,cell);
        if (wrap && cell == 0) page_cursor = PC_GX_VERTEX_STREAM_BYTES - count*sizeof(*vertices);
        if (wrap && cell > 1) page_cursor = PC_GX_VERTEX_STREAM_BYTES;
        persistent_upload(vertices,count); draw(cell%4,count);
        if (wrap && cell == 0) CHECK(page_cursor == PC_GX_VERTEX_STREAM_BYTES, "persistent exact page boundary");
        if (wrap && cell > 0) CHECK(page_cursor == count*sizeof(*vertices), "persistent wrapped batch starts in next page");
    }
    glReadPixels(0,0,256,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    CHECK(glGetError() == GL_NO_ERROR, "persistent parity GL error free");
}

static void persistent_benchmark(void) {
    enum { N=12, BATCHES=3750 }; double total=0, upload_total=0, wait_total=0;
    PCGXVertex vertices[30]; memset(vertices,0,sizeof(vertices));
    for (int i=0; i<30; ++i) {
        vertices[i].position[0]=(i%3==0?-0.012f:0.012f);
        vertices[i].position[1]=(i%3==2?0.012f:-0.012f);
        vertices[i].normal[1]=1; vertices[i].color0[0]=50;
        vertices[i].color0[1]=120; vertices[i].color0[2]=180; vertices[i].color0[3]=255;
    }
    glViewport(0,0,256,128);
    for (int frame=-2; frame<N; ++frame) {
        persistent_begin(); glClear(GL_COLOR_BUFFER_BIT); glFinish(); Uint64 start=ticks();
        for (int batch=0; batch<BATCHES; ++batch) {
            int kind=batch%4,count=kind==3?28:30;
            persistent_upload(vertices,count); draw(kind,count);
        }
        glFinish();
        if (frame>=0) { total+=(ticks()-start)/1e6; upload_total+=upload_ns/1e6; wait_total+=fence_wait_ns/1e6; }
        CHECK(uploads==BATCHES, "persistent benchmark upload count");
        CHECK(glGetError()==GL_NO_ERROR, "persistent benchmark GL error free");
    }
    printf("PERSISTENT_PERF frames=%d draws=%d bytes=%zu upload_mean_ms=%.3f total_mean_ms=%.3f fence_wait_mean_ms=%.3f pages=%d bytes_reserved=%u\n",
           N,BATCHES,upload_bytes,upload_total/N,total/N,wait_total/N,PAGE_COUNT,PAGE_COUNT*PC_GX_VERTEX_STREAM_BYTES);
}

int main(void) {
    if (!device_context_begin(256,128)) return 2;
    GLint extensions=0; int supported=0; glGetIntegerv(GL_NUM_EXTENSIONS,&extensions);
    for (int i=0;i<extensions;++i) if (!strcmp((const char*)glGetStringi(GL_EXTENSIONS,i),"GL_EXT_buffer_storage")) supported=1;
    PFNGLBUFFERSTORAGEEXTPROC storage=(PFNGLBUFFERSTORAGEEXTPROC)eglGetProcAddress("glBufferStorageEXT");
    if (!supported || !storage) { puts("Persistent storage unavailable"); return 2; }
    const char* vs="#version 300 es\nprecision highp float;layout(location=0) in vec3 pos;layout(location=1) in vec3 normal;layout(location=2) in vec4 col;layout(location=3) in vec2 uv;out vec4 color;void main(){gl_Position=vec4(pos,1);color=col+vec4(normal*0.1,0)+vec4(uv*0.1,0,0);}";
    const char* fs="#version 300 es\nprecision highp float;in vec4 color;out vec4 frag;void main(){frag=color;}";
    GLuint vert=shader(GL_VERTEX_SHADER,vs),frag=shader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram(); glAttachShader(program,vert); glAttachShader(program,frag); glLinkProgram(program); glUseProgram(program);
    GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked); CHECK(linked,"program link");
    glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    for (int a=0;a<4;++a) glEnableVertexAttribArray(a);
    glGenBuffers(1,&vbo); glGenBuffers(1,&persistent_vbo); glBindBuffer(GL_ARRAY_BUFFER,persistent_vbo);
    GLbitfield flags=GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT_EXT|GL_MAP_COHERENT_BIT_EXT;
    storage(GL_ARRAY_BUFFER,PAGE_COUNT*PC_GX_VERTEX_STREAM_BYTES,NULL,flags);
    persistent_map=glMapBufferRange(GL_ARRAY_BUFFER,0,PAGE_COUNT*PC_GX_VERTEX_STREAM_BYTES,flags);
    CHECK(persistent_map!=NULL,"coherent persistent map created");
    if (!persistent_map) return 2;
    GLushort indices[42]; for(int i=0;i<7;++i){int b=i*4;GLushort q[6]={b,b+1,b+2,b,b+2,b+3};memcpy(indices+i*6,q,sizeof(q));}
    glGenBuffers(1,&ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_DITHER); glClearColor(0,0,0,1);
    unsigned char expected[256*128*4], actual[sizeof(expected)];
    parity(0,0,expected);
    persistent_parity(0,actual); CHECK(!memcmp(expected,actual,sizeof(expected)),"persistent image matches production append path");
    for (int repeat=0;repeat<20;++repeat) {
        persistent_parity(1,actual);
        CHECK(!memcmp(expected,actual,sizeof(expected)),"three-page wrap/reuse preserves all queued primitives");
    }
    CHECK(page_waits>100,"stress test reused fenced pages within frames");
    CHECK(device_save_bmp("persistent-parity.bmp",256,128,actual),"persistent image saved");
    benchmark(0); persistent_benchmark(); benchmark(0); persistent_benchmark();
    printf("PERSISTENT_TEST %d checks, %d failures, page_advances=%u page_waits=%u\n",checks,failures,page_advances,page_waits);
    glFinish(); glBindBuffer(GL_ARRAY_BUFFER,persistent_vbo); glUnmapBuffer(GL_ARRAY_BUFFER);
    for(int i=0;i<PAGE_COUNT;++i)if(page_fence[i])glDeleteSync(page_fence[i]);
    glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&persistent_vbo); glDeleteBuffers(1,&ebo);
    glDeleteVertexArrays(1,&vao); glDeleteProgram(program); glDeleteShader(vert); glDeleteShader(frag);
    device_context_end(); return failures?1:0;
}
