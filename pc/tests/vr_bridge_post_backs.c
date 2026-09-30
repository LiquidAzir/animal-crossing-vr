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
#include "src/pc_bridge_back.c_inc"

int g_pc_verbose;
static int checks, failures, model;
#define CHECK(x,msg) do {++checks;if(!(x)){if(failures++<20)printf("FAIL model %d: %s (%d)\n",model,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int vertices;} Asset;
typedef struct {const char* name;int source_count;unsigned palette;} Fixture;
#include "bridge_post_fixtures.inc"
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
typedef struct {double x,y,z;} Point;
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}
static double area(Point* p,int n){Point sum={0};for(int i=0;i<n;i++){Point c=cross(p[i],p[(i+1)%n]);sum.x+=c.x;sum.y+=c.y;sum.z+=c.z;}return sqrt(dot(sum,sum))/2;}

/* These four +Z outlines are the unmatched vertical edges in base_model.
 * Every vertex is from the stationary range (75..137), never deck joints. */
static int outline(int m,int patch,Point* out,Point* normal){
    const Vtx* v=m?obj_w_bridgeA_v:obj_s_bridgeA_v;
    const int edges[4][4]={{88,91,95,94},{112,113,106,105},
                          {114,117,121,120},{136,137,132,131}};
    for(int i=0;i<4;i++)out[i]=at(v+edges[patch][i]);
    *normal=(Point){0,0,1};return 4;
}

static void verify(int m,const u8* rel){
    const pc_structure_back_spec* spec=&pc_bridge_back_specs[m];
    cKF_Skeleton_R_c* skeleton=m?&cKF_bs_r_obj_w_bridgeA:&cKF_bs_r_obj_s_bridgeA;
    Vtx saved[180];memcpy(saved,spec->source,fixtures[m].source_count*sizeof(Vtx));
    Gfx* dl=NULL;CHECK(pc_bridge_back_lookup(skeleton,&dl)&&dl,"loaded seasonal bridge has fitted post repair");if(!dl)return;
    int patch=-1,ended=0,binds=0,nv=0;Vtx* vertices=NULL;double totals[6]={0};int triangles[6]={0};
    Point polygon[8],normal;int count=0;unsigned material_mask[6]={0};
    for(int d=0;d<128;d++){
        u32 word=dl[d].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        if(op==G_LOADTLUT){CHECK(pointer(dl[d].words.w1)==spec->palette,"original seasonal bridge palette retained");binds++;}
        if(op==G_SETTIMG){u8* tex=pointer(dl[d].words.w1);unsigned colors=0;
            CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded material tile");
            for(int i=0;i<1024;i++){int ix=(tex[i/2]>>((i&1)?0:4))&15;colors|=1u<<ix;const u8* c=rel+fixtures[m].palette+ix*2;unsigned rgb=(c[0]<<8)|c[1];CHECK((rgb&0x8000)||((rgb>>12)&7)==7,"every generated texel is opaque in actual seasonal bridge palette");}
            int variants=0;for(int i=0;i<16;i++)variants+=(colors>>i)&1;
            CHECK(variants>=3,"material retains original color variation instead of a flat blank wall");
            material_mask[patch+1]=colors;
        }
        if(op==G_VTX){++patch;nv=(word>>12)&255;vertices=pointer(dl[d].words.w1);count=outline(m,patch,polygon,&normal);
            CHECK(nv==count,"expected original stationary post contour size");
            for(int i=0;i<nv;i++){CHECK(same(at(vertices+i),polygon[i]),"closure meets independently reconstructed original outline");CHECK(spec->patches[patch].vertices[i].index>=75 && spec->patches[patch].vertices[i].index<fixtures[m].source_count,"only original stationary base vertices referenced");}
        }
        if(op==G_TRI1){int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};CHECK(vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv,"triangle indices stay inside loaded vertex cache");
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            CHECK(dot(n,normal)>0,"positive area with outward post-back winding");totals[patch]+=sqrt(dot(n,n))/2;triangles[patch]++;
            for(int j=0;j<3;j++){Point vn={vertices[t[j]].n.n[0],vertices[t[j]].n.n[1],vertices[t[j]].n.n[2]};CHECK(dot(vn,normal)>0,"lighting normals agree with exterior direction");}
        }
    }
    CHECK(ended,"terminated display list within fixed budget");CHECK(patch+1==4,"only four missing stationary post backs added");CHECK(binds==patch+1,"all patches bind live material palette");
    for(int i=0;i<=patch;i++){Point p[8],n;int count=outline(m,i,p,&n);CHECK(fabs(totals[i]-area(p,count))<0.1,"triangles exactly cover the missing surface without overlaps");CHECK(triangles[i]==count-2,"minimal triangulation");}
    CHECK(!memcmp(saved,spec->source,fixtures[m].source_count*sizeof(Vtx)),"original geometry is byte-identical after repair");Gfx* next=NULL;CHECK(pc_bridge_back_lookup(skeleton,&next)&&next==dl,"cache reused across frames and both eyes");
    printf("%s: %d stationary post-back patches verified\n",fixtures[m].name,patch+1);
}

int main(void){
    Gfx* dl=(Gfx*)1;cKF_Skeleton_R_c unknown={0};
    CHECK(pc_bridge_back_lookup(&cKF_bs_r_obj_s_bridgeA,&dl)&&dl==NULL,"unloaded summer bridge deferred without reflection");
    CHECK(pc_bridge_back_lookup(&cKF_bs_r_obj_w_bridgeA,&dl)&&dl==NULL,"unloaded winter bridge deferred without reflection");
    CHECK(!pc_bridge_back_lookup(&unknown,&dl)&&dl==NULL,"unrelated skeleton untouched");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){Asset* a=&assets[i];memcpy(a->data,rel+a->offset,a->size);if(a->vertices)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 tmp=p[k];p[k]=p[k+1];p[k+1]=tmp;}}}
    for(model=0;model<2;model++)verify(model,rel);
    free(rel);pc_disc_shutdown();printf("Bridge post backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
