/* Real source-asset boundary, palette, cache and display-list checks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_structure_back_builder.c_inc"
#include "src/pc_shop_back.c_inc"

int g_pc_verbose;
static int checks, failures, season;
#define CHECK(c,msg) do {++checks;if(!(c)){if(failures++<20)printf("FAIL season%d: %s (%d)\n",season,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int vertices;} Asset;
typedef struct {const char* name;int count;unsigned palette;} Fixture;
#include "shop_fixtures.inc"
typedef struct {double x,y,z;} Point;
typedef struct {Point p[5];int count;Point normal;} Loop;
static pc_structure_back_cache caches[2];
extern cKF_Skeleton_R_c cKF_bs_r_obj_s_shop2,cKF_bs_r_obj_w_shop2;
extern cKF_Skeleton_R_c cKF_bs_r_obj_s_shop3,cKF_bs_r_obj_w_shop3;
extern cKF_Skeleton_R_c cKF_bs_r_obj_s_shop4,cKF_bs_r_obj_w_shop4;
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
static double area(const Loop* l){double sum=0;for(int i=1;i<l->count-1;i++){Point n=cross(sub(l->p[i],l->p[0]),sub(l->p[i+1],l->p[0]));sum+=sqrt(dot(n,n))/2;}return sum;}

/* Independent expected holes, derived from actual authored wall/roof corners.
 * The only absent footprint corner uses the existing left wall X and entrance
 * rear Z; no implementation offsets or triangle definitions are read here. */
static int outlines(int patch,Loop* l){
    const Vtx* v=pc_shop_back_specs[season].source;
    Point left_low=at(v+33),left_high=at(v+31),ridge=at(v+29);
    Point front_low=at(v+37),front_high=at(v+34);
    Point rear_low=left_low,rear_high=left_high,rear_ridge=ridge;
    rear_low.z=rear_high.z=rear_ridge.z=front_low.z;
    if(patch==0){*l=(Loop){{left_low,rear_low,rear_high,left_high},4,{-1,0,0}};return 1;}
    if(patch==1){*l=(Loop){{rear_low,front_low,front_high,rear_ridge,rear_high},5,{0,0,1}};return 1;}
    if(patch==2){*l=(Loop){{left_high,rear_high,at(v+23),at(v+24)},4,{0,-1,0}};return 1;}
    if(patch==3){*l=(Loop){{at(v+36),at(v+15),at(v+13),front_high},4,{0,-1,0}};return 1;}
    if(patch==4){
        l[0]=(Loop){{at(v+24),at(v+14),ridge,left_high},4,{0,-1,0}};
        l[1]=(Loop){{at(v+14),at(v+15),at(v+36),ridge},4,{0,-1,0}};return 2;
    }
    l[0]=(Loop){{at(v+23),rear_high,rear_ridge,at(v+12)},4,{0,-1,0}};
    l[1]=(Loop){{at(v+12),rear_ridge,front_high,at(v+13)},4,{0,-1,0}};return 2;
}

static void verify(const u8* rel){
    const pc_structure_back_spec* spec=pc_shop_back_specs+season;
    Vtx originals[50];memcpy(originals,spec->source,sizeof(originals));
    CHECK(fixtures[season].count==50,"verified original seasonal vertex layout");
    Gfx* dl=pc_structure_back_build_spec(spec,caches+season);
    CHECK(dl!=NULL,"loaded Cranny receives fitted closures");if(!dl)return;
    CHECK(spec->patch_count==6&&spec->palette==NULL,"two timber walls and four eaves retain live palette");
    int patch=-1,nv=0,ended=0,triangle_count[6]={0},palette_count=0;
    double areas[6]={0};Vtx* vertices=NULL;
    int edges[6][8][8]={0},balance[6][8][8]={0};
    for(int i=0;i<128;i++){
        u32 word=dl[i].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        CHECK(op!=G_DL,"closure never replays any facade, door or window list");
        if(op==G_LOADTLUT){CHECK(dl[i].words.w1==0x08000000u,"live palette is integer segment8, never tagged host pointer");++palette_count;}
        if(op==G_SETTIMG){
            u8* tex=pointer(dl[i].words.w1);
            CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded 32x32 indexed material");
            for(int p=0;p<1024;p++){
                unsigned index=(tex[p/2]>>((p&1)?0:4))&15;
                const u8* pal=rel+fixtures[season].palette+2*index;unsigned color=(pal[0]<<8)|pal[1];
                CHECK((color&0x8000)||((color>>12)&7)==7,"every texel opaque in actual seasonal palette");
            }
        }
        if(op==G_VTX){
            ++patch;nv=(word>>12)&255;vertices=pointer(dl[i].words.w1);
            CHECK(patch<6&&nv<=8,"generated geometry stays inside fixed cache bounds");if(patch>=6)break;
            Loop loop[2];int count=outlines(patch,loop),expected=0;
            for(int l=0;l<count;l++)expected+=loop[l].count;
            CHECK(nv==expected,"all independently derived boundary points retained");
            const pc_structure_back_patch* p=spec->patches+patch;
            CHECK(p->crop[2]<128&&p->crop[3]<32,"material crop inside original atlas");
            for(int a=0;a<nv;a++){
                int found=0;for(int l=0;l<count;l++)for(int k=0;k<loop[l].count;k++)found|=same(at(vertices+a),loop[l].p[k]);
                CHECK(found,"vertex lies on independently derived absent surface");
                CHECK(p->vertices[a].index<fixtures[season].count,"source index within actual seasonal model");
                CHECK(vertices[a].v.tc[0]>=0&&vertices[a].v.tc[0]<8192&&vertices[a].v.tc[1]>=0&&vertices[a].v.tc[1]<8192,"repeated timber UVs remain bounded");
            }
        }
        if(op==G_TRI1){
            int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};
            int valid=vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv;CHECK(valid,"triangle references active vertex cache");if(!valid)continue;
            Loop loop[2];int count=outlines(patch,loop),belongs=0;
            for(int l=0;l<count;l++){int points=0;for(int a=0;a<3;a++)for(int k=0;k<loop[l].count;k++)points+=same(at(vertices+t[a]),loop[l].p[k]);belongs|=points==3;}
            CHECK(belongs,"triangles remain within a single expected surface");
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            CHECK(dot(n,loop[0].normal)>0,"walls face outward and roof undersides face down");
            areas[patch]+=sqrt(dot(n,n))/2;triangle_count[patch]++;
            for(int j=0;j<3;j++){int a=t[j],b=t[(j+1)%3];if(a<b){edges[patch][a][b]++;balance[patch][a][b]++;}else{edges[patch][b][a]++;balance[patch][b][a]--;}}
        }
    }
    CHECK(ended&&patch==5&&palette_count==6,"all six bounded material patches terminate and bind palette");
    for(int p=0;p<6;p++){
        Loop loop[2];int count=outlines(p,loop),nt=0;double target=0;
        for(int l=0;l<count;l++){target+=area(loop+l);nt+=loop[l].count-2;}
        CHECK(fabs(areas[p]-target)<0.1,"triangles exactly fill missing contours without excess area");
        CHECK(triangle_count[p]==nt,"minimal triangulation only");
        // Each independent contour is represented in its own cache span.
        int base=0;
        for(int l=0;l<count;l++){
            int n=loop[l].count;
            for(int a=base;a<base+n;a++)for(int b=a+1;b<base+n;b++){
                int boundary=b==a+1||(a==base&&b==base+n-1);
                if(boundary)CHECK(edges[p][a][b]==1,"each absent contour edge sealed once");
                else if(edges[p][a][b])CHECK(edges[p][a][b]==2&&balance[p][a][b]==0,"internal diagonals shared with opposite winding");
            }
            base+=n;
        }
    }
    CHECK(!memcmp(originals,spec->source,sizeof(originals)),"original door, roof, windows, sign and walls unchanged");
    CHECK(pc_structure_back_build_spec(spec,caches+season)==dl,"cache stable across frames and stereo eyes");
    printf("%s: two fitted walls, six roof underside strips, seventeen triangles verified\n",fixtures[season].name);
}

int main(void){
    cKF_Skeleton_R_c unknown={0};Gfx* out=(Gfx*)1;
    cKF_Skeleton_R_c* recognized[]={&cKF_bs_r_obj_s_shop1,&cKF_bs_r_obj_w_shop1};
    cKF_Skeleton_R_c* other[]={&unknown,&cKF_bs_r_obj_s_shop2,&cKF_bs_r_obj_w_shop2,&cKF_bs_r_obj_s_shop3,&cKF_bs_r_obj_w_shop3,&cKF_bs_r_obj_s_shop4,&cKF_bs_r_obj_w_shop4};
    for(season=0;season<2;season++){
        CHECK(pc_structure_back_build_spec(pc_shop_back_specs+season,caches+season)==NULL,"unloaded assets defer fitted geometry");
        CHECK(pc_shop_back_lookup(recognized[season],&out)&&out==NULL,"unloaded known shop suppresses facade reflection");
    }
    for(unsigned i=0;i<ARRAY_COUNT(other);i++)CHECK(!pc_shop_back_lookup(other[i],&out)&&out==NULL,"unrelated models and upgrades unchanged");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){
        Asset* a=assets+i;memcpy(a->data,rel+a->offset,a->size);
        if(a->vertices)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 t=p[k];p[k]=p[k+1];p[k+1]=t;}}
    }
    for(season=0;season<2;season++){verify(rel);CHECK(pc_shop_back_lookup(recognized[season],&out)&&out!=NULL,"loaded seasonal shop routes to real closure");}
    free(rel);pc_disc_shutdown();printf("Cranny backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
