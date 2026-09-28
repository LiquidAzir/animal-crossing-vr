/* Extracted production write/rotate/publish path, synthetic GCI-sized bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
enum { FALSE=0,TRUE=1,PC_SAVE_MAX_BACKUPS=3,GCI_HEADER_SIZE=64,GCI_FILE_DATA_SIZE=466944 };
static unsigned checks,failures,renames,removes,write_calls;
static int fail_flush,fail_close,fail_write;
#define CHECK(c) do{++checks;if(!(c)){++failures;printf("FAIL line %u: %s\n",__LINE__,#c);}}while(0)
#define OSReport(...) ((void)0)
static size_t test_write(const void* p,size_t size,size_t count,FILE* f){
    ++write_calls;if(fail_write&&!--fail_write){errno=ENOSPC;return 0;}return fwrite(p,size,count,f);
}
static int test_flush(FILE* f){if(fail_flush){fail_flush=0;errno=ENOSPC;return EOF;}return fflush(f);}
static int test_close(FILE* f){int r=fclose(f);if(fail_close){fail_close=0;errno=EIO;return EOF;}return r;}
static int test_rename(const char* a,const char* b){++renames;return rename(a,b);}
static int test_remove(const char* p){++removes;return remove(p);}
#define fwrite test_write
#define fflush test_flush
#define fclose test_close
#define rename test_rename
#define remove test_remove
#include "village_save_actual.inc"
#undef fwrite
#undef fflush
#undef fclose
#undef rename
#undef remove
static void write_previous(const char* path,unsigned seed){
    FILE* f=fopen(path,"wb");unsigned char bytes[256];
    for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)(seed+i);
    CHECK(f!=NULL);if(f){CHECK(fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));CHECK(fclose(f)==0);}
}
static void check_previous(const char* path,unsigned seed){
    FILE* f=fopen(path,"rb");unsigned char bytes[257];CHECK(f!=NULL);if(!f)return;
    CHECK(fread(bytes,1,sizeof(bytes),f)==256);
    int same=1;for(unsigned i=0;i<256;++i)same&=bytes[i]==(unsigned char)(seed+i);
    CHECK(same);CHECK(fclose(f)==0);
}
int main(void){
    for(int scenario=0;scenario<5;++scenario){
        char current[64],temp[64],backup[3][80];struct stat st;
        snprintf(current,sizeof(current),"scenario-%d.gci",scenario);
        snprintf(temp,sizeof(temp),"scenario-%d.tmp",scenario);
        write_previous(current,16);
        for(int i=0;i<3;++i){snprintf(backup[i],sizeof(backup[i]),"%s.bak%d",current,i+1);write_previous(backup[i],32+i*16);}
        renames=removes=write_calls=0;
        if(scenario==1)fail_write=1;
        if(scenario==2)fail_write=2;
        if(scenario==3)fail_flush=1;
        if(scenario==4)fail_close=1;
        int result=write_test_gci(current,temp);
        CHECK(result==(scenario==0));CHECK(stat(temp,&st)!=0);
        if(scenario){
            CHECK(renames==0);CHECK(removes==1);check_previous(current,16);
            for(int i=0;i<3;++i)check_previous(backup[i],32+i*16);
        }else{
            FILE* f=fopen(current,"rb");CHECK(f!=NULL);
            CHECK(stat(current,&st)==0&&st.st_size==GCI_HEADER_SIZE+GCI_FILE_DATA_SIZE);
            int same=1;
            if(f){
                for(int i=0;i<GCI_HEADER_SIZE;++i)same&=fgetc(f)==(unsigned char)(i+3);
                for(int i=0;i<GCI_FILE_DATA_SIZE;++i)same&=fgetc(f)==(unsigned char)(i*17+5);
                CHECK(fgetc(f)==EOF);CHECK(fclose(f)==0);
            }
            CHECK(same);check_previous(backup[0],16);check_previous(backup[1],32);check_previous(backup[2],48);
        }
    }
    printf("Village save I/O checks=%u failures=%u\n",checks,failures);return failures!=0;
}
