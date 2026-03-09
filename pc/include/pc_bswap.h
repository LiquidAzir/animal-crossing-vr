/* pc_bswap.h - Byte-swap helpers for BE data from ARAM/ROM/GCI */
#ifndef PC_BSWAP_H
#define PC_BSWAP_H

#define pc_bswap16(x) __builtin_bswap16(x)
#define pc_bswap32(x) __builtin_bswap32(x)
#define pc_bswap64(x) __builtin_bswap64(x)

static inline void pc_bswap16_array(void* ptr, int count) {
    unsigned short* arr = (unsigned short*)ptr;
    int i;
    for (i = 0; i < count; i++) {
        arr[i] = __builtin_bswap16(arr[i]);
    }
}

static inline void pc_bswap32_array(void* ptr, int count) {
    unsigned int* arr = (unsigned int*)ptr;
    int i;
    for (i = 0; i < count; i++) {
        arr[i] = __builtin_bswap32(arr[i]);
    }
}

#endif /* PC_BSWAP_H */
