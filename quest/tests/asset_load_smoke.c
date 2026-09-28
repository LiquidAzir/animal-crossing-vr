/* Native ARM32 asset check. No SDL window, XR instance, game loop or saves. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { const char* name; uint32_t size, hash; } AssetExpectation;
static const AssetExpectation expected[] = {
#include "asset_expectations.inc"
};

static uint32_t hash_bytes(const unsigned char* data, uint32_t length) {
    uint32_t hash = 2166136261u;
    for (uint32_t i=0;i<length;++i) hash=(hash^data[i])*16777619u;
    return hash;
}
static double seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    return time.tv_sec+time.tv_nsec/1e9;
}
static void* symbol(void* library,const char* name) {
    dlerror();void* result=dlsym(library,name);const char* error=dlerror();
    if(error){fprintf(stderr,"Missing symbol %s: %s\n",name,error);exit(3);}
    return result;
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);setvbuf(stderr,NULL,_IONBF,0);
    printf("ASSET_SMOKE pointer_bits=%u; no window, XR instance, game loop or saves\n",(unsigned)(8*sizeof(void*)));
    if(sizeof(void*)!=4)return 2;
    double start=seconds();
    // Match QuestActivity.getLibraries(): Android's dependency constructors
    // must run before the game's intentionally separate JKR global new exists.
    const char* dependencies[]={"./libSDL2.so","./libopenxr_loader.so"};
    for(unsigned i=0;i<sizeof(dependencies)/sizeof(dependencies[0]);++i){
        if(!dlopen(dependencies[i],RTLD_NOW|RTLD_GLOBAL)){
            fprintf(stderr,"dependency dlopen: %s\n",dlerror());return 3;
        }
    }
    void* library=dlopen("./libmain.so",RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"dlopen: %s\n",dlerror());return 3;}
    printf("Shared library and dependencies loaded in %.3fs\n",seconds()-start);
    int(*disc_init)(void)=symbol(library,"pc_disc_init");
    int(*assets_init)(void)=symbol(library,"pc_assets_init");
    int(*disc_is_open)(void)=symbol(library,"pc_disc_is_open");
    void(*disc_shutdown)(void)=symbol(library,"pc_disc_shutdown");
    *(int*)symbol(library,"g_pc_verbose")=1;
    if(!disc_init()||!disc_is_open()){fprintf(stderr,"Disc initialization failed\n");return 4;}
    start=seconds();
    if(!assets_init()){fprintf(stderr,"Asset initialization failed\n");return 5;}
    printf("Native asset initialization completed in %.3fs\n",seconds()-start);
    unsigned failures=0, checked=0;uint64_t bytes=0;
    for(size_t i=0;i<sizeof(expected)/sizeof(expected[0]);++i){
        const AssetExpectation* e=&expected[i];
        const unsigned char* data=symbol(library,e->name);
        uint32_t actual=hash_bytes(data,e->size);
        ++checked;bytes+=e->size;
        if(actual!=e->hash){
            ++failures;
            fprintf(stderr,"Asset mismatch %s: bytes=%u expected=%08x actual=%08x\n",e->name,e->size,e->hash,actual);
        }
    }
    disc_shutdown();
    if(disc_is_open()){fprintf(stderr,"Disc remained open after shutdown\n");++failures;}
    printf("ASSET_SMOKE_RESULT checked=%u bytes=%llu failures=%u\n",checked,(unsigned long long)bytes,failures);
    // The game owns global destructors but its heaps were deliberately never
    // started. Process exit releases the library; do not invoke game shutdown.
    fflush(NULL);
    _Exit(failures?6:0);
}
