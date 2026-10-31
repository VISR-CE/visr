/* Apache-2.0. No game fixtures ship; optional private fixtures verify the approved redraws. */
#import <Foundation/Foundation.h>
#include "menu_picture_redraw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
int main(int argc,char **argv) { @autoreleasepool {
    unsigned h=argc>1?(unsigned)atoi(argv[1]):512;
    size_t size=(size_t)512*h*4,top_size=size*16;
    uint8_t *source=calloc(size,1),*top=malloc(top_size),*saved=malloc(top_size),*second=malloc(top_size);
    assert(source&&top&&saved&&second);memset(top,73,top_size);memcpy(saved,top,top_size);
    volatile int cancel=0;struct menu_picture_redraw_metrics metrics;
    assert(menu_picture_redraw_source(NULL,512,h)==-1);
    assert(menu_picture_redraw(source,512,h,top,4,&cancel,NULL,&metrics)==0);
    assert(!memcmp(top,saved,top_size));
    if(argc==6) {
        FILE *f=fopen(argv[2],"rb");assert(f&&fread(source,1,size,f)==size);fclose(f);
        f=fopen(argv[3],"rb");assert(f&&fread(top,1,top_size,f)==top_size);fclose(f);
        memcpy(saved,top,top_size);memcpy(second,top,top_size);
        assert(menu_picture_redraw_source(source,512,h)>=0);
        assert(menu_picture_redraw(source,512,h,top,3,NULL,argv[4],NULL)==0);
        assert(!memcmp(top,saved,top_size));
        cancel=1;assert(menu_picture_redraw(source,512,h,top,4,&cancel,argv[4],NULL)==-1);
        assert(!memcmp(top,saved,top_size));cancel=0;
        assert(menu_picture_redraw(source,512,h,top,4,NULL,"/missing-resources",NULL)==0);
        assert(!memcmp(top,saved,top_size));
        assert(menu_picture_redraw(source,512,h,top,4,&cancel,argv[4],&metrics)==1);
        assert(menu_picture_redraw(source,512,h,second,4,NULL,argv[4],NULL)==1);
        assert(!memcmp(top,second,top_size)&&memcmp(top,saved,top_size));
        source[0]^=1;memcpy(second,saved,top_size);
        assert(menu_picture_redraw(source,512,h,second,4,NULL,argv[4],NULL)==0);
        assert(!memcmp(second,saved,top_size));source[0]^=1;
        f=fopen(argv[5],"wb");assert(f&&fwrite(top,1,top_size,f)==top_size);fclose(f);
        /* Both published mip scales exercise the same source guards and font path. */
        struct menu_picture_redraw_metrics half;
        memcpy(second,saved,size*4);cancel=1;
        assert(menu_picture_redraw(source,512,h,second,2,&cancel,argv[4],NULL)==-1);
        assert(!memcmp(second,saved,size*4));cancel=0;
        assert(menu_picture_redraw(source,512,h,second,2,NULL,"/missing-resources",NULL)==0);
        assert(!memcmp(second,saved,size*4));
        assert(menu_picture_redraw(source,512,h,second,2,NULL,argv[4],&half)==1);
        memcpy(top,saved,size*4);
        assert(menu_picture_redraw(source,512,h,top,2,NULL,argv[4],NULL)==1);
        assert(!memcmp(second,top,size*4));
        struct rusage usage;getrusage(RUSAGE_SELF,&usage);
        printf("seconds=%.6f scratch=%zu seconds_2x=%.6f scratch_2x=%zu peak_rss=%ld\n",
            metrics.seconds,metrics.scratch_bytes,half.seconds,half.scratch_bytes,usage.ru_maxrss);
    }
    free(source);free(top);free(saved);free(second);puts("menu picture redraw probe: passed");return 0;
} }
