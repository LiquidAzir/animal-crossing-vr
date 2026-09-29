#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pc_vr_menu_input.h"
#include "pc_vr_swing.h"

static struct {
    int active, input_ready, head_pose_valid, menu_input_available, input_synced;
    int act_move, act_camera, act_a, act_b, act_x, act_y, act_l, act_r, act_start, act_z;
    PCVRMenuInput menu_input;
    PCVRSwing swing;
} s_vr;
static struct { int focused, resumed; } s_xr;
static uint32_t ticks, pc_frame_counter;
static int g_pc_paused, blocked, opens, navigation, close_on_nav, toggles;
static float nav_x, nav_y, move[2], camera[2];
static int nav_a, nav_b, held[12], checks, failures;
static uint32_t SDL_GetTicks(void) { return ticks; }
static int pcvr_digital(int action) { return held[action]; }
static void pcvr_analog(int action, float* x, float* y) {
    float* v = action == s_vr.act_move ? move : camera;
    *x = v[0]; *y = v[1];
}
static void pc_fp_toggle(void) { ++toggles; }
static int pc_vr_tool_input_allowed(void) { return !g_pc_paused && s_vr.head_pose_valid; }
static int pc_pause_menu_open_vr_settings(void) {
    ++opens;
    if (blocked || g_pc_paused) return 0;
    g_pc_paused = 1;
    return 1;
}
static int pc_pause_menu_vr_input(float x, float y, int a, int b) {
    ++navigation; nav_x=x; nav_y=y; nav_a=a; nav_b=b;
    if (!g_pc_paused) return 0;
    if (close_on_nav) g_pc_paused = 0;
    return 1;
}
#include "menu_input_actual.inc"

typedef struct {
    unsigned short buttons;
    signed char x,y,cx,cy;
    unsigned char l,r;
} Pad;
#define CHECK(c, why) do { ++checks; if (!(c)) { if(failures++ < 20) printf("FAIL: %s (%d)\n",why,__LINE__); } } while(0)
static Pad read_pad(Pad incoming) {
    pc_vr_merge_pad(&incoming.buttons,&incoming.x,&incoming.y,&incoming.cx,&incoming.cy,&incoming.l,&incoming.r);
    return incoming;
}
static Pad read_zero(void) { return read_pad((Pad){0}); }
static void step(unsigned dt) { ticks += dt; ++pc_frame_counter; }
static void empty(Pad p, const char* why) { CHECK(!p.buttons&&!p.x&&!p.y&&!p.cx&&!p.cy&&!p.l&&!p.r,why); }
static void reset(void) {
    memset(&s_vr,0,sizeof(s_vr)); memset(held,0,sizeof(held)); memset(move,0,sizeof(move));memset(camera,0,sizeof(camera));
    s_vr.active=s_vr.input_ready=s_vr.head_pose_valid=s_vr.menu_input_available=s_vr.input_synced=1;
    s_xr.focused=s_xr.resumed=1;
    s_vr.act_move=0;s_vr.act_camera=1;s_vr.act_a=2;s_vr.act_b=3;s_vr.act_x=4;s_vr.act_y=5;
    s_vr.act_l=6;s_vr.act_r=7;s_vr.act_start=8;s_vr.act_z=9;
    g_pc_paused=blocked=opens=navigation=close_on_nav=toggles=0;
    ticks=100;pc_frame_counter=1;
    empty(read_zero(),"initial neutral arms without output");step(16);
}
static void singles(void) {
    for(int key=0;key<2;++key) {
        int bit=key?0x10:0x1000;
        reset();int action=key?s_vr.act_z:s_vr.act_start;
        held[action]=1;empty(read_zero(),"single click waits for grace");
        step(100);empty(read_zero(),"single click still waiting");
        step(80);CHECK(read_zero().buttons==bit,"held single click survives grace");
        CHECK(read_zero().buttons==bit,"duplicate PADRead sees same held click");
        step(20);held[action]=0;CHECK(read_zero().buttons==bit,"release edge follows delayed press");
        step(180);empty(read_zero(),"single click releases normally");CHECK(opens==0,"single never opens menu");

        reset();held[action]=1;empty(read_zero(),"quick tap down pending");
        step(20);held[action]=0;empty(read_zero(),"quick tap release retained");
        step(400);CHECK(read_zero().buttons==bit,"long frame cannot swallow quick tap");
        CHECK(read_zero().buttons==bit,"quick tap present in both PADRead calls");
        step(1);empty(read_zero(),"quick tap release on next frame");
        CHECK(opens==0,"quick tap never opens settings");
    }
    reset();ticks=0xffffff80u;
    held[s_vr.act_start]=1;read_zero();step(180);
    CHECK(read_zero().buttons==0x1000,"grace survives tick wraparound");
}
static void chords(void) {
    for(int first=0;first<2;++first)for(unsigned delay=0;delay<=180;delay+=30) {
        reset();int a=first?s_vr.act_z:s_vr.act_start,b=first?s_vr.act_start:s_vr.act_z;
        held[a]=1;empty(read_zero(),"first chord click is swallowed");
        step(delay);held[b]=1;empty(read_zero(),"chord output consumed");
        CHECK(g_pc_paused&&opens==1,"either chord order opens once within grace");
        empty(read_zero(),"opening duplicate PADRead consumed");
        step(500);empty(read_zero(),"held chord never leaks");CHECK(opens==1,"held chord does not reopen");
    }
    reset();held[s_vr.act_start]=1;read_zero();step(181);held[s_vr.act_z]=1;
    CHECK(read_zero().buttons==0x1000&&opens==0,"late second click preserves first single");
    step(180);CHECK(read_zero().buttons==(0x1000|0x10),"late second click also remains usable");
    reset();blocked=1;held[s_vr.act_start]=held[s_vr.act_z]=1;read_zero();
    CHECK(!g_pc_paused&&opens==1,"blocked menu respects host result");
    step(500);empty(read_zero(),"blocked chord remains swallowed until release");
    held[s_vr.act_start]=0;step(16);read_zero();held[s_vr.act_start]=1;step(16);read_zero();
    CHECK(opens==1,"one released click cannot retrigger chord");
    held[s_vr.act_start]=held[s_vr.act_z]=0;step(16);read_zero();
    held[s_vr.act_start]=held[s_vr.act_z]=1;step(16);read_zero();CHECK(opens==2,"both releases rearm chord");
}
static void menu_and_drain(void) {
    reset();held[s_vr.act_start]=held[s_vr.act_z]=1;read_zero();
    step(16);held[s_vr.act_start]=held[s_vr.act_z]=0;held[s_vr.act_a]=held[s_vr.act_b]=1;
    move[0]=0.75f;move[1]=-0.5f;camera[0]=0.8f;s_vr.swing.pulse=2;
    Pad incoming={0xffff,80,-80,70,-70,255,255};
    empty(read_pad(incoming),"pause suppresses all VR and incoming pad/keyboard outputs");
    CHECK(nav_x==move[0]&&nav_y==move[1]&&nav_a&&nav_b,"left stick/A/B sent to menu host");
    CHECK(s_vr.swing.pulse==0,"pause cancels pending tool gesture");
    step(16);close_on_nav=1;empty(read_zero(),"closing frame consumed");
    CHECK(!g_pc_paused,"host can close menu");
    s_vr.swing.pulse=2;empty(read_pad(incoming),"second PADRead after close still consumed");
    CHECK(s_vr.swing.pulse==0,"closing duplicate read clears gesture");
    step(300);empty(read_pad(incoming),"held controls drain after close");
    memset(held,0,sizeof(held));memset(move,0,sizeof(move));memset(camera,0,sizeof(camera));
    step(16);empty(read_pad(incoming),"incoming non-VR pad also must settle");
    step(16);empty(read_zero(),"neutral frame remains consumed while rearming");
    held[s_vr.act_a]=1;empty(read_zero(),"no new game output in neutral frame duplicate read");
    step(16);CHECK(read_zero().buttons==0x100,"fresh A works after drain");
    CHECK(opens==1,"close never reopens settings");

    reset();g_pc_paused=1;read_zero();g_pc_paused=0;
    step(16);held[s_vr.act_b]=1;empty(read_zero(),"keyboard-closed menu also drains held VR input");
}
static void unavailable(void) {
    for(int reason=0;reason<4;++reason) {
        reset();held[s_vr.act_start]=1;read_zero();step(50);
        if(reason==0)s_vr.active=0;
        if(reason==1)s_vr.input_ready=0;
        if(reason==2)s_vr.head_pose_valid=0;
        if(reason==3){s_vr.menu_input_available=0;s_vr.input_synced=0;}
        s_vr.swing.pulse=2;read_zero();
        CHECK(!s_vr.menu_input.ready,"input loss resets shortcut state");
        s_vr.active=s_vr.input_ready=s_vr.head_pose_valid=s_vr.menu_input_available=s_vr.input_synced=1;
        held[s_vr.act_z]=1;step(300);empty(read_zero(),"stale held chord cannot reopen after input loss");
        CHECK(opens==0,"recovery needs neutral");
        held[s_vr.act_start]=held[s_vr.act_z]=0;step(16);read_zero();
        held[s_vr.act_start]=held[s_vr.act_z]=1;step(16);read_zero();CHECK(opens==1,"new chord after neutral works");
    }
    reset();s_vr.active=0;Pad incoming={0x100,40,-30,20,10,0,255};
    Pad p=read_pad(incoming);CHECK(!memcmp(&p,&incoming,sizeof(p)),"inactive flat mode leaves ordinary pad intact");

    reset();g_pc_paused=1;read_zero();int prior=navigation;
    step(16);s_vr.head_pose_valid=0;read_zero();
    step(16);s_vr.head_pose_valid=1;held[s_vr.act_a]=1;read_zero();
    CHECK(navigation==prior,"focus recovery cannot pass a held confirm to an already-open menu");
    step(16);held[s_vr.act_a]=0;read_zero();
    CHECK(navigation==prior+1&&!nav_a&&!nav_b,"neutral recovery clears the host button latch");
    step(16);held[s_vr.act_a]=1;read_zero();
    CHECK(navigation==prior+2&&nav_a,"fresh confirm works after paused recovery");
}
int main(void) {
    singles();chords();menu_and_drain();unavailable();
    printf("VR menu input: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
