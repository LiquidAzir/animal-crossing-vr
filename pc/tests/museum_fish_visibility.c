/* Exercise the actual museum draw callbacks with recorded display-list output.
 * Source functions, tank positions and fish enum are extracted by the runner. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
typedef float f32;
typedef int BOOL;
typedef unsigned char u8;
typedef short s16;
typedef struct { float x,y,z; } xyz_t;
typedef int Gfx;
typedef int Mtx;
typedef int EVW_ANIME_DATA;
enum { FALSE=0, TRUE=1, MTX_LOAD=0, MTX_MULT=1, G_MTX_NOPUSH=0,
       G_MTX_LOAD=1, G_MTX_MODELVIEW=2, G_TA_DOLPHIN=0, G_TA_N64=1, TAKREG=0 };
#include "museum_fish_enum.inc"
typedef struct { int unused; } GRAPH;
typedef struct { GRAPH* graph; unsigned frame_counter; } GAME;
typedef struct { GAME game; } GAME_PLAY;
typedef struct { int unused; } ACTOR;
typedef struct {
    int _62E_flags,fish_idx;
    xyz_t objchk_pos,position;
    struct { float _08,ofs_y; } init_data;
} MUSEUM_FISH_PRIVATE_DATA;
typedef struct {
    ACTOR actor;
    MUSEUM_FISH_PRIVATE_DATA prvFish[aGYO_TYPE_NUM];
    struct { Mtx mtx[2][9]; int keyframe; } prvKusa[14];
    int _14788;
} MUSEUM_FISH_ACTOR;
static unsigned checks,failures;
#define CHECK(c) do { ++checks; if(!(c)){if(failures++<15)printf("FAIL line %u: %s\n",__LINE__,#c);} }while(0)
int g_pc_full_world;
static int registers[100],project_calls,small_opa,small_xlu,large_opa,large_xlu;
static int plants,reflections,invalid_reflections,fish[aGYO_TYPE_NUM],lilypads;
static int animation_calls,opaque_packets,xlu_packets,light_calls,mode_lists;
static xyz_t projected;
#define GETREG(group,index) registers[index]
#define OPEN_DISP(graph) ((void)(graph))
#define CLOSE_DISP(graph) ((void)(graph))
#define NEXT_POLY_OPA_DISP (++opaque_packets)
#define NEXT_POLY_XLU_DISP (++xlu_packets)
static void matrix_command(int packet,Mtx* matrix,int flags){(void)packet;(void)matrix;(void)flags;}
#define gSPMatrix matrix_command
static void texture_adjust(int packet,int mode){(void)packet;(void)mode;}
#define gDPSetTextureAdjustMode texture_adjust
static Mtx fake_matrix;
static Mtx* _Matrix_to_Mtx_new(GRAPH* graph){(void)graph;return &fake_matrix;}
static void Matrix_translate(float x,float y,float z,int mode){(void)x;(void)y;(void)z;(void)mode;}
static void Matrix_scale(float x,float y,float z,int mode){(void)x;(void)y;(void)z;(void)mode;}
Gfx obj_suisou1_model[1],obj_suisou1_modelT[1],obj_museum5_model[1],obj_museum5_modelT[1],act_mus_fish_set_mode;
EVW_ANIME_DATA obj_suisou1_evw_anime,obj_museum5_evw_anime;
static void display_list(int packet,Gfx* list){
    (void)packet;
    small_opa+=list==obj_suisou1_model;small_xlu+=list==obj_suisou1_modelT;
    large_opa+=list==obj_museum5_model;large_xlu+=list==obj_museum5_modelT;
    mode_lists+=list==&act_mus_fish_set_mode;
}
#define gSPDisplayList display_list
#include "museum_fish_positions.inc"
static void Setpos_HiliteReflect_xlu_init(xyz_t* position,GAME_PLAY* play){
    (void)play;++reflections;
    if((uintptr_t)position<(uintptr_t)suisou_pos||(uintptr_t)position>=(uintptr_t)(suisou_pos+5))++invalid_reflections;
}
static void mfish_hasu_dw(int* p,GAME* game){(void)p;(void)game;++lilypads;}
static void kusa_before_disp(void){}
static void cKF_Si3_draw_R_SV(GAME* game,int* keyframe,Mtx* mtx,void(*before)(void),void* after,void* p){
    (void)game;(void)keyframe;(void)mtx;(void)before;(void)after;(void)p;++plants;
}
static void _texture_z_light_fog_prim(GRAPH* g){(void)g;}
static void _texture_z_light_fog_prim_xlu(GRAPH* g){(void)g;}
static void mfish_normal_light_set(ACTOR* a,GAME* g){(void)a;(void)g;++light_calls;}
static void Evw_Anime_Set(GAME_PLAY* p,EVW_ANIME_DATA* a){(void)p;(void)a;++animation_calls;}
static void draw_fish(MUSEUM_FISH_PRIVATE_DATA* p,GAME* g){(void)g;++fish[p->fish_idx];}
static void (*mfish_dw[aGYO_TYPE_NUM])(MUSEUM_FISH_PRIVATE_DATA*,GAME*);
static void Game_play_Projection_Trans(GAME_PLAY* g,xyz_t* world,xyz_t* screen){
    (void)g;(void)world;++project_calls;*screen=projected;
}
#include "museum_fish_visibility_source.inc"
static void clear_draw_counts(void){
    project_calls=small_opa=small_xlu=large_opa=large_xlu=plants=reflections=invalid_reflections=0;
    lilypads=animation_calls=opaque_packets=xlu_packets=light_calls=mode_lists=0;memset(fish,0,sizeof(fish));
}
int main(void){
    GRAPH graph={0};GAME_PLAY game={{&graph,0}};MUSEUM_FISH_ACTOR actor={0},before;
    const float widths[][3]={{350,20,650},{215,25,0},{0,0,120},{50,10,10},{12,4,4}};
    const float coords[]={-10000,-651,-351,-215,-25,-20,-1,0,.01f,120,239,240,319,320,350,535,650,670,10000,NAN};
    xyz_t position={0};
    for(int mode=0;mode<2;++mode)for(unsigned w=0;w<sizeof(widths)/sizeof(widths[0]);++w)
    for(unsigned xi=0;xi<sizeof(coords)/sizeof(coords[0]);++xi)for(unsigned yi=0;yi<sizeof(coords)/sizeof(coords[0]);++yi){
        g_pc_full_world=mode;projected=(xyz_t){coords[xi],coords[yi],1};project_calls=0;
        int expected=(-widths[w][0]<projected.x&&projected.x<widths[w][0]+320&&
                      -widths[w][1]<projected.y&&projected.y<widths[w][2]+240);
#ifdef TARGET_PC
        if(mode)expected=1;
#endif
        CHECK(mfish_cull_check((GAME*)&game,&position,widths[w][0],widths[w][1],widths[w][2])==expected);
#ifdef TARGET_PC
        CHECK(project_calls==!mode);
#else
        CHECK(project_calls==1);
#endif
    }
    for(int i=0;i<aGYO_TYPE_NUM;++i){actor.prvFish[i].fish_idx=i;mfish_dw[i]=draw_fish;}
    /* All draws retain their original donation gates and mutate no fish state. */
    for(int mode=0;mode<2;++mode)for(int donation=0;donation<3;++donation)for(int view=0;view<12;++view){
        clear_draw_counts();g_pc_full_world=mode;projected=(xyz_t){view?10000.0f*view:160,view?-10000.0f*view:120,1};
        game.game.frame_counter=view;
        for(int i=0;i<aGYO_TYPE_NUM;++i)actor.prvFish[i]._62E_flags=donation==0?0:donation==1?1:(i%2?1:2);
        before=actor;Museum_Fish_Actor_draw((ACTOR*)&actor,(GAME*)&game);
        int visible=view==0;
#ifdef TARGET_PC
        visible|=mode;
#endif
        CHECK(small_opa==4*visible&&small_xlu==4*visible);
        CHECK(large_opa==visible&&large_xlu==visible);CHECK(plants==14*visible);
        CHECK(reflections==5*visible&&invalid_reflections==0);
        CHECK(animation_calls==5);CHECK(mode_lists==1);CHECK(!memcmp(&actor,&before,sizeof(actor)));
        for(int i=0;i<aGYO_TYPE_NUM;++i)CHECK(fish[i]==((actor.prvFish[i]._62E_flags&1)&& (visible||i==aGYO_TYPE_FROG)));
        CHECK(lilypads==((actor.prvFish[aGYO_TYPE_FROG]._62E_flags&1)?0:4*visible));
    }
    /* Debug draw suppression remains authoritative even in full-world mode. */
    clear_draw_counts();g_pc_full_world=1;projected=(xyz_t){160,120,1};
    registers[90]=registers[91]=registers[92]=1;Museum_Fish_Actor_draw((ACTOR*)&actor,(GAME*)&game);
    CHECK(!small_opa&&!small_xlu&&!large_opa&&!large_xlu&&!plants&&!mode_lists);
    for(int i=0;i<aGYO_TYPE_NUM;++i)CHECK(!fish[i]);
    printf("Museum visibility checks=%u failures=%u TARGET_PC=%d\n",checks,failures,
#ifdef TARGET_PC
        1
#else
        0
#endif
    );return failures!=0;
}
