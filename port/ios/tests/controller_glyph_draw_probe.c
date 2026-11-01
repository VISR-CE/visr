#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "halo_controller_glyphs.h"
#define HALO_IOS 1
#define TRUE 1
#define NONE (-1)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PIN(v,lo,hi) MAX(lo,MIN(v,hi))
typedef float real;
typedef unsigned long pixel32;
typedef struct {short y0,x0,y1,x1;} rectangle2d;
#include "interface/controller_glyphs.h"
/* RENDERER BLENDS */
/* STOCK BLEND */
struct bitmap_data {short width,height;void *base_address;};
struct point {float x,y;};
struct dynamic_screen_vertex {struct point position,texture_coordinates;pixel32 color;};
struct vector {float i,j;};
struct rasterizer_dynamic_screen_geometry_parameters {struct bitmap_data *map[3];struct vector map_scale[3],map_texture_scale[3];short framebuffer_blend_function;int alpha_weighted;};
/* BLEND TABLES */
/* FINAL COMBINER */
static double combiner_input(unsigned int input,double rgb,double alpha) {
 if(input==0)return 0;
 if(input==0x0c)return rgb;
 if(input==0x1c)return alpha;
 assert(!"unsupported final-combiner input");return 0;
}
static double blend_factor(unsigned long mode,double alpha) {
 if(mode==D3DBLEND_ONE)return 1;
 if(mode==D3DBLEND_SRCALPHA)return alpha;
 if(mode==D3DBLEND_INVSRCALPHA)return 1-alpha;
 if(mode==D3DBLEND_ZERO)return 0;
 assert(!"unsupported blend factor");return 0;
}
static double composite_sample(struct rasterizer_dynamic_screen_geometry_parameters *p,
 pixel32 vertex,double coverage,double destination) {
 /* White straight-alpha texels, modulated by the submitted vertex color. */
 double rgb=(vertex&255)/255.0,alpha=coverage*((vertex>>24)&255)/255.0;
 unsigned long word=glyph_final_combiner(p);
 double a=combiner_input((word>>24)&255,rgb,alpha),b=combiner_input((word>>16)&255,rgb,alpha);
 double c=combiner_input((word>>8)&255,rgb,alpha),d=combiner_input(word&255,rgb,alpha);
 double final_rgb=a*b+(1-a)*c+d;
 double final_alpha=combiner_input((GLYPH_FINAL_ALPHA>>8)&255,rgb,alpha);
 return final_rgb*blend_factor(srcblend_table[p->framebuffer_blend_function],final_alpha)+
  destination*blend_factor(destblend_table[p->framebuffer_blend_function],final_alpha);
}
static void check_coverage_compositing(struct rasterizer_dynamic_screen_geometry_parameters *p) {
 /* tint .8 over background .2; vertex fade is128/255, not opaque. */
 assert(fabs(composite_sample(p,0x806633cc,0,.2)-.2)<.000001);
 assert(fabs(composite_sample(p,0x806633cc,.5,.2)-.35058823529411765)<.000001);
 assert(fabs(composite_sample(p,0x806633cc,1,.2)-.5011764705882353)<.000001);
 assert(fabs(composite_sample(p,0x006633cc,1,.2)-.2)<.000001);
 assert(fabs(composite_sample(p,0xff6633cc,.5,.2)-.5)<.000001);
}
static struct dynamic_screen_vertex last_vertices[4];
static struct rasterizer_dynamic_screen_geometry_parameters last_parameters;
static int draws,rasters,allocations,deletions,kind=2,raster_width,raster_height,raster_ok=1;
static const char *request_label="a.circle";
static int allocation_fail,hardware_fail;
int halo_controller_glyph_request(short player,short semantic,short physical,char *label,unsigned int size) {
 (void)player;(void)semantic;(void)physical;snprintf(label,size,"%s",request_label);return kind;
}
static float halo_screen_pixel_scale(void) {return 2;}
static int host_controller_glyph_raster(int k,const char *label,int width,int height,unsigned char *pixels) {
 (void)k;(void)label;rasters++;raster_width=width;raster_height=height;memset(pixels,255,width*height*4);return raster_ok;
}
static struct bitmap_data *bitmap_2d_new(short w,short h,short m,short f) {
 (void)m;(void)f;struct bitmap_data *b=calloc(1,sizeof(*b));b->width=w;b->height=h;b->base_address=allocation_fail?NULL:calloc(w*h,4);allocations++;return b;
}
static void bitmap_delete(struct bitmap_data *b) {deletions++;free(b->base_address);free(b);}
static int rasterizer_bitmap_new(struct bitmap_data *b) {(void)b;return !hardware_fail;}
static void rasterizer_bitmap_changed(struct bitmap_data *b) {(void)b;}
static void rasterizer_psuedo_dynamic_screen_quad_draw(struct rasterizer_dynamic_screen_geometry_parameters *p,struct dynamic_screen_vertex *v) {
 assert(p->framebuffer_blend_function>=0 && p->framebuffer_blend_function<NUMBER_OF_SHADER_FRAMEBUFFER_BLEND_FUNCTIONS);
 last_parameters=*p;memcpy(last_vertices,v,sizeof(last_vertices));draws++;
}
/* DRAW */
int main(void) {
 rectangle2d bounds={10,20,36,41},clip={10,30,36,41};
 assert(controller_glyph_ready(0,18,2,&bounds));assert(draws==0);
 assert(controller_glyph_draw(0,18,2,&bounds,&clip,0xff2996ff,_controller_glyph_blend_hud));
 assert(raster_width==42 && raster_height==52); /* content keeps 21:26 aspect */
 assert(last_parameters.map[0]->width==64 && last_parameters.map[0]->height==64);
 assert(last_parameters.framebuffer_blend_function==stock_hud_blend());
 check_coverage_compositing(&last_parameters);
 assert(last_vertices[0].position.x==30 && last_vertices[1].position.x==41);
 assert(fabs(last_vertices[0].texture_coordinates.x-20.0f/64)<.0001f);
 assert(fabs(last_vertices[1].texture_coordinates.x-42.0f/64)<.0001f);
 assert(last_vertices[0].color==0xff2996ff); /* text blue survives original face colors */
 assert(controller_glyph_draw(1,18,2,&bounds,NULL,0x7f2996ff,0));
 assert(rasters==1 && allocations==1 && last_parameters.framebuffer_blend_function==0);
 assert(last_vertices[0].color==0x7f2996ff);
 check_coverage_compositing(&last_parameters);
 kind=1;assert(controller_glyph_hidden(0,2));assert(controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));assert(draws==2);
 kind=0;assert(!controller_glyph_hidden(0,2));assert(!controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));assert(draws==2);
 kind=2;request_label="missing";raster_ok=0;
 assert(!controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));assert(draws==2);
 raster_ok=1;request_label="allocation";allocation_fail=1;
 assert(!controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));assert(draws==2);
 allocation_fail=0;hardware_fail=1;request_label="hardware";
 assert(!controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));assert(draws==2);
 hardware_fail=0;request_label="b.circle";
 assert(controller_glyph_draw(1,18,2,&bounds,NULL,0xffffffff,0));assert(rasters==5);
 for(int i=0;i<90;i++) {
  char label[32];snprintf(label,sizeof(label),"symbol.%d",i);request_label=label;
  assert(controller_glyph_draw(0,18,2,&bounds,NULL,0xffffffff,0));
  assert(allocations-deletions<=64);
 }
 controller_glyphs_dispose();assert(deletions==allocations);
 return 0;
}
