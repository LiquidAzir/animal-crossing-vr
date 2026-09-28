/* Compile the actual CARD implementation with deterministic libc failures.
 * Every file lives in a fresh runner-created test working directory. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <strings.h>
#endif
#include <sys/stat.h>
static int fail_seek, fail_tell, short_write, fail_flush, fail_close, fail_rename, fail_remove;
static unsigned seeks,writes,flushes,closes,checks,failures;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("FAIL line %u: %s\n",__LINE__,#c);}}while(0)
static int injected_seek(FILE* f,long offset,int origin){
    ++seeks;if(fail_seek&&!--fail_seek){errno=EIO;return -1;}return fseek(f,offset,origin);
}
static long injected_tell(FILE* f){if(fail_tell){fail_tell=0;errno=EIO;return -1;}return ftell(f);}
static size_t injected_write(const void* p,size_t size,size_t count,FILE* f){
    ++writes;if(short_write){short_write=0;errno=EIO;return fwrite(p,size,count?count-1:0,f);}return fwrite(p,size,count,f);
}
static int injected_flush(FILE* f){++flushes;if(fail_flush){fail_flush=0;errno=EIO;return EOF;}return fflush(f);}
static int injected_close(FILE* f){++closes;int result=fclose(f);if(fail_close){fail_close=0;errno=EIO;return EOF;}return result;}
static int injected_rename(const char* a,const char* b){if(fail_rename){fail_rename=0;errno=EACCES;return -1;}return rename(a,b);}
static int injected_remove(const char* p){if(fail_remove){fail_remove=0;errno=EACCES;return -1;}return remove(p);}
#define fseek injected_seek
#define ftell injected_tell
#define fwrite injected_write
#define fflush injected_flush
#define fclose injected_close
#define rename injected_rename
#define remove injected_remove
#include "pc_card_actual.inc"
#undef fseek
#undef ftell
#undef fwrite
#undef fflush
#undef fclose
#undef rename
#undef remove
static int callback_calls,callback_result;
static void callback(s32 channel,s32 result){(void)channel;++callback_calls;callback_result=result;}
static void verify_data(const unsigned char* expected,size_t size){
    unsigned char actual[8192];FILE* f=fopen("save/card_a/roundtrip.gci","rb");CHECK(f!=NULL);
    if(!f)return;
    CHECK(fread(actual,1,size,f)==size);CHECK(!memcmp(actual,expected,size));CHECK(fclose(f)==0);
}
int main(void){
    CARDFileInfo_PC file={0};unsigned char expected[8192]={0},buffer[8193],patch[512];
    for(unsigned i=0;i<sizeof(patch);++i)patch[i]=(unsigned char)i;
    CARDInit();CHECK(CARDMount(0,NULL,NULL)==CARD_RESULT_READY);
    CHECK(CARDCreate(0,"roundtrip.gci",sizeof(expected),&file)==CARD_RESULT_READY);
    CHECK(CARDWrite(&file,patch,sizeof(patch),137)==CARD_RESULT_READY);memcpy(expected+137,patch,sizeof(patch));
    CHECK(CARDRead(&file,buffer,sizeof(expected),0)==CARD_RESULT_READY);CHECK(!memcmp(buffer,expected,sizeof(expected)));
    CHECK(CARDClose(&file)==CARD_RESULT_READY);verify_data(expected,sizeof(expected));
    CHECK(CARDOpen(0,"roundtrip.gci",&file)==CARD_RESULT_READY);CHECK(file.length==sizeof(expected));
    unsigned before_writes=writes;fail_seek=1;
    CHECK(CARDWrite(&file,patch,16,7)==CARD_RESULT_IOERROR);CHECK(writes==before_writes);verify_data(expected,sizeof(expected));
    fail_seek=1;CHECK(CARDRead(&file,buffer,16,7)==CARD_RESULT_IOERROR);
    CHECK(CARDRead(&file,buffer,sizeof(buffer),0)==CARD_RESULT_IOERROR);
    before_writes=writes;
    CHECK(CARDWrite(&file,patch,-1,0)==CARD_RESULT_IOERROR);
    CHECK(CARDWrite(&file,patch,1,-1)==CARD_RESULT_IOERROR);
    CHECK(CARDWrite(&file,NULL,1,0)==CARD_RESULT_IOERROR);CHECK(writes==before_writes);
    CHECK(CARDRead(&file,buffer,-1,0)==CARD_RESULT_IOERROR);
    CHECK(CARDRead(&file,buffer,1,-1)==CARD_RESULT_IOERROR);
    CHECK(CARDRead(&file,NULL,1,0)==CARD_RESULT_IOERROR);
    short_write=1;CHECK(CARDWrite(&file,patch,16,7)==CARD_RESULT_IOERROR);
    fail_flush=1;CHECK(CARDWrite(&file,patch,16,7)==CARD_RESULT_IOERROR);
    fail_flush=1;callback_calls=0;
    CHECK(CARDWriteAsync(&file,patch,16,7,(void*)callback)==CARD_RESULT_IOERROR);
    CHECK(callback_calls==1&&callback_result==CARD_RESULT_IOERROR);
    fail_seek=1;callback_calls=0;
    CHECK(CARDReadAsync(&file,buffer,16,7,(void*)callback)==CARD_RESULT_IOERROR);
    CHECK(callback_calls==1&&callback_result==CARD_RESULT_IOERROR);
    fail_close=1;CHECK(CARDClose(&file)==CARD_RESULT_IOERROR);CHECK(card_slot_find(&file)==NULL);
    CHECK(CARDClose(&file)==CARD_RESULT_READY);CHECK(CARDOpen(0,"roundtrip.gci",&file)==CARD_RESULT_READY);
    CHECK(CARDClose(&file)==CARD_RESULT_READY);
    for(int fault=0;fault<3;++fault){
        if(fault<2)fail_seek=fault+1;else fail_tell=1;
        CHECK(CARDOpen(0,"roundtrip.gci",&file)==CARD_RESULT_IOERROR);CHECK(card_slot_find(&file)==NULL);
        CHECK(CARDOpen(0,"roundtrip.gci",&file)==CARD_RESULT_READY);CHECK(CARDClose(&file)==CARD_RESULT_READY);
    }
    for(int fault=0;fault<3;++fault){
        if(fault==0)short_write=1;
        if(fault==1)fail_flush=1;
        if(fault==2)fail_seek=1;
        CHECK(CARDCreate(0,"create-fault.gci",128,&file)==CARD_RESULT_IOERROR);CHECK(card_slot_find(&file)==NULL);
    }
    fail_flush=1;callback_calls=0;
    CHECK(CARDCreateAsync(0,"create-fault.gci",128,&file,(void*)callback)==CARD_RESULT_IOERROR);
    CHECK(callback_calls==1&&callback_result==CARD_RESULT_IOERROR);
    CHECK(CARDDelete(0,"create-fault.gci")==CARD_RESULT_READY);
    fail_rename=1;callback_calls=0;
    CHECK(CARDRenameAsync(0,"roundtrip.gci","renamed.gci",(void*)callback)==CARD_RESULT_IOERROR);
    CHECK(callback_calls==1&&callback_result==CARD_RESULT_IOERROR);
    CHECK(CARDRename(0,"roundtrip.gci","renamed.gci")==CARD_RESULT_READY);
    CHECK(CARDRename(0,"roundtrip.gci","absent.gci")==CARD_RESULT_NOFILE);
    fail_remove=1;callback_calls=0;
    CHECK(CARDDeleteAsync(0,"renamed.gci",(void*)callback)==CARD_RESULT_IOERROR);
    CHECK(callback_calls==1&&callback_result==CARD_RESULT_IOERROR);
    CHECK(CARDDelete(0,"renamed.gci")==CARD_RESULT_READY);CHECK(CARDDelete(0,"renamed.gci")==CARD_RESULT_NOFILE);
    CHECK(CARDOpen(0,"renamed.gci",&file)==CARD_RESULT_NOFILE);
    for(unsigned i=0;i<CARD_MAX_OPEN;++i)CHECK(card_slots[i].owner==NULL&&card_slots[i].fp==NULL);
    printf("CARD I/O checks=%u failures=%u seeks=%u writes=%u flushes=%u closes=%u\n",checks,failures,seeks,writes,flushes,closes);
    return failures!=0;
}
