#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "menu_geometry.h"
int host_controller_glyph_raster(int kind, const char *label, int width, int height, unsigned char *pixels);
int main(void) {
 @autoreleasepool {
  unsigned char pixels[64*64*4];
  assert(host_controller_glyph_raster(2,"a.circle",64,64,pixels));
  unsigned long alpha=0;for(int i=0;i<64*64;i++){assert(pixels[i*4]==255);alpha+=pixels[i*4+3];}
  assert(alpha>1000 && alpha<64*64*255);
  assert(host_controller_glyph_raster(3,"Mouse Left",64,32,pixels));
  alpha=0;for(int i=0;i<64*32;i++)alpha+=pixels[i*4+3];assert(alpha>1000);
  unsigned char escape[64*32*4],short_escape[64*32*4];
  assert(host_controller_glyph_raster(3,"Escape",64,32,escape));
  assert(host_controller_glyph_raster(3,"Esc",64,32,short_escape));
  assert(!memcmp(escape,short_escape,sizeof(escape)) && memcmp(escape,pixels,sizeof(escape)));
  unsigned char key[64*64*4],symbol[64*64*4];
  assert(host_controller_glyph_raster(3,"A",64,64,key));
  assert(host_controller_glyph_raster(2,"a.square",64,64,symbol));
  assert(!memcmp(key,symbol,sizeof(key)));
  assert(host_controller_glyph_raster(3,"1",64,64,key));
  assert(host_controller_glyph_raster(2,"1.square",64,64,symbol));
  assert(!memcmp(key,symbol,sizeof(key)));
  assert(host_controller_glyph_raster(2,"square",64,32,symbol));
  assert(memcmp(escape,symbol,sizeof(escape))); /* Escape includes its symbol inside the square. */
  assert(!host_controller_glyph_raster(2,"visr.missing.symbol",64,64,pixels));
  assert(!host_controller_glyph_raster(2,"a.circle",0,64,pixels));
  assert(!host_controller_glyph_raster(2,"a.circle",1024,64,pixels));
  assert(!host_controller_glyph_raster(3,"",64,64,pixels));
 }
 /* Bug class: legend glyph clipped by its box. Every glyph the legends draw keeps a clear margin to the box edge
    at the legend's box size (MENU_BAR_GLYPH_WIDTH x MENU_BAR_GLYPH_HEIGHT design units) at each screen scale, so its outline is never cut flat. */
 {
  const char *labels[] = {"Mouse Left","Mouse Right","Escape","A","Return","Delete","Space","Tab"};
  for (int scale = 1; scale <= 4; scale++) {
   int w = MENU_BAR_GLYPH_WIDTH*scale, h = MENU_BAR_GLYPH_HEIGHT*scale;
   unsigned char *box = malloc((size_t)w*h*4);
   for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); i++) {
    assert(host_controller_glyph_raster(3,labels[i],w,h,box)); /* every label the legend draws must rasterize */
    int x0=w,y0=h,x1=-1,y1=-1;
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) if (box[(y*w+x)*4+3]>8) {
     if (x<x0)x0=x; if (x>x1)x1=x; if (y<y0)y0=y; if (y>y1)y1=y; }
    assert(x1>=0);
    assert(x0>=1 && y0>=1 && x1<=w-2 && y1<=h-2);
    /* centered vertically in the box (within 1 unit), so the raise puts it on the caption's capitals */
    assert(abs((y0+y1+1)-h) <= 2*scale);
   }
   free(box);
  }
 }
 return 0;
}
