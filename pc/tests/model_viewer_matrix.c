/* Test real guLookAtF/guMtxF2L/guPerspective plus the production viewer helper.
 * Projection/decoding below is independent of the packing implementation. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "libultra/gu.h"

void guLookAtF(float m[4][4],float ex,float ey,float ez,float tx,float ty,float tz,float ux,float uy,float uz);
void guPerspectiveF(float m[4][4],u16* norm,float fov,float aspect,float near,float far,float scale);
#include "model_viewer_matrix_source.inc"

static int checks,failures,cases;
static double max_pixel_error;
#define CHECK(c,msg) do {checks++;if(!(c)){if(failures++<14)printf("FAIL case%d: %s (%d)\n",cases,msg,__LINE__);}}while(0)
static void unpack(const Mtx* packed,float out[4][4]){
    const uint32_t* p=(const uint32_t*)packed;
    for(int r=0;r<4;r++)for(int c=0;c<4;c++){
        unsigned k=r*2+c/2,shift=(c&1)?0:16;
        uint32_t value=(((p[k]>>shift)&65535u)<<16)|((p[k+8]>>shift)&65535u);
        out[r][c]=(int32_t)value/65536.0;
    }
}
static void multiply(const double point[4],const float matrix[4][4],double out[4]){
    for(int c=0;c<4;c++){out[c]=0;for(int r=0;r<4;r++)out[c]+=point[r]*matrix[r][c];}
}
static void project(const double p[4],const float view[4][4],const float projection[4][4],double out[3]){
    double eye[4],clip[4];multiply(p,view,eye);multiply(eye,projection,clip);
    for(int c=0;c<3;c++)out[c]=clip[c]/clip[3];
}

static void camera_case(float distance,float yaw,float pitch,float height,float pan){
    const float pi=3.14159265358979323846f;
    yaw*=pi/180;pitch*=pi/180;
    float tx=pan,ty=height,tz=-pan*0.5f;
    float ex=tx+distance*cosf(pitch)*sinf(yaw),ey=ty+distance*sinf(pitch),ez=tz+distance*cosf(pitch)*cosf(yaw);
    float original[4][4],fitted[4][4],decoded[4][4],reference_projection[4][4],decoded_projection[4][4];
    Mtx packed,ordinary,projection;u16 norm;
    guLookAtF(original,ex,ey,ez,tx,ty,tz,0,1,0);
    memcpy(fitted,original,sizeof(fitted));
    float scale=mv_fit_view_matrix(&packed,fitted);unpack(&packed,decoded);cases++;
    float largest=fmaxf(fabsf(original[3][0]),fmaxf(fabsf(original[3][1]),fabsf(original[3][2])));
    CHECK(scale>0&&scale<=1,"finite positive view scale");
    int exponent;CHECK(frexpf(scale,&exponent)==0.5f,"view scale is an exact power of two");
    if(largest<=32000){
        guMtxF2L(original,&ordinary);
        CHECK(scale==1&&!memcmp(original,fitted,sizeof(fitted)),"normal-range float view unchanged");
        CHECK(!memcmp(&packed,&ordinary,sizeof(packed)),"normal-range packed view byte-identical");
    }else CHECK(scale<1,"overflow-range camera is rescaled");
    for(int r=0;r<4;r++)for(int c=0;c<4;c++){
        float expected=original[r][c]*(c<3?scale:1);
        CHECK(fitted[r][c]==expected,"all three spatial columns scale uniformly; homogeneous column unchanged");
        CHECK(isfinite(fitted[r][c])&&fabsf(fitted[r][c])<=32000,"all packed entries inside signed16.16 range");
        CHECK(fabs(decoded[r][c]-fitted[r][c])<=1.0/65536.0,"fixed-point round trip retains entry without overflow");
    }
    CHECK(decoded[3][3]==1&&decoded[0][3]==0&&decoded[1][3]==0&&decoded[2][3]==0,"homogeneous view remains affine");
    float near_plane=distance<1000?1:10,far_plane=fmaxf(50000,distance+30000);
    guPerspectiveF(reference_projection,&norm,45,640.0f/480.0f,near_plane,far_plane,1);
    guPerspective(&projection,&norm,45,640.0f/480.0f,near_plane*scale,far_plane*scale,1);unpack(&projection,decoded_projection);
    double center[4]={tx,ty,tz,1},eye[4];multiply(center,decoded,eye);
    double position_bound=(fabs(tx)+fabs(ty)+fabs(tz)+1)/65536.0+0.05;
    CHECK(eye[2]<0&&fabs(eye[2]+distance*scale)<=position_bound,"target remains in front at correct scaled distance");
    CHECK(fabs(eye[0])<=position_bound&&fabs(eye[1])<=position_bound,"target stays centered across orbit/pan/height");
    double size=fmin(distance*0.2,7000);
    for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)for(int z=-1;z<=1;z++){
        double point[4]={tx+x*size,ty+y*size,tz+z*size,1},expected[3],actual[3],fit_float[3];
        float fit_projection[4][4];guPerspectiveF(fit_projection,NULL,45,640.0f/480.0f,near_plane*scale,far_plane*scale,1);
        project(point,original,reference_projection,expected);project(point,decoded,decoded_projection,actual);project(point,fitted,fit_projection,fit_float);
        double eye_ref[4];multiply(point,original,eye_ref);
        double error_bound=(fabs(point[0])+fabs(point[1])+fabs(point[2])+1)/65536.0;
        double tolerance=6*error_bound/(-eye_ref[2]*scale)+0.00008;
        for(int axis=0;axis<3;axis++){
            CHECK(fabs(fit_float[axis]-expected[axis])<0.000002,"scaled near/far preserve floating NDC coordinates");
            CHECK(isfinite(actual[axis])&&fabs(actual[axis]-expected[axis])<tolerance,"packed projection matches float reference within fixed-point precision");
            if(axis<2&&distance>=10000){double pixels=fabs(actual[axis]-expected[axis])*(axis?240:320);if(pixels>max_pixel_error)max_pixel_error=pixels;}
        }
    }
}

static double projected_width(float distance){
    float view[4][4],decoded[4][4],projection[4][4];Mtx packed,proj;
    guLookAtF(view,0,6000,distance,0,6000,0,0,1,0);
    float scale=mv_fit_view_matrix(&packed,view);unpack(&packed,decoded);
    guPerspective(&proj,NULL,45,640.0f/480.0f,10*scale,(distance+30000)*scale,1);unpack(&proj,projection);
    double left[4]={-5000,6000,0,1},right[4]={5000,6000,0,1},l[3],r[3];
    project(left,decoded,projection,l);project(right,decoded,projection,r);
    return r[0]-l[0];
}

int main(void){
    const float distances[]={100,1000,10000,28000,31999,32000,32001,32767,32769,50000,62000,63999,64000,64001,75000,150000};
    const float pitches[]={-60,0,20,60},heights[]={0,6000,18000,50000};
    for(unsigned d=0;d<sizeof(distances)/sizeof(*distances);d++)for(int angle=0;angle<8;angle++)
        for(unsigned p=0;p<sizeof(pitches)/sizeof(*pitches);p++)for(unsigned h=0;h<sizeof(heights)/sizeof(*heights);h++)
            camera_case(distances[d],angle*45,pitches[p],heights[h],h==3?24000:0);
    double previous=projected_width(30000);
    for(int distance=30250;distance<=85000;distance+=250){
        double current=projected_width(distance);
        CHECK(current<previous,"projected model shrinks continuously as distance increases");
        CHECK(fabs(current/previous-(distance-250.0)/distance)<0.00002,"zoom follows inverse distance through fixed-point scale boundaries");previous=current;
    }
    double at50=projected_width(50000),at75=projected_width(75000);
    CHECK(at75<at50*0.67&&at75>at50*0.66,"75000-distance model is two-thirds the size at50000");
    printf("Model viewer matrix: %d cases, %d checks, %d failures; max structure projection error %.4fpx\n",cases,checks,failures,max_pixel_error);
    return failures!=0;
}
