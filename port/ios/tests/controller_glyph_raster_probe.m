#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
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
  assert(!host_controller_glyph_raster(2,"visr.missing.symbol",64,64,pixels));
  assert(!host_controller_glyph_raster(2,"a.circle",0,64,pixels));
  assert(!host_controller_glyph_raster(2,"a.circle",1024,64,pixels));
  assert(!host_controller_glyph_raster(3,"",64,64,pixels));
 }
 return 0;
}
