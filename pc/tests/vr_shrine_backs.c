/* Production rear lists checked against original ROM-backed fountain geometry. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "pc_disc.h"
#include "src/data/model/obj_s_shrine.c"
#include "src/actor/pc_shrine_back.c_inc"

int g_pc_verbose;
static u8* rel;
static int checks, failures;
#define CHECK(x,msg) do { ++checks; if (!(x)) { printf("FAIL: %s (%d)\n",msg,__LINE__); ++failures; } } while (0)
void pc_load_asset(const char* n, void* dst, unsigned size, unsigned off, int dol, int kind) {
    (void)n; CHECK(!dol,"fixture comes from the REL"); memcpy(dst,rel+off,size);
    if (kind == 1) for (unsigned i=0;i<size;i+=2) {u8* p=(u8*)dst+i;u8 a=p[0];p[0]=p[1];p[1]=a;}
}
typedef struct {
    const char* name; Vtx* vertices; Gfx* model; const u16* triangles; int triangle_count;
    unsigned vertex_offset, texture_offset; u16* palette; int figure;
} Fixture;
#include "shrine_fixtures.inc"
static int counts[529][529], direction[529][529], changed[529][529];
static int canonical(Vtx* v, const s16* p) {
    for (int i=0;i<529;i++) if (!memcmp(v[i].v.ob,p,3*sizeof(s16))) return i;
    return -1;
}
static void edge(int a,int b,int patch) {
    if(a==b)return;
    if(a<b){counts[a][b]++;direction[a][b]++;if(patch)changed[a][b]=1;}
    else {counts[b][a]++;direction[b][a]--;if(patch)changed[b][a]=1;}
}
int main(void) {
    CHECK(pc_shrine_back_dl(obj_s_shrine_trunk_model)==NULL,"defer until original assets load");
    if(!pc_disc_init() || !(rel=pc_disc_extract_rel()))return 2;
    _pc_load_src_data_model_obj_s_shrine_c();
    for(unsigned fidx=0;fidx<sizeof(fixtures)/sizeof(fixtures[0]);fidx++){
        Fixture* f=fixtures+fidx;Vtx original[529];
        memcpy(f->vertices,rel+f->vertex_offset,sizeof(original));
        for(int i=0;i<529;i++)for(int j=0;j<12;j+=2){u8* p=(u8*)&f->vertices[i]+j;u8 a=p[0];p[0]=p[1];p[1]=a;}
        memcpy(original,f->vertices,sizeof(original));
        memset(counts,0,sizeof(counts));memset(direction,0,sizeof(direction));memset(changed,0,sizeof(changed));
        for(int i=0;i<f->triangle_count;i++){
            int ids[3];for(int j=0;j<3;j++)ids[j]=canonical(original,original[f->triangles[i*3+j]].v.ob);
            for(int j=0;j<3;j++)edge(ids[j],ids[(j+1)%3],0);
        }
        Gfx* dl=pc_shrine_back_dl(f->model);
        CHECK(dl!=NULL,"seasonal mesh has a rear closure");if(!dl)continue;
        CHECK(dl[0].words.w0>>24==G_VTX,"rear list only adds vertices and triangles");
        uintptr_t addr=pc_gbi_unpack_runtime_ptr(dl[0].words.w1);if(!addr)addr=dl[0].words.w1&~1u;
        Vtx* v=(Vtx*)addr;int nv=(dl[0].words.w0>>12)&255;int expected=f->figure?8:23;
        CHECK(nv==expected,"bounded rear vertex set");
        for(int i=0;i<nv;i++){
            CHECK(canonical(original,v[i].v.ob)>=0,"rear shares an exact original boundary position");
            CHECK(v[i].n.n[2]<0 || (!f->figure && v[i].n.n[1]<0),"rear lighting faces backward or below bent branches");
            float len=0;for(int a=0;a<3;a++)len+=v[i].n.n[a]*v[i].n.n[a];
            CHECK(len>120*120 && len<128*128,"rear lighting normals remain normalized");
            CHECK(v[i].v.tc[0]>0 && v[i].v.tc[0]<128*32 && v[i].v.tc[1]>0 && v[i].v.tc[1]<32*32,"rear UVs remain inside original atlas");
        }
        int n=0;
        while(n<18 && (dl[n+1].words.w0>>24)!=G_ENDDL){
            u32 word=dl[n+1].words.w0;CHECK(word>>24==G_TRI1,"closure inherits original material without replaying original parts");
            int t[3]={(word>>17)&127,(word>>9)&127,(word>>1)&127};int ids[3];
            CHECK(t[0]<nv && t[1]<nv && t[2]<nv,"rear indices in range");
            for(int j=0;j<3;j++)ids[j]=canonical(original,v[t[j]].v.ob);
            float a[3],b[3],cross[3],normal[3]={0};
            for(int axis=0;axis<3;axis++){
                a[axis]=v[t[1]].v.ob[axis]-v[t[0]].v.ob[axis];
                b[axis]=v[t[2]].v.ob[axis]-v[t[0]].v.ob[axis];
                for(int j=0;j<3;j++)normal[axis]+=v[t[j]].n.n[axis];
            }
            cross[0]=a[1]*b[2]-a[2]*b[1];cross[1]=a[2]*b[0]-a[0]*b[2];cross[2]=a[0]*b[1]-a[1]*b[0];
            CHECK(cross[0]*normal[0]+cross[1]*normal[1]+cross[2]*normal[2]>0,"nondegenerate face agrees with its outward lighting normals");
            for(int j=0;j<3;j++)edge(ids[j],ids[(j+1)%3],1);n++;
        }
        CHECK(n==(f->figure?6:17),"expected bounded face count");
        for(int a=0;a<529;a++)for(int b=a+1;b<529;b++)if(changed[a][b]){
            if(counts[a][b]>2)printf("  edge %d,%d count=%d\n",a,b,counts[a][b]);
            CHECK(counts[a][b]<=2,"no duplicate or nonmanifold rear surface");
            if(original[a].v.ob[1]!=0 || original[b].v.ob[1]!=0)
                CHECK(counts[a][b]==2 && direction[a][b]==0,"all new above-ground edges close existing rim or join another new face");
        }
        /* Include the bilinear-filter border in the opaque atlas check. */
        int x0=f->figure?113:71,x1=f->figure?127:125;
        int y0=f->figure?18:2,y1=f->figure?31:30;
        const u8* tex=rel+f->texture_offset;
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
            int off=((y/8)*16+x/8)*32+(y%8)*4+(x%8)/2;
            int index=(tex[off]>>(x%2?0:4))&15;u16 color=f->palette[index];
            int alpha=(color&0x8000)?255:((color>>12)&7)*255/7;
            CHECK(alpha>=144,"rear atlas sample and filter border contain no transparent holes");
        }
        CHECK(!memcmp(original,f->vertices,sizeof(original)),"original fountain vertices untouched");
        CHECK(pc_shrine_back_dl(f->model)==dl,"repeat draws reuse cached closure");
        printf("%s: %d rear faces, original material\n",f->name,n);
    }
    CHECK(pc_shrine_back_dl(obj_s_shrine_base_model)==NULL,"complete basin is never duplicated");
    CHECK(pc_shrine_back_dl(obj_w_shrine_base_model)==NULL,"winter basin is never duplicated");
    CHECK(pc_shrine_back_dl(obj_s_shrine_leaf_model)==NULL,"foliage billboards untouched");
    CHECK(pc_shrine_back_dl(obj_w_shrine_leaf_model)==NULL,"winter foliage untouched");
    CHECK(pc_shrine_back_dl(obj_s_shrine_statue_model)==NULL,"statue untouched");
    CHECK(pc_shrine_back_dl(obj_s_shrine_water_model)==NULL,"water untouched");
    free(rel);pc_disc_shutdown();printf("Fountain geometry: %d checks, %d failures\n",checks,failures);return failures!=0;
}
