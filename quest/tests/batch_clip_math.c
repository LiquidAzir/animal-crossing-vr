#include "quest_batch_clip.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { if (failures++ < 12) \
    printf("FAIL line %u: %s\n", __LINE__, #c); } } while (0)
typedef struct { float position[3]; unsigned char padding[84]; } TestVertex;
static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static unsigned seed = 0x643BC219u;
static float random_float(void) { seed=seed*1664525u+1013904223u; return (float)(seed>>8)/16777216.0f; }

static unsigned reference_outside(const float* matrix, const TestVertex* vertices, unsigned count) {
    unsigned common=63;
    for (unsigned i=0;i<count;++i) {
        double v[4];
        unsigned mask=0;
        for (int r=0;r<4;++r) v[r]=(double)matrix[r*4]*vertices[i].position[0]+
            (double)matrix[r*4+1]*vertices[i].position[1]+
            (double)matrix[r*4+2]*vertices[i].position[2]+matrix[r*4+3];
        for (int r=0;r<3;++r) {
            if(v[r]<-v[3]) mask|=1u<<(r*2);
            if(v[r]> v[3]) mask|=2u<<(r*2);
        }
        common&=mask;
    }
    return common;
}

static unsigned reference_two_stage(const float* projection,const float* mv,
                                    const TestVertex* vertices,unsigned count) {
    unsigned common=63;
    for(unsigned i=0;i<count;++i){
        double eye[4]={0,0,0,1},clip[4];unsigned mask=0;
        for(int r=0;r<3;++r)eye[r]=(double)mv[r*4]*vertices[i].position[0]+
            (double)mv[r*4+1]*vertices[i].position[1]+(double)mv[r*4+2]*vertices[i].position[2]+mv[r*4+3];
        for(int r=0;r<4;++r)clip[r]=(double)projection[r*4]*eye[0]+
            (double)projection[r*4+1]*eye[1]+(double)projection[r*4+2]*eye[2]+projection[r*4+3];
        for(int r=0;r<3;++r){
            if(clip[r]<-clip[3])mask|=1u<<(r*2);
            if(clip[r]>clip[3])mask|=2u<<(r*2);
        }
        common&=mask;
    }
    return common;
}

int main(void) {
    TestVertex v[4]={0};
    float matrix[16];
    CHECK(sizeof(TestVertex)==96);
    for(int axis=0;axis<3;++axis) for(int sign=-1;sign<=1;sign+=2) {
        memset(v,0,sizeof(v));
        for(int i=0;i<4;++i) v[i].position[axis]=sign*2.0f;
        CHECK(quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),4));
        for(int i=0;i<4;++i) v[i].position[axis]=sign*(1.0f+FLT_EPSILON);
        CHECK(!quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),4));
        v[3].position[axis]=0;
        CHECK(!quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),4));
    }
    memset(v,0,sizeof(v));
    v[0].position[0]=-3;v[1].position[0]=3;v[2].position[1]=3;
    CHECK(!quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),3));
    CHECK(!quest_batch_clip_outside(NULL,identity,v,sizeof(v[0]),3));
    CHECK(!quest_batch_clip_outside(identity,identity,NULL,sizeof(v[0]),3));
    CHECK(!quest_batch_clip_outside(identity,identity,v,8,3));
    CHECK(!quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),0));
    CHECK(!quest_batch_clip_outside(identity,identity,v,SIZE_MAX,3));
    CHECK(!quest_batch_clip_outside(identity,identity,(void*)(UINTPTR_MAX-2),12,1));
    /* A stride is unused for a single element and must not advance a pointer. */
    memset(v,0,sizeof(v));v[0].position[0]=4;
    CHECK(quest_batch_clip_outside(identity,identity,v,SIZE_MAX,1));
    /* Negative w and an eye-plane crossing: all-behind may be rejected, but
     * mixing opposite outside planes must remain available to hardware. */
    memcpy(matrix,identity,sizeof(matrix));matrix[15]=-1;
    memset(v,0,sizeof(v));
    CHECK(quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),4));
    matrix[12]=1;matrix[15]=0;
    v[0].position[0]=-2;v[1].position[0]=2;
    CHECK(!quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),4));
    for(int slot=0;slot<16;++slot) {
        memcpy(matrix,identity,sizeof(matrix));matrix[slot]=NAN;
        CHECK(!quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),3));
        matrix[slot]=INFINITY;CHECK(!quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),3));
        matrix[slot]=FLT_MAX;CHECK(!quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),3));
        matrix[slot]=NAN;CHECK(!quest_batch_clip_outside(identity,matrix,v,sizeof(v[0]),3));
    }
    memcpy(matrix,identity,sizeof(matrix));matrix[12]=.01f;
    CHECK(!quest_batch_clip_outside(identity,matrix,v,sizeof(v[0]),3));
    for(int bad=0;bad<3;++bad) for(int axis=0;axis<3;++axis) {
        for(int i=0;i<4;++i) {v[i].position[0]=4;v[i].position[1]=v[i].position[2]=0;}
        v[3].position[axis]=bad==0?NAN:bad==1?INFINITY:1.0e20f;
        CHECK(!quest_batch_clip_outside(identity,identity,v,sizeof(v[0]),4));
    }
    /* Deliberately unaligned, padded input must only read its three positions. */
    unsigned char unaligned[sizeof(v)+1];
    memset(v,0xFF,sizeof(v));
    for(int i=0;i<4;++i) {v[i].position[0]=4;v[i].position[1]=v[i].position[2]=0;}
    memcpy(unaligned+1,v,sizeof(v));
    CHECK(quest_batch_clip_outside(identity,identity,unaligned+1,sizeof(v[0]),4));
    /* Float cancellation may lose a small inside coordinate; retain it. */
    memcpy(matrix,identity,sizeof(matrix));matrix[0]=1.0e8f;matrix[1]=-1.0e8f;
    for(int i=0;i<4;++i) {v[i].position[0]=1;v[i].position[1]=1;}
    CHECK(!quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),4));
    unsigned culled=0;
    for(unsigned test=0;test<50000;++test) {
        for(int j=0;j<16;++j) matrix[j]=(random_float()-.5f)*10;
        for(int j=0;j<4;++j) for(int k=0;k<3;++k)
            v[j].position[k]=(random_float()-.5f)*40;
        int rejected=quest_batch_clip_outside(matrix,identity,v,sizeof(v[0]),4);
        CHECK(!rejected || reference_outside(matrix,v,4)!=0);
        culled+=rejected;
    }
    CHECK(culled>500);
    unsigned two_stage_rejected=0;float mv[16];
    for(unsigned test=0;test<10000;++test){
        memcpy(mv,identity,sizeof(mv));
        for(int j=0;j<12;++j)mv[j]=(random_float()-.5f)*2000;
        for(int j=0;j<16;++j)matrix[j]=(random_float()-.5f)*10;
        for(int j=0;j<4;++j)for(int k=0;k<3;++k)v[j].position[k]=(random_float()-.5f)*40000;
        int rejected=quest_batch_clip_outside(matrix,mv,v,sizeof(v[0]),4);
        CHECK(!rejected||reference_two_stage(matrix,mv,v,4)!=0);
        two_stage_rejected+=rejected;
    }
    CHECK(two_stage_rejected>500);
    printf("math checks=%u failures=%u randomized_rejections=%u two_stage_rejections=%u\n",checks,failures,culled,two_stage_rejected);
    return failures!=0;
}
