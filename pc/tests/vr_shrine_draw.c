/* Exercise the actual fountain actor submission with original model lists.
 * Rendering and scroll allocation are seams; segment ordering is not mocked. */
#include <stdio.h>
#include <string.h>
#include "ac_shrine.h"
#include "bg_item_h.h"
#include "m_field_make.h"
#include "m_camera2.h"
#include "sys_matrix.h"
#include "libforest/gbi_extensions.h"

common_data_t common_data;
static mFM_fdinfo_c field;
mFM_fdinfo_c* g_fdinfo=&field;
u8 obj_s_tree3_leaf_tex[8],obj_w_tree_leaf_tex[8],obj_f_tree_leaf_tex[8];
static bIT_ShadowData_c aSHR_shadow_data;
static Gfx scrolls[3][1],rear[1],previous_actor_list[1];
static Mtx matrix;
static int checks,failures,scroll_count,shadow_count,repair,culled;
static int scroll_args[3][10];
#define CHECK(c,msg) do{checks++;if(!(c)){if(failures++<12)printf("FAIL: %s (%d)\n",msg,__LINE__);}}while(0)
/* This submission test reads commands only; separate ROM-backed tests load
 * and verify the vertex and artwork data. */
void pc_load_asset(const char* path,void* dest,unsigned size,unsigned offset,int source,int swap){}
int Camera2_CheckCullingMode(void){return culled;}
int Camera2_CheckEnterCullingArea(f32 x,f32 z,f32 width){return culled;}
void _texture_z_light_fog_prim_npc(GRAPH* g){}
void _texture_z_light_fog_prim_xlu(GRAPH* g){}
void _texture_z_light_fog_prim_shadow(GRAPH* g){}
Mtx* _Matrix_to_Mtx_new(GRAPH* g){return &matrix;}
Gfx* two_tex_scroll_dolphin(GRAPH* g,int tile1,int x1,int y1,int w1,int h1,int tile2,int x2,int y2,int w2,int h2){
    int args[]={tile1,x1,y1,w1,h1,tile2,x2,y2,w2,h2};
    CHECK(scroll_count<3,"three original scroll allocations only");
    if(scroll_count>=3)return scrolls[0];
    memcpy(scroll_args[scroll_count],args,sizeof(args));
    gSPEndDisplayList(scrolls[scroll_count]);return scrolls[scroll_count++];
}
int cKF_shell_wanted(void){return repair;}
Gfx* pc_shrine_back_dl(Gfx* original){return rear;}
static void shadow(GAME* game,bIT_ShadowData_c* data,int arg){
    shadow_count++;CHECK(data==&aSHR_shadow_data&&arg==FALSE,"original shadow callback arguments retained");
}

#include "src/actor/ac_shrine_draw.c_inc"

static void* pointer(u32 p){uintptr_t v=pc_gbi_unpack_runtime_ptr(p);return(void*)(v?v:p&~1u);}
static void scan(Gfx* stream,int count,Gfx* bubble,Gfx* const* expected,int expected_count){
    /* Each stream must bind its own scroll before use regardless of an earlier
     * actor's segment B. The real graph executes opaque before translucent. */
    void* segments[16]={0};segments[11]=previous_actor_list;
    int draws=0;
    for(int i=0;i<count;i++){
        unsigned op=stream[i].words.w0>>24;
        if(op==G_MOVEWORD&&((stream[i].words.w0>>16)&255)==G_MW_SEGMENT){
            unsigned segment=(stream[i].words.w0&65535)/4;
            if(segment<16)segments[segment]=pointer(stream[i].words.w1);
        }
        if(op==G_DL){
            Gfx* dl=pointer(stream[i].words.w1);
            CHECK(draws<expected_count&&dl==expected[draws],"original model order and rear placement unchanged");draws++;
            if(dl==bubble){
                int nested=0;
                for(int d=0;d<32&&(dl[d].words.w0>>24)!=G_ENDDL;d++)if((dl[d].words.w0>>24)==G_DL){
                    CHECK(dl[d].words.w1==anime_4_txt,"bubble reads scrolling display list through segment B");
                    CHECK(segments[11]==scrolls[2],"fountain scroll B bound before bubble draw, independent of earlier actors");nested++;
                }
                CHECK(nested==1,"original bubble has exactly one scrolling-list call");
            }
        }
    }
    CHECK(draws==expected_count,"all original geometry drawn exactly once per intended stream");
}

int main(void){
    static bIT_Clip_c clip;
    common_data.clip.bg_item_clip=&clip;clip.draw_shadow_proc=shadow;
    gSPEndDisplayList(rear);gSPEndDisplayList(previous_actor_list);
    for(int winter=0;winter<2;winter++)for(repair=0;repair<2;repair++)for(int frame=0;frame<3;frame++){
        static const int frames[]={0,13,101};
        GAME game={0};GRAPH graph={0};SHRINE_ACTOR actor={0};Gfx opa[128],xlu[128],sha[32];
        game.graph=&graph;graph.polygon_opaque_thaga.thaGfx.head_p=opa;
        graph.polygon_translucent_thaga.thaGfx.head_p=xlu;graph.shadow_thaga.thaGfx.head_p=sha;
        actor.structure_class.season=winter?mTM_SEASON_WINTER:mTM_SEASON_SUMMER;
        actor.structure_class.arg1=winter;actor.texture_frame=frames[frame];
        scroll_count=shadow_count=culled=0;
        aSHR_actor_draw((ACTOR*)&actor,&game);
        CHECK(scroll_count==3&&shadow_count==1,"scroll allocation and shadow callback counts unchanged");
        CHECK(scroll_args[0][1]==-frames[frame]*50&&scroll_args[0][7]==-frames[frame]*13,"original water animation speed retained");
        CHECK(scroll_args[1][2]==frames[frame]*5&&scroll_args[1][7]==frames[frame]*3,"original splash animation speed retained");
        CHECK(scroll_args[2][2]==-frames[frame]*12&&scroll_args[2][3]==16&&scroll_args[2][4]==64,"original bubble animation speed and dimensions retained");
        Gfx *bubble=winter?obj_w_shrine_bubble_model:obj_s_shrine_bubble_model;
        Gfx *originals[]={bubble,winter?obj_w_shrine_trunk_model:obj_s_shrine_trunk_model,
            winter?obj_w_shrine_leaf_model:obj_s_shrine_leaf_model,winter?obj_w_shrine_figure_model:obj_s_shrine_figure_model,
            winter?obj_w_shrine_base_model:obj_s_shrine_base_model,winter?obj_w_shrine_statue_model:obj_s_shrine_statue_model};
        Gfx *expected_opa[8];int n=0;for(int i=0;i<6;i++){expected_opa[n++]=originals[i];if(repair&&(i==1||i==3))expected_opa[n++]=rear;}
        Gfx *expected_xlu[]={winter?obj_w_shrine_water_model:obj_s_shrine_water_model,
            winter?obj_w_shrine_sprash_model:obj_s_shrine_sprash_model,bubble};
        scan(opa,graph.polygon_opaque_thaga.thaGfx.head_p-opa,bubble,expected_opa,n);
        scan(xlu,graph.polygon_translucent_thaga.thaGfx.head_p-xlu,bubble,expected_xlu,3);
    }
    printf("Fountain actor submission: %d checks, %d failures\n",checks,failures);return failures!=0;
}
