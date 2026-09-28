/* Generated field coverage lives in save_layout_fields.inc. No save-file I/O. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "m_common_data.h"
#include "m_quest.h"
#include "dolphin/card.h"
#include "pc_save_bswap.h"

static Save_t save_layout_scratch;
static CARDDir save_layout_card;
static mCD_keep_mail_c save_layout_mail;
static mCD_keep_original_c save_layout_original;
static mCD_keep_diary_c save_layout_diary;
static mCD_foreigner_c save_layout_foreigner;

/* The production swap unit reports optional verification diagnostics only. */
void OSReport(const char* format, ...) { (void)format; }

static void fill_pattern(void* memory, size_t size) {
    unsigned char* bytes = (unsigned char*)memory;
    unsigned int state = 0x4d535641u;
    size_t i;
    for (i = 0; i < size; ++i) {
        state = state * 1664525u + 1013904223u;
        bytes[i] = (unsigned char)(state >> 24);
    }
}

static void print_hash(const char* name, const void* memory, size_t size) {
    const unsigned char* bytes = (const unsigned char*)memory;
    unsigned int hash = 2166136261u;
    size_t i;
    for (i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    printf("payload.%s=%08x\n", name, hash);
}

static void print_bits(const char* name, const void* memory, size_t size) {
    const unsigned char* bytes = (const unsigned char*)memory;
    size_t i;
    printf("bits.%s=", name);
    for (i = 0; i < size; ++i) {
        if (bytes[i]) printf("%lu:%02x,", (unsigned long)i, bytes[i]);
    }
    putchar('\n');
}

#define TYPE(T) do { printf("size.%s=%lu\nalign.%s=%lu\n", #T, (unsigned long)sizeof(T), #T, (unsigned long)_Alignof(T)); } while (0)
#define FIELD(T, P) do { printf("offset.%s.%s=%lu\nfieldsize.%s.%s=%lu\n", #T, #P, (unsigned long)offsetof(T, P), #T, #P, (unsigned long)sizeof(((T*)0)->P)); } while (0)
#define BITS(T, S, P) do { memset(&(S), 0, sizeof(S)); (S).P = -1; print_bits(#T "." #P, &(S), sizeof(S)); } while (0)
#define SWAP(S, F) do { fill_pattern(&(S), sizeof(S)); print_hash(#S ".input", &(S), sizeof(S)); F(&(S), PC_BSWAP_FROM_BE); print_hash(#S ".from_be", &(S), sizeof(S)); F(&(S), PC_BSWAP_TO_BE); print_hash(#S ".to_be", &(S), sizeof(S)); } while (0)

int main(void) {
    TYPE(void*); TYPE(long); TYPE(u16); TYPE(u32); TYPE(u64); TYPE(OSTime);
    TYPE(Save); TYPE(CARDDir);
    printf("gci.header=%lu\ngci.main=%lu\ngci.backup=%lu\n", (unsigned long)sizeof(CARDDir), 0x26000ul, 0x26000ul + (unsigned long)sizeof(Save));
#include "save_layout_fields.inc"
    SWAP(save_layout_scratch, pc_save_bswap);
    SWAP(save_layout_mail, pc_save_bswap_keep_mail);
    SWAP(save_layout_original, pc_save_bswap_keep_original);
    SWAP(save_layout_diary, pc_save_bswap_keep_diary);
    SWAP(save_layout_foreigner, pc_save_bswap_foreigner);
    return 0;
}
