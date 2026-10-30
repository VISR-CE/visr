#include "harness.h"
#define PIN(v, low, high) ((v)<(low)?(low):((v)>(high)?(high):(v)))
#define _game_variant_no_shields_bit 0
#define _game_variant_draw_object_in_motion_sensor_bit 1
#define _radar_players_none 0
struct game_variant {struct {long flags;real health;} universal_variant;};
struct game_variant_options {byte radar_players;};
static char const *setting_value="auto";
static int writes;
static char const *config_string(char const *name){CHECK(!strcmp(name,"network.coop_team_wipe"),"wrong setting");return setting_value;}
static int config_write(char const *name,char const *value){CHECK(!strcmp(name,"network.coop_team_wipe"),"wrong write setting");setting_value=value;writes++;return TRUE;}
#include "types.inc"
#include "under_test.inc"
int main(void)
{
 for(int i=0;i<3;i++){
  CHECK(option.values[i]==i,"wrong setting index");
  gametype_option_value_set(&option,option.values[i],NULL,NULL);
  CHECK(gametype_option_value(&option,NULL,NULL)==i,"setting did not round trip");
 }
 CHECK(writes==3&&!strcmp(setting_value,"respawn"),"wrong setting writes");
 setting_value="invalid";
 CHECK(gametype_option_value(&option,NULL,NULL)==0,"unknown setting did not use AUTO");
 return 0;
}
