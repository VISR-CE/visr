/* Apache-2.0. The button tables and erase slots reproduce the approved authored redraw.
   Core Text reads our bundled Overpass bytes directly; it never substitutes a system font. */
#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#include "menu_picture_redraw.h"
#include "menu_picture_trace.h"
#include "menu_picture_shapes.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double seconds(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }
int menu_picture_redraw_source(const uint8_t *p,unsigned w,unsigned h) {
    int carousel=menu_carousel_shapes_source(p,w,h);
    if(carousel>=0)return carousel+9;
    static const uint64_t hashes[]={UINT64_C(0x10abeb8e84292ebb),UINT64_C(0x4f2391de207baaf5),
        UINT64_C(0xb21af00c0b190d4b),UINT64_C(0xe24cf6469a4cbaed),UINT64_C(0xa30fdfccfcf19037)};
    if(!p||w!=512||(h!=256&&h!=512))return -1;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<(size_t)w*h*4;i++)hash=(hash^p[i])*UINT64_C(1099511628211);
    if(h==256)return hash==UINT64_C(0xd42615c20141636f)?0:-1;
    for(unsigned i=0;i<5;i++)if(hash==hashes[i])return (int)i+4;
    return -1;
}
struct slot { int x0,y0,x1,y1,x,y,right;const char *fixed; };
static const struct slot slots[]={
    {300,50,760,120,752,88,1,NULL},{1128,50,1750,120,1136,86,0,NULL},
    {1338,185,2000,250,1345,217,0,"Flashlight On/Off"},{1450,270,2000,330,1458,300,0,"Switch Grenade"},
    {1498,360,2000,422,1506,392,0,NULL},{1506,452,2000,512,1515,483,0,NULL},
    {1532,538,2000,598,1542,567,0,NULL},{1518,625,2000,685,1526,654,0,"Action/Reload"},
    {1442,815,2000,875,1448,845,0,NULL},{200,335,450,395,440,366,1,"Crouch"},
    {40,655,432,710,410,682,1,"Back Button"},{60,750,456,808,440,778,1,"Start Button"}};
/* tl, tr, switch weapon, melee, jump, scope, in the original map's frame order. */
static const char *const labels[5][6]={
    {"Throw Grenade","Fire Weapon","Switch Weapon","Melee Attack","Jump","Scope Zoom"},
    {"Fire Weapon","Throw Grenade","Switch Weapon","Melee Attack","Jump","Scope Zoom"},
    {"Jump","Fire Weapon","Switch Weapon","Melee Attack","Throw Grenade","Scope Zoom"},
    {"Melee Attack","Fire Weapon","Switch Weapon","Throw Grenade","Jump","Scope Zoom"},
    {"Throw Grenade","Fire Weapon","Switch Weapon","Scope Zoom","Jump","Melee Attack"}};
static NSString *resource(const char *folder,NSString *name,NSString *ext) {
    return folder?[[NSString stringWithUTF8String:folder] stringByAppendingPathComponent:[name stringByAppendingPathExtension:ext]]:
        [NSBundle.mainBundle pathForResource:name ofType:ext];
}
static void over(uint8_t *p,float alpha) {
    float a=alpha+p[3]/255.f*(1-alpha);
    const float rgb[]={40,150,255};
    for(int c=0;c<3;c++)p[c]=(uint8_t)lrintf(a?(rgb[c]*alpha+p[c]*p[3]/255.f*(1-alpha))/a:0);
    p[3]=(uint8_t)lrintf(a*255);
}
int menu_picture_redraw(const uint8_t *source,unsigned sw,unsigned sh,uint8_t *bpf,unsigned scale,
    const volatile int *cancel,const char *resources,struct menu_picture_redraw_metrics *metrics) {
    double start=seconds();if(metrics)memset(metrics,0,sizeof(*metrics));
    int picture=menu_picture_redraw_source(source,sw,sh);
    if(picture<0||!bpf||(scale!=2&&scale!=4))return 0;
    if(cancel&&*cancel)return -1;
    if(picture>=9) {
        int result=menu_carousel_shapes(source,sw,sh,bpf,scale,cancel);
        if(metrics) { metrics->seconds=seconds()-start;metrics->scratch_bytes=(size_t)sw*sh*scale*scale*4; }
        return result;
    }
    unsigned w=sw*scale,h=sh*scale;size_t n=(size_t)w*h;
    uint8_t *result=calloc(n,4),*mask=NULL;float *temp=NULL,*blur=NULL;
    CGContextRef ctx=NULL;CGColorSpaceRef space=NULL;CGFontRef cgfont=NULL;CTFontRef font=NULL;
    int status=0;
    if(!result)goto done;
    if(metrics)metrics->scratch_bytes=n*4;
    if(picture==0) {
        NSString *path=resource(resources,@"keyboard",@"png");
        NSData *data=path?[NSData dataWithContentsOfFile:path]:nil;
        CGImageSourceRef imageSource=data?CGImageSourceCreateWithData((__bridge CFDataRef)data,NULL):NULL;
        CGImageRef image=imageSource?CGImageSourceCreateImageAtIndex(imageSource,0,NULL):NULL;
        if(imageSource)CFRelease(imageSource);
        if(!image)goto done;
        space=CGColorSpaceCreateDeviceRGB();
        ctx=CGBitmapContextCreate(result,w,h,8,w*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
        if(ctx){CGContextSetInterpolationQuality(ctx,kCGInterpolationHigh);CGContextDrawImage(ctx,CGRectMake(0,0,w,h),image);}
        CGImageRelease(image);if(!ctx)goto done;
        for(unsigned y=0;y<h;y++) {
            if(cancel&&*cancel){status=-1;goto done;}
            for(unsigned x=0;x<w;x++){uint8_t *p=result+((size_t)y*w+x)*4;
                for(int c=0;c<3;c++)p[c]=p[3]?(uint8_t)MIN(255,(p[c]*255+p[3]/2)/p[3]):0;}
        }
    } else {
        NSString *path=resource(resources,@"Overpass-650",@"ttf");
        NSData *data=path?[NSData dataWithContentsOfFile:path]:nil;
        CGDataProviderRef provider=data?CGDataProviderCreateWithCFData((__bridge CFDataRef)data):NULL;
        cgfont=provider?CGFontCreateWithDataProvider(provider):NULL;
        if(provider)CGDataProviderRelease(provider);
        if(!cgfont)goto done;
        float k=1.024f*scale/4.f;
        font=CTFontCreateWithGraphicsFont(cgfont,60*scale/4.f,NULL,NULL);if(!font)goto done;
        mask=calloc(n,1);temp=malloc(n*sizeof(float));blur=malloc(n*sizeof(float));
        if(!mask||!temp||!blur)goto done;
        if(metrics)metrics->scratch_bytes=n*13;
        space=CGColorSpaceCreateDeviceGray();ctx=CGBitmapContextCreate(mask,w,h,8,w,space,(CGBitmapInfo)kCGImageAlphaNone);
        if(!ctx)goto done;
        CGContextSetGrayFillColor(ctx,1,1);CGContextSetTextMatrix(ctx,CGAffineTransformIdentity);
        memcpy(result,bpf,n*4);
        for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);i++) {
            const struct slot *s=&slots[i];
            for(unsigned y=(unsigned)(s->y0*k);y<=s->y1*k&&y<h;y++)
                for(unsigned x=(unsigned)(s->x0*k);x<=s->x1*k&&x<w;x++)result[((size_t)y*w+x)*4+3]=0;
        }
        /* Erase before tracing: the replacement halo may extend into a label slot.
           Trace also clears the dark exterior haze that made the old rectangular erases visible.
           Both steps publish together, so a rejected silhouette keeps the entire plain bpf picture. */
        struct menu_picture_trace_metrics trace;
        int traced=menu_picture_trace(source,sw,sh,result,scale,cancel,&trace);
        if(metrics)metrics->scratch_bytes+=trace.scratch_bytes;
        if(traced<=0){status=traced;goto done;}
        unsigned variable=0;
        for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);i++) {
            if(cancel&&*cancel){status=-1;goto done;}
            const struct slot *s=&slots[i];const char *label=s->fixed?s->fixed:labels[picture-4][variable++];
            NSDictionary *attrs=@{(__bridge NSString *)kCTFontAttributeName:(__bridge id)font,
                (__bridge NSString *)kCTForegroundColorFromContextAttributeName:@YES};
            NSAttributedString *text=[[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:label] attributes:attrs];
            CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text);
            double width=CTLineGetTypographicBounds(line,NULL,NULL,NULL);
            CGContextSetTextPosition(ctx,(int)(s->x*k)-(s->right?width:0),h-(int)(s->y*k)-(CTFontGetAscent(font)-CTFontGetDescent(font))/2);
            CTLineDraw(line,ctx);CFRelease(line);
        }
        /* Bounded separable Gaussian: only label alpha is blurred, preserving the player's controller pixels. */
        float sigma=5*scale/4.f,weights[31],sum=0;int radius=(int)ceilf(3*sigma);
        for(int i=-radius;i<=radius;i++){weights[i+radius]=expf(-i*i/(2*sigma*sigma));sum+=weights[i+radius];}
        for(int i=0;i<=2*radius;i++)weights[i]/=sum;
        for(unsigned y=0;y<h;y++) {
            if(cancel&&*cancel){status=-1;goto done;}
            for(unsigned x=0;x<w;x++){float a=0;for(int d=-radius;d<=radius;d++){
                int xx=(int)x+d;if(xx>=0&&xx<(int)w)a+=mask[(size_t)y*w+xx]*weights[d+radius];}temp[(size_t)y*w+x]=a;}
        }
        for(unsigned y=0;y<h;y++) {
            if(cancel&&*cancel){status=-1;goto done;}
            for(unsigned x=0;x<w;x++){float a=0;for(int d=-radius;d<=radius;d++){
                int yy=(int)y+d;if(yy>=0&&yy<(int)h)a+=temp[(size_t)yy*w+x]*weights[d+radius];}blur[(size_t)y*w+x]=a;}
        }
        for(size_t i=0;i<n;i++){if((i%w)==0&&cancel&&*cancel){status=-1;goto done;}
            over(result+i*4,blur[i]/255*.55f);over(result+i*4,mask[i]/255.f);}
    }
    if(cancel&&*cancel){status=-1;goto done;}
    memcpy(bpf,result,n*4);status=1;
done:
    if(ctx)CGContextRelease(ctx);if(space)CGColorSpaceRelease(space);if(font)CFRelease(font);if(cgfont)CGFontRelease(cgfont);
    free(result);free(mask);free(temp);free(blur);if(metrics)metrics->seconds=seconds()-start;return status;
}
