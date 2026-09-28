/* Exercise production museum metadata and builder against original ROM assets.
 * Independent outlines, projected coverage, manifold local edges, opaque
 * seasonal materials, exact palette binding, and unchanged originals. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_structure_back_builder.c_inc"
#include "src/pc_museum_back_data.c_inc"

int g_pc_verbose;
static int checks, failures, model;
static pc_structure_back_cache caches[2];
#define CHECK(x,msg) do {checks++;if(!(x)){if(failures++<20)printf("FAIL model%d: %s (%d)\n",model,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int kind;} Asset;
typedef struct {const char* name;int source_count;unsigned palette;} Fixture;
#include "museum_fixtures.inc"
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
typedef struct {double x,y,z;} Point;
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}

/* Boundaries traced independently through the original body and roof lists.
 * These source indices are identical in the authored summer/winter models. */
static const int outline_ids[6][8]={
    {31,30,64,63,-1}, {56,55,89,88,-1},
    {116,115,127,121,135,114,113,-1}, {116,113,112,117,-1},
    {28,46,45,29,-1}, {66,65,76,75,-1}
};
static int outline(const Vtx* v,int patch,Point* p){
    int n=0;while(n<8&&outline_ids[patch][n]>=0){p[n]=at(v+outline_ids[patch][n]);n++;}return n;
}
static double projected_area(Point* p,int n,int axis){
    Point sum={0};for(int i=0;i<n;i++){Point c=cross(p[i],p[(i+1)%n]);sum.x+=c.x;sum.y+=c.y;sum.z+=c.z;}
    return fabs(axis==1?sum.y:sum.z)/2;
}
static unsigned pixel(unsigned x,unsigned y,unsigned width){return ((y/8)*(width/8)+x/8)*64+(y%8)*8+x%8;}
static int ci4(const u8* tex,unsigned x,unsigned y,unsigned width){unsigned p=pixel(x,y,width);return(tex[p/2]>>((p&1)?0:4))&15;}

static void verify(int m,const u8* rel){
    const pc_structure_back_spec* spec=pc_museum_back_specs+m;
    Vtx original[141];memcpy(original,spec->source,sizeof(original));
    Gfx* dl=pc_structure_back_build_spec(spec,caches+m);
    CHECK(dl!=NULL,"loaded model builds a rear patch list");if(!dl)return;
    CHECK(spec->patch_count==6,"six actual missing surfaces, no mirrored facade");
    CHECK(spec->palette==(m?obj_winter_museum_pal:obj_summer_museum_pal),"same explicit seasonal palette as original body");
    int p=-1,ended=0,nv=0,texture_count=0,palette_count=0;
    int triangles[6]={0},edges[6][8][8]={0},balance[6][8][8]={0};
    double areas[6]={0};Vtx* vertices=NULL;
    for(int d=0;d<128;d++){
        u32 word=dl[d].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        if(op==G_LOADTLUT){CHECK(pointer(dl[d].words.w1)==spec->palette,"display list binds body palette directly");palette_count++;}
        if(op==G_SETTIMG){
            const u8* tex=pointer(dl[d].words.w1);int patch=texture_count++;
            CHECK(patch<6,"texture patch count bounded");if(patch>=6)break;
            CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded 32x32 atlas crop");
            const pc_structure_back_patch* material=spec->patches+patch;
            CHECK(material->texture==(m?obj_w_museum_t1_tex:obj_s_museum_t1_tex),"wall/foundation use original seasonal masonry atlas");
            CHECK(material->crop[0]==(patch==1?114:0)&&material->crop[1]==0&&material->crop[2]==(patch==1?127:47)&&material->crop[3]==31,"foundation and plain wall crops remain distinct");
            for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++){
                int index=ci4(tex,x,y,32);const u8* q=rel+fixtures[m].palette+index*2;unsigned color=(q[0]<<8)|q[1];
                CHECK((color&0x8000)||((color>>12)&7)==7,"every sampled texel opaque in real seasonal palette");
                unsigned sx=material->crop[0]+x*(material->crop[2]-material->crop[0])/31;
                unsigned sy=y;CHECK(index==ci4(material->texture,sx,sy,128),"cropped texture preserves original palette indices");
            }
        }
        if(op==G_VTX){
            p++;nv=(word>>12)&255;vertices=pointer(dl[d].words.w1);CHECK(p<6,"vertex patch count bounded");if(p>=6)break;
            Point points[8];int n=outline(spec->source,p,points);CHECK(nv==n,"every original outline vertex retained");
            for(int i=0;i<nv;i++){
                int found=0;for(int j=0;j<n;j++)found|=same(at(vertices+i),points[j]);CHECK(found,"new vertices exactly match original contour");
                CHECK(vertices[i].n.a==255,"rear faces remain opaque");
                double len=0;for(int a=0;a<3;a++)len+=vertices[i].n.n[a]*vertices[i].n.n[a];
                CHECK(len>15500&&len<16300,"smooth normals normalized");
                CHECK(vertices[i].v.tc[0]>=0&&vertices[i].v.tc[0]<=1800&&vertices[i].v.tc[1]>=0&&vertices[i].v.tc[1]<=3600,"stone repeat UVs bounded");
            }
        }
        if(op==G_TRI1){
            int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};
            int valid=vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv;CHECK(valid,"triangle indices inside current cache");if(!valid)continue;
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            double projection=p==3?n.y:n.z;CHECK(projection<0,"rear surfaces face backward and underside faces down");
            areas[p]+=fabs(projection)/2;triangles[p]++;
            for(int i=0;i<3;i++){
                Point normal={vertices[t[i]].n.n[0],vertices[t[i]].n.n[1],vertices[t[i]].n.n[2]};CHECK(dot(n,normal)>0,"smooth normal agrees with each bent seam face");
                int a=t[i],b=t[(i+1)%3];if(a<b){edges[p][a][b]++;balance[p][a][b]++;}else{edges[p][b][a]++;balance[p][b][a]--;}
            }
        }
        CHECK(op!=G_MTX&&op!=G_DL,"no reflection transform or original geometry replay");
    }
    CHECK(ended&&p==5&&texture_count==6&&palette_count==6,"terminated six-patch list with six explicit palette bindings");
    for(int patch=0;patch<6;patch++){
        Point points[8];int count=outline(spec->source,patch,points);Vtx* v=caches[m].vertices[patch];
        CHECK(fabs(areas[patch]-projected_area(points,count,patch==3?1:2))<0.1,"triangles cover missing contour once without excess projected area");
        CHECK(triangles[patch]==count-2,"minimal contour triangulation");
        for(int a=0;a<count;a++)for(int b=a+1;b<count;b++){
            int boundary=0;for(int k=0;k<count;k++)if((same(at(v+a),points[k])&&same(at(v+b),points[(k+1)%count]))||(same(at(v+b),points[k])&&same(at(v+a),points[(k+1)%count])))boundary=1;
            if(boundary)CHECK(edges[patch][a][b]==1,"each original rim edge covered once");
            else if(edges[patch][a][b])CHECK(edges[patch][a][b]==2&&balance[patch][a][b]==0,"interior diagonal manifold and oppositely wound");
        }
    }
    CHECK(!memcmp(original,spec->source,sizeof(original)),"all original roof, entrance, lighting, and facade vertices untouched");
    CHECK(pc_structure_back_build_spec(spec,caches+m)==dl,"cache reused across frames and eyes");
    printf("%s: six fitted surfaces verified\n",fixtures[m].name);
}

int main(void){
    for(model=0;model<2;model++)CHECK(pc_structure_back_build_spec(pc_museum_back_specs+model,caches+model)==NULL,"unloaded models defer cache construction");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){
        Asset* a=assets+i;memcpy(a->data,rel+a->offset,a->size);
        if(a->kind==1)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 t=p[k];p[k]=p[k+1];p[k+1]=t;}}
        if(a->kind==2)for(unsigned n=0;n<a->size;n+=2){u8* p=(u8*)a->data+n;u8 t=p[0];p[0]=p[1];p[1]=t;}
    }
    for(model=0;model<2;model++)verify(model,rel);
    free(rel);pc_disc_shutdown();printf("Museum backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
