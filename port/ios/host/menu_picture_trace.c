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
/* Pictures 0-5 and 6-10 are the editor silhouettes and controller maps. 11-16 are the six gametype icons of
   ui\\shell\\bitmaps\\game_type_grafix (256 x 128, a vertical gradient field behind a black figure; the sixth is a plain
   question mark), which are drawn again from level sets instead of traced. */
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
static void sample(const uint8_t *p, unsigned h, float x, float y, float out[4]) {
    int ix=(int)floorf(x),iy=(int)floorf(y); memset(out,0,4*sizeof(float));
    for(int j=-1;j<=2;j++)for(int i=-1;i<=2;i++) {
        float v=cubic(x-ix-i)*cubic(y-iy-j)/255;
        int sx=(int)clampf(ix+i,0,511),sy=(int)clampf(iy+j,0,h-1);
        for(int c=0;c<4;c++)out[c]+=v*p[((size_t)sy*512+sx)*4+c];
    }
    for(int c=0;c<4;c++)out[c]=clampf(out[c],0,1);
}
static float blue_signal(const uint8_t *p, unsigned h, float x, float y) {
    float v[4];sample(p,h,x,y,v);return (v[2]-.55f*v[1]-.25f*v[0])*v[3];
}
static int colored(const float v[4]) {
    return v[3]>.25f&&fmaxf(v[0],v[1])>.2f&&
        ((v[0]>v[1]*1.2f&&v[0]>v[2]*.65f)||(v[1]>v[0]*1.35f&&v[1]>v[2]*.85f));
}
static float median(unsigned bins[256]) {
    unsigned sum=0,acc=0;for(int i=0;i<256;i++)sum+=bins[i];
    for(int i=0;i<256;i++){acc+=bins[i];if(acc>sum/2)return i/255.f;}return 0;
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

/* ---- Gradient gametype icons: a black figure and a thin blue line over a field that varies only by row.
   Every edge is the sub-pixel iso-line of a smoothed, 8x bicubic copy of two fields: how far the pixel departs from
   its row's field in blue (the line) and in darkness (the figure). The thresholds keep the original line width, slits
   stay open, and the picture is composed again with anti-aliased edges from those levels. */
enum { iu=8, ipad=6 };
static void icon_resize8(const float *in,unsigned w,unsigned h,float *out,float *tmp) {
    /* separable bicubic, pixel-center aligned, replicated borders: the same sampling as cv2.INTER_CUBIC */
    const unsigned uw=w*iu,uh=h*iu;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<uw;x++) {
        float fx=(x+.5f)/iu-.5f;int ix=(int)floorf(fx);float v=0;
        for(int i=-1;i<=2;i++)v+=cubic(fx-ix-i)*in[(size_t)y*w+(unsigned)clampf(ix+i,0,w-1)];
        tmp[(size_t)y*uw+x]=v;
    }
    for(unsigned y=0;y<uh;y++){float fy=(y+.5f)/iu-.5f;int iy=(int)floorf(fy);float wt[4];int rows[4];
        for(int j=-1;j<=2;j++){wt[j+1]=cubic(fy-iy-j);rows[j+1]=(int)clampf(iy+j,0,h-1);}
        for(unsigned x=0;x<uw;x++)out[(size_t)y*uw+x]=wt[0]*tmp[(size_t)rows[0]*uw+x]+wt[1]*tmp[(size_t)rows[1]*uw+x]+wt[2]*tmp[(size_t)rows[2]*uw+x]+wt[3]*tmp[(size_t)rows[3]*uw+x];
    }
}
static void icon_blur(float *f,unsigned w,unsigned h,float sigma,float *tmp) {
    int r=(int)ceilf(sigma*3);float k[32],sum=0;if(r>15)r=15;
    for(int i=-r;i<=r;i++){k[i+r]=expf(-.5f*i*i/(sigma*sigma));sum+=k[i+r];}
    for(int i=0;i<=2*r;i++)k[i]/=sum;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){float v=0;for(int i=-r;i<=r;i++)v+=k[i+r]*f[(size_t)y*w+(unsigned)clampf((int)x+i,0,w-1)];tmp[(size_t)y*w+x]=v;}
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){float v=0;for(int i=-r;i<=r;i++)v+=k[i+r]*tmp[(size_t)(unsigned)clampf((int)y+i,0,h-1)*w+x];f[(size_t)y*w+x]=v;}
}
/* keeps only the 8-connected components of mask that have at least min pixels; with dist, a component also has to
   be 'fat': at least big pixels, or an inscribed radius of need */
static int icon_components_fat(uint8_t *mask,unsigned w,unsigned h,unsigned min,const float *dist,float need,unsigned big,
    unsigned *queue,const volatile int *cancel) {
    const size_t n=(size_t)w*h;
    for(size_t i=0;i<n;i++){
        if(mask[i]!=1)continue;
        if(cancel&&*cancel&&(i&0xffff)==0)return -1;
        size_t head=0,tail=0;queue[tail++]=(unsigned)i;mask[i]=2;float widest=0;
        while(head<tail){unsigned j=queue[head++],x=j%w,y=j/w;if(dist&&dist[j]>widest)widest=dist[j];
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int nx=(int)x+dx,ny=(int)y+dy;
                if(nx<0||ny<0||nx>=(int)w||ny>=(int)h)continue;size_t t=(size_t)ny*w+nx;
                if(mask[t]==1){mask[t]=2;queue[tail++]=(unsigned)t;}}}
        if(tail<min||(dist&&tail<big&&widest<need))for(size_t k=0;k<tail;k++)mask[queue[k]]=0;
    }
    for(size_t i=0;i<n;i++)mask[i]=mask[i]==2;
    return 0;
}
static int icon_components(uint8_t *mask,unsigned w,unsigned h,unsigned min,unsigned *queue,const volatile int *cancel) {
    return icon_components_fat(mask,w,h,min,NULL,0,0,queue,cancel);
}
static float icon_edge(float f,float level,float gx,float gy) {
    float fw=fmaxf(hypotf(gx,gy),1e-4f);return clampf((f-level)/fw+.5f,0,1);
}
/* ---- Centerline strokes for the gradient gametype icons. The line art is continuous lines of one constant width, so
   the line is found as curves and drawn as strokes: a thinned skeleton of the line mask, its graph of end points and
   junctions, short spurs and crumbs removed, small breaks bridged, corner-preserving smoothing, then every curve
   rasterized with round caps and joins at the one width the art measures to. Parts of the mask thicker than the
   stroke (the ring, the skull, the bolt) are filled shapes drawn from their level set instead. */
static const int cl_dx[8]={0,1,1,1,0,-1,-1,-1},cl_dy[8]={-1,-1,0,1,1,1,0,-1};   /* N NE E SE S SW W NW */
struct cl_path { float *p; unsigned n; int closed,ja,jb; };
static void cl_thin(uint8_t *m,unsigned w,unsigned h,unsigned *queue,const volatile int *cancel,int *stopped) {
    /* Zhang-Suen thinning; the border row and column are never set */
    for(int again=1;again;) {
        again=0;
        for(int step=0;step<2;step++) {
            size_t count=0;
            if(cancel&&*cancel){*stopped=1;return;}
            for(unsigned y=1;y+1<h;y++)for(unsigned x=1;x+1<w;x++) {
                size_t i=(size_t)y*w+x;if(!m[i])continue;
                unsigned P[8];for(int k=0;k<8;k++)P[k]=m[i+(size_t)cl_dy[k]*w+cl_dx[k]]!=0;
                unsigned B=0,A=0;for(int k=0;k<8;k++){B+=P[k];A+=(!P[k]&&P[(k+1)&7]);}
                if(B<2||B>6||A!=1)continue;
                /* P2=N P4=E P6=S P8=W */
                int c3=step?(!(P[0]&&P[2]&&P[6])&&!(P[0]&&P[4]&&P[6])):(!(P[0]&&P[2]&&P[4])&&!(P[2]&&P[4]&&P[6]));
                if(c3)queue[count++]=(unsigned)i;
            }
            for(size_t k=0;k<count;k++)m[queue[k]]=0;
            if(count)again=1;
        }
    }
}
/* chamfer distance to the nearest zero pixel (inside mask) or to the nearest one pixel (target=1) */
static void cl_dist(const uint8_t *m,int target,float *d,unsigned w,unsigned h) {
    const float inf=1e9f,a=1.f,b=1.41421356f;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        size_t i=(size_t)y*w+x;float v=(m[i]==target)?0.f:inf;
        if(v>0){ if(x)v=fminf(v,d[i-1]+a); if(y){v=fminf(v,d[i-w]+a); if(x)v=fminf(v,d[i-w-1]+b); if(x+1<w)v=fminf(v,d[i-w+1]+b);} }
        d[i]=v;
    }
    for(unsigned yy=h;yy-->0;)for(unsigned xx=w;xx-->0;) {
        size_t i=(size_t)yy*w+xx;float v=d[i];
        if(xx+1<w)v=fminf(v,d[i+1]+a);
        if(yy+1<h){v=fminf(v,d[i+w]+a); if(xx+1<w)v=fminf(v,d[i+w+1]+b); if(xx)v=fminf(v,d[i+w-1]+b);}
        d[i]=v;
    }
}
static void cl_neighbors(const uint8_t *sk,unsigned w,size_t i,unsigned *nb,unsigned *cn) {
    unsigned P[8],n=0,c=0;
    for(int k=0;k<8;k++){P[k]=sk[i+(size_t)cl_dy[k]*w+cl_dx[k]]!=0;n+=P[k];}
    for(int k=0;k<8;k++)c+=(!P[k]&&P[(k+1)&7]);
    *nb=n;*cn=c;
}
/* the skeleton's graph as paths between end points and junction pixels; node[] marks them */
static int cl_graph(const uint8_t *sk,uint8_t *node,uint8_t *visited,unsigned w,unsigned h,struct cl_path **paths,unsigned *count) {
    size_t n=(size_t)w*h;unsigned cap=64,np=0;
    struct cl_path *out=malloc(cap*sizeof(*out));if(!out)return 0;
    memset(node,0,n);memset(visited,0,n);
    for(unsigned y=1;y+1<h;y++)for(unsigned x=1;x+1<w;x++){size_t i=(size_t)y*w+x;if(!sk[i])continue;unsigned nb,cn;cl_neighbors(sk,w,i,&nb,&cn);
        if(nb==1||cn>=3)node[i]=1;}
    unsigned *buf=malloc(sizeof(unsigned)*65536);if(!buf){free(out);return 0;}
    for(unsigned y=1;y+1<h;y++)for(unsigned x=1;x+1<w;x++) {
        size_t s=(size_t)y*w+x;if(!node[s])continue;
        for(int k=0;k<8;k++) {
            size_t q=s+(size_t)cl_dy[k]*w+cl_dx[k];
            if(!sk[q]||node[q]||visited[q])continue;
            unsigned len=0;buf[len++]=(unsigned)s;buf[len++]=(unsigned)q;visited[q]=1;
            size_t cur=q,prev=s;
            for(;;) {
                size_t hit=0,pick=0;int hitf=0,pickd=99;
                for(int j=0;j<8;j++){size_t r=cur+(size_t)cl_dy[j]*w+cl_dx[j];
                    if(!sk[r]||r==prev)continue;
                    if(len>=3&&r==buf[len-3])continue;
                    if(node[r]){if(!hitf){hit=r;hitf=1;}continue;}
                    if(visited[r])continue;
                    int dd=abs(cl_dx[j])+abs(cl_dy[j]);if(dd<pickd){pickd=dd;pick=r;}}
                if(hitf){buf[len++]=(unsigned)hit;break;}
                if(!pickd||pickd==99)break;
                prev=cur;cur=pick;if(len>=65530)break;buf[len++]=(unsigned)cur;visited[cur]=1;
            }
            if(np==cap){cap*=2;struct cl_path *t=realloc(out,cap*sizeof(*out));if(!t){free(buf);goto fail;}out=t;}
            struct cl_path *pp=&out[np++];pp->n=len;pp->p=malloc((size_t)len*2*sizeof(float));pp->closed=0;
            if(!pp->p){np--;free(buf);goto fail;}
            for(unsigned t=0;t<len;t++){pp->p[2*t]=(float)(buf[t]%w);pp->p[2*t+1]=(float)(buf[t]/w);}
            unsigned nb,cn;cl_neighbors(sk,w,buf[0],&nb,&cn);pp->ja=cn>=3;cl_neighbors(sk,w,buf[len-1],&nb,&cn);pp->jb=cn>=3;
        }
    }
    /* loops with no node */
    for(unsigned y=1;y+1<h;y++)for(unsigned x=1;x+1<w;x++) {
        size_t s=(size_t)y*w+x;if(!sk[s]||node[s]||visited[s])continue;
        unsigned len=0;buf[len++]=(unsigned)s;visited[s]=1;size_t cur=s;
        for(;;){size_t pick=0;int pickd=99;
            for(int j=0;j<8;j++){size_t r=cur+(size_t)cl_dy[j]*w+cl_dx[j];if(!sk[r]||node[r]||visited[r])continue;
                int dd=abs(cl_dx[j])+abs(cl_dy[j]);if(dd<pickd){pickd=dd;pick=r;}}
            if(pickd==99||len>=65530)break;cur=pick;buf[len++]=(unsigned)cur;visited[cur]=1;}
        if(len>8){
            if(np==cap){cap*=2;struct cl_path *t=realloc(out,cap*sizeof(*out));if(!t){free(buf);goto fail;}out=t;}
            struct cl_path *pp=&out[np++];pp->n=len;pp->p=malloc((size_t)len*2*sizeof(float));pp->closed=1;pp->ja=pp->jb=0;
            if(!pp->p){np--;free(buf);goto fail;}
            for(unsigned t=0;t<len;t++){pp->p[2*t]=(float)(buf[t]%w);pp->p[2*t+1]=(float)(buf[t]/w);}
        }
    }
    free(buf);*paths=out;*count=np;return 1;
fail:
    for(unsigned i=0;i<np;i++)free(out[i].p);free(out);return 0;
}
static void cl_free_paths(struct cl_path *p,unsigned n){for(unsigned i=0;i<n;i++)free(p[i].p);free(p);}
static float cl_length(const float *p,unsigned n){float s=0;for(unsigned i=1;i<n;i++)s+=hypotf(p[2*i]-p[2*i-2],p[2*i+1]-p[2*i-1]);return s;}
/* uniform 1 pixel resampling along the arc */
static unsigned cl_resample(const float *p,unsigned n,int closed,float *out,unsigned cap) {
    float total=0;unsigned m=0;
    unsigned segs=closed?n:n-1;if(n<2)return 0;
    float *cum=malloc((size_t)(segs+1)*sizeof(float));if(!cum)return 0;cum[0]=0;
    for(unsigned i=0;i<segs;i++){const float *a=p+2*i,*b=p+2*((i+1)%n);total+=hypotf(b[0]-a[0],b[1]-a[1]);cum[i+1]=total;}
    unsigned seg=0;
    for(float t=0;t<=total&&m<cap;t+=1.f){
        while(seg+1<segs&&cum[seg+1]<t)seg++;
        const float *a=p+2*seg,*b=p+2*((seg+1)%n);float span=cum[seg+1]-cum[seg];float u=span>1e-6f?(t-cum[seg])/span:0;
        out[2*m]=a[0]+(b[0]-a[0])*u;out[2*m+1]=a[1]+(b[1]-a[1])*u;m++;
    }
    free(cum);return m;
}
/* Gaussian smoothing of an open piece with fixed end points (point reflection) or of a closed loop */
static void cl_gauss(const float *p,unsigned n,float sigma,int closed,float *out) {
    if(sigma>n/6.f)sigma=n/6.f;
    if(sigma<=.8f||n<4){memcpy(out,p,(size_t)n*2*sizeof(float));return;}
    int r=(int)ceilf(sigma*3);if(r>40)r=40;float k[81],sum=0;for(int i=-r;i<=r;i++){k[i+r]=expf(-.5f*i*i/(sigma*sigma));sum+=k[i+r];}
    for(int i=0;i<=2*r;i++)k[i]/=sum;
    for(unsigned i=0;i<n;i++){float sx=0,sy=0;
        for(int j=-r;j<=r;j++){int q=(int)i+j;float x,y;
            if(closed){q=((q%(int)n)+(int)n)%(int)n;x=p[2*q];y=p[2*q+1];}
            else{ if(q<0){int rq=-q;if(rq>=(int)n)rq=(int)n-1;x=2*p[0]-p[2*rq];y=2*p[1]-p[2*rq+1];}
                  else if(q>=(int)n){int rq=2*((int)n-1)-q;if(rq<0)rq=0;x=2*p[2*(n-1)]-p[2*rq];y=2*p[2*(n-1)+1]-p[2*rq+1];}
                  else{x=p[2*q];y=p[2*q+1];} }
            sx+=k[j+r]*x;sy+=k[j+r]*y;}
        out[2*i]=sx;out[2*i+1]=sy;}
}
/* smooths a curve where it bends gently and leaves its real corners: splits at turns above 50 degrees */
static unsigned cl_smooth(const float *p,unsigned n,int closed,float width,float *out,unsigned cap) {
    float *R=malloc((size_t)(n*2+8)*2*sizeof(float)),*T=malloc((size_t)(n*2+8)*2*sizeof(float));
    unsigned m=0;if(!R||!T)goto done;
    m=cl_resample(p,n,closed,R,n*2+8);
    if(m<2){m=0;goto done;}
    if(m<7){if(m>cap)m=cap;memcpy(out,R,(size_t)m*2*sizeof(float));goto done;}
    int k=(int)lroundf(width*.9f);if(k<2)k=2;
    unsigned corners[256],nc=0;
    for(unsigned i=0;i<m&&nc<255;i++){
        int ia=closed?(int)((i+m*4-(unsigned)k)%m):(int)i-k,ib=closed?(int)((i+(unsigned)k)%m):(int)i+k;
        if(!closed){if(ia<0)ia=0;if(ib>(int)m-1)ib=(int)m-1;}
        float ax=R[2*ia]-R[2*i],ay=R[2*ia+1]-R[2*i+1],bx=R[2*ib]-R[2*i],by=R[2*ib+1]-R[2*i+1];
        float na=hypotf(ax,ay),nb=hypotf(bx,by);if(na<1e-3f||nb<1e-3f)continue;
        float turn=180.f-acosf(clampf((ax*bx+ay*by)/(na*nb),-1,1))*57.29578f;
        if(turn<=50.f)continue;
        /* a local maximum within k */
        int best=1;for(int j=-k;j<=k&&best;j++){int q=(int)i+j;if(closed)q=((q%(int)m)+(int)m)%(int)m;else if(q<1||q>=(int)m-1)continue;
            int qa=closed?(int)(((unsigned)q+m*4-(unsigned)k)%m):q-k,qb=closed?(int)(((unsigned)q+(unsigned)k)%m):q+k;if(!closed){if(qa<0)qa=0;if(qb>(int)m-1)qb=(int)m-1;}
            float cx=R[2*qa]-R[2*q],cy=R[2*qa+1]-R[2*q+1],dx=R[2*qb]-R[2*q],dy=R[2*qb+1]-R[2*q+1];float nc2=hypotf(cx,cy),nd=hypotf(dx,dy);if(nc2<1e-3f||nd<1e-3f)continue;
            float t2=180.f-acosf(clampf((cx*dx+cy*dy)/(nc2*nd),-1,1))*57.29578f;if(t2>turn+1e-3f)best=0;}
        if(best&&(!closed||1)&&(nc==0||(int)(i-corners[nc-1])>k))corners[nc++]=i;
    }
    float sigma=width*.7f;
    if(!closed){
        unsigned cuts[258],ncuts=0;cuts[ncuts++]=0;for(unsigned c=0;c<nc;c++)if(corners[c]>0&&corners[c]<m-1)cuts[ncuts++]=corners[c];cuts[ncuts++]=m-1;
        unsigned o=0;
        for(unsigned c=0;c+1<ncuts;c++){unsigned len=cuts[c+1]-cuts[c]+1;if(len<2)continue;
            cl_gauss(R+2*cuts[c],len,sigma,0,T);if(o+len>cap)break;memcpy(out+2*o,T,(size_t)len*2*sizeof(float));o+=len;}
        m=o;
    } else if(!nc) {
        cl_gauss(R,m,sigma,1,T);if(m>cap)m=cap;memcpy(out,T,(size_t)m*2*sizeof(float));
    } else {
        unsigned c0=corners[0],o=0;
        float *Q=malloc((size_t)(m+1)*2*sizeof(float));if(!Q){m=0;goto done;}
        for(unsigned i=0;i<=m;i++){unsigned q=(c0+i)%m;Q[2*i]=R[2*q];Q[2*i+1]=R[2*q+1];}
        unsigned cuts[258],ncuts=0;cuts[ncuts++]=0;
        for(unsigned c=1;c<nc;c++)cuts[ncuts++]=(corners[c]+m-c0)%m;cuts[ncuts++]=m;
        for(unsigned a=1;a<ncuts;a++)for(unsigned b=a;b>0&&cuts[b]<cuts[b-1];b--){unsigned t=cuts[b];cuts[b]=cuts[b-1];cuts[b-1]=t;}
        for(unsigned c=0;c+1<ncuts;c++){unsigned len=cuts[c+1]-cuts[c]+1;if(len<2)continue;
            cl_gauss(Q+2*cuts[c],len,sigma,0,T);if(o+len>cap)break;memcpy(out+2*o,T,(size_t)len*2*sizeof(float));o+=len;}
        free(Q);m=o;
    }
done:
    free(R);free(T);return m;
}
/* anti-aliased coverage of the segment a-b thickened to `width` with round caps, max-combined into cov */
static void cl_stroke_segment(float *cov,unsigned w,unsigned h,float ax,float ay,float bx,float by,float half) {
    int x0=(int)floorf(fminf(ax,bx)-half-1.5f),x1=(int)ceilf(fmaxf(ax,bx)+half+1.5f),y0=(int)floorf(fminf(ay,by)-half-1.5f),y1=(int)ceilf(fmaxf(ay,by)+half+1.5f);
    if(x0<0)x0=0;if(y0<0)y0=0;if(x1>=(int)w)x1=(int)w-1;if(y1>=(int)h)y1=(int)h-1;
    float vx=bx-ax,vy=by-ay,l2=vx*vx+vy*vy;
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
        float px=x+.5f-ax,py=y+.5f-ay,t=l2>1e-8f?clampf((px*vx+py*vy)/l2,0,1):0;
        float dx=px-t*vx,dy=py-t*vy,d=sqrtf(dx*dx+dy*dy);
        float c=clampf(half+.5f-d,0,1);size_t i=(size_t)y*w+x;if(c>cov[i])cov[i]=c;}
}
/* Round parts: a thick component that is an ellipse or an elliptical ring within a small error (a circle when the
   axes agree) is drawn exactly; k is the ring's inner size as a fraction of the outer, 0 for a solid dot. */
struct cl_prim { float cx,cy,Rx,Ry,k; };
static unsigned cl_fit_circles(const uint8_t *blob,uint8_t *prim,uint8_t *mark,unsigned w,unsigned h,unsigned *queue,
    struct cl_prim *out,unsigned cap) {
    const size_t n=(size_t)w*h;unsigned count=0;memset(mark,0,n);
    for(size_t i=0;i<n;i++){
        if(!blob[i]||mark[i])continue;
        size_t head=0,tail=0;queue[tail++]=(unsigned)i;mark[i]=2;
        unsigned minx=w,maxx=0,miny=h,maxy=0;
        while(head<tail){unsigned j=queue[head++],x=j%w,y=j/w;
            if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int nx=(int)x+dx,ny=(int)y+dy;if(nx<0||ny<0||nx>=(int)w||ny>=(int)h)continue;
                size_t t=(size_t)ny*w+nx;if(blob[t]&&!mark[t]){mark[t]=2;queue[tail++]=(unsigned)t;}}}
        double A=(double)tail,F=M_PI*((maxx-minx+1)/2.0)*((maxy-miny+1)/2.0);
        float cx=(minx+maxx+1)*.5f,cy=(miny+maxy+1)*.5f,Rx=(maxx-minx+1)*.5f,Ry=(maxy-miny+1)*.5f;
        /* the hole of a ring: the filled ellipse's area minus the component's */
        double Hh=F-A;float k=Hh>.08*F?(float)sqrt(Hh/F):0.f;
        double bad=0;
        for(int y=(int)miny-2;y<=(int)maxy+2;y++)for(int x=(int)minx-2;x<=(int)maxx+2;x++){
            if(x<0||y<0||x>=(int)w||y>=(int)h)continue;
            float u=hypotf((x+.5f-cx)/Rx,(y+.5f-cy)/Ry);int ideal=u<=1.f&&(k<=0||u>=k);int is=mark[(size_t)y*w+x]==2;
            if(ideal!=is)bad++;}
        int fits=fminf(Rx,Ry)>=8.f&&bad<.10*A&&count<cap;
        for(size_t q=0;q<tail;q++){mark[queue[q]]=1;if(fits)prim[queue[q]]=1;}
        if(fits){out[count].cx=cx;out[count].cy=cy;out[count].Rx=Rx;out[count].Ry=Ry;out[count].k=k;count++;}
    }
    return count;
}
/* coverage of the strokes and filled shapes in AL, of the dark fill in AK (both at the 8x working size); 1 on success,
   0 when the line art cannot be read as strokes (the caller falls back), -1 on cancellation */
static int icon_centerline_alpha(const float *S,const float *K,const uint8_t *line,const uint8_t *black,unsigned uw,unsigned uh,
    float *AL,float *AK,unsigned *queue,const volatile int *cancel) {
    const size_t un=(size_t)uw*uh;int status=0,stopped=0;
    uint8_t *grown=calloc(un,1),*blob=calloc(un,1),*sk=calloc(un,1),*node=calloc(un,1),*vis=calloc(un,1),*aux=calloc(un,1),*prim=calloc(un,1),*primd=calloc(un,1);
    struct cl_prim prims[16];unsigned nprim=0;
    float *D1=malloc(un*sizeof(float)),*D2=malloc(un*sizeof(float));
    struct cl_path *paths=NULL;unsigned np=0;
    if(!grown||!blob||!sk||!node||!vis||!aux||!prim||!primd||!D1||!D2)goto done;
    /* faint stretches of a line that touch a strong one belong to it: hysteresis closes breaks where the art is thin */
    {size_t head=0,tail=0;for(size_t i=0;i<un;i++)if(line[i]&&S[i]>.34f){grown[i]=1;queue[tail++]=(unsigned)i;}
     while(head<tail){unsigned j=queue[head++],x=j%uw,y=j/uw;
        for(int k=0;k<8;k++){int nx=(int)x+cl_dx[k],ny=(int)y+cl_dy[k];if(nx<0||ny<0||nx>=(int)uw||ny>=(int)uh)continue;
            size_t t=(size_t)ny*uw+nx;if(!grown[t]&&S[t]>.34f){grown[t]=1;queue[tail++]=(unsigned)t;}}}}
    if(cancel&&*cancel){status=-1;goto done;}
    /* the stroke half-width is the commonest distance to the edge along the skeleton of the whole mask */
    float base_half;
    {memcpy(sk,line,un);cl_thin(sk,uw,uh,queue,cancel,&stopped);if(stopped){status=-1;goto done;}
     cl_dist(line,0,D1,uw,uh);
     unsigned hist[64]={0},total=0;for(size_t i=0;i<un;i++)if(sk[i]){int b=(int)(D1[i]*4);if(b<0)b=0;if(b>63)b=63;hist[b]++;total++;}
     if(!total)goto done;
     unsigned acc=0;int bm=0;for(int b=0;b<64;b++){acc+=hist[b];if(acc>=total*.3f){bm=b;break;}}
     base_half=(bm+.5f)*.25f;}
    /* parts thicker than the stroke are filled shapes, drawn from their level set */
    {int r=(int)ceilf(base_half*1.9f);
     for(size_t i=0;i<un;i++)aux[i]=D1[i]>r;
     cl_dist(aux,1,D2,uw,uh);
     for(size_t i=0;i<un;i++)blob[i]=D2[i]<=r;
     if(icon_components_fat(blob,uw,uh,700,D1,13.f,3000,queue,cancel)){status=-1;goto done;}
     nprim=cl_fit_circles(blob,prim,vis,uw,uh,queue,prims,16);memset(vis,0,un);
     cl_dist(prim,1,D2,uw,uh);int rb=(int)base_half+1;
     for(size_t i=0;i<un;i++)primd[i]=D2[i]<=rb+1;
     cl_dist(blob,1,D2,uw,uh);
     /* a point of the line is only valid where it borders the dark fill: the stroke follows the silhouette */
     for(size_t i=0;i<un;i++)node[i]=K[i]>.3f;
     cl_dist(node,1,D1,uw,uh);float dmax=base_half*3+6;
     for(size_t i=0;i<un;i++){blob[i]=D2[i]<=rb;aux[i]=grown[i]&&!blob[i]&&D1[i]<=dmax;}
     if(icon_components(aux,uw,uh,30,queue,cancel)){status=-1;goto done;}}
    memcpy(sk,aux,un);memset(aux,0,un);
    cl_thin(sk,uw,uh,queue,cancel,&stopped);if(stopped){status=-1;goto done;}
    /* spurs: short branches with one free end are skeleton noise, not art */
    for(int round=0;round<3;round++){
        if(cl_graph(sk,node,vis,uw,uh,&paths,&np)==0)goto done;
        unsigned killed=0;float spur=base_half*2*.9f;
        for(unsigned i=0;i<np;i++){struct cl_path *c=&paths[i];if(c->closed)continue;
            if((!c->ja)!=(!c->jb)&&c->n<spur){killed++;
                for(unsigned t=0;t<c->n;t++){if((t==0&&c->ja)||(t==c->n-1&&c->jb))continue;sk[(size_t)c->p[2*t+1]*uw+(size_t)c->p[2*t]]=0;}}}
        cl_free_paths(paths,np);paths=NULL;np=0;
        if(!killed)break;
        cl_thin(sk,uw,uh,queue,cancel,&stopped);if(stopped){status=-1;goto done;}
    }
    /* crumbs: skeleton pieces shorter than 60 pixels */
    memcpy(aux,sk,un);if(icon_components(aux,uw,uh,60,queue,cancel)){status=-1;goto done;}
    memcpy(sk,aux,un);memset(aux,0,un);
    if(cl_graph(sk,node,vis,uw,uh,&paths,&np)==0)goto done;
    if(!np)goto done;
    /* bridge small breaks: free ends within 3 source pixels that point at each other */
    {unsigned nend=0,*eid=malloc((size_t)np*2*sizeof(unsigned));float (*et)[2]=malloc((size_t)np*2*sizeof(*et));
     if(!eid||!et){free(eid);free(et);goto done;}
     unsigned *epath=malloc((size_t)np*2*sizeof(unsigned));char *eside=malloc((size_t)np*2);
     if(!epath||!eside){free(eid);free(et);free(epath);free(eside);goto done;}
     for(unsigned i=0;i<np;i++){struct cl_path *c=&paths[i];if(c->closed||c->n<2)continue;unsigned k=c->n-1<14?c->n-1:14;
        if(!c->ja){float dx=c->p[0]-c->p[2*k],dy=c->p[1]-c->p[2*k+1],l=hypotf(dx,dy)+1e-6f;epath[nend]=i;eside[nend]=0;et[nend][0]=dx/l;et[nend][1]=dy/l;eid[nend++]=i;}
        if(!c->jb){unsigned e=c->n-1;float dx=c->p[2*e]-c->p[2*(e-k)],dy=c->p[2*e+1]-c->p[2*(e-k)+1],l=hypotf(dx,dy)+1e-6f;epath[nend]=i;eside[nend]=1;et[nend][0]=dx/l;et[nend][1]=dy/l;eid[nend++]=i;}}
     char *used=calloc(nend?nend:1,1);
     unsigned bridges=0;struct cl_path *extra=NULL;unsigned cap=0;
     for(;;){float bd=1e9f;int ba=-1,bb=-1;
        for(unsigned a=0;a<nend;a++){if(used[a])continue;
            for(unsigned b=a+1;b<nend;b++){if(used[b])continue;
                if(epath[a]==epath[b]&&paths[epath[a]].n<80)continue;
                const struct cl_path *pa=&paths[epath[a]],*pb=&paths[epath[b]];
                const float *A=pa->p+(eside[a]?2*(pa->n-1):0),*B=pb->p+(eside[b]?2*(pb->n-1):0);
                float vx=B[0]-A[0],vy=B[1]-A[1],d=hypotf(vx,vy);if(d>24.f||d<1e-3f)continue;
                float ux=vx/d,uy=vy/d;
                if(et[a][0]*ux+et[a][1]*uy>.6f&&-(et[b][0]*ux+et[b][1]*uy)>.6f&&d<bd){bd=d;ba=(int)a;bb=(int)b;}}}
        if(ba<0)break;used[ba]=used[bb]=1;
        if(bridges==cap){cap=cap?cap*2:8;struct cl_path *t=realloc(extra,cap*sizeof(*extra));if(!t)break;extra=t;}
        const struct cl_path *pa=&paths[epath[ba]],*pb=&paths[epath[bb]];
        const float *A=pa->p+(eside[ba]?2*(pa->n-1):0),*B=pb->p+(eside[bb]?2*(pb->n-1):0);
        struct cl_path *nbp=&extra[bridges];nbp->p=malloc(4*sizeof(float));if(!nbp->p)break;nbp->n=2;nbp->closed=0;nbp->ja=nbp->jb=0;
        nbp->p[0]=A[0];nbp->p[1]=A[1];nbp->p[2]=B[0];nbp->p[3]=B[1];bridges++;}
     free(used);free(eid);free(et);free(epath);free(eside);
     if(bridges){struct cl_path *t=realloc(paths,(size_t)(np+bridges)*sizeof(*paths));
        if(t){paths=t;for(unsigned i=0;i<bridges;i++)paths[np+i]=extra[i];np+=bridges;}else for(unsigned i=0;i<bridges;i++)free(extra[i].p);}
     free(extra);}
    /* the one stroke width: the blue the line carries per unit of length, measured on the curves as found */
    float width;
    {float total=0;memset(aux,0,un);
     for(unsigned i=0;i<np;i++){const struct cl_path *c=&paths[i];total+=cl_length(c->p,c->n);
        for(unsigned t=0;t+1<c->n;t++){for(int s=0;s<=2;s++){float u=s*.5f;int x=(int)lroundf(c->p[2*t]+(c->p[2*t+2]-c->p[2*t])*u),y=(int)lroundf(c->p[2*t+1]+(c->p[2*t+3]-c->p[2*t+1])*u);
            if(x>=0&&y>=0&&x<(int)uw&&y<(int)uh)aux[(size_t)y*uw+x]=1;}}}
     if(total<20.f)goto done;
     cl_dist(aux,1,D2,uw,uh);double blue=0;
     for(size_t i=0;i<un;i++)if(D2[i]<=12.f&&!blob[i])blue+=clampf(S[i],0,1);
     width=(float)(blue/total);
     if(width<2.f||width>24.f)goto done;}
    /* strokes: every curve smoothed where it bends gently, then rasterized at that one width with round caps and joins */
    memset(AL,0,un*sizeof(float));
    for(unsigned i=0;i<np;i++){const struct cl_path *c=&paths[i];if(c->n<2)continue;
        unsigned cap=c->n*2+16;float *sm=malloc((size_t)cap*2*sizeof(float));if(!sm)goto done;
        unsigned m=cl_smooth(c->p,c->n,c->closed,width,sm,cap);
        if(c->closed&&m>=24){   /* a closed curve that is a circle within a small error is drawn as one */
            double mx=0,my=0;for(unsigned t=0;t<m;t++){mx+=sm[2*t];my+=sm[2*t+1];}mx/=m;my/=m;
            double rm=0;for(unsigned t=0;t<m;t++)rm+=hypot(sm[2*t]-mx,sm[2*t+1]-my);rm/=m;
            double dev=0;for(unsigned t=0;t<m;t++){double e=hypot(sm[2*t]-mx,sm[2*t+1]-my)-rm;dev+=e*e;}dev=sqrt(dev/m);
            unsigned want=(unsigned)lround(2*M_PI*rm);
            if(rm>=6&&dev<.03*rm&&want>=24&&want<cap){for(unsigned t=0;t<want;t++){double a=2*M_PI*t/want;sm[2*t]=(float)(mx+rm*cos(a));sm[2*t+1]=(float)(my+rm*sin(a));}m=want;}
        }
        for(unsigned t=0;t+1<m;t++)cl_stroke_segment(AL,uw,uh,sm[2*t]+.5f,sm[2*t+1]+.5f,sm[2*t+2]+.5f,sm[2*t+3]+.5f,width*.5f);
        if(c->closed&&m>2)cl_stroke_segment(AL,uw,uh,sm[2*m-2]+.5f,sm[2*m-1]+.5f,sm[0]+.5f,sm[1]+.5f,width*.5f);
        free(sm);}
    /* round parts: a stroked ring or a dot, exact */
    for(unsigned k=0;k<nprim;k++){const struct cl_prim *p=&prims[k];float Rm=sqrtf(p->Rx*p->Ry);
        int x0=(int)floorf(p->cx-p->Rx-2),x1=(int)ceilf(p->cx+p->Rx+2),y0=(int)floorf(p->cy-p->Ry-2),y1=(int)ceilf(p->cy+p->Ry+2);
        for(int y=y0<0?0:y0;y<=y1&&y<(int)uh;y++)for(int x=x0<0?0:x0;x<=x1&&x<(int)uw;x++){
            float u=hypotf((x+.5f-p->cx)/p->Rx,(y+.5f-p->cy)/p->Ry);
            float c=clampf((1.f-u)*Rm+.5f,0,1)*(p->k>0?clampf((u-p->k)*Rm+.5f,0,1):1.f);
            size_t q=(size_t)y*uw+x;AL[q]=AL[q]+c-AL[q]*c;}}
    /* filled shapes: the level set of the line field inside the thick parts */
    for(unsigned y=1;y+1<uh;y++)for(unsigned x=1;x+1<uw;x++){size_t u=(size_t)y*uw+x;if(!blob[u]||primd[u])continue;
        float gx=(S[u+1]-S[u-1])*.5f,gy=(S[u+uw]-S[u-uw])*.5f;float aB=icon_edge(S[u],.48f,gx,gy);AL[u]=AL[u]+aB-AL[u]*aB;}
    /* the dark fill: the figure's darkness, reaching under the strokes, and only where the strokes enclose it */
    for(unsigned y=1;y+1<uh;y++)for(unsigned x=1;x+1<uw;x++){size_t u=(size_t)y*uw+x;
        float gx=(K[u+1]-K[u-1])*.5f,gy=(K[u+uw]-K[u-uw])*.5f;AK[u]=black[u]?icon_edge(K[u],.3f,gx,gy):0;}
    for(size_t i=0;i<un;i++)aux[i]=AK[i]>.5f;
    cl_dist(aux,1,D2,uw,uh);
    for(size_t i=0;i<un;i++)if(D2[i]<=6.f&&AK[i]<1.f)AK[i]=1.f;
    {memset(blob,0,un);size_t head=0,tail=0;
     if(AL[0]<.5f){blob[0]=1;queue[tail++]=0;}
     while(head<tail){unsigned j=queue[head++],x=j%uw,y=j/uw;
        static const int ox[4]={1,-1,0,0},oy[4]={0,0,1,-1};
        for(int k=0;k<4;k++){int nx=(int)x+ox[k],ny=(int)y+oy[k];if(nx<0||ny<0||nx>=(int)uw||ny>=(int)uh)continue;
            size_t t=(size_t)ny*uw+nx;if(!blob[t]&&AL[t]<.5f){blob[t]=1;queue[tail++]=(unsigned)t;}}}
     for(size_t i=0;i<un;i++)if(blob[i])AK[i]=0;
     /* pockets of field no bigger than a speck that touch the dark fill are fill as well (a slit's tip, walled by strokes only, stays) */
     for(size_t i=0;i<un;i++){
         if(blob[i]||AL[i]>=.5f||AK[i]>=.5f||blob[i]==2)continue;
         head=0;tail=0;queue[tail++]=(unsigned)i;blob[i]=2;int small=1,dark=0;
         while(head<tail){unsigned j=queue[head++],x=j%uw,y=j/uw;
            static const int ox[4]={1,-1,0,0},oy[4]={0,0,1,-1};
            for(int k=0;k<4;k++){int nx=(int)x+ox[k],ny=(int)y+oy[k];if(nx<0||ny<0||nx>=(int)uw||ny>=(int)uh)continue;
                size_t t=(size_t)ny*uw+nx;if(AK[t]>=.5f&&blob[t]!=1)dark=1;if(blob[t]||AL[t]>=.5f||AK[t]>=.5f)continue;blob[t]=2;
                if(tail>=400){small=0;}else queue[tail++]=(unsigned)t;}}
         if(small&&dark&&tail<400)for(size_t k=0;k<tail;k++)AK[queue[k]]=1.f;
     }}
    status=1;
done:
    if(paths)cl_free_paths(paths,np);
    free(grown);free(blob);free(sk);free(node);free(vis);free(aux);free(prim);free(primd);free(D1);free(D2);
    return status;
}

static int menu_icon_levelset(const uint8_t *src,unsigned sw,unsigned sh,uint8_t *out,unsigned scale,
    const volatile int *cancel,struct menu_picture_trace_metrics *metrics) {
    unsigned W=0,H=0;
    for(unsigned y=0;y<sh;y++)for(unsigned x=0;x<sw;x++)if(src[((size_t)y*sw+x)*4+3]>=128){if(x+1>W)W=x+1;if(y+1>H)H=y+1;}
    if(W<16||H<16)return 0;
    const unsigned pw=W+2*ipad,ph=H+2*ipad,uw=pw*iu,uh=ph*iu;const size_t un=(size_t)uw*uh;
    float *rgb=malloc((size_t)pw*ph*3*sizeof(float)),*s1=malloc((size_t)pw*ph*sizeof(float)),*k1=malloc((size_t)pw*ph*sizeof(float));
    float *ref=malloc((size_t)ph*3*sizeof(float)),*S=malloc(un*sizeof(float)),*K=malloc(un*sizeof(float)),*tmp=malloc(un*sizeof(float));
    float *L=malloc(un*sizeof(float)),*AL=NULL,*AK=NULL;uint8_t *line=malloc(un),*black=malloc(un),*result=malloc((size_t)W*scale*H*scale*4);unsigned *queue=malloc(un*sizeof(unsigned));
    int status=0;
    if(!rgb||!s1||!k1||!ref||!S||!K||!tmp||!L||!line||!black||!result||!queue)goto done;
    if(metrics)metrics->scratch_bytes=un*(4*4+2+4)+(size_t)pw*ph*5*sizeof(float);
    for(unsigned py=0;py<ph;py++){unsigned sy=(unsigned)clampf((int)py-ipad,0,H-1);
        for(unsigned px=0;px<pw;px++){unsigned sx=(unsigned)clampf((int)px-ipad,0,W-1);
            for(int c=0;c<3;c++)rgb[((size_t)py*pw+px)*3+c]=src[((size_t)sy*sw+sx)*4+c];}}
    for(unsigned py=0;py<ph;py++)for(int c=0;c<3;c++)ref[py*3+c]=.5f*(rgb[((size_t)py*pw+ipad+1)*3+c]+rgb[((size_t)py*pw+ipad+2)*3+c]);
    for(unsigned py=0;py<ph;py++){
        float rl=.3f*ref[py*3]+.59f*ref[py*3+1]+.11f*ref[py*3+2];
        for(unsigned px=0;px<pw;px++){const float *p=rgb+((size_t)py*pw+px)*3;
            s1[(size_t)py*pw+px]=(p[2]-ref[py*3+2])/fmaxf(255-ref[py*3+2],1);
            k1[(size_t)py*pw+px]=1-(.3f*p[0]+.59f*p[1]+.11f*p[2])/fmaxf(rl,1);}}
    if(cancel&&*cancel){status=-1;goto done;}
    icon_resize8(s1,pw,ph,S,tmp);icon_resize8(k1,pw,ph,K,tmp);
    icon_blur(S,uw,uh,2.f,tmp);icon_blur(K,uw,uh,2.f,tmp);
    if(cancel&&*cancel){status=-1;goto done;}
    /* the blue of the line: mean color of the strongest line pixels */
    {unsigned hist[512]={0},total=0;for(size_t i=0;i<un;i++)if(S[i]>.4f){int b=(int)clampf(S[i]*256,0,511);hist[b]++;total++;}
     if(!total)goto done;
     unsigned acc=0;int b95=0;for(int b=0;b<512;b++){acc+=hist[b];if(acc>total*.95f){b95=b;break;}}
     float cut=.85f*(b95+.5f)/256;double sum[3]={0,0,0};size_t cnt=0;
     for(unsigned y=0;y<uh;y++)for(unsigned x=0;x<uw;x++)if(S[(size_t)y*uw+x]>cut){
         /* colors are read from the 1x padded image: nearest source pixel */
         const float *p=rgb+((size_t)(y/iu)*pw+x/iu)*3;sum[0]+=p[0];sum[1]+=p[1];sum[2]+=p[2];cnt++;}
     if(!cnt)goto done;
     float lc[3];for(int c=0;c<3;c++)lc[c]=(float)(sum[c]/cnt);
     /* line level 0.48 of the field, the width the art measures to (see the approaches study) */
     for(size_t i=0;i<un;i++){line[i]=S[i]>.48f;black[i]=K[i]>.3f;}
     if(icon_components(line,uw,uh,60,queue,cancel)||icon_components(black,uw,uh,1500,queue,cancel)){status=-1;goto done;}
     /* strokes first; the picture is smoothed level sets when its line art cannot be read as strokes */
     AL=malloc(un*sizeof(float));AK=malloc(un*sizeof(float));
     if(AL&&AK){int made=icon_centerline_alpha(S,K,line,black,uw,uh,AL,AK,queue,cancel);
         if(made<0){status=-1;goto done;}
         if(!made){free(AL);free(AK);AL=AK=NULL;}}
     else {free(AL);free(AK);AL=AK=NULL;}
     if(!AL){for(size_t i=0;i<un;i++)L[i]=line[i];icon_blur(L,uw,uh,2.5f,tmp);}
     /* composition: a coverage for the line and for the figure, edges anti-aliased from the level's own gradient */
     const unsigned f=iu/scale,ow=sw*scale;(void)ow;
     for(unsigned oy=0;oy<H*scale;oy++){
         if(cancel&&*cancel){status=-1;goto done;}
         for(unsigned ox=0;ox<W*scale;ox++){
             double acc3[3]={0,0,0};
             for(unsigned j=0;j<f;j++)for(unsigned i=0;i<f;i++){
                 unsigned x=ipad*iu+ox*f+i,y=ipad*iu+oy*f+j;size_t u=(size_t)y*uw+x;
                 float aL,aK;
                 if(AL){aL=AL[u];aK=AK[u];}
                 else {float gx=(L[u+1]-L[u-1])*.5f,gy=(L[u+uw]-L[u-uw])*.5f;
                     aL=icon_edge(L[u],.5f,gx,gy);
                     gx=(K[u+1]-K[u-1])*.5f;gy=(K[u+uw]-K[u-uw])*.5f;
                     aK=black[u]?icon_edge(K[u],.3f,gx,gy):0;}
                 /* the field's row color, interpolated between rows at 8x */
                 float fy=(y+.5f)/iu-.5f;int y0=(int)floorf(fy);float t=fy-y0;int ya=(int)clampf(y0,0,ph-1),yb=(int)clampf(y0+1,0,ph-1);
                 for(int c=0;c<3;c++){float bg=ref[ya*3+c]*(1-t)+ref[yb*3+c]*t;
                     float v=bg*(1-aK);v=v*(1-aL)+aL*lc[c];acc3[c]+=v;}
             }
             uint8_t *o=result+((size_t)oy*W*scale+ox)*4;
             for(int c=0;c<3;c++)o[c]=(uint8_t)lrintf(clampf((float)(acc3[c]/(f*f)),0,255));o[3]=255;
         }
     }
    }
    /* publish together: the whole region or nothing */
    for(unsigned oy=0;oy<H*scale;oy++)memcpy(out+((size_t)oy*sw*scale)*4,result+((size_t)oy*W*scale)*4,(size_t)W*scale*4);
    status=1;
done:
    free(result);free(rgb);free(s1);free(k1);free(ref);free(S);free(K);free(tmp);free(L);free(AL);free(AK);free(line);free(black);free(queue);
    return status;
}
int menu_picture_trace(const uint8_t *source,unsigned sw,unsigned sh,uint8_t *bpf,unsigned scale,
    const volatile int *cancel,struct menu_picture_trace_metrics *metrics) {
    double started=clock_seconds();if(metrics)memset(metrics,0,sizeof(*metrics));
    int picture=menu_picture_trace_source(source,sw,sh);
    if(picture<0||!bpf||(scale!=2&&scale!=4))return 0;
    if(cancel&&*cancel)return -1;
    if(picture>=11) {
        /* The picture is composed again whole: the model's result is not read, and bpf stays as it was unless this succeeds. */
        int made=menu_icon_levelset(source,sw,sh,bpf,scale,cancel,metrics);
        if(metrics){metrics->paths=made>0;metrics->seconds=clock_seconds()-started;}
        return made;
    }
    /* Detection always runs at 4x; rendering at 2x uses the same fitted curves. */
    const unsigned w=2048,h=sh*4,n=w*h,cap=65536;
    const int controller=picture>=6;
    uint8_t *mask=calloc(n,1),*outside=calloc(n,1),*visited=calloc(n,1),*seed=calloc(n,1),*keep=calloc(cap+1,1);
    uint16_t *coordinates=calloc(n,sizeof(*coordinates));
    unsigned *queue=malloc((size_t)n*sizeof(*queue));int *nearest=malloc((size_t)n*sizeof(*nearest));
    struct point *path=malloc((cap+1)*sizeof(*path)),*smoothed=malloc((cap+1)*sizeof(*smoothed)),*q=malloc((cap+1)*sizeof(*q));
    uint8_t *result=malloc((size_t)sw*sh*scale*scale*4);int status=0;
    if(!mask||!outside||!visited||!seed||!keep||!coordinates||!queue||!nearest||!path||!smoothed||!q||!result)goto done;
    if(metrics)metrics->scratch_bytes=(size_t)n*14+(cap+1)*(1+3*sizeof(struct point))+(size_t)sw*sh*scale*scale*4;
    for(unsigned y=0;y<h;y++) {
        if(cancel&&*cancel){status=-1;goto done;}
        for(unsigned x=0;x<w;x++){float v[4];sample(source,sh,(x+.5f)/4-.5f,(y+.5f)/4-.5f,v);
            mask[y*w+x]=v[3]>(picture==4?.34f:.50f);
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
    for(unsigned i=0;i<n;i++)mask[i]=!outside[i];
    unsigned paths=0,area=0,boundary=0;
    for(unsigned i=0;i<n;i++)if(mask[i]&&!visited[i]) {
        head=0;tail=0;queue[tail++]=i;visited[i]=1;
        while(head<tail){if((head%2048)==0&&cancel&&*cancel){status=-1;goto done;}unsigned j=queue[head++],x=j%w,y=j/w;unsigned ns[4]={x?j-1:j,x+1<w?j+1:j,y?j-w:j,y+1<h?j+w:j};
            for(int k=0;k<4;k++){unsigned t=ns[k];if(mask[t]&&!visited[t]){visited[t]=1;queue[tail++]=t;}}}
        /* Controller maps also contain lettering whose compressed-alpha haze can survive opening.
           Keep only components large enough to be a controller; the one-path guard still rejects
           a second substantial subject instead of silently choosing one. */
        if(tail<(controller?n/40:1600)){for(unsigned k=0;k<tail;k++)mask[queue[k]]=0;continue;}
        area+=tail;paths++;unsigned count=contour(mask,w,h,i,path,cap,cancel);if(count<12||count>10000){status=cancel&&*cancel?-1:0;goto done;}
        boundary+=count;
        float sigma=(picture==4||controller)?9.f:2.f;int radius=(int)ceilf(sigma*3);
        memcpy(smoothed,path,count*sizeof(*path));
        /* Snap to the blue ridge along the alpha edge's normal, then smooth displacements. */
        for(unsigned k=0;k<count;k++) {
            if(cancel&&*cancel){status=-1;goto done;}
            struct point a=smoothed[(k+count-3)%count],b=smoothed[(k+3)%count];float nx=-(b.y-a.y),ny=b.x-a.x,len=hypotf(nx,ny);
            nx/=fmaxf(len,1e-6f);ny/=fmaxf(len,1e-6f);float best=-1e9f,shift=0,limit=(picture==4||controller)?1.1f:.85f;
            for(int t=0;t<23;t++){float d=-limit+2*limit*t/22.f;float v=blue_signal(source,sh,smoothed[k].x/4-.5f+nx*d,smoothed[k].y/4-.5f+ny*d)-.025f*d*d/(limit*limit);
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
    if(paths!=((picture==0||picture==5)?3u:1u)||area<n/10||area>n/5||boundary<1000||boundary>10000)goto done;
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
        float v[4];sample(source,sh,(x+.5f)/4-.5f,(y+.5f)/4-.5f,v);
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
    float lc[3],gc[3];for(int c=0;c<3;c++){lc[c]=median(line[c]);gc[c]=median(glow_hist[c]);}
    if(controller) {
        /* Restore the upscaled leaders above the new rim, including their antialiased fringe.
           These masks are computed from the player's pixels and never stored as geometry. */
        for(unsigned i=0;i<sw*sh;i++)outside[i]=leader_pixel(source+i*4);
        if(!mask_filter(outside,visited,sw,sh,2,0,1,cancel)||
           !mask_filter(visited,outside,sw,sh,2,1,1,cancel)){status=-1;goto done;}
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
            if(dist>=64||(inside&&dist>=4))continue;
            float v[4];sample(source,sh,(x+.5f)/scale-.5f,(y+.5f)/scale-.5f,v);if(colored(v))continue;
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
    free(mask);free(outside);free(visited);free(seed);free(keep);free(queue);free(coordinates);free(nearest);free(path);free(smoothed);free(q);free(result);
    if(metrics)metrics->seconds=clock_seconds()-started;
    return status;
}
