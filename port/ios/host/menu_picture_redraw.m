/* Apache-2.0. The button tables and erase slots reproduce the approved authored redraw.
   Core Text reads our bundled Overpass bytes directly; it never substitutes a system font. */
#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
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
/* The approved artwork was authored at twice 2048 x 1024, then reduced with
   Lanczos. Keep those sample locations at both import scales: changing the
   working resolution changes the narrow outline and the overlap of the glows. */
enum { keyboard_width=4096,keyboard_height=2048 };
static double keyboard_distance(double x,double y,double bottom) {
    double top=bottom==253?157:271;
    double qx=fabs(x-578)-477,qy=fabs(y-(top+bottom)/2)-(bottom-top)/2+14;
    return hypot(fmax(qx,0),fmax(qy,0))+fmin(fmax(qx,qy),0)-14;
}
static double keyboard_outer_distance(double x,double y) {
    double qx=fabs(x-578)-477,qy=fabs(y-414)-243;
    return hypot(fmax(qx,0),fmax(qy,0))+fmin(fmax(qx,qy),0)-14;
}
static double keyboard_clamp(double x) { return fmin(1,fmax(0,x)); }
static double keyboard_glow(double d) {
    double fade=keyboard_clamp((46-d)/20);fade=fade*fade*(3-2*fade);
    return .78*exp(-fmax(d,0)/15)*fade;
}
static void keyboard_over(double *p,const double *rgb,double alpha) {
    double a=alpha+p[3]*(1-alpha);
    for(int c=0;c<3;c++)p[c]=a?(rgb[c]*alpha+p[c]*p[3]*(1-alpha))/a:0;
    p[3]=a;
}
static void keyboard_composite(uint8_t *p,const unsigned *rgb,unsigned alpha) {
    /* Pillow's alpha_composite retains straight RGB until the final resize. */
    unsigned a=alpha*255+p[3]*(255-alpha);
    if(!a)return;
    for(int c=0;c<3;c++)p[c]=(uint8_t)((rgb[c]*alpha*255+p[c]*p[3]*(255-alpha)+a/2)/a);
    p[3]=(uint8_t)((a+127)/255);
}
static void keyboard_text(CGContextRef ctx,CTFontRef font,int x,int y,const char *label) {
    NSDictionary *attrs=@{(__bridge NSString *)kCTFontAttributeName:(__bridge id)font,
        (__bridge NSString *)kCTForegroundColorFromContextAttributeName:@YES};
    NSAttributedString *text=[[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:label] attributes:attrs];
    CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text);
    double width=CTLineGetTypographicBounds(line,NULL,NULL,NULL);
    /* Pillow's mm anchor uses the rounded font ascent and descent. Its input
       positions are truncated before applying the centered text anchor. */
    double baseline=(int)(y*2.048)+(ceil(CTFontGetAscent(font))-ceil(CTFontGetDescent(font)))/2;
    CGContextSetTextPosition(ctx,(int)(x*2.048)-width/2+.5,keyboard_height-baseline);
    CTLineDraw(line,ctx);CFRelease(line);
}
static double keyboard_lanczos(double x) {
    if(fabs(x)<1e-12)return 1;
    if(fabs(x)>=3)return 0;
    return sin(M_PI*x)*sin(M_PI*x/3)/(M_PI*M_PI*x*x/3);
}
static uint8_t keyboard_byte(double x) { return (uint8_t)fmin(255,fmax(0,floor(x+.5))); }
static int keyboard_picture(uint8_t *result,unsigned w,unsigned h,const volatile int *cancel,
    const char *resources,struct menu_picture_redraw_metrics *metrics) {
    const size_t count=(size_t)keyboard_width*keyboard_height;
    uint8_t *pixels=calloc(count,4),*mask=calloc(count,1),*intermediate=NULL;
    float *temp=malloc(count*sizeof(float));
    CGContextRef ctx=NULL;CGColorSpaceRef space=NULL;CGFontRef cgfont=NULL;CTFontRef font=NULL;
    int status=0;
    if(metrics)metrics->scratch_bytes+=(count*9)+(size_t)w*keyboard_height*4;
    if(!pixels||!mask||!temp)goto done;
    {
        NSString *path=resource(resources,@"Overpass-650",@"ttf");
        NSData *data=path?[NSData dataWithContentsOfFile:path]:nil;
        CGDataProviderRef provider=data?CGDataProviderCreateWithCFData((__bridge CFDataRef)data):NULL;
        cgfont=provider?CGFontCreateWithDataProvider(provider):NULL;
        if(provider)CGDataProviderRelease(provider);
    }
    if(!cgfont)goto done;
    font=CTFontCreateWithGraphicsFont(cgfont,88,NULL,NULL);if(!font)goto done;
    space=CGColorSpaceCreateDeviceGray();
    ctx=CGBitmapContextCreate(mask,keyboard_width,keyboard_height,8,keyboard_width,space,(CGBitmapInfo)kCGImageAlphaNone);
    if(!ctx)goto done;
    CGContextSetGrayFillColor(ctx,1,1);CGContextSetGrayStrokeColor(ctx,1,1);
    CGContextSetTextMatrix(ctx,CGAffineTransformIdentity);
    static const double glowRGB[]={36,150,252},fillRGB[]={2,25,66},coreRGB[]={41,209,255};
    /* Both authored glows end at distance 46; leave the other pixels clear. */
    for(unsigned y=222;y<1434;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=82;x<2230;x++) {
            double xx=(x+.5)/2,yy=(y+.5)/2;
            double d1=keyboard_distance(xx,yy,253),d2=keyboard_distance(xx,yy,671),d=fmin(d1,d2);
            double g1=keyboard_glow(d1),g2=keyboard_glow(d2),blend=keyboard_clamp((10-keyboard_outer_distance(xx,yy))/20);
            blend=blend*blend*(3-2*blend);
            double p[4]={0,0,0,0};
            keyboard_over(p,glowRGB,fmax(g1,g2)*(1-blend)+(1-(1-g1)*(1-g2))*blend);
            double inside=keyboard_clamp(.5-d*2);
            p[3]*=1-inside;
            keyboard_over(p,fillRGB,inside*(120/255.));
            keyboard_over(p,coreRGB,keyboard_clamp(.5-(fabs(d)-1.7)*2)*.85);
            uint8_t *dst=pixels+((size_t)y*keyboard_width+x)*4;
            for(int c=0;c<3;c++)dst[c]=(uint8_t)p[c];
            dst[3]=(uint8_t)(p[3]*255);
        }
    }
    static const int cols[]={365,435,505,575,645,715,785,855,925,995};
    static const int rows[]={313,383,454,524,592};
    static const char *const letters[]={"1234567890","abcdefghij","klmnopqrst","uvwxyz"};
    for(int row=0;row<4;row++)for(unsigned col=0;col<strlen(letters[row]);col++) {
        char label[]={letters[row][col],0};keyboard_text(ctx,font,cols[col],rows[row],label);
    }
    keyboard_text(ctx,font,213,rows[0],"Done");keyboard_text(ctx,font,213,rows[1],"Shift");
    keyboard_text(ctx,font,213,rows[2],"Caps Lock");keyboard_text(ctx,font,882,rows[3],"Backspace");
    keyboard_text(ctx,font,532,rows[4],"Space");
    CGContextSetLineWidth(ctx,12);
    for(int arrow=0;arrow<2;arrow++) {
        int x=arrow?948:808,sign=arrow?1:-1,y=rows[4];
        int points[][4]={{x-sign*18,y,x+sign*18,y},{x+sign*18,y,x+sign*4,y-12},{x+sign*18,y,x+sign*4,y+12}};
        for(int i=0;i<3;i++) {
            int x0=(int)(points[i][0]*2.048),y0=(int)(points[i][1]*2.048);
            int x1=(int)(points[i][2]*2.048),y1=(int)(points[i][3]*2.048);
            /* Pillow includes both endpoints and places an even-width
               horizontal line one pixel lower when it runs to the right. */
            if(i==0)CGContextFillRect(ctx,CGRectMake(MIN(x0,x1),keyboard_height-y0-(sign>0?7:6),abs(x1-x0)+1,12));
            else {
                CGContextMoveToPoint(ctx,x0+.5,keyboard_height-y0-.5);
                CGContextAddLineToPoint(ctx,x1+.5,keyboard_height-y1-.5);
                CGContextStrokePath(ctx);
            }
        }
    }
    CGContextRelease(ctx);ctx=NULL;
    /* The shadow is translated by eight working pixels, then blurred at sigma
       four, as in the authored drawing. Limit the convolution to actual glyph
       bounds; the keyboard's transparent margins need no shadow samples. */
    int minX=keyboard_width,maxX=0,minY=keyboard_height,maxY=0;
    for(int y=0;y<keyboard_height;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(int x=0;x<keyboard_width;x++)if(mask[(size_t)y*keyboard_width+x]) {
            minX=MIN(minX,x);maxX=MAX(maxX,x);minY=MIN(minY,y);maxY=MAX(maxY,y);
        }
    }
    int shadowMinX=MAX(0,minX+8-12),shadowMaxX=MIN(keyboard_width-1,maxX+8+12);
    int shadowMinY=MAX(0,minY+8-12),shadowMaxY=MIN(keyboard_height-1,maxY+8+12);
    float weights[25],total=0;
    for(int i=-12;i<=12;i++){weights[i+12]=expf(-i*i/32.f);total+=weights[i+12];}
    for(int i=0;i<25;i++)weights[i]/=total;
    for(int y=minY+8;y<=maxY+8&&y<keyboard_height;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(int x=shadowMinX;x<=shadowMaxX;x++) {
            float a=0;
            for(int d=-12;d<=12;d++) {
                int xx=x-8+d;if(xx>=0&&xx<keyboard_width)a+=mask[(size_t)(y-8)*keyboard_width+xx]*weights[d+12];
            }
            temp[(size_t)y*keyboard_width+x]=a;
        }
    }
    static const unsigned shadowRGB[]={0,0,30},textRGB[]={2,55,198};
    for(unsigned y=0;y<keyboard_height;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=0;x<keyboard_width;x++) {
            float a=0;
            if((int)x>=shadowMinX&&(int)x<=shadowMaxX&&(int)y>=shadowMinY&&(int)y<=shadowMaxY)
                for(int d=-12;d<=12;d++){int yy=(int)y+d;if(yy>=minY+8&&yy<=maxY+8&&yy<keyboard_height)a+=temp[(size_t)yy*keyboard_width+x]*weights[d+12];}
            size_t i=(size_t)y*keyboard_width+x;
            keyboard_composite(pixels+i*4,shadowRGB,(unsigned)lrintf(a*200/255));
            keyboard_composite(pixels+i*4,textRGB,mask[i]);
            for(int c=0;c<3;c++)pixels[i*4+c]=(uint8_t)((pixels[i*4+c]*pixels[i*4+3]+127)/255);
        }
    }
    free(temp);temp=NULL;free(mask);mask=NULL;
    /* Pillow resizes RGBA through premultiplied RGBa, quantizing after each
       separable pass. Keep that order so transparent edges retain their color. */
    intermediate=malloc((size_t)w*keyboard_height*4);if(!intermediate)goto done;
    unsigned factor=keyboard_width/w;
    double filter[25],filterSum=0;
    int offset=(int)floor(factor*.5-3*factor+.5),taps=6*factor;
    for(int i=0;i<taps;i++){filter[i]=keyboard_lanczos((offset+i+.5-factor*.5)/factor);filterSum+=filter[i];}
    for(int i=0;i<taps;i++)filter[i]/=filterSum;
    for(unsigned y=0;y<keyboard_height;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=0;x<w;x++) {
            int first=(int)(x*factor)+offset;
            for(int c=0;c<4;c++) {
                double value=0;for(int i=0;i<taps;i++){int xx=first+i;if(xx>=0&&xx<keyboard_width)value+=pixels[((size_t)y*keyboard_width+xx)*4+c]*filter[i];}
                intermediate[((size_t)y*w+x)*4+c]=keyboard_byte(value);
            }
        }
    }
    free(pixels);pixels=NULL;
    for(unsigned y=0;y<h;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        int first=(int)(y*factor)+offset;
        for(unsigned x=0;x<w;x++) {
            uint8_t *dst=result+((size_t)y*w+x)*4;
            for(int c=0;c<4;c++) {
                double value=0;for(int i=0;i<taps;i++){int yy=first+i;if(yy>=0&&yy<keyboard_height)value+=intermediate[((size_t)yy*w+x)*4+c]*filter[i];}
                dst[c]=keyboard_byte(value);
            }
            for(int c=0;c<3;c++)dst[c]=dst[3]?(uint8_t)MIN(255,dst[c]*255/dst[3]):0;
        }
    }
    status=1;
done:
    if(ctx)CGContextRelease(ctx);if(space)CGColorSpaceRelease(space);if(font)CFRelease(font);if(cgfont)CGFontRelease(cgfont);
    free(pixels);free(mask);free(temp);free(intermediate);return status;
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
        status=keyboard_picture(result,w,h,cancel,resources,metrics);
        if(status<=0)goto done;
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
