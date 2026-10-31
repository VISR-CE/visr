/* Apache-2.0. Authored marker geometry with source-fitted gradients. */
#include "menu_picture_shapes.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double x, y; } Point;
typedef struct { double c[4]; } Pixel;
typedef struct {
    const Point *points;
    unsigned count;
} Polygon;
typedef struct {
    Polygon polygons[2];
    unsigned count;
    double edge[3], width, edge_alpha, glow, sigma, alpha;
    int kind;
} Shape;

/* These small authored shapes use source-pixel coordinates. The gradient
 * itself is fitted from the imported picture in fit_gradient(). */
static const Point red_front[] = {{100.8,20.35},{126.7,20.35},{113.75,33.1}};
static const Point red_rear[] = {{113.75,20.35},{124.75,31.05},{113.75,41.65},{102.75,31.05}};
static const Point green[] = {{185,13.05},{223.9,13.05},{203.55,31.8}};
static const Point arrow[] = {{110.35,17.15},{167.45,17.15},{167.45,47.1},{195.9,47.1},{138.9,94.6},{81.6,47.1},{110.35,47.1}};
static const Point turret[] = {{94,28},{98.5,28},{104,32.5},{114,32.5},{115.3,34},{115.3,37},{107,38.5},{104.5,47},{99,47},{98,40},{94,39}};
static const Shape shapes[] = {
    {{{red_rear,4},{red_front,3}},2,{1,43.0/255,44.0/255},.8,.66,.045,.55,.4,0},
    {{{green,3},{NULL,0}},1,{0,1,0},.8,.56,.035,.55,68.0/255,1},
    {{{arrow,7},{NULL,0}},1,{41.0/255,150.0/255,1},.7,.95,.17,1.65,.6,2}
};
static double clip(double v) { return fmin(1, fmax(0,v)); }
static int stopped(const volatile int *cancel) { return cancel && *cancel; }
static double distance_to(Polygon p, double x, double y, int *inside)
{
    double minimum = 1e10;
    int in = 0;
    for (unsigned i=0,j=p.count-1;i<p.count;j=i++) {
        Point a=p.points[j], b=p.points[i];
        double dx=b.x-a.x,dy=b.y-a.y;
        double t=clip(((x-a.x)*dx+(y-a.y)*dy)/(dx*dx+dy*dy));
        minimum=fmin(minimum,hypot(x-a.x-t*dx,y-a.y-t*dy));
        if ((a.y>y)!=(b.y>y) && x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x) in=!in;
    }
    *inside=in;
    return minimum;
}
static double shape_distance(const Shape *s,double x,double y,int *inside)
{
    double d=1e10; *inside=0;
    for(unsigned i=0;i<s->count;i++) {
        int in; d=fmin(d,distance_to(s->polygons[i],x,y,&in)); *inside|=in;
    }
    return d;
}
static Pixel read_pixel(const uint8_t *p)
{
    Pixel v; v.c[3]=p[3]/255.0;
    for(unsigned c=0;c<3;c++) v.c[c]=p[c]/255.0*v.c[3];
    return v;
}
static Pixel over(Pixel back,Pixel front)
{
    for(unsigned c=0;c<4;c++) front.c[c]+=back.c[c]*(1-front.c[3]);
    return front;
}
static int fit_gradient(const Shape *s,const uint8_t *source,double intercept[3],double slope[3],const volatile int *cancel)
{
    double n=0,sy=0,syy=0,sc[3]={0},syc[3]={0};
    Polygon p=s->polygons[s->count-1];
    for(unsigned y=0;y<256;y++) {
        if(stopped(cancel)) return 0;
        for(unsigned x=0;x<512;x++) {
            int in; double yy=y+.5;
            if(distance_to(p,x+.5,yy,&in)<=1.6 || !in) continue;
            const uint8_t *a=source+((size_t)y*512+x)*4;
            int take=s->kind==0 ? yy<28 && a[0]/255.0>.9 && a[2]/255.0<.22 :
                     s->kind==1 ? a[1]/255.0>.8 && a[0]/255.0<.1 :
                     yy>20 && yy<72 && a[3]/255.0>.57 && a[3]/255.0<.63;
            if(!take) continue;
            n++;sy+=yy;syy+=yy*yy;
            for(unsigned c=0;c<3;c++){ double v=a[c]/255.0;sc[c]+=v;syc[c]+=yy*v; }
        }
    }
    double determinant=n*syy-sy*sy;
    /* A malformed or unrelated source must not supply an invented gradient. */
    if(n<2 || determinant<=0) return 0;
    for(unsigned c=0;c<3;c++) {
        slope[c]=(n*syc[c]-sy*sc[c])/determinant;
        intercept[c]=(sc[c]-slope[c]*sy)/n;
    }
    return 1;
}
static Pixel layer_at(const Shape *s,double x,double y,const double base[3],const double slope[3])
{
    Pixel layer={{0}};
    for(unsigned i=0;i<s->count;i++) {
        int in; double d=distance_to(s->polygons[i],x,y,&in);
        double fill=in?s->alpha:0;
        double line=d<s->width/2?s->edge_alpha:0;
        double glow=s->glow*exp(-.5*(d/s->sigma)*(d/s->sigma));
        Pixel next;
        next.c[3]=1-(1-fill)*(1-line)*(1-glow);
        for(unsigned c=0;c<3;c++) next.c[c]=clip(base[c]+slope[c]*y)*fill*(1-line)*(1-glow)+s->edge[c]*(line+glow*(1-line));
        if(s->kind==0 && (in || y<21)) memset(&layer,0,sizeof(layer));
        layer=over(layer,next);
    }
    return layer;
}
static int redraw(const Shape *s,const uint8_t *source,uint8_t *rgba,unsigned scale,const volatile int *cancel)
{
    double base[3],slope[3];
    if(!fit_gradient(s,source,base,slope,cancel)) return 0;
    double left=512,top=256,right=0,bottom=0,pad=s->kind==2?22:18;
    for(unsigned i=0;i<s->count;i++) for(unsigned j=0;j<s->polygons[i].count;j++) {
        Point p=s->polygons[i].points[j];
        left=fmin(left,p.x);right=fmax(right,p.x);top=fmin(top,p.y);bottom=fmax(bottom,p.y);
    }
    unsigned x0=(unsigned)fmax(0,floor(left-pad))*scale;
    unsigned x1=(unsigned)fmin(512,ceil(right+pad))*scale;
    unsigned y0=(unsigned)fmax(0,floor(top-pad))*scale;
    unsigned y1=(unsigned)fmin(256,ceil(bottom+pad))*scale;
    unsigned width=x1-x0;
    Pixel *row=malloc((size_t)width*sizeof(*row));
    if(!row) return 0;
    for(unsigned yy=y0;yy<y1;yy++) {
        if(stopped(cancel)) { free(row);return 0; }
        double y=(yy+.5)/scale;
        uint8_t *output=rgba+((size_t)yy*512*scale+x0)*4;
        for(unsigned i=0;i<width;i++) row[i]=read_pixel(output+i*4);
        unsigned l=width,r=0;
        if(s->kind==2) for(unsigned i=0;i<width;i++) {
            int in;double d=shape_distance(s,(x0+i+.5)/scale,y,&in);
            if(in || d<1.8) { if(l==width) l=i;r=i; }
        }
        if(l<width) { l=l>2?l-2:0;r=r+2<width?r+2:width-1; }
        for(unsigned i=0;i<width;i++) {
            double x=(x0+i+.5)/scale;
            int in;double d=shape_distance(s,x,y,&in);
            Pixel old=row[i],clean={{0}},layer={{0}};
            double weight;
            if(s->kind==1) weight=in || d<3.3;
            else if(s->kind==0) {
                int ti;double td=distance_to((Polygon){turret,11},x,y,&ti);
                double fa=clip((old.c[3]-.4)/.6);
                double blue=clip((old.c[2]-.22*old.c[0])/.55);
                double keep=ti?1:exp(-.5*(td/.85)*(td/.85));
                fa=y>28 && x<117?fmax(fa,blue)*keep:0;
                const double red[3]={1,.14,.15};
                clean.c[3]=fa;
                for(unsigned c=0;c<3;c++) clean.c[c]=fmin(clip(old.c[c]-red[c]*.4*(1-clip((old.c[3]-.4)/.6))),fa);
                double halo=ti?0:.22*exp(-td/3)*clip((16-td)/5);
                Pixel h={{21.0/255*halo,140.0/255*halo,252.0/255*halo,halo}};
                clean=over(h,clean);
                double own=!in && old.c[2]>old.c[0]*1.8?clip((d+3-td)/3):0;
                for(unsigned c=0;c<4;c++) clean.c[c]=clean.c[c]*(1-own)+old.c[c]*own;
                weight=in?1:clip((14-d)/10);
            } else {
                double top_y=79.8+.00016*(x-139)*(x-139),cd=fabs(y-top_y);
                double halo=.2755546425*exp(-cd/4)*clip((16-cd)/5);
                clean=(Pixel){{21.0/255*halo,140.0/255*halo,252.0/255*halo,halo}};
                if(l<width && y>=top_y) {
                    clean=old;
                    if(in || d<1.8) {
                        double t=clip(((double)i-l)/fmax(1,(double)r-l));
                        for(unsigned c=0;c<4;c++) clean.c[c]=row[l].c[c]*(1-t)+row[r].c[c]*t;
                    }
                }
                double ca=clip(.5-cd)*.95*(in || d<1.8);
                Pixel line={{35.0/255*ca,143.0/255*ca,246.0/255*ca,ca}};
                clean=over(clean,line);
                if(y>=94) clean=old;
                weight=(in?1:clip((19-d)/12))*clip((96-y)/1.5);
            }
            if(weight<=0) continue;
            for(unsigned sy=0;sy<2;sy++) for(unsigned sx=0;sx<2;sx++) {
                Pixel sample=layer_at(s,(x0+i+(sx+.5)/2)/scale,(yy+(sy+.5)/2)/scale,base,slope);
                for(unsigned c=0;c<4;c++) layer.c[c]+=sample.c[c]*.25;
            }
            Pixel target=s->kind==0?over(layer,clean):over(clean,layer);
            for(unsigned c=0;c<4;c++) target.c[c]=old.c[c]*(1-weight)+target.c[c]*weight;
            for(unsigned c=0;c<3;c++) output[i*4+c]=(uint8_t)lround(clip(target.c[3]>0?target.c[c]/target.c[3]:0)*255);
            output[i*4+3]=(uint8_t)lround(clip(target.c[3])*255);
        }
    }
    free(row);return 1;
}
int menu_picture_shapes(unsigned picture,const uint8_t *source,uint8_t *rgba,unsigned scale,const volatile int *cancel)
{
    if(stopped(cancel)) return 0;
    if(picture!=2 && picture!=3) return 1;
    if(!source || !rgba || !scale || scale>UINT_MAX/512 ||
       (size_t)scale>SIZE_MAX/512/256/4/scale) return 0;
    if(picture==3) return redraw(&shapes[2],source,rgba,scale,cancel);
    return redraw(&shapes[0],source,rgba,scale,cancel) &&
           redraw(&shapes[1],source,rgba,scale,cancel);
}

int menu_carousel_shapes_source(const uint8_t *source,unsigned width,unsigned height)
{
    static const uint64_t hashes[]={UINT64_C(0xfc5484a0d9744add),UINT64_C(0x9cdebcc37bf18524),
        UINT64_C(0x0944245a78c04241),UINT64_C(0x35288b532daae554)};
    if(!source || width!=32 || height!=256) return -1;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<(size_t)width*height*4;i++) hash=(hash^source[i])*UINT64_C(1099511628211);
    for(unsigned i=0;i<4;i++) if(hash==hashes[i]) return (int)i;
    return -1;
}

/* The list draws frame zero at rest and frame one while stepping that direction.
 * The source's DXT blocks distort the rim's color; fit each RGBA channel along
 * its height, separately from the fill, rather than magnifying those blocks. */
static int carousel_gradient(Polygon polygon,const uint8_t *source,int rim,double base[4],double slope[4])
{
    double n=0,sy=0,syy=0,sc[4]={0},syc[4]={0};
    for(unsigned y=0;y<171;y++) for(unsigned x=0;x<18;x++) {
        int inside;
        double d=distance_to(polygon,x+.5,y+.5,&inside);
        const uint8_t *p=source+((size_t)y*32+x)*4;
        if(rim ? p[3]<110 || d>.55 : !inside || d<2) continue;
        double yy=y+.5;
        n++;sy+=yy;syy+=yy*yy;
        for(unsigned c=0;c<4;c++) { double v=p[c]/255.0;sc[c]+=v;syc[c]+=yy*v; }
    }
    double determinant=n*syy-sy*sy;
    if(n<2 || determinant<=0) return 0;
    for(unsigned c=0;c<4;c++) {
        slope[c]=(n*syc[c]-sy*sc[c])/determinant;
        base[c]=(sc[c]-slope[c]*sy)/n;
    }
    return 1;
}
int menu_carousel_shapes(const uint8_t *source,unsigned width,unsigned height,uint8_t *rgba,
    unsigned scale,const volatile int *cancel)
{
    int picture=menu_carousel_shapes_source(source,width,height);
    if(picture<0 || !rgba || (scale!=2 && scale!=4)) return 0;
    if(stopped(cancel)) return -1;
    const Point left[]={{17.5,.5},{.5,85.5},{17.5,170.5}};
    const Point right[]={{.5,.5},{17.5,85.5},{.5,170.5}};
    Polygon polygon={picture<2?left:right,3};
    double base[2][4],slope[2][4];
    if(!carousel_gradient(polygon,source,0,base[0],slope[0]) ||
       !carousel_gradient(polygon,source,1,base[1],slope[1])) return 0;
    size_t size=(size_t)width*height*scale*scale*4;
    uint8_t *result=calloc(size,1);
    if(!result) return 0;
    for(unsigned y=0;y<172*scale;y++) {
        if(stopped(cancel)) { free(result);return -1; }
        for(unsigned x=0;x<19*scale;x++) {
            Pixel pixel={{0}};
            for(unsigned sy=0;sy<4;sy++) for(unsigned sx=0;sx<4;sx++) {
                double xx=(x+(sx+.5)/4)/scale,yy=(y+(sy+.5)/4)/scale;
                int inside;double d=distance_to(polygon,xx,yy,&inside);
                int layer=d<=.5?1:0;
                if(!inside && !layer) continue;
                double alpha=clip(base[layer][3]+slope[layer][3]*yy);
                pixel.c[3]+=alpha/16;
                for(unsigned c=0;c<3;c++) pixel.c[c]+=clip(base[layer][c]+slope[layer][c]*yy)*alpha/16;
            }
            uint8_t *p=result+((size_t)y*width*scale+x)*4;
            for(unsigned c=0;c<3;c++) p[c]=(uint8_t)lround(clip(pixel.c[3]?pixel.c[c]/pixel.c[3]:0)*255);
            p[3]=(uint8_t)lround(pixel.c[3]*255);
        }
    }
    if(stopped(cancel)) { free(result);return -1; }
    memcpy(rgba,result,size);free(result);return 1;
}
