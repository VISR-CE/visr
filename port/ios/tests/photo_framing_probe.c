/* Synthetic numbers only: no maps or engine state are needed. */
#include "photo_framing.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct photo_shot shot = {{1,2,3},{1,0,0},{0,0,1},1.12f,0,30,1,{0.5f,0.5f},{0.19f,0,0.81f,1},"slayer"};
    struct photo_shot loaded;
    unsigned char hash[32] = {0};
    char key[128];
    int revision;
    float up[3], forward[] = {0.6f,0,0.8f}, pole[] = {0,0,1};
    if (argc == 4 && !strcmp(argv[1], "--read"))
        return photo_fragment_read(argv[2], argv[3], &loaded, &revision) ? 0 : 1;
    assert(argc == 2);
    assert(photo_stock_scenario("levels\\a10\\a10"));
    assert(!photo_stock_scenario("example"));
    assert(photo_map_key("levels\\Example\\Example",0,NULL,key,sizeof(key)));
    assert(!strcmp(key,"stock:example"));
    assert(!photo_map_key("../bad.map",0,NULL,key,sizeof(key)));
    assert(!photo_map_key("custom_maps\\Example",1,NULL,key,sizeof(key)));
    hash[31] = 255;
    assert(photo_map_key("custom_maps\\Example",1,hash,key,sizeof(key)));
    assert(strlen(key)==72 && !strcmp(key+70,"ff"));
    assert(!photo_map_key("x",1,hash,key,72));
    assert(fabsf(photo_fov_step(1,1)-1.02f)<1e-6f);
    assert(fabsf(photo_fov_step(1,-1)-0.98f)<1e-6f);
    assert(photo_fov_step(2.6f,1)==2.6f && photo_fov_step(0.2f,-1)==0.2f);
    photo_level_up(forward,up);
    assert(fabsf(up[0]+0.8f)<1e-6f && fabsf(up[2]-0.6f)<1e-6f);
    assert(fabsf(up[0]*forward[0]+up[2]*forward[2])<1e-6f);
    photo_level_up(pole,up);
    assert(up[0]==1 && up[2]==0);
    assert(photo_fragment_write(argv[1],"stock:example",&shot));
    assert(photo_fragment_read(argv[1],"stock:example",&loaded,&revision) && revision==1);
    assert(loaded.position[1]==2 && loaded.erase_ai && !strcmp(loaded.variant,"slayer"));
    assert(photo_fragment_write(argv[1],"stock:example",&loaded));
    assert(photo_fragment_read(argv[1],"stock:example",&loaded,&revision) && revision==2);
    assert(!photo_fragment_read(argv[1],"stock:other",&loaded,&revision));
    shot.fov = NAN;
    assert(!photo_fragment_write(argv[1],"stock:example",&shot));
    assert(photo_fragment_read(argv[1],"stock:example",&loaded,&revision) && revision==2);
    {
        char camera_path[1024];
        FILE *camera;
        snprintf(camera_path, sizeof(camera_path), "%s.camera", argv[1]);
        camera = fopen(camera_path, "w");
        assert(camera);
        fputs("1 2 3\n1 0 0\n0 0 1\n1.12\n", camera);
        fclose(camera);
        assert(photo_camera_read(camera_path, &loaded));
        assert(fabsf(loaded.fov-1.12f)<1e-6f);
        camera = fopen(camera_path, "a");
        fputs("garbage", camera); fclose(camera);
        assert(!photo_camera_read(camera_path, &loaded));
    }
    puts("photo framing probe passed");
    return 0;
}
