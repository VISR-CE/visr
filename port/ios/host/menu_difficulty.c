/* Apache-2.0. Resample the player's matching large frame in premultiplied space. */
#include "menu_difficulty.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Hashes and placement measurements identify the original paired frames, in
   shield, one sword, two swords, skull order. No game pixels are embedded. */
static const uint64_t small_hashes[4]={UINT64_C(0xb793ae798c0f1369),UINT64_C(0x56a3647c9606d6d6),
    UINT64_C(0xd8c596d2a6610985),UINT64_C(0x4361910fbae3ec2b)};
static const uint64_t large_hashes[4]={UINT64_C(0x71ad7f2ea0e4333f),UINT64_C(0x3089569cfe5f8e99),
    UINT64_C(0x2354cc354210dfa3),UINT64_C(0xf2a880f2ff8d1c31)};
struct placement { double scale_x,scale_y,x,y; };
/* Small pixel-edge coordinates = large pixel-edge coordinates * scale + offset.
   Fit against alpha and premultiplied color together; retain the large source's
   metallic contrast and continuous shadow instead of the small frame's blur. */
static const struct placement placements[4]={
    {0.225374730,0.223839387,-0.264656560,-0.446151220},
    {0.223642588,0.223648278,-0.207052723,-0.333087574},
    {0.223788772,0.223424077,-0.411146774,-0.519115286},
    {0.224992495,0.223175649,-0.292809478,0.044210149}
};
static int identify(const uint8_t *rgba,unsigned width,unsigned height,unsigned side,const uint64_t hashes[4]) {
    if(!rgba||width!=side||height!=side)return -1;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<(size_t)side*side*4;i++)hash=(hash^rgba[i])*UINT64_C(1099511628211);
    for(int i=0;i<4;i++)if(hash==hashes[i])return i;
    return -1;
}
int menu_difficulty_source(const uint8_t *rgba,unsigned width,unsigned height) {
    return identify(rgba,width,height,64,small_hashes);
}
int menu_difficulty_large_source(const uint8_t *rgba,unsigned width,unsigned height) {
    return identify(rgba,width,height,256,large_hashes);
}
struct taps { int first,count; double weights[32]; };
static double sinc(double x) {
    if(fabs(x)<1e-10)return 1;
    x*=3.14159265358979323846;return sin(x)/x;
}
static int axis(struct taps *taps,unsigned side,double scale,double offset,unsigned output_scale) {
    double ratio=1/(scale*output_scale),filter=fmax(1,ratio),radius=3*filter;
    for(unsigned i=0;i<side;i++) {
        double center=((i+.5)/output_scale-offset)/scale-.5,sum=0;
        int first=(int)ceil(center-radius),last=(int)floor(center+radius);
        if(last-first+1>32)return 0;
        taps[i].first=first;taps[i].count=last-first+1;
        for(int j=0;j<taps[i].count;j++) {
            double distance=(first+j-center)/filter;
            double weight=fabs(distance)<3?sinc(distance)*sinc(distance/3):0;
            taps[i].weights[j]=weight;sum+=weight;
        }
        if(fabs(sum)<1e-10)return 0;
        for(int j=0;j<taps[i].count;j++)taps[i].weights[j]/=sum;
    }
    return 1;
}
static uint8_t quantize(double value) { return (uint8_t)lround(fmin(255,fmax(0,value))); }
int menu_difficulty_replace(const uint8_t *small,const uint8_t *large,uint8_t *output,unsigned scale,const volatile int *cancel) {
    int index=menu_difficulty_source(small,64,64);
    if(!output||(scale!=2&&scale!=4)||index<0||menu_difficulty_large_source(large,256,256)!=index)return 0;
    if(cancel&&*cancel)return -1;
    unsigned side=64*scale;size_t bytes=(size_t)side*side*4;
    uint8_t *result=calloc(1,bytes);
    struct taps *axes=calloc(side*2,sizeof(*axes));
    if(!result||!axes){free(result);free(axes);return 0;}
    const struct placement *p=&placements[index];
    int status=0;
    if(!axis(axes,side,p->scale_x,p->x,scale)||!axis(axes+side,side,p->scale_y,p->y,scale))goto done;
    for(unsigned y=0;y<side;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        const struct taps *ty=axes+side+y;
        for(unsigned x=0;x<side;x++) {
            const struct taps *tx=axes+x;double rgba[4]={0};
            for(int j=0;j<ty->count;j++) {
                int yy=ty->first+j;if(yy<0||yy>=256)continue;
                for(int i=0;i<tx->count;i++) {
                    int xx=tx->first+i;if(xx<0||xx>=256)continue;
                    const uint8_t *pixel=large+((size_t)yy*256+xx)*4;
                    double weight=ty->weights[j]*tx->weights[i],a=pixel[3]/255.;
                    for(unsigned c=0;c<3;c++)rgba[c]+=pixel[c]*a*weight;
                    rgba[3]+=pixel[3]*weight;
                }
            }
            uint8_t *pixel=result+((size_t)y*side+x)*4;
            pixel[3]=quantize(rgba[3]);
            if(pixel[3])for(unsigned c=0;c<3;c++)pixel[c]=quantize(rgba[c]*255/fmax(1e-10,rgba[3]));
        }
    }
    if(cancel&&*cancel){status=-1;goto done;}
    memcpy(output,result,bytes);status=1;
done:
    free(result);free(axes);return status;
}
