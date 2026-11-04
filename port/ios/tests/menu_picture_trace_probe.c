/* Apache-2.0. Asset-free fallback checks, plus optional private source fixtures. */
#include "menu_picture_trace.h"
#include "menu_picture_shapes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <pthread.h>
#include <time.h>
static void *stop_work(void *p) { struct timespec delay={0,10000000};nanosleep(&delay,NULL);*(volatile int *)p=1;return NULL; }

int main(int argc,char **argv)
{
    unsigned height=argc==5?(unsigned)atoi(argv[4]):argc==4?256:512;
    size_t source_size=(size_t)512*height*4,top_size=source_size*16;
    uint8_t *source=calloc(source_size,1),*top=malloc(top_size),*saved=malloc(top_size),*second=malloc(top_size);
    assert(source&&top&&saved&&second);
    memset(top,73,top_size);memcpy(saved,top,top_size);
    struct menu_picture_trace_metrics metrics;
    volatile int cancel=0;
    assert(menu_picture_trace_source(NULL,512,256)==-1);
    assert(!menu_picture_shapes(2,source,top,4,&cancel));
    assert(!memcmp(top,saved,top_size));
    assert(menu_picture_trace_source(source,1,1)==-1);
    {   /* the gametype icons' size: unknown pixels of that size are rejected without touching the output */
        static const unsigned sizes[1][2]={{256,128}};
        for(int k=0;k<1;k++) {
            size_t bytes=(size_t)sizes[k][0]*sizes[k][1]*4;
            uint8_t *blank=calloc(bytes,1),*mine=malloc(bytes*16),*mine_saved=malloc(bytes*16);
            assert(blank&&mine&&mine_saved);memset(mine,91,bytes*16);memcpy(mine_saved,mine,bytes*16);
            assert(menu_picture_trace_source(blank,sizes[k][0],sizes[k][1])==-1);
            assert(menu_picture_trace(blank,sizes[k][0],sizes[k][1],mine,4,&cancel,&metrics)==0);
            assert(!memcmp(mine,mine_saved,bytes*16));
            free(blank);free(mine);free(mine_saved);
        }
    }
    assert(menu_picture_trace(source,512,height,top,4,&cancel,&metrics)==0);
    assert(!memcmp(top,saved,top_size));
    assert(menu_picture_trace(source,512,height,top,4,&cancel,&metrics)==0);
    assert(!memcmp(top,saved,top_size));
    if(argc==4||argc==5) {
        FILE *file=fopen(argv[1],"rb");assert(file&&fread(source,1,source_size,file)==source_size);fclose(file);
        file=fopen(argv[2],"rb");assert(file&&fread(top,1,top_size,file)==top_size);fclose(file);
        memcpy(saved,top,top_size);memcpy(second,top,top_size);
        assert(menu_picture_trace_source(source,512,height)>=0);
        cancel=1;assert(menu_picture_trace(source,512,height,top,4,&cancel,&metrics)==-1);
        assert(!memcmp(top,saved,top_size));cancel=0;
        pthread_t thread;assert(!pthread_create(&thread,NULL,stop_work,(void *)&cancel));
        assert(menu_picture_trace(source,512,height,top,4,&cancel,&metrics)==-1);
        assert(!pthread_join(thread,NULL));assert(!memcmp(top,saved,top_size));cancel=0;
        assert(menu_picture_trace(source,512,height,top,4,&cancel,&metrics)==1);
        assert(menu_picture_trace(source,512,height,second,4,&cancel,NULL)==1);
        assert(!memcmp(top,second,top_size));assert(memcmp(top,saved,top_size));
        source[0]^=1;memcpy(second,saved,top_size);
        assert(menu_picture_trace(source,512,height,second,4,&cancel,NULL)==0);
        assert(!memcmp(second,saved,top_size));
        file=fopen(argv[3],"wb");assert(file&&fwrite(top,1,top_size,file)==top_size);fclose(file);
        struct rusage usage;getrusage(RUSAGE_SELF,&usage);
        printf("paths=%u area=%u boundary=%u seconds=%.6f scratch=%zu peak_rss=%ld\n",metrics.paths,metrics.area,metrics.boundary,metrics.seconds,metrics.scratch_bytes,usage.ru_maxrss);
    }
    free(source);free(top);free(saved);free(second);
    puts("menu picture trace probe: passed");return 0;
}
