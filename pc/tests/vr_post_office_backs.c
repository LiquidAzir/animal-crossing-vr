/* Original disc outlines and live indexed materials, through the shared builder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_structure_back_builder.c_inc"
#include "src/pc_post_office_back_data.c_inc"

int g_pc_verbose;
static int checks, failures, season;
#define CHECK(c,msg) do {checks++;if(!(c)){if(failures++<20)printf("FAIL season%d: %s (%d)\n",season,msg,__LINE__);}}while(0)
typedef struct {void* data;unsigned size,offset;int vertices;} Asset;
typedef struct {const char* name;int count;unsigned palette;} Fixture;
#include "post_office_fixtures.inc"
typedef struct {double x,y,z;} Point;
typedef struct {Point p[4],normal;} Loop;
static pc_structure_back_cache caches[2];
extern Gfx obj_s_yubinkyoku_light_model[],obj_w_yubinkyoku_light_model[];
extern cKF_Skeleton_R_c cKF_bs_r_obj_s_yubinkyoku,cKF_bs_r_obj_w_yubinkyoku;
static pc_structure_back_cache pc_post_office_back_caches[2];
#include "post_office_source.inc"
static Point at(const Vtx* v){return(Point){v->v.ob[0],v->v.ob[1],v->v.ob[2]};}
static Point sub(Point a,Point b){return(Point){a.x-b.x,a.y-b.y,a.z-b.z};}
static Point cross(Point a,Point b){return(Point){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static int same(Point a,Point b){return dot(sub(a,b),sub(a,b))<0.01;}
static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
static double area(const Loop* l){Point sum={0};for(int i=0;i<4;i++){Point n=cross(l->p[i],l->p[(i+1)%4]);sum.x+=n.x;sum.y+=n.y;sum.z+=n.z;}return sqrt(dot(sum,sum))/2;}

/* Derive the missing corner from the three original floor/top corners.
 * These independent contours deliberately do not read implementation offsets,
 * crop/UV definitions, triangulation, or synthetic vertices. */
static int outlines(int patch,Loop* l){
    const Vtx* v=pc_post_office_back_specs[season].source;int w=season;
    Point a=at(v+(w?25:22)),b=at(v+4),c=at(v+(w?26:23)),d=at(v+7);
    Point missing_low={a.x,a.y,b.z},missing_high={c.x,c.y,d.z};
    Point rf=at(v+9),lf=at(v+10),lb=at(v+(w?12:11)),rb=at(v+(w?13:12)),front_top=at(v+6);
    if(patch==0){*l=(Loop){{a,missing_low,missing_high,c},{-1,0,0}};return 1;}
    if(patch==1){*l=(Loop){{missing_low,b,d,missing_high},{0,-250,12600}};return 1;}
    if(patch==2){l[0]=(Loop){{lf,lb,missing_high,c},{0,-1,0}};l[1]=(Loop){{d,rb,rf,front_top},{0,-1,0}};return 2;}
    if(patch==3){l[0]=(Loop){{rf,lf,c,front_top},{0,-1,0}};l[1]=(Loop){{lb,rb,d,missing_high},{0,-1,0}};return 2;}
    Point end_low=at(v+(w?34:31)),end_front=at(v+(w?36:33));end_low.z=end_front.z;
    if(patch==4){*l=(Loop){{end_low,end_front,at(v+(w?39:36)),at(v+(w?31:28))},{0,0,1}};return 1;}
    *l=(Loop){{at(v+(w?34:31)),at(v+(w?33:30)),end_front,end_low},{0,-1,0}};return 1;
}

static void verify(const u8* rel){
    const pc_structure_back_spec* spec=pc_post_office_back_specs+season;
    Vtx originals[52];memcpy(originals,spec->source,fixtures[season].count*sizeof(Vtx));
    unsigned palette_count=0;
    Gfx* dl=pc_structure_back_build_spec(spec,caches+season);
    CHECK(dl!=NULL,"loaded post office receives closures");if(!dl)return;
    CHECK(spec->patch_count==6,"only two walls, four eaves and two awning surfaces");
    CHECK(spec->palette==NULL,"caps retain the actor's live palette segment");
    int patch=-1,nv=0,ended=0,triangles[6]={0},edge[6][8][8]={0},balance[6][8][8]={0};
    double areas[6]={0};Vtx* vertices=NULL;
    for(int i=0;i<128;i++){
        u32 word=dl[i].words.w0,op=word>>24;
        if(op==G_ENDDL){ended=1;break;}
        if(op==G_LOADTLUT){
            CHECK(dl[i].words.w1==0x08000000u,"live palette remains a segmented address, never a tagged host pointer");
            ++palette_count;
        }
        if(op==G_SETTIMG){
            u8* texture=pointer(dl[i].words.w1);
            CHECK((word&1023)+1==32&&(((word>>10)&255)+1)*4==32,"bounded 32x32 indexed material");
            for(int p=0;p<1024;p++){int index=(texture[p/2]>>((p&1)?0:4))&15;const u8* pal=rel+fixtures[season].palette+2*index;unsigned color=(pal[0]<<8)|pal[1];CHECK((color&0x8000)||((color>>12)&7)==7,"every texel opaque in actual seasonal palette");}
        }
        if(op==G_VTX){
            ++patch;nv=(word>>12)&255;vertices=pointer(dl[i].words.w1);
            CHECK(patch<6&&nv<=8,"display list stays within patch/cache bounds");if(patch>=6)break;
            Loop loop[2];int n=outlines(patch,loop);
            CHECK(nv==4*n,"all independent contour points retained");
            CHECK(spec->patches[patch].crop[2]<128&&spec->patches[patch].crop[3]<32,"material crop in original atlas bounds");
            for(int a=0;a<nv;a++){
                int found=0;for(int l=0;l<n;l++)for(int k=0;k<4;k++)found|=same(at(vertices+a),loop[l].p[k]);
                CHECK(found,"vertex is on an independently derived missing contour");
                CHECK(spec->patches[patch].vertices[a].index<fixtures[season].count,"original source index in bounds");
                CHECK(vertices[a].v.tc[0]>=0&&vertices[a].v.tc[0]<8192&&vertices[a].v.tc[1]>=0&&vertices[a].v.tc[1]<8192,"repeated UVs bounded");
            }
        }
        if(op==G_TRI1){
            int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};
            int valid=vertices&&t[0]<nv&&t[1]<nv&&t[2]<nv;CHECK(valid,"triangle references active vertex cache");if(!valid)continue;
            Loop loop[2];int nl=outlines(patch,loop),belongs=0;
            for(int l=0;l<nl;l++){int count=0;for(int a=0;a<3;a++)for(int k=0;k<4;k++)count+=same(at(vertices+t[a]),loop[l].p[k]);belongs|=count==3;}
            CHECK(belongs,"triangles never bridge disjoint opposite eaves");
            Point n=cross(sub(at(vertices+t[1]),at(vertices+t[0])),sub(at(vertices+t[2]),at(vertices+t[0])));
            CHECK(dot(n,loop[0].normal)>0,"positive-area outward winding, including downward undersides");
            areas[patch]+=sqrt(dot(n,n))/2;triangles[patch]++;
            for(int j=0;j<3;j++){int a=t[j],b=t[(j+1)%3];if(a<b){edge[patch][a][b]++;balance[patch][a][b]++;}else{edge[patch][b][a]++;balance[patch][b][a]--;}}
        }
    }
    CHECK(palette_count==6,"each patch binds the live palette");
    CHECK(ended&&patch==5,"all six bounded display lists terminate");
    for(int p=0;p<6;p++){
        Loop loop[2];int count=outlines(p,loop);double target=0;Vtx* v=caches[season].vertices[p];
        for(int i=0;i<count;i++)target+=area(loop+i);
        CHECK(fabs(target-areas[p])<0.1,"triangles cover exact missing area without overlapping original faces");
        CHECK(triangles[p]==2*count,"minimal two triangles per quad");
        for(int a=0;a<count*4;a++)for(int b=a+1;b<count*4;b++){
            int boundary=0;for(int l=0;l<count;l++)for(int k=0;k<4;k++)boundary|=(same(at(v+a),loop[l].p[k])&&same(at(v+b),loop[l].p[(k+1)%4]))||(same(at(v+b),loop[l].p[k])&&same(at(v+a),loop[l].p[(k+1)%4]));
            if(boundary)CHECK(edge[p][a][b]==1,"each missing boundary edge sealed once");
            else if(edge[p][a][b])CHECK(edge[p][a][b]==2&&balance[p][a][b]==0,"interior diagonals shared with opposite winding");
        }
    }
    CHECK(!memcmp(originals,spec->source,fixtures[season].count*sizeof(Vtx)),"original walls, roof, doors, signs and light vertices unchanged");
    CHECK(pc_structure_back_build_spec(spec,caches+season)==dl,"cache stable across draws and stereo eyes");
    printf("%s: six patches and sixteen triangles verified\n",fixtures[season].name);
}

static void verify_light(void){
    Gfx* original=season?obj_w_yubinkyoku_light_model:obj_s_yubinkyoku_light_model;
    Vtx* source=season?obj_w_yubinkyoku_v+40:obj_s_yubinkyoku_v+37;
    Vtx saved[12];Gfx commands[8];memcpy(saved,source,sizeof(saved));memcpy(commands,original,sizeof(commands));
    original[4].words.w0=0;
    CHECK(pc_post_office_light_for_back(original)==original,"malformed vertex opcode cannot populate light cache");
    original[4]=commands[4];original[4].words.w0&=~(255u<<12);
    CHECK(pc_post_office_light_for_back(original)==original,"wrong vertex count cannot populate light cache");
    original[4]=commands[4];original[7].words.w0=0;
    CHECK(pc_post_office_light_for_back(original)==original,"missing bounded terminator rejects light copy");
    original[7]=commands[7];source[1].v.ob[2]=8000;
    CHECK(pc_post_office_light_for_back(original)==original,"unexpected original geometry remains untouched");
    source[1]=saved[1];
    Gfx* copy=pc_post_office_light_for_back(original);
    CHECK(copy!=original,"loaded known light gets a separate copy");
    Vtx* vertices=pointer(copy[4].words.w1);
    CHECK(vertices!=source,"clipped light vertices have independent storage");
    for(int i=0;i<8;i++)if(i!=4)CHECK(!memcmp(copy+i,commands+i,sizeof(Gfx)),"original pipeline/tint inheritance and triangles preserved");
    CHECK(copy[4].words.w0==commands[4].words.w0,"original count and cache destination retained");
    for(int i=0;i<12;i++){
        Vtx expected=saved[i];if(i==1)expected.v.ob[2]=7768;
        CHECK(!memcmp(vertices+i,&expected,sizeof(Vtx)),"only the protruding corner Z changes; normals/UVs/alpha unchanged");
        double wall=7707.0+250.0*vertices[i].v.ob[1]/12600.0;
        CHECK(vertices[i].v.ob[2]<wall,"all light vertices lie strictly inside new positive-Z wall");
        CHECK(vertices[i].v.ob[0]>-9270,"all light vertices lie inside new negative-X wall");
    }
    CHECK(7707.0+250.0*saved[1].v.ob[1]/12600.0<saved[1].v.ob[2],"original corner actually protrudes through new wall");
    CHECK(7832-vertices[1].v.ob[2]==64,"clipped corner has robust depth separation");
    CHECK(!memcmp(saved,source,sizeof(saved))&&!memcmp(commands,original,sizeof(commands)),"all original light bytes untouched after building cache");
    CHECK(pc_post_office_light_for_back(original)==copy,"light cache remains stable across frames and eyes");
}

int main(void){
    for(season=0;season<2;season++)CHECK(pc_structure_back_build_spec(pc_post_office_back_specs+season,caches+season)==NULL,"unloaded original geometry defers all repairs");
    Gfx* out=(Gfx*)1;cKF_Skeleton_R_c unknown={0};
    CHECK(pc_post_office_back_lookup(&cKF_bs_r_obj_s_yubinkyoku,&out)&&out==NULL,"unloaded summer post office recognized without mirror fallback");
    CHECK(pc_post_office_back_lookup(&cKF_bs_r_obj_w_yubinkyoku,&out)&&out==NULL,"unloaded winter post office recognized without mirror fallback");
    CHECK(!pc_post_office_back_lookup(&unknown,&out)&&out==NULL,"unrelated skeleton untouched");
    CHECK(pc_post_office_light_for_back(obj_s_yubinkyoku_light_model)==obj_s_yubinkyoku_light_model&&pc_post_office_light_for_back(obj_w_yubinkyoku_light_model)==obj_w_yubinkyoku_light_model,"unloaded light source deferred");
    CHECK(pc_post_office_light_for_back(NULL)==NULL&&pc_post_office_light_for_back((Gfx*)&unknown)==(Gfx*)&unknown,"unknown and callback-suppressed light lists unchanged");
    if(!pc_disc_init())return 2;u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);i++){Asset* a=assets+i;memcpy(a->data,rel+a->offset,a->size);if(a->vertices)for(unsigned n=0;n<a->size;n+=16){u8* p=(u8*)a->data+n;for(int k=0;k<12;k+=2){u8 t=p[k];p[k]=p[k+1];p[k+1]=t;}}}
    for(season=0;season<2;season++){verify(rel);verify_light();}
    free(rel);pc_disc_shutdown();printf("Post office backs: %d checks, %d failures\n",checks,failures);return failures!=0;
}
