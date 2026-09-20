/* Native production display-list checks using vertices from the user's disc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "pc_disc.h"
#include "src/bg_item/bg_item_stone_back.c_inc"

int g_pc_verbose;
static int checks, failures;
#define CHECK(x,msg) do {checks++;if(!(x)){printf("FAIL: %s (%d)\n",msg,__LINE__);failures++;}}while(0)
typedef struct {const char* name; Vtx* vertices; Gfx* model; unsigned offset; int nv,nt; const u8* triangles;} Fixture;
#include "rock_fixtures.inc"
static void edge(int counts[15][15],int winding[15][15],int a,int b){
    if(a<b){counts[a][b]++;winding[a][b]++;}
    else {counts[b][a]++;winding[b][a]--;}
}
int main(void){
    CHECK(pc_bg_stone_back_dl(obj_s_stoneA_gfx_model)==NULL,"unloaded asset is deferred");
    if(!pc_disc_init()){puts("No game disc found for geometry verification");return 2;}
    u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned m=0;m<sizeof(fixtures)/sizeof(fixtures[0]);m++){
        Fixture* f=&fixtures[m]; Vtx original[15];
        for(int i=0;i<f->nv;i++){
            memcpy(&f->vertices[i],rel+f->offset+i*16,16);
            unsigned char* p=(unsigned char*)&f->vertices[i];
            for(int j=0;j<12;j+=2){unsigned char t=p[j];p[j]=p[j+1];p[j+1]=t;}
        }
        memcpy(original,f->vertices,f->nv*sizeof(Vtx));
        Gfx* dl=pc_bg_stone_back_dl(f->model);
        CHECK(dl!=NULL,"known seasonal rock gets rear faces");if(!dl)continue;
        CHECK((dl[0].words.w0>>24)==G_VTX,"rear list loads its own vertices");
        uintptr_t addr=pc_gbi_unpack_runtime_ptr(dl[0].words.w1);
        if(!addr)addr=dl[0].words.w1 & ~1u;
        Vtx* v=(Vtx*)addr;
        int counts[15][15]={{0}}, winding[15][15]={{0}}, used[15]={0};
        for(int t=0;t<f->nt;t++){
            const u8* tri=f->triangles+t*3;
            for(int j=0;j<3;j++)edge(counts,winding,tri[j],tri[(j+1)%3]);
        }
        int n=0;
        for(int d=1;d<7 && (dl[d].words.w0>>24)!=G_ENDDL;d++){
            CHECK((dl[d].words.w0>>24)==G_TRI1,"rear list contains only triangles");
            u32 w=dl[d].words.w0;
            int tri[3]={(w>>17)&127,(w>>9)&127,(w>>1)&127};
            int valid=tri[0]<f->nv && tri[1]<f->nv && tri[2]<f->nv;
            CHECK(valid,"triangle indices in bounds");if(!valid)continue;
            for(int j=0;j<3;j++){edge(counts,winding,tri[j],tri[(j+1)%3]);used[tri[j]]=1;}
            float ax=v[tri[1]].v.ob[0]-v[tri[0]].v.ob[0],ay=v[tri[1]].v.ob[1]-v[tri[0]].v.ob[1];
            float bx=v[tri[2]].v.ob[0]-v[tri[0]].v.ob[0],by=v[tri[2]].v.ob[1]-v[tri[0]].v.ob[1];
            CHECK(ax*by-ay*bx<0,"rear triangles face backward without degeneracy");n++;
        }
        CHECK(n>=2 && n<=5 && (dl[n+1].words.w0>>24)==G_ENDDL,"small bounded terminated display list");
        for(int a=0;a<f->nv;a++){
            CHECK(memcmp(v[a].v.ob,original[a].v.ob,6)==0,"new mesh exactly shares the original boundary positions");
            CHECK(v[a].v.tc[0]>=0 && v[a].v.tc[0]<=1024 && v[a].v.tc[1]>=0 && v[a].v.tc[1]<=1024,"rear UVs cover a valid texture tile");
            if(used[a]){
                float len=0;for(int j=0;j<3;j++)len+=v[a].n.n[j]*v[a].n.n[j];
                CHECK(v[a].n.n[2]<0 && len>120*120 && len<=128*128,"rear lighting normals point out and remain normalized");
            }
            for(int b=a+1;b<f->nv;b++){
                if(!counts[a][b])continue;
                CHECK(counts[a][b]<=2,"no duplicate surfaces or nonmanifold edges");
                if(original[a].v.ob[1]!=0 || original[b].v.ob[1]!=0)
                    CHECK(counts[a][b]==2 && winding[a][b]==0,"all above-ground holes closed with consistent winding");
            }
        }
        CHECK(memcmp(original,f->vertices,f->nv*sizeof(Vtx))==0,"original model vertices remain untouched");
        CHECK(pc_bg_stone_back_dl(f->model)==dl,"repeat draws reuse stable cached geometry");
        printf("%s: %d new faces\n",f->name,n);
    }
    Gfx unknown[1];CHECK(pc_bg_stone_back_dl(unknown)==NULL,"unknown meshes left alone");
    free(rel);pc_disc_shutdown();printf("Rock geometry: %d checks, %d failures\n",checks,failures);return failures!=0;
}
