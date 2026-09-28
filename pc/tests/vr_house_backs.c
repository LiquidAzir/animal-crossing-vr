/* Exercise the production residence fill against original disc assets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "c_keyframe.h"
#include "pc_disc.h"
#include "src/pc_house_back.c_inc"

int g_pc_verbose;
/* Only an unused original model asset-loader wrapper references this symbol. */
void pc_load_asset(const char* path, void* out, unsigned size, unsigned offset, int source, int swap) {
    (void)path; (void)out; (void)size; (void)offset; (void)source; (void)swap;
    abort();
}

static int checks, failures;
static const char* current_model = "lookup";
#define CHECK(x,msg) do { ++checks; if (!(x)) { ++failures; \
    printf("FAIL %s: %s (line %d)\n", current_model, msg, __LINE__); } } while (0)
typedef struct { void* data; unsigned size, offset; int vertices; } Asset;
typedef struct {
    const char* name;
    cKF_Skeleton_R_c* skeleton;
    Vtx* source;
    int source_count, polygon_count, polygon_sizes[3], polygons[3][12];
    int palette_count;
    unsigned palettes[12];
} Fixture;
#include "house_fixtures.inc"

static void* pointer(u32 packed) {
    uintptr_t p = pc_gbi_unpack_runtime_ptr(packed);
    return (void*)(p ? p : packed & ~1u);
}
static double cross_yz(const Vtx* a, const Vtx* b, const Vtx* c) {
    return (double)(b->v.ob[1]-a->v.ob[1])*(c->v.ob[2]-a->v.ob[2]) -
           (double)(b->v.ob[2]-a->v.ob[2])*(c->v.ob[1]-a->v.ob[1]);
}
static int same_position(const Vtx* a, const Vtx* b) {
    return memcmp(a->v.ob, b->v.ob, sizeof(a->v.ob)) == 0;
}
static int matches_boundary(const Fixture* f, const Vtx* v) {
    for (int p=0; p<f->polygon_count; ++p)
        for (int i=0; i<f->polygon_sizes[p]; ++i)
            if (same_position(v, &f->source[f->polygons[p][i]])) return 1;
    return 0;
}
static int on_polygon(const Fixture* f, int polygon, const Vtx* v) {
    for (int i=0;i<f->polygon_sizes[polygon];++i)
        if(same_position(v,&f->source[f->polygons[polygon][i]]))return 1;
    return 0;
}
static int common_polygon(const Fixture* f, const Vtx* a, const Vtx* b, const Vtx* c, const Vtx* d) {
    for(int p=0;p<f->polygon_count;++p)
        if(on_polygon(f,p,a)&&on_polygon(f,p,b)&&on_polygon(f,p,c)&&on_polygon(f,p,d))return 1;
    return 0;
}
static int is_boundary_edge(const Fixture* f, const Vtx* a, const Vtx* b) {
    for (int p=0; p<f->polygon_count; ++p) {
        int n=f->polygon_sizes[p];
        for (int i=0; i<n; ++i) {
            const Vtx* x=&f->source[f->polygons[p][i]];
            const Vtx* y=&f->source[f->polygons[p][(i+1)%n]];
            if ((same_position(a,x)&&same_position(b,y)) ||
                (same_position(a,y)&&same_position(b,x))) return 1;
        }
    }
    return 0;
}
static double expected_area(const Fixture* f) {
    double area=0;
    for (int p=0; p<f->polygon_count; ++p) {
        double part=0;
        int n=f->polygon_sizes[p];
        for (int i=0; i<n; ++i) {
            const Vtx* a=&f->source[f->polygons[p][i]];
            const Vtx* b=&f->source[f->polygons[p][(i+1)%n]];
            part+=(double)a->v.ob[1]*b->v.ob[2]-(double)a->v.ob[2]*b->v.ob[1];
        }
        area+=fabs(part);
    }
    return area;
}
static int expected_point(const Fixture* f, double y, double z) {
    for (int p=0; p<f->polygon_count; ++p) {
        int inside=0, n=f->polygon_sizes[p];
        for (int i=0,j=n-1; i<n; j=i++) {
            const Vtx* a=&f->source[f->polygons[p][i]];
            const Vtx* b=&f->source[f->polygons[p][j]];
            if ((a->v.ob[2]>z)!=(b->v.ob[2]>z) &&
                y<(double)(b->v.ob[1]-a->v.ob[1])*(z-a->v.ob[2])/(b->v.ob[2]-a->v.ob[2])+a->v.ob[1])
                inside=!inside;
        }
        if (inside) return 1;
    }
    return 0;
}

static void verify(Fixture* f, const u8* rel) {
    Gfx* list=NULL;
    Vtx* v=NULL;
    u8* texture=NULL;
    int nv=0, nt=0, ended=0, edges[12][12]={{0}}, winding[12][12]={{0}};
    int palette=0, clamp=0;
    double area=0;
    Vtx original[256];
    current_model=f->name;
    CHECK(f->source_count<=256,"fixture backup fits");
    memcpy(original,f->source,f->source_count*sizeof(Vtx));
    CHECK(pc_house_back_lookup(f->skeleton,&list)==1 && list!=NULL,"loaded known house gets rear geometry");
    if (!list) return;
    for (int d=0; d<32; ++d) {
        u32 op=list[d].words.w0>>24, w=list[d].words.w0;
        if (op==G_ENDDL) { ended=1; break; }
        if (op==G_LOADTLUT) {
            palette=list[d].words.w1==(ANIME_1_TXT_SEG<<24);
        } else if (op==G_SETTIMG) {
            CHECK(((w>>21)&7)==G_IM_FMT_CI && ((w>>19)&3)==G_IM_SIZ_4b,"patch retains indexed palette format");
            CHECK((w&1023)+1==64 && (((w>>10)&255)+1)*4==64,"bounded 64 by 64 wall material");
            texture=pointer(list[d].words.w1);
        } else if (op==G_SETTILE_DOLPHIN) {
            clamp=((w>>10)&3)==GX_CLAMP && ((w>>8)&3)==GX_CLAMP;
        } else if (op==G_VTX) {
            nv=(w>>12)&255;
            CHECK(v==NULL,"one self-contained vertex load");
            CHECK(nv>=3 && nv<=12,"bounded rear vertex count");
            v=pointer(list[d].words.w1);
        } else if (op==G_TRI1) {
            int tri[3]={(w>>17)&127,(w>>9)&127,(w>>1)&127};
            int valid=v && nv<=12 && tri[0]<nv && tri[1]<nv && tri[2]<nv;
            CHECK(valid,"triangle indices reference the loaded rear vertices");
            if (!valid) continue;
            double a=cross_yz(&v[tri[0]],&v[tri[1]],&v[tri[2]]);
            int underside=f->polygon_count==3 && on_polygon(f,1,&v[tri[0]]) &&
                          on_polygon(f,1,&v[tri[1]]) && on_polygon(f,1,&v[tri[2]]);
            double normal_y=(double)(v[tri[1]].v.ob[2]-v[tri[0]].v.ob[2])*(v[tri[2]].v.ob[0]-v[tri[0]].v.ob[0]) -
                            (double)(v[tri[1]].v.ob[0]-v[tri[0]].v.ob[0])*(v[tri[2]].v.ob[2]-v[tri[0]].v.ob[2]);
            CHECK(underside ? normal_y<0 && a!=0 : a<0,"triangles face rearward or downward under the authored eave");
            area+=fabs(a);
            CHECK(expected_point(f,(v[tri[0]].v.ob[1]+v[tri[1]].v.ob[1]+v[tri[2]].v.ob[1])/3.0,
                                   (v[tri[0]].v.ob[2]+v[tri[1]].v.ob[2]+v[tri[2]].v.ob[2])/3.0),
                  "triangle lies inside the original rear opening");
            for (int i=0;i<3;++i) {
                int a=tri[i], b=tri[(i+1)%3];
                if(a<b){edges[a][b]++;winding[a][b]++;}
                else{edges[b][a]++;winding[b][a]--;}
            }
            ++nt;
        }
    }
    CHECK(ended && nt>=2 && nt<=8,"small terminated rear-only display list");
    CHECK(palette,"reuse live house palette through segment 8");
    CHECK(clamp,"filtering cannot wrap into another atlas region");
    CHECK(fabs(area-expected_area(f))<0.5,"triangles exactly cover original rear contour area");
    if (v && nv<=12) {
        int boundary_edges=0, expected_edges=0;
        for(int p=0;p<f->polygon_count;++p)expected_edges+=f->polygon_sizes[p];
        for(int i=0;i<nv;++i) {
            CHECK(matches_boundary(f,&v[i]),"new geometry uses exact original opening vertices");
            int len=0;for(int j=0;j<3;++j)len+=(int)v[i].n.n[j]*v[i].n.n[j];
            int outward=v[i].n.n[0]<0 || (f->polygon_count==3 && on_polygon(f,1,&v[i]) && v[i].n.n[1]<0);
            CHECK(outward && len>120*120 && len<=128*128,"normalized outward lighting normals");
            CHECK(v[i].v.tc[0]>=0 && v[i].v.tc[0]<=2016 && v[i].v.tc[1]>=0 && v[i].v.tc[1]<=2016,"UVs stay within opaque material");
            CHECK(v[i].n.a==255,"rear vertex alpha is opaque");
            for(int j=i+1;j<nv;++j) {
                if (!edges[i][j]) continue;
                int border=is_boundary_edge(f,&v[i],&v[j]);
                CHECK(edges[i][j]==(border?1:2),"rear boundary exact and interior edges paired");
                if(border)++boundary_edges;
                else CHECK(winding[i][j]==0,"interior triangles meet with consistent winding");
                for(int k=0;k<nv;++k)for(int l=k+1;l<nv;++l) {
                    if(!edges[k][l] || i==k || i==l || j==k || j==l)continue;
                    if(!common_polygon(f,&v[i],&v[j],&v[k],&v[l]))continue;
                    double a=cross_yz(&v[i],&v[j],&v[k]),b=cross_yz(&v[i],&v[j],&v[l]);
                    double c=cross_yz(&v[k],&v[l],&v[i]),d=cross_yz(&v[k],&v[l],&v[j]);
                    CHECK(!(a*b<0 && c*d<0),"rear triangles do not cross or overlap");
                }
            }
        }
        CHECK(boundary_edges==expected_edges,"every original rear contour edge is closed");
    }
    CHECK(texture!=NULL,"wall patch image present");
    if(texture) {
        int used[16]={0};
        for(int i=0;i<2048;++i){used[texture[i]>>4]=1;used[texture[i]&15]=1;}
        int colors=0;for(int i=0;i<16;++i)colors+=used[i];
        CHECK(colors>=3,"rear wall keeps material detail instead of a flat color");
        for(int p=0;p<f->palette_count;++p)for(int i=0;i<16;++i)if(used[i]) {
            unsigned val=(rel[f->palettes[p]+i*2]<<8)|rel[f->palettes[p]+i*2+1];
            CHECK((val&0x8000)||((val>>12)&7)==7,"every used texel is opaque under every seasonal palette");
        }
    }
    Gfx* cached=NULL;
    CHECK(pc_house_back_lookup(f->skeleton,&cached)==1 && cached==list,"repeat draws reuse stable cached geometry");
    CHECK(memcmp(original,f->source,f->source_count*sizeof(Vtx))==0,"original model vertices unchanged");
    printf("%s: %d rear vertices, %d triangles, %d palettes verified\n",f->name,nv,nt,f->palette_count);
}

int main(void) {
    Gfx* dl=(Gfx*)1;
    cKF_Skeleton_R_c unknown={0};
    CHECK(pc_house_back_lookup(&unknown,&dl)==0 && dl==NULL,"unknown models left alone");
    CHECK(pc_house_back_lookup(NULL,&dl)==0 && dl==NULL,"null skeleton left alone");
    for(unsigned i=0;i<ARRAY_COUNT(fixtures);++i) {
        dl=(Gfx*)1;
        CHECK(pc_house_back_lookup(fixtures[i].skeleton,&dl)==1 && dl==NULL,"unloaded residences defer without old-shell fallback");
    }
    if(!pc_disc_init()){puts("No game disc found for residence verification");return 2;}
    u8* rel=pc_disc_extract_rel();if(!rel)return 3;
    for(unsigned i=0;i<ARRAY_COUNT(assets);++i) {
        Asset* a=&assets[i];memcpy(a->data,rel+a->offset,a->size);
        if(a->vertices)for(unsigned j=0;j<a->size;j+=16)for(int k=0;k<12;k+=2) {
            u8* p=(u8*)a->data+j+k;u8 t=p[0];p[0]=p[1];p[1]=t;
        }
    }
    for(unsigned i=0;i<ARRAY_COUNT(fixtures);++i)verify(&fixtures[i],rel);
    for(unsigned i=0;i<ARRAY_COUNT(assets);++i)if(!assets[i].vertices)
        CHECK(memcmp(assets[i].data,rel+assets[i].offset,assets[i].size)==0,"original texture atlases unchanged");
    free(rel);pc_disc_shutdown();
    printf("Residence rear geometry: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
