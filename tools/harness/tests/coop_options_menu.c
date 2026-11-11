#include "harness.h"
#include <strings.h>
#include <math.h>
#define _stricmp strcasecmp
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define PIN(value,low,high) ((value)<(low)?(low):((value)>(high)?(high):(value)))
#define _friendly_fire_off 0
#define _friendly_fire_on 1
#define _friendly_fire_shields_only 2
#define _friendly_fire_explosives_only 3
struct widget_instance {
 const char *name;
 struct widget_instance *focused_child;
 struct {
  struct { short selected_index; struct widget_instance *extended_description; } list;
  struct { short string_list_index; } text_box;
 } parameters;
};
static struct widget_instance widgets[] = {
 {.name="friendly_fire_spinner"}, {.name="extra_enemies_spinner"},
 {.name="player_collisions_spinner"}, {.name="fireteam_spinner"},
 {.name="op_friendly_fire"}, {.name="op_extra_enemies"},
 {.name="op_player_collisions"}, {.name="op_fireteam"}
};
static struct widget_instance *named(struct widget_instance *list,const char *name,int depth){(void)list;(void)depth;for(unsigned int i=0;i<NUMBEROF(widgets);i++)if(!strcmp(widgets[i].name,name))return &widgets[i];return NULL;}
static void settings_help(struct widget_instance *list){list->parameters.list.extended_description->parameters.text_box.string_list_index=0;}
static char stored_friendly_fire[32]="off",stored_mode[32]="none";
static long stored_percentage=50,stored_multiplier=4;
static int stored_collisions,stored_ally,applied_friendly_fire,applied_collisions,applied_ally;
static int mode_writes,percentage_writes,multiplier_writes,player_writes,ally_writes;
static const char *config_string(const char *key){
 if(!strcmp(key,"network.coop_friendly_fire"))return stored_friendly_fire;
 CHECK(!strcmp(key,"network.coop_enemies_mode"),"wrong string key");return stored_mode;
}
static long config_integer(const char *key){
 if(!strcmp(key,"network.coop_enemies"))return stored_percentage;
 CHECK(!strcmp(key,"network.coop_enemies_multiplier"),"wrong integer key");return stored_multiplier;
}
static int config_boolean(const char *key){if(!strcmp(key,"network.coop_player_collisions"))return stored_collisions;CHECK(!strcmp(key,"network.coop_ally_friendly_fire"),"wrong boolean key");return stored_ally;}
static int config_write(const char *key,const char *value){
 if(!strcmp(key,"network.coop_enemies_mode")){strcpy(stored_mode,value);mode_writes++;}
 else if(!strcmp(key,"network.coop_enemies")){stored_percentage=atol(value);percentage_writes++;}
 else if(!strcmp(key,"network.coop_enemies_multiplier")){stored_multiplier=atol(value);multiplier_writes++;}
 else if(!strcmp(key,"network.coop_friendly_fire")){if(value!=stored_friendly_fire)strcpy(stored_friendly_fire,value);player_writes++;}
 else {CHECK(!strcmp(key,"network.coop_ally_friendly_fire"),"wrong write key");stored_ally=!strcmp(value,"true");ally_writes++;}
 return TRUE;
}
static void network_game_server_port_set_cooperative_friendly_fire(int value){applied_friendly_fire=value;}
static void network_game_server_port_set_cooperative_player_collisions(int value){applied_collisions=value;}
static void network_game_server_port_set_cooperative_ally_friendly_fire(int value){applied_ally=value;}
#include "constants.inc"
#include "update.inc"
static struct widget_instance help,list={.parameters.list.extended_description=&help};
static void select_row(const char *name){list.focused_child=named(&list,name,0);}
static void choice(const char *name,int value){named(&list,name,0)->parameters.list.selected_index=value;}
static int help_index(void){coop_options_update(&list);return help.parameters.text_box.string_list_index;}
int main(void){
 const char *stored_values[]={"off","on","shields_only","explosives_only","invalid","OFF","SHIELDS_ONLY"};
 int expected[]={0,1,2,3,0,0,2};
 for(int value=0;value<7;value++)for(int flags=0;flags<4;flags++){
  strcpy(stored_friendly_fire,stored_values[value]);stored_collisions=flags&1;stored_ally=(flags>>1)&1;
  server_coop_rules_apply();
  CHECK(applied_friendly_fire==expected[value],"stored friendly fire ignored without options page");
  CHECK(applied_collisions==stored_collisions&&applied_ally==stored_ally,"stored flags ignored without options page");
  CHECK(coop_friendly_fire_choice(stored_friendly_fire,stored_ally)==((_stricmp(stored_friendly_fire,"off")?1:0)|(stored_ally?2:0)),"legacy player mode load");
 }
 const long amounts[]={0,25,50,100,150,200,2,4,8,16,32};
 for(int index=0;index<11;index++){
  long amount=-1;const char *mode=coop_enemies_apply(index,&amount);
  CHECK(!strcmp(mode,index==0?"none":index<=5?"per_player":"multiplier"),"enemy mode mapping");
  CHECK(amount==amounts[index],"enemy amount mapping");
  CHECK(coop_enemies_choice(mode,amount,amount)==index,"enemy round trip");
  strcpy(stored_mode,mode);stored_percentage=index<=5?amount:77;stored_multiplier=index>5?amount:13;
  coop_options_init(&list);CHECK(named(&list,"extra_enemies_spinner",0)->parameters.list.selected_index==index,"enemy screen init");
  long old_percentage=stored_percentage,old_multiplier=stored_multiplier;
  mode_writes=percentage_writes=multiplier_writes=0;
  coop_options_save(&list);
  CHECK(mode_writes==1&&percentage_writes==(index>0&&index<=5)&&multiplier_writes==(index>5),"write only the selected amount key");
  if(index==0||index>5)CHECK(stored_percentage==old_percentage,"preserve inactive percentage");
  if(index<=5)CHECK(stored_multiplier==old_multiplier,"preserve inactive multiplier");
  select_row("op_extra_enemies");
  CHECK(help_index()==COOPERATIVE_EXTRA_ENEMIES_HELP+(index==0?0:index<=5?1:2),"enemy choice help");
 }
 CHECK(coop_enemies_choice("per_player",74,32)==2,"nearest percentage");
 CHECK(coop_enemies_choice("per_player",75,32)==2,"tie picks lower percentage");
 CHECK(coop_enemies_choice("per_player",-5,32)==1,"percentage below range");
 CHECK(coop_enemies_choice("per_player",999,32)==5,"percentage above range");
 CHECK(coop_enemies_choice("multiplier",25,7)==8,"nearest multiplier");
 CHECK(coop_enemies_choice("multiplier",25,-1)==6,"multiplier below range");
 CHECK(coop_enemies_choice("multiplier",25,999)==10,"multiplier above range");
 CHECK(coop_enemies_choice("invalid",100,4)==0,"unknown mode");
 strcpy(stored_mode,"per_player");stored_percentage=74;
 coop_options_init(&list);CHECK(named(&list,"extra_enemies_spinner",0)->parameters.list.selected_index==2,"off-list config init");
 strcpy(stored_mode,"multiplier");stored_multiplier=7;
 coop_options_init(&list);CHECK(named(&list,"extra_enemies_spinner",0)->parameters.list.selected_index==8,"off-list multiplier init");
 for(int index=0;index<4;index++){
  boolean allies;
  strcpy(stored_friendly_fire,index&1?"on":"off");stored_ally=!!(index&2);
  CHECK(coop_friendly_fire_choice(stored_friendly_fire,stored_ally)==index,"friendly fire load mapping");
  CHECK(!strcmp(coop_friendly_fire_apply(index,"off",&allies),index&1?"on":"off"),"friendly fire save mapping");
  CHECK(allies==!!(index&2),"ally save mapping");
  coop_options_init(&list);CHECK(named(&list,"friendly_fire_spinner",0)->parameters.list.selected_index==index,"friendly fire screen init");
  strcpy(stored_friendly_fire,"off");stored_ally=0;player_writes=ally_writes=0;
  coop_options_save(&list);
  CHECK(!strcmp(stored_friendly_fire,index&1?"on":"off")&&stored_ally==!!(index&2),"both friendly fire keys saved");
  CHECK(player_writes==1&&ally_writes==1,"friendly fire writes");
  select_row("op_friendly_fire");CHECK(help_index()==COOPERATIVE_FRIENDLY_FIRE_HELP+index,"friendly fire choice help");
 }
 for(int legacy=2;legacy<=3;legacy++)for(int index=0;index<4;index++){
  strcpy(stored_friendly_fire,stored_values[legacy]);stored_ally=0;
  coop_options_init(&list);CHECK(named(&list,"friendly_fire_spinner",0)->parameters.list.selected_index==1,"legacy loads as PLAYERS ONLY");
  choice("friendly_fire_spinner",index);coop_options_save(&list);
  CHECK(!strcmp(stored_friendly_fire,index&1?stored_values[legacy]:"off"),"preserve finer player rule while enabled");
  CHECK(stored_ally==!!(index&2),"legacy ally save");
 }
 const char *rows[]={"op_player_collisions","op_fireteam"};
 const char *spinners[]={"player_collisions_spinner","fireteam_spinner"};
 int offsets[]={COOPERATIVE_PLAYER_COLLISIONS_HELP,COOPERATIVE_FIRETEAM_HELP};
 for(int row=0;row<2;row++)for(int value=0;value<2;value++){
  select_row(rows[row]);choice(spinners[row],value);CHECK(help_index()==offsets[row]+value,"choice help");
 }
 return 0;
}
