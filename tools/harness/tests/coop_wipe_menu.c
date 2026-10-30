#include "harness.h"
#define MAXIMUM_STRINGS 16
#define _stricmp strcasecmp
#include <strings.h>
struct widget_instance {
 struct widget_instance *parent;
 long definition_tag_index;
 struct {struct {short selected_index,number_of_items;} list;} parameters;
};
static char const *setting_value="auto";
static int reads,writes;
static int config_text(char const *name,char *text,unsigned int size){CHECK(!strcmp(name,"network.coop_team_wipe"),"wrong read setting");snprintf(text,size,"%s",setting_value);reads++;return TRUE;}
static int config_write(char const *name,char const *value){CHECK(!strcmp(name,"network.coop_team_wipe"),"wrong write setting");setting_value=value;writes++;return TRUE;}
static boolean setting_text(char const *name,char *text,unsigned int size,boolean defaults){CHECK(!defaults,"loaded default instead of stored choice");return config_text(name,text,size);}
#define platform_log(...) ((void)0)
#include "types.inc"
#include "metadata.inc"
static struct pc_menu_setting *pc_menu_setting_get(long index){CHECK(index==1,"wrong definition");return &setting;}
#include "under_test.inc"
int main(void)
{
 struct widget_instance spinner={.definition_tag_index=1,.parameters.list.number_of_items=3};
 char const *values[]={"auto","revert","respawn"};
 for(int i=0;i<3;i++){
  setting_value=values[i];CHECK(setting_load(&spinner),"load failed");
  CHECK(spinner.parameters.list.selected_index==i,"stored choice not loaded");
  spinner.parameters.list.selected_index=(i+1)%3;CHECK(setting_save(&spinner),"save failed");
  CHECK(!strcmp(setting_value,values[(i+1)%3]),"selected choice not written");
 }
 CHECK(reads==3&&writes==3,"binding did not read/write config");
 setting_value="invalid";CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==0,"invalid choice did not use AUTO");
 return 0;
}
