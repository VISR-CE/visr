#include "harness.h"
#include <strings.h>
#define _stricmp strcasecmp
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define PIN(value,low,high) ((value)<(low)?(low):((value)>(high)?(high):(value)))
#define _friendly_fire_off 0
#define _friendly_fire_on 1
#define _friendly_fire_shields_only 2
#define _friendly_fire_explosives_only 3
struct widget_instance {
 const char *name;
 int visible;
 struct widget_instance *focused_child;
 struct {
  struct { short selected_index, string_list_index; struct widget_instance *extended_description; } list;
  struct { short string_list_index; } text_box;
 } parameters;
};
static struct widget_instance widgets[] = {
 {.name="friendly_fire_spinner"}, {.name="extra_enemies_spinner"},
 {.name="player_collisions_spinner"}, {.name="ally_friendly_fire_spinner"}, {.name="coop_team_wipe_spinner"},
 {.name="op_friendly_fire"}, {.name="op_extra_enemies"}, {.name="op_enemies_per_player"},
 {.name="op_enemies_multiplier"}, {.name="op_player_collisions"}, {.name="op_ally_friendly_fire"},
 {.name="op_coop_team_wipe"}
};

static struct widget_instance *named(struct widget_instance *list,const char *name,int depth){(void)list;(void)depth;for(unsigned int i=0;i<NUMBEROF(widgets);i++)if(!strcmp(widgets[i].name,name))return &widgets[i];return NULL;}
static void visible_set(struct widget_instance *w,int visible){CHECK(w,"missing row");w->visible=visible;}
static void settings_help(struct widget_instance *list){list->parameters.list.extended_description->parameters.text_box.string_list_index=0;}
#include "constants.inc"
static const char *stored_friendly_fire;
static int stored_collisions,stored_ally,applied_friendly_fire,applied_collisions,applied_ally;
static const char *config_string(const char *key){CHECK(!strcmp(key,"network.coop_friendly_fire"),"wrong string key");return stored_friendly_fire;}
static int config_boolean(const char *key){if(!strcmp(key,"network.coop_player_collisions"))return stored_collisions;CHECK(!strcmp(key,"network.coop_ally_friendly_fire"),"wrong boolean key");return stored_ally;}
static void network_game_server_port_set_cooperative_friendly_fire(int value){applied_friendly_fire=value;}
static void network_game_server_port_set_cooperative_player_collisions(int value){applied_collisions=value;}
static void network_game_server_port_set_cooperative_ally_friendly_fire(int value){applied_ally=value;}
#include "update.inc"
static struct widget_instance help,list={.parameters.list.extended_description=&help};
static void select_row(const char *name){list.focused_child=named(&list,name,0);}
static void choice(const char *name,int value){named(&list,name,0)->parameters.list.selected_index=value;}
static int help_index(void){coop_options_update(&list);return help.parameters.text_box.string_list_index;}
int main(void){
 const char *stored_values[]={"off","on","shields_only","explosives_only","invalid","OFF","SHIELDS_ONLY"};
 int expected[]={0,1,2,3,0,0,2};
 for(int value=0;value<7;value++)for(int flags=0;flags<4;flags++){
  stored_friendly_fire=stored_values[value];stored_collisions=flags&1;stored_ally=(flags>>1)&1;
  server_coop_rules_apply();
  CHECK(applied_friendly_fire==expected[value],"stored friendly fire ignored without options page");
  CHECK(applied_collisions==stored_collisions&&applied_ally==stored_ally,"stored flags ignored without options page");
 }

 select_row("op_extra_enemies");
 for(int mode=0;mode<3;mode++){
  choice("extra_enemies_spinner",mode);
  CHECK(help_index()==COOPERATIVE_EXTRA_ENEMIES_HELP+mode,"enemy choice help");
  CHECK(named(&list,"op_enemies_per_player",0)->visible==(mode==1),"per-player visibility");
  CHECK(named(&list,"op_enemies_multiplier",0)->visible==(mode==2),"multiplier visibility");
 }
 select_row("op_enemies_per_player");CHECK(help_index()==COOPERATIVE_ENEMIES_PER_PLAYER_HELP,"per-player help");
 select_row("op_enemies_multiplier");CHECK(help_index()==COOPERATIVE_ENEMIES_MULTIPLIER_HELP,"multiplier help");
 const char *rows[]={"op_friendly_fire","op_player_collisions","op_ally_friendly_fire","op_coop_team_wipe"};
 const char *spinners[]={"friendly_fire_spinner","player_collisions_spinner","ally_friendly_fire_spinner","coop_team_wipe_spinner"};
 int counts[]={4,2,2,3};
 int offsets[]={COOPERATIVE_FRIENDLY_FIRE_HELP,COOPERATIVE_PLAYER_COLLISIONS_HELP,COOPERATIVE_ALLY_FRIENDLY_FIRE_HELP,COOPERATIVE_TEAM_WIPE_HELP};
 for(int row=0;row<4;row++)for(int value=0;value<counts[row];value++){
  select_row(rows[row]);choice(spinners[row],value);CHECK(help_index()==offsets[row]+value,"choice help");
 }
 return 0;
}
