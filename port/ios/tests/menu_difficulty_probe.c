/* Apache-2.0. Private fixtures are optional; game pixels never ship with the probe. */
#include "menu_difficulty.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    uint8_t *small=calloc(64*64,4),*large=calloc(256*256,4);
    uint8_t *output=malloc(256*256*4),*saved=malloc(256*256*4);
    assert(small&&large&&output&&saved);
    memset(output,73,256*256*4);memcpy(saved,output,256*256*4);
    assert(menu_difficulty_source(NULL,64,64)==-1);
    assert(menu_difficulty_large_source(NULL,256,256)==-1);
    assert(menu_difficulty_source(small,64,64)==-1);
    assert(menu_difficulty_large_source(large,256,256)==-1);
    assert(menu_difficulty_source(large,256,256)==-1);
    assert(menu_difficulty_large_source(small,64,64)==-1);
    assert(!menu_difficulty_replace(small,large,output,4,NULL));
    assert(!memcmp(saved,output,256*256*4));
    if(argc==3) {
        FILE *file=fopen(argv[1],"rb");assert(file&&fread(small,4,64*64,file)==64*64);fclose(file);
        file=fopen(argv[2],"rb");assert(file&&fread(large,4,256*256,file)==256*256);fclose(file);
        int index=menu_difficulty_source(small,64,64);
        assert(index>=0&&index==menu_difficulty_large_source(large,256,256));
        for(unsigned scale=2;scale<=4;scale+=2) {
            size_t bytes=(size_t)64*64*scale*scale*4;
            volatile int cancel=1;
            assert(menu_difficulty_replace(small,large,output,scale,&cancel)==-1);
            assert(!memcmp(saved,output,256*256*4));
            assert(!menu_difficulty_replace(small,NULL,output,scale,NULL));
            small[0]^=1;assert(!menu_difficulty_replace(small,large,output,scale,NULL));small[0]^=1;
            large[0]^=1;assert(!menu_difficulty_replace(small,large,output,scale,NULL));large[0]^=1;
            assert(!menu_difficulty_replace(small,large,output,3,NULL));
            assert(!memcmp(saved,output,256*256*4));
            assert(menu_difficulty_replace(small,large,output,scale,NULL)==1);
            assert(memcmp(saved,output,bytes));
            uint8_t *second=malloc(bytes);assert(second);
            assert(menu_difficulty_replace(small,large,second,scale,NULL)==1);
            assert(!memcmp(output,second,bytes));free(second);
            for(size_t i=0;i<bytes;i+=4)if(!output[i+3])assert(!(output[i]|output[i+1]|output[i+2]));
            memcpy(output,saved,256*256*4);
        }
    }
    free(small);free(large);free(output);free(saved);puts("menu difficulty probe: passed");return 0;
}
