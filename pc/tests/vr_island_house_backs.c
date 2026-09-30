/* Original asset geometry supplies the independent outlines. The production
 * builder supplies vertices, texture samples, GBI commands and triangulation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_structure_back_builder.c_inc"
#include "src/pc_island_house_back.c_inc"

int g_pc_verbose;
static int checks, failures, model;
#define CHECK(x,msg) do {++checks;if(!(x)){if(failures++<20)printf("FAIL model %d: %s (%d)\n",model,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int vertices;} Asset;
typedef struct {const char* name;int source_count;unsigned palette;} Fixture;
#include "island_house_fixtures.inc"
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
typedef struct {double x,y,z;} Point;
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}
static double area(Point* p,int n){Point sum={0};for(int i=0;i<n;i++){Point c=cross(p[i],p[(i+1)%n]);sum.x+=c.x;sum.y+=c.y;sum.z+=c.z;}return sqrt(dot(sum,sum))/2;}

static int outline(int m,int patch,Point* out,Point* normal){
    const Vtx* v=m?obj_s_house_i_v:obj_s_myhome_i_v;
    if(!m && patch<2){
        Point front=at(v+16),right=at(v+12),back=at(v+15),corner={front.x+back.x-right.x,front.y,front.z+back.z-right.z};
        Point top=corner;top.y=v[17].v.ob[1];
        if(!patch){out[0]=front;out[1]=corner;out[2]=top;out[3]=at(v+17);*normal=(Point){-1,0,-1};}
        else{out[0]=corner;out[1]=back;out[2]=at(v+14);out[3]=top;*normal=(Point){-1,0,1};}
        return 4;
    }
    if(m && !patch){int ids[]={28,27,26,29};for(int i=0;i<4;i++)out[i]=at(v+ids[i]);*normal=(Point){-1,0,0};return 4;}
    const int rim[2][12]={{59,60,62,64,75,79,82,100,93,94,109,105},{98,99,102,104,149,146,139,137,133,115,116,118}};
    int half=patch-(m?1:2),start=half*6;
    /* Centre is independently inferred from opposite original rim vertices. */
    Point a=at(v+rim[m][0]),b=at(v+rim[m][6]);
    out[0]=(Point){floor((a.x+b.x)/2+0.5),a.y,floor((a.z+b.z)/2+0.5)};
    for(int i=0;i<7;i++)out[i+1]=at(v+rim[m][(start+i)%12]);
    *normal=(Point){0,-1,0};return 8;
}

static void verify(int m,const u8* rel){
    const pc_structure_back_spec* spec=&pc_island_house_back_specs[m];
    cKF_Skeleton_R_c* skeleton=m?&cKF_bs_r_obj_s_house_i:&cKF_bs_r_obj_s_myhome_i;
    Vtx saved[180];memcpy(saved,spec->source,fixtures[m].source_count*sizeof(Vtx));
    Gfx* dl=NULL;CHECK(pc_island_house_back_lookup(skeleton,&dl)&&dl,"loaded island model has fitted repair");if(!dl)return;
    int patch=-1,ended=0,binds=0,nv=0;Vtx* vertices=NULL;double totals[6]={0};int triangles[6]={0};
    Point polygon[8],normal;int count=0;unsigned material_mask[6]={0};
    for(int d=0;d<128;d++){
        u32 word=dl[d].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        if(op==G_LOADTLUT){CHECK(dl[d].words.w1==0x08000000u,"live island palette is retained as segment 8");binds++;}
        if(op==G_SETTIMG){u8* tex=pointer(dl[d].words.w1);unsigned colors=0;
            CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded material tile");
            for(int i=0;i<1024;i++){int ix=(tex[i/2]>>((i&1)?0:4))&15;colors|=1u<<ix;const u8* c=rel+fixtures[m].palette+ix*2;unsigned rgb=(c[0]<<8)|c[1];CHECK((rgb&0x8000)||((rgb>>12)&7)==7,"every generated texel is opaque in actual island palette");}
            int variants=0;for(int i=0;i<16;i++)variants+=(colors>>i)&1;
            CHECK(variants>=3,"material retains original color variation instead of a flat blank wall");
            material_mask[patch+1]=colors;
        }
        if(op==G_VTX){++patch;nv=(word>>12)&255;vertices=pointer(dl[d].words.w1);count=outline(m,patch,polygon,&normal);
            CHECK(nv==count,"expected original wall/roof contour size");
            for(int i=0;i<nv;i++){CHECK(same(at(vertices+i),polygon[i]),"closure meets independently reconstructed original outline");CHECK(spec->patches[patch].vertices[i].index<fixtures[m].source_count,"bounded original vertex index");}
        }
        if(op==G_TRI1){int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};CHECK(vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv,"triangle indices stay inside loaded vertex cache");
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            CHECK(dot(n,normal)>0,"positive area with outward wall or downward underside winding");totals[patch]+=sqrt(dot(n,n))/2;triangles[patch]++;
            for(int j=0;j<3;j++){Point vn={vertices[t[j]].n.n[0],vertices[t[j]].n.n[1],vertices[t[j]].n.n[2]};CHECK(dot(vn,normal)>0,"lighting normals agree with exterior direction");}
        }
    }
    CHECK(ended,"terminated display list within fixed budget");CHECK(patch+1==(m?3:4),"only missing walls and two roof underside halves added");CHECK(binds==patch+1,"all patches bind live material palette");
    for(int i=0;i<=patch;i++){Point p[8],n;int count=outline(m,i,p,&n);CHECK(fabs(totals[i]-area(p,count))<0.1,"triangles exactly cover the missing surface without overlaps");CHECK(triangles[i]==count-2,"minimal triangulation");}
    CHECK(!memcmp(saved,spec->source,fixtures[m].source_count*sizeof(Vtx)),"original geometry is byte-identical after repair");Gfx* next=NULL;CHECK(pc_island_house_back_lookup(skeleton,&next)&&next==dl,"cache reused across frames and both eyes");
    printf("%s: %d wall/underside patches verified\n",fixtures[m].name,patch+1);
}

int main(void){
    Gfx* dl=(Gfx*)1;cKF_Skeleton_R_c unknown={0};
    CHECK(pc_island_house_back_lookup(&cKF_bs_r_obj_s_myhome_i,&dl)&&dl==NULL,"unloaded cottage deferred without mirrored facade");
    CHECK(pc_island_house_back_lookup(&cKF_bs_r_obj_s_house_i,&dl)&&dl==NULL,"unloaded islander house deferred without mirrored facade");
    CHECK(!pc_island_house_back_lookup(&unknown,&dl)&&dl==NULL,"unrelated skeleton untouched");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){Asset* a=&assets[i];memcpy(a->data,rel+a->offset,a->size);if(a->vertices)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 tmp=p[k];p[k]=p[k+1];p[k+1]=tmp;}}}
    for(model=0;model<2;model++)verify(model,rel);
    free(rel);pc_disc_shutdown();printf("Island house backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
