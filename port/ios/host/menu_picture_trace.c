/* Apache-2.0. Alpha-mask contour tracing, cubic fitting and sampled glow replacement.
   The approved prototype's exposed union is sufficient: hidden subject edges stay in the bpf pixels. */
#include "menu_picture_trace.h"
#include "menu_picture_shapes.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct point { float x, y; };
static float clampf(float x, float a, float b) { return fminf(b, fmaxf(a, x)); }
static double clock_seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec+t.tv_nsec*1e-9; }
static uint64_t digest(const uint8_t *p, size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++) h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
/* Pictures 0-5 and 6-10 are the editor silhouettes and controller maps. 11-16 are the six gametype icons with a
   gradient field (256 x 128, ui\\shell\\bitmaps\\game_type_grafix; the sixth frame is a plain question mark). */
int menu_picture_trace_source(const uint8_t *p, unsigned w, unsigned h) {
    static const uint64_t hashes[]={UINT64_C(0xf36bb16cd42f5bd6),UINT64_C(0xe632c00dd1625c3c),
        UINT64_C(0x7adcf52f208bec59),UINT64_C(0x6ea78954e953d88b),UINT64_C(0x7ce8581a05529296),UINT64_C(0xabbf90e26606d4ae),
        UINT64_C(0x10abeb8e84292ebb),UINT64_C(0x4f2391de207baaf5),UINT64_C(0xb21af00c0b190d4b),
        UINT64_C(0xe24cf6469a4cbaed),UINT64_C(0xa30fdfccfcf19037),
        UINT64_C(0xc6b773ef9f4465e8),UINT64_C(0x7a0bf259285a236c),UINT64_C(0x7d72ff39be91b80b),
        UINT64_C(0x1bc31137c979f6ca),UINT64_C(0xb31f2f1343077074),UINT64_C(0x12d016601cc04f8c)};
    unsigned first,last;
    if(!p)return -1;
    if(w==512&&h==256){first=0;last=6;}
    else if(w==512&&h==512){first=6;last=11;}
    else if(w==256&&h==128){first=11;last=17;}
    else return -1;
    uint64_t hash=digest(p,(size_t)w*h*4);
    for(unsigned i=first;i<last;i++)if(hash==hashes[i])return (int)i;
    return -1;
}
static float cubic(float x) {
    x=fabsf(x);
    return x<=1 ? (1.25f*x-2.25f)*x*x+1 : x<2 ? ((-.75f*x+3.75f)*x-6)*x+3 : 0;
}
/* OpenCV's prototype uses cubic reconstruction on matching texel centers. */
static void sample(const uint8_t *p, unsigned sw, unsigned h, float x, float y, float out[4]) {
    int ix=(int)floorf(x),iy=(int)floorf(y); memset(out,0,4*sizeof(float));
    for(int j=-1;j<=2;j++)for(int i=-1;i<=2;i++) {
        float v=cubic(x-ix-i)*cubic(y-iy-j)/255;
        int sx=(int)clampf(ix+i,0,sw-1),sy=(int)clampf(iy+j,0,h-1);
        for(int c=0;c<4;c++)out[c]+=v*p[((size_t)sy*sw+sx)*4+c];
    }
    for(int c=0;c<4;c++)out[c]=clampf(out[c],0,1);
}
static float blue_signal(const uint8_t *p, unsigned sw, unsigned h, float x, float y) {
    float v[4];sample(p,sw,h,x,y,v);return (v[2]-.55f*v[1]-.25f*v[0])*v[3];
}
static int colored(const float v[4]) {
    return v[3]>.25f&&fmaxf(v[0],v[1])>.2f&&
        ((v[0]>v[1]*1.2f&&v[0]>v[2]*.65f)||(v[1]>v[0]*1.35f&&v[1]>v[2]*.85f));
}
static float percentile(unsigned bins[256],float fraction) {
    unsigned sum=0,acc=0;for(int i=0;i<256;i++)sum+=bins[i];
    for(int i=0;i<256;i++){acc+=bins[i];if(acc>sum*fraction)return i/255.f;}return 0;
}
static float median(unsigned bins[256]) { return percentile(bins,.5f); }
/* The gradient icons draw a black figure and a thin blue line over a field that only varies by row. Each row's
   field color comes from its first opaque pixels, which the figure never reaches; how far a pixel departs from it
   is the figure's coverage. The result replaces the alpha channel for detection only. */
static int field_coverage(const uint8_t *source,unsigned sw,unsigned sh,uint8_t *out,uint8_t (*rows)[3]) {
    memcpy(out,source,(size_t)sw*sh*4);
    int last_row=-1;
    for(unsigned y=0;y<sh;y++) {
        const uint8_t *row=source+(size_t)y*sw*4;unsigned first=0,last=sw;
        while(first<sw&&row[first*4+3]<128)first++;
        while(last>first&&row[(last-1)*4+3]<128)last--;
        if(first+3>=last){for(unsigned x=0;x<sw;x++)out[((size_t)y*sw+x)*4+3]=0;memset(rows[y],0,3);continue;}
        last_row=(int)y;
        int ref[3];for(int c=0;c<3;c++)rows[y][c]=(uint8_t)(ref[c]=(row[(first+1)*4+c]+row[(first+2)*4+c])/2);
        int top=ref[0]>ref[1]?ref[0]:ref[1];if(ref[2]>top)top=ref[2];if(top<8)top=8;
        for(unsigned x=0;x<sw;x++) {
            const uint8_t *p=row+(x<last?x:last-1)*4;uint8_t *o=out+((size_t)y*sw+x)*4+3;
            if(x<first){*o=0;continue;}
            int d=0;for(int c=0;c<3;c++){int e=abs((int)p[c]-ref[c]);if(e>d)d=e;}
            float rel=clampf(((float)d/top-.2f)/.5f,0,1);
            *o=(uint8_t)(rel*255+.5f);
        }
    }
    /* The picture is a rectangle inside a larger texture. Figures that reach its right or bottom edge are
       continued past it, so the contour does not run along the edge; drawing stays inside the rectangle. */
    if(last_row<0)return 0;
    for(unsigned y=(unsigned)last_row+1;y<sh;y++){
        memcpy(out+(size_t)y*sw*4,out+(size_t)last_row*sw*4,(size_t)sw*4);memcpy(rows[y],rows[last_row],3);}
    return 1;
}
/* Moore neighborhood walk on the filled mask. Orthogonal ordering prevents diagonal junctions. */
static unsigned contour(const uint8_t *mask,unsigned w,unsigned h,unsigned start,struct point *p,unsigned cap,const volatile int *cancel) {
    static const int dx[]={-1,-1,0,1,1,1,0,-1},dy[]={0,-1,-1,-1,0,1,1,1};
    int x=start%w,y=start/w,bx=x-1,by=y,firstx=-1,firsty=-1;unsigned n=0;
    do {
        if(n==cap||(cancel&&*cancel))return 0;p[n++]=(struct point){x+.5f,y+.5f};
        int back=0;for(int k=0;k<8;k++)if(x+dx[k]==bx&&y+dy[k]==by){back=k;break;}
        int found=-1;
        for(int t=1;t<=8;t++) {int k=(back+t)%8,nx=x+dx[k],ny=y+dy[k];
            if(nx>=0&&ny>=0&&nx<(int)w&&ny<(int)h&&mask[(size_t)ny*w+nx]){found=k;break;}}
        if(found<0)return 0;
        int nx=x+dx[found],ny=y+dy[found];
        if(firstx<0){firstx=nx;firsty=ny;}else if(x==(int)(start%w)&&y==(int)(start/w)&&nx==firstx&&ny==firsty){n--;break;}
        bx=x+dx[(found+7)%8];by=y+dy[(found+7)%8];x=nx;y=ny;
    }while(n<cap);
    return n;
}
static float point_distance(struct point a,struct point b) {return hypotf(a.x-b.x,a.y-b.y);}
static void simplify(const struct point *p,unsigned first,unsigned last,uint8_t *keep,float epsilon) {
    if(last<=first+1)return;
    struct point a=p[first],b=p[last];float vx=b.x-a.x,vy=b.y-a.y,den=vx*vx+vy*vy,max=0;unsigned split=0;
    for(unsigned i=first+1;i<last;i++) {float t=den?clampf(((p[i].x-a.x)*vx+(p[i].y-a.y)*vy)/den,0,1):0;
        float d=hypotf(p[i].x-a.x-vx*t,p[i].y-a.y-vy*t);if(d>max){max=d;split=i;}}
    if(max>epsilon){keep[split]=1;simplify(p,first,split,keep,epsilon);simplify(p,split,last,keep,epsilon);}
}
static struct point tangent(struct point a,struct point b,struct point c) {
    float ax=b.x-a.x,ay=b.y-a.y,bx=c.x-b.x,by=c.y-b.y,la=hypotf(ax,ay),lb=hypotf(bx,by);
    if(la*lb<1e-8f||(ax*bx+ay*by)/(la*lb)<=.65f)return (struct point){0,0};
    float k=.5f*fminf(la,lb);return (struct point){(ax/la+bx/lb)*k,(ay/la+by/lb)*k};
}
static float curve_distance2(unsigned s,const uint16_t *coordinates,unsigned w,float x,float y) {
    float dx=s%w+(coordinates[s]&255)/255.f-x,dy=s/w+(coordinates[s]>>8)/255.f-y;
    return dx*dx+dy*dy;
}
static void update_nearest(int *nearest,const uint16_t *coordinates,unsigned w,unsigned h,int x,int y,int nx,int ny) {
    if(nx<0||ny<0||nx>=(int)w||ny>=(int)h)return;
    int s=nearest[(size_t)ny*w+nx];if(s<0)return;
    int old=nearest[(size_t)y*w+x];
    float d=curve_distance2((unsigned)s,coordinates,w,x+.5f,y+.5f);
    if(old<0||d<curve_distance2((unsigned)old,coordinates,w,x+.5f,y+.5f))nearest[(size_t)y*w+x]=s;
}
/* A four-source-pixel opening removes thin leaders and lettering before contouring.
   Sliding sums keep this linear in the decoded image area. */
static int mask_filter(const uint8_t *input,uint8_t *output,unsigned w,unsigned h,
    unsigned radius,int vertical,int dilate,const volatile int *cancel) {
    unsigned lines=vertical?w:h,length=vertical?h:w,step=vertical?w:1;
    for(unsigned line=0;line<lines;line++) {
        if(cancel&&*cancel)return 0;
        unsigned base=vertical?line:line*w,sum=0;
        for(unsigned k=0;k<=radius&&k<length;k++)sum+=input[base+k*step]!=0;
        for(unsigned k=0;k<length;k++) {
            output[base+k*step]=dilate?sum>0:sum==2*radius+1;
            if(k>=radius)sum-=input[base+(k-radius)*step]!=0;
            if(k+radius+1<length)sum+=input[base+(k+radius+1)*step]!=0;
        }
    }
    return 1;
}
static int leader_pixel(const uint8_t *p) {
    /* Leaders are pale cyan; the saturated blue silhouette has a lower red/blue ratio. */
    return p[3]>80&&p[2]>80&&p[0]>.23f*p[2]&&p[0]<.60f*p[2]&&p[1]>.48f*p[2];
}
int menu_picture_trace(const uint8_t *source,unsigned sw,unsigned sh,uint8_t *bpf,unsigned scale,
    const volatile int *cancel,struct menu_picture_trace_metrics *metrics) {
    double started=clock_seconds();if(metrics)memset(metrics,0,sizeof(*metrics));
    int picture=menu_picture_trace_source(source,sw,sh);
    if(picture<0||!bpf||(scale!=2&&scale!=4))return 0;
    if(cancel&&*cancel)return -1;
    /* Detection always runs at 4x; rendering at 2x uses the same fitted curves. */
    const unsigned w=sw*4,h=sh*4,n=w*h,cap=65536;
    const int controller=picture>=6&&picture<11,icon=picture>=11,gradient=icon;
    uint8_t *holes=NULL,*field=gradient?malloc((size_t)sw*sh*4):NULL;
    uint8_t (*field_rows)[3]=gradient?malloc((size_t)sh*3):NULL;
    if(gradient&&(!field||!field_rows||!field_coverage(source,sw,sh,field,field_rows))){free(field);free(field_rows);return 0;}
    /* Detection and ridge snapping read the field-relative coverage on a gradient icon, the player's alpha otherwise. */
    const uint8_t *detect=gradient?field:source;
    uint8_t *mask=calloc(n,1),*outside=calloc(n,1),*visited=calloc(n,1),*seed=calloc(n,1),*keep=calloc(cap+1,1);
    uint16_t *coordinates=calloc(n,sizeof(*coordinates));
    unsigned *queue=malloc((size_t)n*sizeof(*queue));int *nearest=malloc((size_t)n*sizeof(*nearest));
    struct point *path=malloc((cap+1)*sizeof(*path)),*smoothed=malloc((cap+1)*sizeof(*smoothed)),*q=malloc((cap+1)*sizeof(*q));
    uint8_t *result=malloc((size_t)sw*sh*scale*scale*4);int status=0;
    if(!mask||!outside||!visited||!seed||!keep||!coordinates||!queue||!nearest||!path||!smoothed||!q||!result)goto done;
    if(metrics)metrics->scratch_bytes=(size_t)n*14+(cap+1)*(1+3*sizeof(struct point))+(size_t)sw*sh*scale*scale*4;
    for(unsigned y=0;y<h;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=0;x<w;x++){float v[4];sample(detect,sw,sh,(x+.5f)/4-.5f,(y+.5f)/4-.5f,v);
            mask[y*w+x]=v[3]>(picture==4?.34f:gradient?.4f:.50f);
            /* Translucent red/green indicators stay separate; opaque armor still supplies its alpha silhouette. */
            if(picture==2&&v[3]<.85f&&colored(v))mask[y*w+x]=0;}
    }
    if(controller) {
        if(!mask_filter(mask,outside,w,h,8,0,0,cancel)||
           !mask_filter(outside,visited,w,h,8,1,0,cancel)||
           !mask_filter(visited,outside,w,h,8,0,1,cancel)||
           !mask_filter(outside,mask,w,h,8,1,1,cancel)){status=-1;goto done;}
        memset(outside,0,n);memset(visited,0,n);
    }
    /* Flood transparent exterior; enclosed detail is filled without changing source pixels. */
    unsigned head=0,tail=0;queue[tail++]=0;outside[0]=1;
    while(head<tail){if((head%2048)==0&&cancel&&*cancel){status=-1;goto done;}unsigned i=queue[head++],x=i%w,y=i/w;
        unsigned ns[4]={x?i-1:i,x+1<w?i+1:i,y?i-w:i,y+1<h?i+w:i};
        for(int k=0;k<4;k++){unsigned j=ns[k];if(!mask[j]&&!outside[j]){outside[j]=1;queue[tail++]=j;}}}
    /* An icon's enclosed background (the gaps between arms and torso) is traced as a second set of contours
       once it is large enough to show; smaller pockets are filled like any other enclosed detail. */
    holes=icon?calloc(n,1):NULL;
    if(icon&&!holes)goto done;
    if(icon)for(unsigned i=0;i<n;i++)holes[i]=!outside[i]&&!mask[i];
    for(unsigned i=0;i<n;i++)mask[i]=!outside[i];
    if(icon)for(unsigned i=0;i<n;i++)if(holes[i]==1) {
        head=0;tail=0;queue[tail++]=i;holes[i]=3;
        while(head<tail){if((head%2048)==0&&cancel&&*cancel){status=-1;goto done;}unsigned j=queue[head++],x=j%w,y=j/w;unsigned ns[4]={x?j-1:j,x+1<w?j+1:j,y?j-w:j,y+1<h?j+w:j};
            for(int k=0;k<4;k++){unsigned t=ns[k];if(holes[t]==1){holes[t]=3;queue[tail++]=t;}}}
        for(unsigned k=0;k<tail;k++){holes[queue[k]]=tail>=300?2:0;if(tail>=300)mask[queue[k]]=0;}
    }
    unsigned paths=0,area=0,boundary=0;
    for(int pass=0;pass<(icon?2:1);pass++)
    for(unsigned i=0;i<n;i++)if((pass?holes[i]==2:mask[i])&&!visited[i]) {
        uint8_t *cm=pass?holes:mask;
        head=0;tail=0;queue[tail++]=i;visited[i]=1;
        while(head<tail){if((head%2048)==0&&cancel&&*cancel){status=-1;goto done;}unsigned j=queue[head++],x=j%w,y=j/w;unsigned ns[4]={x?j-1:j,x+1<w?j+1:j,y?j-w:j,y+1<h?j+w:j};
            for(int k=0;k<4;k++){unsigned t=ns[k];if(cm[t]&&!visited[t]){visited[t]=1;queue[tail++]=t;}}}
        /* Controller maps also contain lettering whose compressed-alpha haze can survive opening.
           Keep only components large enough to be a controller; the one-path guard still rejects
           a second substantial subject instead of silently choosing one. */
        if(tail<(controller?n/40:icon?300u:1600u)){for(unsigned k=0;k<tail;k++)cm[queue[k]]=0;continue;}
        area+=tail;paths++;unsigned count=contour(cm,w,h,i,path,cap,cancel);if(count<12||count>10000){status=cancel&&*cancel?-1:0;goto done;}
        boundary+=count;
        float sigma=(picture==4||controller)?9.f:gradient?2.5f:2.f;int radius=(int)ceilf(sigma*3);
        memcpy(smoothed,path,count*sizeof(*path));
        /* Snap to the blue ridge along the alpha edge's normal, then smooth displacements. */
        for(unsigned k=0;k<count;k++) {
            if(cancel&&*cancel){status=-1;goto done;}
            struct point a=smoothed[(k+count-3)%count],b=smoothed[(k+3)%count];float nx=-(b.y-a.y),ny=b.x-a.x,len=hypotf(nx,ny);
            nx/=fmaxf(len,1e-6f);ny/=fmaxf(len,1e-6f);float best=-1e9f,shift=0,limit=(picture==4||controller)?1.1f:.85f;
            for(int t=0;t<23;t++){float d=-limit+2*limit*t/22.f;float v=blue_signal(detect,sw,sh,smoothed[k].x/4-.5f+nx*d,smoothed[k].y/4-.5f+ny*d)-.025f*d*d/(limit*limit);
                if(v>best){best=v;shift=d;}}
            path[k]=(struct point){smoothed[k].x+nx*shift*4,smoothed[k].y+ny*shift*4};
        }
        for(unsigned k=0;k<count;k++){struct point sum={0,0};float weight=0;
            for(int d=-6;d<=6;d++){float z=expf(-.5f*d*d/4);unsigned j=(unsigned)(((int)k+(int)count+d)%(int)count);
                sum.x+=(path[j].x-smoothed[j].x)*z;sum.y+=(path[j].y-smoothed[j].y)*z;weight+=z;}
            q[k]=(struct point){smoothed[k].x+sum.x/weight,smoothed[k].y+sum.y/weight};}
        /* Smooth after snapping, so the blue ridge cannot restore the source's raster steps. */
        for(unsigned k=0;k<count;k++) {
            if(cancel&&*cancel){status=-1;goto done;}
            struct point sum={0,0};float weight=0;
            for(int d=-radius;d<=radius;d++){unsigned j=(unsigned)(((int)k+(int)count+d)%(int)count);
                float z=expf(-.5f*d*d/(sigma*sigma));sum.x+=q[j].x*z;sum.y+=q[j].y*z;weight+=z;}
            path[k]=(struct point){sum.x/weight,sum.y/weight};
        }
        path[count]=path[0];memset(keep,0,count+1);keep[0]=keep[count]=1;simplify(path,0,count,keep,.76f);
        unsigned qc=0;for(unsigned k=0;k<count;k++)if(keep[k])q[qc++]=path[k];if(qc<4)goto done;
        for(unsigned k=0;k<qc;k++) {
            struct point a=q[k],b=q[(k+1)%qc],ta=tangent(q[(k+qc-1)%qc],a,b),tb=tangent(a,b,q[(k+2)%qc]);
            unsigned steps=(unsigned)ceilf(point_distance(a,b)*3)+3;
            for(unsigned j=0;j<=steps;j++){float t=(float)j/steps,u=1-t;
                float x=u*u*u*a.x+3*u*u*t*(a.x+ta.x/3)+3*u*t*t*(b.x-tb.x/3)+t*t*t*b.x;
                float y=u*u*u*a.y+3*u*u*t*(a.y+ta.y/3)+3*u*t*t*(b.y-tb.y/3)+t*t*t*b.y;
                int ix=(int)x,iy=(int)y;if(ix>=0&&iy>=0&&ix<(int)w&&iy<(int)h){unsigned pos=iy*w+ix;
                    uint16_t xy=(uint16_t)roundf((x-ix)*255)|((uint16_t)roundf((y-iy)*255)<<8);
                    /* Retain the curve sample closest to the pixel center for subpixel coverage. */
                    float d=(x-ix-.5f)*(x-ix-.5f)+(y-iy-.5f)*(y-iy-.5f);
                    if(!seed[pos]||d<curve_distance2(pos,coordinates,w,ix+.5f,iy+.5f))coordinates[pos]=xy;
                    seed[pos]=1;}}
        }
    }
    if(metrics){metrics->paths=paths;metrics->area=area;metrics->boundary=boundary;}
    /* Broad structural guards reject broken masks before any publication. */
    if(icon?(!paths||paths>12||area<n/60||area>n/2||boundary<200||boundary>10000):
       (paths!=((picture==0||picture==5)?3u:1u)||area<n/10||area>n/5||boundary<1000||boundary>10000))goto done;
    for(unsigned i=0;i<n;i++)nearest[i]=seed[i]?(int)i:-1;
    for(int pass=0;pass<2;pass++)for(unsigned yy=0;yy<h;yy++){
        if(cancel&&*cancel){status=-1;goto done;}
        int y=pass?(int)h-1-(int)yy:(int)yy;
        for(unsigned xx=0;xx<w;xx++){int x=pass?(int)w-1-(int)xx:(int)xx,d=pass?1:-1;
            update_nearest(nearest,coordinates,w,h,x,y,x+d,y);update_nearest(nearest,coordinates,w,h,x,y,x,y+d);
            update_nearest(nearest,coordinates,w,h,x,y,x+d,y+d);update_nearest(nearest,coordinates,w,h,x,y,x-d,y+d);}}
    unsigned profile[33][256]={{0}},line[3][256]={{0}},glow_hist[3][256]={{0}};
    for(unsigned y=0;y<h;y++){if(cancel&&*cancel){status=-1;goto done;}for(unsigned x=0;x<w;x++) {
        unsigned i=y*w+x;int s=nearest[i];if(s<0)goto done;
        float d=sqrtf(curve_distance2((unsigned)s,coordinates,w,x+.5f,y+.5f));if(d>=33)continue;
        float v[4];sample(detect,sw,sh,(x+.5f)/4-.5f,(y+.5f)/4-.5f,v);
        if(v[2]>v[1]*1.18f&&v[1]>v[0]*1.4f) {
            /* Include the core bin on both sides: snapping can put the ridge just inside the alpha mask. */
            if((!mask[i]||d<1)&&v[3]<.95f)profile[(int)d][bpf[((size_t)(y*scale/4)*(sw*scale)+x*scale/4)*4+3]]++;
            if(d<1.5f&&v[3]>.4f)for(int c=0;c<3;c++)line[c][(int)roundf(v[c]*255)]++;
            if(!mask[i]&&d>5&&d<16)for(int c=0;c<3;c++)glow_hist[c][(int)roundf(v[c]*255)]++;
        }
    }
    }
    float observed[33];for(int k=0;k<33;k++){unsigned total=0;for(int j=0;j<256;j++)total+=profile[k][j];observed[k]=total>20?median(profile[k]):0;}
    float best=1e9f,amp=.25f,spread=12,core=.65f,width=2.5f;
    for(int sp=0;sp<33;sp++)for(int cw=0;cw<27;cw++) {
        float a=0,b=0,c=0,d=0,e=0,ss=2+sp*.5f,ww=.4f+cw*.1f;
        for(int k=0;k<33;k++){float x=k+.5f,f=clampf((32-x)/12,0,1);f=f*f*(3-2*f);float g=expf(-x/ss)*f,l=expf(-.5f*x*x/(ww*ww));a+=g*g;b+=g*l;c+=l*l;d+=g*observed[k];e+=l*observed[k];}
        float den=a*c-b*b;if(den<1e-8f)continue;float aa=clampf((d*c-e*b)/den,.02f,.85f),cc=clampf((e*a-d*b)/den,.05f,.95f),err=0;
        for(int k=0;k<33;k++){float x=k+.5f,f=clampf((32-x)/12,0,1);f=f*f*(3-2*f);float v=aa*expf(-x/ss)*f+cc*expf(-.5f*x*x/(ww*ww))-observed[k];err+=v*v;}
        if(err<best){best=err;amp=aa;spread=ss;core=cc;width=clampf(ww*2.355f,2.2f,3);}
    }
    float lc[3],gc[3];for(int c=0;c<3;c++){lc[c]=gradient?percentile(line[c],.85f):median(line[c]);gc[c]=median(glow_hist[c]);}
    if(controller) {
        /* Restore the upscaled leaders above the new rim, including their antialiased fringe.
           These masks are computed from the player's pixels and never stored as geometry. */
        for(unsigned i=0;i<sw*sh;i++)outside[i]=leader_pixel(source+i*4);
        if(!mask_filter(outside,visited,sw,sh,2,0,1,cancel)||
           !mask_filter(visited,outside,sw,sh,2,1,1,cancel)){status=-1;goto done;}
    }
    if(gradient) {
        /* The alpha edge is the stroke's outer rim and the new line follows its center, so a pixel between them is
           outside the line although inside the mask. Eroding the inside test by the stroke's half width puts its
           edge on the line. */
        if(!mask_filter(mask,outside,w,h,3,0,0,cancel)||!mask_filter(outside,visited,w,h,3,1,0,cancel)){status=-1;goto done;}
        memcpy(mask,visited,n);
    }
    unsigned ow=sw*scale,oh=sh*scale;
    memcpy(result,bpf,(size_t)ow*oh*4);
    for(unsigned y=0;y<oh;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=0;x<ow;x++) {
            float px=(x+.5f)*4/scale,py=(y+.5f)*4/scale;
            unsigned gx=(unsigned)px,gy=(unsigned)py,i=gy*w+gx;int s=nearest[i];
            float dist=sqrtf(curve_distance2((unsigned)s,coordinates,w,px,py));int inside=mask[i];
            if(controller) {
                const uint8_t *old=bpf+((size_t)y*ow+x)*4;
                /* Protect cyan stroke coverage, not its rectangular dark source shadow. */
                if(outside[(y/scale)*sw+x/scale]&&old[2]>40&&old[0]>.23f*old[2]&&
                   old[0]<.60f*old[2]&&old[1]>.48f*old[2]&&old[3])continue;
                /* The redraw adds labels after tracing. Only the protected leaders belong
                   outside this halo; retaining source haze here would expose the erase slots. */
                if(!inside&&dist>=64) {
                    memset(result+((size_t)y*ow+x)*4,0,4);
                    continue;
                }
            }
            if(gradient) {
                /* The field has no glow to replace. The old soft stroke is covered with what lies on its side of
                   the new line: the row's field color outside, the figure's black inside. */
                const float band=inside?10.f:14.f;
                if(dist>=band||source[((size_t)(y/scale)*sw+x/scale)*4+3]<128)continue;
                uint8_t *out=result+((size_t)y*ow+x)*4;
                float row=clampf((y+.5f)/scale-.5f,0,sh-1),frac=row-floorf(row);
                unsigned r0=(unsigned)row,r1=r0+1<sh?r0+1:r0;
                float pixel_width=4.f/scale,weight=clampf((band-dist)/1.5f,0,1);
                float coverage=clampf((4.5f/2+pixel_width*.5f-dist)/pixel_width,0,1)*.95f;
                for(int c=0;c<3;c++){
                    float fill=inside?0:field_rows[r0][c]*(1-frac)+field_rows[r1][c]*frac;
                    out[c]=(uint8_t)roundf(out[c]*(1-weight)+fill*weight);
                    out[c]=(uint8_t)roundf(out[c]*(1-coverage)+lc[c]*255*coverage);}
                continue;
            }
            if(dist>=64||(inside&&dist>=4))continue;
            float v[4];sample(detect,sw,sh,(x+.5f)/scale-.5f,(y+.5f)/scale-.5f,v);if(colored(v))continue;
            float pixel_width=4.f/scale;
            float coverage=clampf((width/2+pixel_width*.5f-dist)/pixel_width,0,1)*fminf(.95f,core+amp);
            float fade=clampf((64-dist)/20,0,1);fade=fade*fade*(3-2*fade);float glow=amp*expf(-dist/spread)*fade;
            float target[4]={gc[0]*glow,gc[1]*glow,gc[2]*glow,glow};
            if(inside){
                /* Continue the closest inward fill through the old stroke's inner half. */
                float nx=px-(s%(int)w+(coordinates[s]&255)/255.f),ny=py-(s/(int)w+(coordinates[s]>>8)/255.f),len=hypotf(nx,ny);
                if(len<.5f){nx=gx>0&&gx+1<w?mask[i-1]-mask[i+1]:0;ny=gy>0&&gy+1<h?mask[i-w]-mask[i+w]:0;len=hypotf(nx,ny);}
                int tx=(int)clampf(x+nx/fmaxf(len,1)*6*scale/4,0,ow-1),ty=(int)clampf(y+ny/fmaxf(len,1)*6*scale/4,0,oh-1);
                const uint8_t *p=bpf+((size_t)ty*ow+tx)*4;target[3]=p[3]/255.f;for(int c=0;c<3;c++)target[c]=p[c]/255.f*target[3];
            }
            for(int c=0;c<3;c++)target[c]=target[c]*(1-coverage)+lc[c]*coverage;
            target[3]=target[3]*(1-coverage)+coverage;
            /* Controller exterior is fully replaced so the old dark leader shadows cannot
               survive at the halo's smooth zero-alpha cutoff. */
            float weight=inside?clampf(4-dist,0,1):(controller?1.f:clampf((64-dist)/4,0,1));
            uint8_t *out=result+((size_t)y*ow+x)*4;float olda=out[3]/255.f,a=olda*(1-weight)+target[3]*weight;
            for(int c=0;c<3;c++)out[c]=(uint8_t)roundf(clampf((out[c]/255.f*olda*(1-weight)+target[c]*weight)/fmaxf(a,1e-8f),0,1)*255);
            out[3]=(uint8_t)roundf(clampf(a,0,1)*255);
        }
    }
    if(!controller&&!menu_picture_shapes((unsigned)picture,source,result,scale,cancel)){status=cancel&&*cancel?-1:0;goto done;}
    if(cancel&&*cancel){status=-1;goto done;}
    memcpy(bpf,result,(size_t)ow*oh*4);status=1;
done:
    free(holes);free(field);free(field_rows);free(mask);free(outside);free(visited);free(seed);free(keep);free(queue);free(coordinates);free(nearest);free(path);free(smoothed);free(q);free(result);
    if(metrics)metrics->seconds=clock_seconds()-started;
    return status;
}
