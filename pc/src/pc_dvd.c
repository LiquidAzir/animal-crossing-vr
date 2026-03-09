/* pc_dvd.c - DVD filesystem replaced with file I/O from extracted assets */
#include "pc_platform.h"

typedef struct {
    char gameName[4];
    char company[2];
    u8   diskNumber;
    u8   gameVersion;
    u8   streaming;
    u8   streamBufSize;
    u8   padding[22];
} DVDDiskID;

static DVDDiskID disk_id = {
    {'G', 'A', 'F', 'E'},
    {'0', '1'},
    0, 0,
    0, 0,
    {0}
};

DVDDiskID* DVDGetCurrentDiskID(void) { return &disk_id; }

#define MAX_DVD_ENTRIES 512

static struct {
    char path[256];
    int  used;
} dvd_entry_table[MAX_DVD_ENTRIES];
static int dvd_entry_count = 0;

static char assets_base_path[512] = {0};

static void dvd_init_base_path(void) {
    if (assets_base_path[0] != 0) return;

    /* probe for extracted game files by checking for a known file */
    const char* candidates[] = {
        "assets/files",
        "assets",
        "../assets/files",
        "../assets",
        "../../assets/files",
        "../../assets",
    };
    for (int i = 0; i < (int)(sizeof(candidates)/sizeof(candidates[0])); i++) {
        char test[768];
        snprintf(test, sizeof(test), "%s/COPYDATE", candidates[i]);
        FILE* f = fopen(test, "rb");
        if (f) {
            fclose(f);
            strncpy(assets_base_path, candidates[i], sizeof(assets_base_path)-1);
            assets_base_path[sizeof(assets_base_path)-1] = '\0';
            return;
        }
    }
    strncpy(assets_base_path, "assets", sizeof(assets_base_path)-1);
    assets_base_path[sizeof(assets_base_path)-1] = '\0';
}

s32 DVDConvertPathToEntrynum(const char* path) {
    dvd_init_base_path();

    for (int i = 0; i < dvd_entry_count; i++) {
        if (dvd_entry_table[i].used && strcmp(dvd_entry_table[i].path, path) == 0) {
            return i;
        }
    }

    if (dvd_entry_count >= MAX_DVD_ENTRIES) {
        fprintf(stderr, "[PC/DVD] Entry table full (%d entries)! Cannot register: %s\n",
                MAX_DVD_ENTRIES, path);
        return -1;
    }

    int idx = dvd_entry_count++;
    strncpy(dvd_entry_table[idx].path, path, sizeof(dvd_entry_table[idx].path) - 1);
    dvd_entry_table[idx].path[sizeof(dvd_entry_table[idx].path) - 1] = '\0';
    dvd_entry_table[idx].used = 1;
    return idx;
}

/* DVDFileInfo: 0x3C bytes. We store FILE* in the DVDCommandBlock area at offset 0x18. */
static FILE** dvd_fi_fp(void* fileInfo) {
    return (FILE**)((u8*)fileInfo + 0x18);
}
static u32* dvd_fi_length(void* fileInfo) {
    return (u32*)((u8*)fileInfo + 0x34);
}
static u32* dvd_fi_startAddr(void* fileInfo) {
    return (u32*)((u8*)fileInfo + 0x30);
}

BOOL DVDFastOpen(s32 entrynum, void* fileInfo) {
    dvd_init_base_path();

    if (entrynum < 0 || entrynum >= dvd_entry_count || !dvd_entry_table[entrynum].used) {
        return FALSE;
    }

    char fullpath[768];
    const char* path = dvd_entry_table[entrynum].path;
    if (path[0] == '/') {
        snprintf(fullpath, sizeof(fullpath), "%s%s", assets_base_path, path);
    } else {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", assets_base_path, path);
    }

    FILE* fp = fopen(fullpath, "rb");
    if (!fp) {
        return FALSE;
    }

    fseek(fp, 0, SEEK_END);
    u32 len = (u32)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    memset(fileInfo, 0, 0x3C);
    *dvd_fi_fp(fileInfo) = fp;
    *dvd_fi_startAddr(fileInfo) = 0;
    *dvd_fi_length(fileInfo) = len;

    return TRUE;
}

BOOL DVDOpen(const char* filename, void* fileInfo) {
    s32 entry = DVDConvertPathToEntrynum(filename);
    if (entry < 0) return FALSE;
    return DVDFastOpen(entry, fileInfo);
}

BOOL DVDClose(void* fileInfo) {
    FILE* fp = *dvd_fi_fp(fileInfo);
    if (fp) {
        fclose(fp);
        *dvd_fi_fp(fileInfo) = NULL;
    }
    return TRUE;
}

s32 DVDReadPrio(void* fileInfo, void* buf, s32 length, s32 offset, s32 prio) {
    FILE* fp = *dvd_fi_fp(fileInfo);
    (void)prio;

    if (!fp) {
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    size_t nread = fread(buf, 1, length, fp);
    return (s32)nread;
}

s32 DVDRead(void* fileInfo, void* buf, s32 length, s32 offset) {
    return DVDReadPrio(fileInfo, buf, length, offset, 2);
}

u32 DVDGetLength(void* fileInfo) {
    return *dvd_fi_length(fileInfo);
}

typedef void (*pc_DVDCallback)(s32, void*);

BOOL DVDReadAsyncPrio(void* fileInfo, void* buf, s32 length, s32 offset,
                      pc_DVDCallback callback, s32 prio) {
    s32 nread = DVDReadPrio(fileInfo, buf, length, offset, prio);
    if (callback) {
        callback(nread, fileInfo);
    }
    return TRUE;
}

void OSDVDFatalError(void) {
    fprintf(stderr, "[PC/DVD] Fatal DVD error\n");
}

void DVDInit(void) {
    dvd_init_base_path();
}

void DVDSetAutoFatalMessaging(BOOL enable) { (void)enable; }

s32 DVDGetFileInfoStatus(void* fileInfo) {
    (void)fileInfo;
    return 0;
}

s32 DVDGetTransferredSize(void* fileInfo) {
    (void)fileInfo;
    return 0;
}

BOOL DVDFastClose(void* fileInfo) {
    return DVDClose(fileInfo);
}

s32 DVDGetDriveStatus(void) { return 0; }
s32 DVDCancel(void* block) { (void)block; return 0; }
BOOL DVDCancelAsync(void* block, void* callback) { (void)block; (void)callback; return TRUE; }
s32 DVDChangeDisk(void* block, void* id) { (void)block; (void)id; return 0; }
BOOL DVDChangeDiskAsync(void* block, void* id, void* callback) { (void)block; (void)id; (void)callback; return TRUE; }
s32 DVDGetCommandBlockStatus(void* block) { (void)block; return 0; }

BOOL DVDPrepareStreamAsync(void* fi, u32 len, u32 off, void* cb) {
    (void)fi; (void)len; (void)off; (void)cb;
    return TRUE;
}
s32 DVDCancelStream(void* block) { (void)block; return 0; }
