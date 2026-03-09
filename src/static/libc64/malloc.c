#include "libc64/__osMalloc.h"
#include "libc64/malloc.h"

OSArena malloc_arena;

extern void* malloc(size_t size) {
    return __osMalloc(&malloc_arena, size);
}

extern void free(void* ptr) {
    __osFree(&malloc_arena, ptr);
}

extern void DisplayArena(void) {
    __osDisplayArena(&malloc_arena);
}

#ifdef TARGET_PC
extern void GetFreeArena(size_t* max, size_t* free, size_t* alloc) {
    u32 m, f, a;
    __osGetFreeArena(&malloc_arena, &m, &f, &a);
    *max = m; *free = f; *alloc = a;
}
extern void MallocInit(void* start, size_t size) {
    __osMallocInit(&malloc_arena, start, (s32)size);
}
#else
extern void GetFreeArena(u32* max, u32* free, u32* alloc) {
    __osGetFreeArena(&malloc_arena, max, free, alloc);
}
extern void MallocInit(void* start, u32 size) {
    __osMallocInit(&malloc_arena, start, size);
}
#endif

extern void MallocCleanup(void) {
    __osMallocCleanup(&malloc_arena);
}
