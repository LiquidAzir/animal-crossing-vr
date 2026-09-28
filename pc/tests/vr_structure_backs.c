/* Production caps, original vertices/materials and independent missing outlines. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_structure_back.c_inc"

int g_pc_verbose;
static int checks, failures, model;
#define CHECK(x,msg) do {checks++;if(!(x)){if(failures++<20)printf("FAIL model%d: %s (%d)\n",model,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int vertices;} Asset;
typedef struct {const char* name;int source_count;unsigned palette;} Fixture;
#include "structure_fixtures.inc"
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
typedef struct {double x,y,z;} Point;
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}
static double area(Point* p,int n){Point sum={0};for(int i=0;i<n;i++){Point a=cross(p[i],p[(i+1)%n]);sum.x+=a.x;sum.y+=a.y;sum.z+=a.z;}return sqrt(dot(sum,sum))/2;}

/* Derive the tailor's missing corner from the three existing floor corners;
 * metadata does not participate in this independent outline construction. */
static int outline(int m,int patch,Point* p,Point* normal){
    const Vtx* v=pc_structure_back_specs[m].source;
    if(m<2){int ids[2][4]={{16,15,29,28},{12,11,25,24}};for(int i=0;i<4;i++)p[i]=at(v+ids[m][i]);*normal=(Point){0,0,-1};return 4;}
    int w=m==3;
    Point front_left=at(v+(w?25:64)),front_right=at(v+(w?23:62)),rear_right=at(v+(w?30:69));
    Point rear_left={front_left.x+rear_right.x-front_right.x,0,front_left.z+rear_right.z-front_right.z};
    Point front_top=at(v+(w?24:63)),rear_top=at(v+(w?31:70)),left_top=rear_left;left_top.y=front_top.y;
    Point roof_right=at(v+(w?36:7)),roof_left=at(v+(w?50:21)),roof_front=at(v+(w?49:20));
    if(patch==0){p[0]=front_left;p[1]=rear_left;p[2]=left_top;p[3]=front_top;*normal=(Point){-1,0,1};return 4;}
    if(patch==1){p[0]=rear_left;p[1]=rear_right;p[2]=rear_top;p[3]=left_top;*normal=(Point){-1,0,-1};return 4;}
    if(patch==2){int ids[2][5]={{7,9,15,16,21},{36,38,44,45,50}};for(int i=0;i<5;i++)p[i]=at(v+ids[w][i]);*normal=(Point){-1,0,-1};return 5;}
    if(patch==3){p[0]=rear_top;p[1]=roof_right;p[2]=roof_left;p[3]=left_top;}
    else {p[0]=front_top;p[1]=left_top;p[2]=roof_left;p[3]=roof_front;}
    *normal=(Point){0,-1,0};return 4;
}

static void verify(int m,const u8* rel){
    const pc_structure_back_spec* spec=&pc_structure_back_specs[m];
    Vtx original[128];memcpy(original,spec->source,fixtures[m].source_count*sizeof(Vtx));
    unsigned palette_count=0;
    Gfx* dl=pc_structure_back_build(m);CHECK(dl!=NULL,"known loaded model has cap");if(!dl)return;
    int ended=0,patch=-1,triangles[6]={0},edge[6][8][8]={0},balance[6][8][8]={0};
    Vtx* vertices=NULL;int nv=0;double totals[6]={0};Point polygon[8],normal;int np=0;
    CHECK(spec->patch_count==(m<2?1:5),"expected missing surfaces only");
    for(int d=0;d<128;d++){
        u32 word=dl[d].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        if(op==G_LOADTLUT){
            CHECK(dl[d].words.w1==0x08000000u,"live palette remains a segmented address, never a tagged host pointer");
            ++palette_count;
        }
        if(op==G_SETTIMG){u8* tex=pointer(dl[d].words.w1);CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded 32x32 patch");
            for(int i=0;i<1024;i++){int index=(tex[i/2]>>((i&1)?0:4))&15;const u8* q=rel+fixtures[m].palette+index*2;unsigned color=(q[0]<<8)|q[1];CHECK((color&0x8000)||((color>>12)&7)==7,"every texel opaque in actual seasonal palette");}}
        if(op==G_VTX){patch++;nv=(word>>12)&255;vertices=pointer(dl[d].words.w1);CHECK(patch<(int)spec->patch_count,"patch count bounded");if(patch>=6)break;
            np=outline(m,patch,polygon,&normal);CHECK(nv==np,"all contour points retained");
            for(int i=0;i<nv;i++){int found=0;for(int j=0;j<np;j++)found|=same(at(vertices+i),polygon[j]);CHECK(found,"vertex lies on independent original contour");
                CHECK(spec->patches[patch].vertices[i].index<fixtures[m].source_count,"source reference in original array");
                CHECK(vertices[i].v.tc[0]>=0&&vertices[i].v.tc[0]<=8192&&vertices[i].v.tc[1]>=0&&vertices[i].v.tc[1]<=1024,"UVs bounded, optional horizontal plank repetition");}}
        if(op==G_TRI1){int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};int valid=vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv;CHECK(valid,"triangle indices in active vertex cache");if(!valid)continue;
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            CHECK(dot(n,normal)>0,"triangle faces outward with positive area");totals[patch]+=sqrt(dot(n,n))/2;triangles[patch]++;
            for(int i=0;i<3;i++){int a=t[i],b=t[(i+1)%3];if(a<b){edge[patch][a][b]++;balance[patch][a][b]++;}else{edge[patch][b][a]++;balance[patch][b][a]--;}}
        }
    }
    CHECK(palette_count==spec->patch_count,"each patch binds the live palette");
    CHECK(ended,"bounded terminated display list");CHECK(patch+1==(int)spec->patch_count,"all surfaces emitted exactly once");
    for(int p=0;p<(int)spec->patch_count;p++){
        Point points[8],n;int count=outline(m,p,points,&n);Vtx* v=pc_structure_back_caches[m].vertices[p];
        CHECK(fabs(totals[p]-area(points,count))<0.1,"triangles cover entire missing outline without excess area");
        CHECK(triangles[p]==count-2,"minimal triangulation retains collinear boundary points");
        for(int a=0;a<count;a++)for(int b=a+1;b<count;b++){
            int boundary=0;for(int k=0;k<count;k++)if((same(at(v+a),points[k])&&same(at(v+b),points[(k+1)%count]))||(same(at(v+b),points[k])&&same(at(v+a),points[(k+1)%count])))boundary=1;
            if(boundary)CHECK(edge[p][a][b]==1,"each original outline edge is capped once");
            else if(edge[p][a][b])CHECK(edge[p][a][b]==2&&balance[p][a][b]==0,"internal diagonal shared by opposite triangle winding");
        }
    }
    CHECK(!memcmp(original,spec->source,fixtures[m].source_count*sizeof(Vtx)),"original mesh untouched");
    CHECK(pc_structure_back_build(m)==dl,"stable cache across draws and eyes");
    printf("%s: %u bounded patches verified\n",fixtures[m].name,spec->patch_count);
}

static void verify_light(int winter){
    Gfx* original=winter?obj_w_tailor_light_model:obj_s_tailor_light_model;
    Vtx* source=winter?obj_w_tailor_v+4:obj_s_tailor_v+80;
    Vtx saved[11];Gfx commands[10];
    memcpy(saved,source,sizeof(saved));memcpy(commands,original,sizeof(commands));
    Gfx* copy=pc_tailor_light_for_back(original);
    CHECK(copy!=original,"repaired light has separate stable display list");
    CHECK(pc_tailor_light_for_back(original)==copy,"light cache reused across eyes");
    Vtx* vertices=pointer(copy[6].words.w1);
    CHECK(vertices!=source,"light vertices copied without asset mutation");
    CHECK((copy[9].words.w0>>24)==G_ENDDL,"light list terminates within ten commands");
    for(int i=0;i<10;i++)if(i!=6)CHECK(!memcmp(copy+i,original+i,sizeof(Gfx)),"all original material and triangle commands preserved");
    CHECK(copy[6].words.w0==original[6].words.w0,"same vertex count and cache destination");
    for(int i=0;i<11;i++){
        Vtx expected=saved[i];
        if(i==0||i==3){
            CHECK(expected.v.ob[2]-expected.v.ob[0]==8758,"original light edge touches left wall exactly");
            expected.v.ob[0]+=32;expected.v.ob[2]-=32;
            CHECK(vertices[i].v.ob[2]-vertices[i].v.ob[0]<8758,"touching edge strictly inside new wall");
            double separation=(8758.0-(vertices[i].v.ob[2]-vertices[i].v.ob[0]))/sqrt(2.0);
            CHECK(separation<8000.0*sqrt(2.0)*0.005,"inset remains below half a percent of wall width");
            /* Native structure viewer uses near=10, far=50000. Even at the
             * far limit require >2 representable 24-bit depth steps, so a
             * mathematically distinct but quantized-equal inset cannot pass. */
            double depth_steps=10.0*50000.0/(50000.0-10.0)*(1.0/(50000.0-separation)-1.0/50000.0)*16777215.0;
            CHECK(depth_steps>2.0,"light separation survives distant 24-bit depth quantization");
        }
        CHECK(!memcmp(vertices+i,&expected,sizeof(Vtx)),"only two wall-touching positions change; UVs, normals and alpha preserved");
    }
    CHECK(!memcmp(saved,source,sizeof(saved))&&!memcmp(commands,original,sizeof(commands)),"original light geometry and commands remain byte-identical");
}

int main(void){
    Gfx* dl=(Gfx*)1;cKF_Skeleton_R_c unknown={0};
    CHECK(pc_tailor_back_lookup(&cKF_bs_r_obj_s_tailor,&dl)&&dl==NULL,"unloaded summer tailor recognized without mirrored fallback");
    CHECK(pc_tailor_back_lookup(&cKF_bs_r_obj_w_tailor,&dl)&&dl==NULL,"unloaded winter tailor recognized without mirrored fallback");
    CHECK(!pc_tailor_back_lookup(&unknown,&dl)&&dl==NULL,"unknown skeleton untouched");
    CHECK(pc_police_back(0)==NULL&&pc_police_back(1)==NULL,"unloaded police deferred");
    CHECK(pc_tailor_light_for_back(obj_s_tailor_light_model)==obj_s_tailor_light_model&&pc_tailor_light_for_back(obj_w_tailor_light_model)==obj_w_tailor_light_model,"unloaded light copies deferred");
    CHECK(pc_tailor_light_for_back(NULL)==NULL&&pc_tailor_light_for_back((Gfx*)&unknown)==(Gfx*)&unknown,"unknown or suppressed geometry untouched");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){Asset* a=&assets[i];memcpy(a->data,rel+a->offset,a->size);if(a->vertices)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 t=p[k];p[k]=p[k+1];p[k+1]=t;}}}
    for(model=0;model<4;model++)verify(model,rel);
    verify_light(0);verify_light(1);
    free(rel);pc_disc_shutdown();printf("Structure backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
