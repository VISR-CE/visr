/* Synthetic numbers only: no maps or engine state are needed. */
#include "photo_framing.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct photo_shot shot = {.position={1,2,3}, .forward={1,0,0}, .up={0,0,1}, .fov=1.12f,
        .settle_ticks=30, .erase_ai=1, .focus={0.5f,0.5f}, .safe={0.19f,0,0.81f,1}, .variant="slayer"};
    struct photo_shot loaded;
    unsigned char hash[32] = {0};
    char key[128];
    int revision;
    float up[3], forward[] = {0.6f,0,0.8f}, pole[] = {0,0,1};
    if (argc == 4 && !strcmp(argv[1], "--read"))
        return photo_fragment_read(argv[2], argv[3], &loaded, &revision) ? 0 : 1;
    assert(argc == 2);
    {
        float d[3];
        photo_fly_step(PHOTO_FLY_FORWARD | PHOTO_FLY_LEFT | PHOTO_FLY_UP, 0.5f, 0, d);
        assert(fabsf(d[0] - 0.75f) < 1e-6f && fabsf(d[1] - 0.75f) < 1e-6f && fabsf(d[2] - 0.75f) < 1e-6f);
        photo_fly_step(PHOTO_FLY_BACKWARD | PHOTO_FLY_RIGHT | PHOTO_FLY_DOWN, 1.f, 2, d);
        assert(fabsf(d[0] + 6.f) < 1e-6f && fabsf(d[1] + 6.f) < 1e-6f && fabsf(d[2] + 6.f) < 1e-6f);
        photo_fly_step(PHOTO_FLY_FORWARD | PHOTO_FLY_BACKWARD, 1.f, 0, d);
        assert(d[0] == 0.f && d[1] == 0.f && d[2] == 0.f);
        photo_fly_step(PHOTO_FLY_FORWARD, 0.f, 4, d);
        assert(d[0] == 0.f);
        photo_fly_step(1UL << 6, 1.f, 0, d);
        assert(d[0] == 0.f && d[1] == 0.f && d[2] == 0.f);
    }
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
    assert(fabsf(photo_fov_step(PHOTO_ENTRY_FOV,0)-PHOTO_ENTRY_FOV)<1e-6f);
    assert(fabsf(photo_fov_step(PHOTO_ENTRY_FOV,1)*180.f/3.14159265f-91.f)<1e-4f);
    assert(fabsf(photo_fov_step(PHOTO_ENTRY_FOV,-1)*180.f/3.14159265f-89.f)<1e-4f);
    {   /* From a saved 84.9 degrees, stepping up reaches 90 exactly. */
        float fov = 1.4817f; int i;
        for (i=0;i<5;i++) fov = photo_fov_step(fov,1);
        assert(fabsf(fov-PHOTO_ENTRY_FOV)<1e-6f);
    }
    assert(fabsf(photo_fov_step(2.6f,1)*180.f/3.14159265f-148.f)<1e-4f);
    assert(fabsf(photo_fov_step(0.2f,-1)*180.f/3.14159265f-12.f)<1e-4f);
    photo_level_up(forward,up);
    assert(fabsf(up[0]+0.8f)<1e-6f && fabsf(up[2]-0.6f)<1e-6f);
    assert(fabsf(up[0]*forward[0]+up[2]*forward[2])<1e-6f);
    photo_level_up(pole,up);
    assert(up[0]==1 && up[2]==0);
    {
        static const float rolls[] = {0.f, 0.4f, -1.2f, 2.9f, -3.1f};
        float level[3], east[] = {1,0,0}, slanted[] = {0.48f,0.6f,0.64f};
        const float *forwards[] = {east, forward, slanted};
        for (int f = 0; f < 3; f++)
            for (int r = 0; r < 5; r++)
            {
                photo_roll_up(forwards[f],rolls[r],up);
                assert(fabsf(up[0]*forwards[f][0]+up[1]*forwards[f][1]+up[2]*forwards[f][2])<1e-5f);
                assert(fabsf(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]-1)<1e-5f);
                assert(fabsf(photo_roll_from_up(forwards[f],up)-rolls[r])<1e-5f);
            }
        photo_roll_up(forward,0,up);
        photo_level_up(forward,level);
        assert(fabsf(up[0]-level[0])<1e-6f && fabsf(up[1]-level[1])<1e-6f && fabsf(up[2]-level[2])<1e-6f);
        /* Positive roll tilts the top of the view right (toward -y when facing +x). */
        photo_roll_up(east,0.5f,up);
        assert(fabsf(up[1]+sinf(0.5f))<1e-6f && fabsf(up[2]-cosf(0.5f))<1e-6f);
        assert(fabsf(photo_roll_step(0,1,1.f,0)-PHOTO_ROLL_SPEED)<1e-6f);
        assert(fabsf(photo_roll_step(0,-1,1.f,1)+2*PHOTO_ROLL_SPEED)<1e-6f);
        assert(photo_roll_step(0.25f,0,1.f,3)==0.25f);
        assert(fabsf(photo_roll_step(3.f,1,1.f,0)-(3.f+PHOTO_ROLL_SPEED-6.2831853f))<1e-5f);
        /* A saved framing keeps its roll to within float precision. */
        {
            struct photo_shot rolled = shot, back;
            int rolled_revision;
            char rolled_path[1024];
            snprintf(rolled_path, sizeof(rolled_path), "%s.rolled", argv[1]);
            memcpy(rolled.forward,slanted,sizeof(slanted));
            photo_roll_up(rolled.forward,-0.7f,rolled.up);
            assert(photo_fragment_write(rolled_path,"stock:example",&rolled));
            assert(photo_fragment_read(rolled_path,"stock:example",&back,&rolled_revision));
            assert(fabsf(photo_roll_from_up(back.forward,back.up)+0.7f)<1e-5f);
            remove(rolled_path);
        }
    }
    shot.has_mp = 1;
    shot.mp.teams = 1;
    shot.mp.vehicle_sets[0] = 254; shot.mp.vehicle_sets[1] = 255;
    shot.mp.vehicle_counts[1][5] = PHOTO_MAX_VEHICLE_COUNT;
    shot.mp.starting_equipment = 1; shot.mp.no_shields = 1;
    shot.mp.infinite_grenades = 1; shot.mp.always_invisible = 1;
    shot.mp.weapon_set = 13; shot.mp.vehicle_set = 255;
    shot.mp.loadout = 1; shot.mp.primary_weapon = 9; shot.mp.secondary_weapon = 2;
    shot.mp.no_map_weapons = 1; shot.mp.vehicle_respawn_time = 60;
    shot.moved_vehicle_count = 1;
    strcpy(shot.moved_vehicles[0].tag,"vehicles\\warthog\\warthog");
    memcpy(shot.moved_vehicles[0].position,shot.position,sizeof(shot.position));
    memcpy(shot.moved_vehicles[0].forward,shot.forward,sizeof(shot.forward));
    memcpy(shot.moved_vehicles[0].up,shot.up,sizeof(shot.up));
    assert(photo_fragment_write(argv[1],"stock:example",&shot));
    assert(photo_fragment_read(argv[1],"stock:example",&loaded,&revision) && revision==1);
    assert(loaded.position[1]==2 && loaded.erase_ai && !strcmp(loaded.variant,"slayer"));
    assert(loaded.has_mp && !memcmp(&loaded.mp,&shot.mp,sizeof(shot.mp)));
    assert(loaded.moved_vehicle_count==1 && !memcmp(loaded.moved_vehicles,shot.moved_vehicles,sizeof(shot.moved_vehicles)));
    shot.mp.vehicle_counts[0][0] = 5; assert(!photo_shot_valid(&shot)); shot.mp.vehicle_counts[0][0] = 0;
    shot.moved_vehicles[0].up[0] = 1; assert(!photo_shot_valid(&shot)); shot.moved_vehicles[0].up[0] = 0;

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
