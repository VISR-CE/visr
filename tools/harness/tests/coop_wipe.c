#include "harness.h"
#define MAXIMUM_TRACKED_PLAYERS 4
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 3
#define MAXIMUM_LOCAL_PLAYERS 2
#define MIN(a,b) ((a)<(b)?(a):(b))
#define VALID_INDEX(index,count) ((index)>=0&&(index)<(count))
#define csmemset memset
#define _error_silent 0
static int diagnostics;
#define error(...) (++diagnostics)
enum {_game_connection_local,_game_connection_network_client,_game_connection_network_server};
struct player_datum {short identifier;boolean quit_out_of_game;long unit_index,quit_out_of_game_time;};
struct data_iterator {long datum_index;int next;};
static struct data_array *player_data;
static long live_machine_players[3][2];
static long *machine_get_player_list(long machine){return live_machine_players[machine];}
static void data_iterator_new(struct data_iterator *i,struct data_array *array){(void)array;i->next=0;}
static void *data_iterator_next(struct data_iterator *i)
{
 while(i->next<player_data->maximum_count){
  int slot=i->next++;struct player_datum *p=(void *)(player_data->data+slot*player_data->size);
  if(p->identifier){i->datum_index=((long)(unsigned short)p->identifier<<16)|slot;return p;}
 }
 return NULL;
}
static struct player_datum *player_try_and_get(long index){int slot=DATUM_INDEX_TO_ABSOLUTE_INDEX(index);if(slot>=player_data->maximum_count)return NULL;struct player_datum *p=datum_get(player_data,index);return (unsigned short)p->identifier==(unsigned long)index>>16?p:NULL;}
static void machine_remove_player(long index){for(int m=0;m<3;m++)for(int s=0;s<2;s++)if(live_machine_players[m][s]!=NONE&&DATUM_INDEX_TO_ABSOLUTE_INDEX(live_machine_players[m][s])==DATUM_INDEX_TO_ABSOLUTE_INDEX(index))live_machine_players[m][s]=NONE;}
static long last_killed_unit;
static boolean game_engine_running(void){return FALSE;}
static void unit_get(long index){(void)index;}
static void unit_kill_no_statistics(long index){last_killed_unit=index;}
#include "types.inc"
static struct coop_checkpoint_players checkpoint_players;
static int connection,active;
static struct {int public,has_password,listed;} lobby;

#define pthread_mutex_lock(lock) ((void)0)
#define pthread_mutex_unlock(lock) ((void)0)

static boolean late_join;
static int browser_enabled=TRUE,coop_public;
static struct {int hosting_lan_only;} p2p;
static int config_boolean(char const *name){if(!strcmp(name,"network.public_lobby"))return browser_enabled;if(!strcmp(name,"network.coop_late_join"))return late_join;CHECK(!strcmp(name,"network.coop_public"),"wrong config key");return coop_public;}
static void p2p_new_invite_if_listed(void){}
static short game_connection(void){return (short)connection;}
static boolean coop_game(void){return active;}
static long tick,main_loss_last_tick;
static boolean main_loss_scripted;
static boolean paused,all_dead,save_succeeds;
static int reverts,respawns;
static struct {boolean lost_map,saving_map;short loss_timer;} main_globals;
static struct {boolean saved_game_valid;} game_state_globals;
static boolean game_state_port_saved_game_valid(void){return game_state_globals.saved_game_valid;}
static long game_time_get(void){return tick;}
static boolean game_time_get_paused(void){return paused;}
static boolean players_are_all_dead(void){return all_dead;}
static boolean network_coop_active(void){return active;}
static boolean main_coop_host_revert(void){if(!game_state_globals.saved_game_valid)return FALSE;reverts++;return TRUE;}
static void players_respawn_at_checkpoint(void){respawns++;}
static void game_state_revert(void){reverts++;}
static void game_state_call_before_save_procs(void){}
static void main_stop_time(void){}
static void main_start_time(void){}
static boolean game_state_write_to_file(void){return save_succeeds;}
static void game_state_note_event(char const *note){(void)note;}
/* The unrelated state reset by the actual new-game lifecycle entry point. */
static int coop_presentation,coop_events,host_devices,client_devices,client_names_differing[1];
static int host_sent_transforms[1],host_sent_looks[1],host_sent_devices[1],host_player_structure_bsps[1],host_friendly_fire[1];
static int host_sent_screen_effect,host_resend,players_vitality,host_attachment_count,host_looping_sound_count;
static struct {int definition_indices[4];} client_script_sounds;
static struct {int objective,nav_point_count;} host_hud_state;
static struct {boolean offered,skip_save_written;int voters,cooldown_until;} skip_vote;
static void skip_vote_clear(void){}
static void coop_enemies_new_game(void){}
#include "under_test.inc"
static long add_player(void){long id=datum_new(player_data);int slot=DATUM_INDEX_TO_ABSOLUTE_INDEX(id);CHECK(id!=NONE,"player fixture full");((struct player_datum *)datum_get(player_data,id))->unit_index=100+slot;((struct player_datum *)datum_get(player_data,id))->quit_out_of_game_time=NONE;live_machine_players[slot<2?slot:2][slot<3?0:1]=id;return id;}
static struct player_datum *player(int slot){return (void *)(player_data->data+slot*player_data->size);}
static void setup(void)
{
 memset(live_machine_players,0xff,sizeof(live_machine_players));
 player_data=game_state_data_new("players",4,sizeof(struct player_datum));add_player();add_player();
 connection=_game_connection_network_server;active=TRUE;browser_enabled=TRUE;coop_public=FALSE;p2p.hosting_lan_only=FALSE;memset(&lobby,0,sizeof(lobby));late_join=FALSE;
 tick=main_loss_last_tick=0;paused=FALSE;all_dead=TRUE;save_succeeds=TRUE;reverts=respawns=diagnostics=0;
 memset(&main_globals,0,sizeof(main_globals));game_state_globals.saved_game_valid=FALSE;
 network_coop_new_game();game_state_save();
}
static void wipe(void){main_lost_map();tick+=92;main_lost_map_private();}
int main(int argc,char **argv)
{
 char const *case_name=argc>1?argv[1]:"";setup();
 CASE("policy") {
  for(int drop_in=0;drop_in<2;drop_in++)for(int listed=0;listed<2;listed++)for(int locked=0;locked<2;locked++)for(int browser=0;browser<2;browser++){
   setup();late_join=drop_in;lobby.public=listed;lobby.has_password=locked;browser_enabled=browser;wipe();wipe();
   CHECK(reverts==2*(1-drop_in)&&respawns==2*drop_in,"fireteam policy depended on visibility/password/browser");
  }
 } else CASE("authority") {
  late_join=FALSE;connection=_game_connection_network_client;CHECK(!network_coop_wipe_reverts(),"client decides wipe");
  connection=_game_connection_network_server;active=FALSE;CHECK(!network_coop_wipe_reverts(),"multiplayer decides co-op wipe");
  connection=_game_connection_local;wipe();CHECK(reverts==1&&!respawns,"local campaign behavior changed");
 } else CASE("roster") {
  setup();add_player();wipe();CHECK(!reverts&&respawns==1,"new player reverted");
  setup();player(1)->identifier++;wipe();CHECK(!reverts&&respawns==1,"different datum reverted");
  setup();live_machine_players[2][0]=live_machine_players[1][0];live_machine_players[1][0]=NONE;
  wipe();CHECK(!reverts&&respawns==1,"different machine reverted");
  setup();live_machine_players[1][1]=live_machine_players[1][0];live_machine_players[1][0]=NONE;
  wipe();CHECK(!reverts&&respawns==1,"different local slot reverted");
  setup();player(1)->quit_out_of_game=TRUE;wipe();CHECK(!reverts&&respawns==1,"quit player on a current machine reverted");
  setup();add_player();game_state_save();wipe();CHECK(reverts==1&&!respawns,"new matching checkpoint rejected");
  for(int pending=0;pending<2;pending++){
   setup();player(1)->quit_out_of_game=!pending;live_machine_players[1][0]=NONE;
   wipe();wipe();CHECK(reverts==2&&!respawns,"subset roster did not revert twice");
   /* The checkpoint restores a living departed player, but not its machine. */
   player(1)->quit_out_of_game=FALSE;player(1)->unit_index=101;
   last_killed_unit=NONE;coop_reconcile_departures();
   CHECK(player(1)->quit_out_of_game&&last_killed_unit==101,"departed player did not take the normal quit/death path after load");
   CHECK(!player(0)->quit_out_of_game&&player(0)->unit_index!=NONE,"current player was removed");
   game_state_save();wipe();CHECK(reverts==3&&!respawns,"checkpoint after departure rejected");
  }
  /* A scripted revert bypasses the wipe guard and can restore an old
     datum beneath a replacement's live machine entry. */
  setup();long replacement=live_machine_players[1][0]+0x10000;
  live_machine_players[1][0]=replacement;last_killed_unit=NONE;
  coop_reconcile_departures();
  CHECK(live_machine_players[1][0]==replacement&&!player(1)->quit_out_of_game&&last_killed_unit==NONE,
   "incompatible scripted load cleared a replacement's live mapping");
  setup();memset(live_machine_players,0xff,sizeof(live_machine_players));
  CHECK(!checkpoint_players_match(),"empty live roster accepted");
 } else CASE("save") {
  add_player();save_succeeds=FALSE;game_state_save();
  CHECK(checkpoint_players.count==2&&!game_state_port_saved_game_valid(),"failed save replaced membership snapshot");
  setup();game_state_globals.saved_game_valid=FALSE;wipe();CHECK(!reverts&&respawns==1&&diagnostics>0,"missing checkpoint did not safely respawn CLASSIC");
  setup();save_succeeds=FALSE;game_state_save();wipe();
  CHECK(!reverts&&respawns==1&&diagnostics>0,"failed save did not safely respawn CLASSIC");
  setup();late_join=TRUE;game_state_globals.saved_game_valid=FALSE;wipe();
  CHECK(!reverts&&respawns==1,"DROP-IN without a checkpoint restarted");
 } else CASE("new-map") {
  CHECK(network_coop_wipe_reverts(),"valid checkpoint rejected");
  host_attachment_count=3;host_looping_sound_count=2;
  network_coop_new_game();
  CHECK(!network_coop_wipe_reverts(),"checkpoint metadata survived a new game/map");
  CHECK(host_attachment_count==0&&host_looping_sound_count==0,"attachment or sound history survived a new game/map");
 } else CASE("delay") {
  main_lost_map();tick=91;main_lost_map_private();CHECK(!reverts&&!respawns,"rewound early");
  tick=92;main_lost_map_private();CHECK(reverts==1&&!main_globals.lost_map&&!main_globals.loss_timer,"wipe did not clear loss state");
  wipe();CHECK(reverts==2&&!respawns,"second wipe failed");
  setup();main_lost_map();paused=TRUE;tick=92;main_lost_map_private();CHECK(!reverts,"pause advanced death timer");
  paused=FALSE;main_lost_map_private();tick=184;main_lost_map_private();CHECK(reverts==1,"resumed death timer stalled");
 } else CASE("failure") {
  for(int drop_in=0;drop_in<2;drop_in++)for(int dead=0;dead<2;dead++){
   setup();late_join=drop_in;all_dead=dead;add_player();main_lost_map_scripted();tick+=92;main_lost_map_private();
   CHECK(reverts==1&&!respawns,"scripted failure used wipe policy/roster guard");
  }
  setup();late_join=TRUE;main_lost_map();main_lost_map_scripted();tick+=92;main_lost_map_private();
  CHECK(reverts==1&&!respawns,"scripted failure did not override pending wipe");
  wipe();CHECK(reverts==1&&respawns==1,"scripted loss leaked into the next DROP-IN wipe");
  setup();active=FALSE;all_dead=FALSE;game_state_globals.saved_game_valid=FALSE;
  main_lost_map_scripted();tick+=92;main_lost_map_private();
  CHECK(!reverts&&respawns==1,"multiplayer loss behavior changed");
  setup();active=FALSE;all_dead=TRUE;main_lost_map_scripted();tick+=92;main_lost_map_private();
  CHECK(!reverts&&respawns==1,"multiplayer all-dead loss behavior changed");
 } else return 2;
 return 0;
}
