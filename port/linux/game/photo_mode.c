/* Developer framing controls; scene and camera ownership stays on the game thread. */
#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "photo_mode.h"
#include "photo_framing.h"
#include "camera/director.h"
#include "camera/flying_camera.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/cheats.h"
#include "units/units.h"
#include "objects/object_types.h"
#include "objects/objects.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "networking/network_connection.h"
#include "interface/hud.h"
#include "interface/player_ui.h"
#include "interface/hud_messaging.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "main/main.h"
#include "ai/ai_script.h"
#include "effects/player_effects.h"
#include "custom_edition_cache.h"
#include "map_share.h"
#include "map_pictures.h"
#include "physics/collisions.h"
#include "../src/port_config.h"
#include "halo_keyboard.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

void platform_log(const char *format, ...);
int halo_linux_mouse_look(short gamepad_index, float *yaw, float *pitch);
/* photo_fly_step reads halo_keyboard_actions bits directly. */
typedef char photo_fly_bits_match[(PHOTO_FLY_FORWARD == 1UL << HALO_KEYBOARD_MOVE_FORWARD &&
	PHOTO_FLY_BACKWARD == 1UL << HALO_KEYBOARD_MOVE_BACKWARD && PHOTO_FLY_LEFT == 1UL << HALO_KEYBOARD_STRAFE_LEFT &&
	PHOTO_FLY_RIGHT == 1UL << HALO_KEYBOARD_STRAFE_RIGHT && PHOTO_FLY_UP == 1UL << HALO_KEYBOARD_JUMP &&
	PHOTO_FLY_DOWN == 1UL << HALO_KEYBOARD_CROUCH) ? 1 : -1];
typedef char photo_vehicle_limits_match[(PHOTO_VARIANT_VEHICLES == NUMBER_OF_VARIANT_VEHICLES &&
	PHOTO_MAX_VEHICLE_COUNT == MAXIMUM_VARIANT_VEHICLE_COUNT) ? 1 : -1];
const char *platform_data_root(void);
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
#ifdef HALO_ILP32
int host_photo_mode_available(void);
int host_map_pictures_import_stage(void);
void host_photo_mode_overlay(float fov, int speed, int guides, float focus_x, float focus_y, float roll_degrees);
#else
static int host_photo_mode_available(void) { return 0; }
static int host_map_pictures_import_stage(void) { return 0; }
static void host_photo_mode_overlay(float fov, int speed, int guides, float focus_x, float focus_y, float roll_degrees) { (void)fov; (void)speed; (void)guides; (void)focus_x; (void)focus_y; (void)roll_degrees; }
#endif

static int active, pending, guides = 1, speed_step;
/* Q and E held (set on the input thread, read on the game thread); roll in radians. */
static volatile int roll_left, roll_right;
static float roll;
static long settle_until;
static struct director saved_director, *current_director;
static struct cinematic_global_data saved_cinematic;
static struct hud_scripted_globals_definition saved_hud;
static int capture_bsp = -1, capture_cleared;
static int saved_input, saved_scripted, saved_bsp, saved_dont_timeout, saved_deathless = -1, report_settle;
static float saved_speed, fov;
static struct photo_shot metadata;
/* Scripted capture (map_pictures_render.c): entered without a key or debug.photo_mode, no
fly input, no overlay. capture_failed is set when entry was refused. */
static int scripted, capture_failed, capture_framed, capture_revision, last_from_fragment, last_revision;
/* A scripted capture hides the local players' units and what they carry once the entry settle ends. */
static int capture_hide;

int photo_mode_active(void) { return active; }

void photo_mode_trace_key(int scancode, int action)
{
	if (!host_photo_mode_available())
		return; /* distribution hosts: these are ordinary keys */
	error(_error_silent, "photo key: scancode %d action %d (host available %d, debug.photo_mode %d)", scancode, action, host_photo_mode_available(), config_boolean("debug.photo_mode") ? 1 : 0);
}

int photo_mode_key(int action)
{
	if (!host_photo_mode_available() || !config_boolean("debug.photo_mode"))
	{
		if (action == PHOTO_TOGGLE) error(_error_silent, "photo mode: key ignored (host available %d, debug.photo_mode %d)", host_photo_mode_available(), config_boolean("debug.photo_mode") ? 1 : 0);
		return 0;
	}
	if (action != PHOTO_TOGGLE && !active) return 0;
	if (action == PHOTO_ROLL_LEFT) { roll_left = 1; return 1; }
	if (action == PHOTO_ROLL_RIGHT) { roll_right = 1; return 1; }
	if (!active && (!game_map_loaded() || !global_scenario ||
		global_scenario->type == _scenario_type_main_menu || (game_connection() != _game_connection_local && game_connection() != _game_connection_network_server)))
	{
		error(_error_silent, "photo mode: key ignored (no map is running on this machine as the host)");
		return 0;
	}
	pending = action;
	return 1;
}

void photo_mode_key_up(int action)
{
	if (action == PHOTO_ROLL_LEFT) roll_left = 0;
	if (action == PHOTO_ROLL_RIGHT) roll_right = 0;
}

void photo_mode_end(void)
{
	pending = 0;
	roll_left = roll_right = 0;
	roll = 0;
	scripted = 0;
	capture_bsp = -1;
	if (!active) return;
	active = 0;
	global_connection_dont_timeout = saved_dont_timeout;
	if (saved_bsp != global_structure_bsp_index_get()) scenario_switch_structure_bsp((short)saved_bsp);
	if (saved_deathless >= 0) cheat.deathless_player = (boolean)saved_deathless;
	saved_deathless = -1; report_settle = 0;
	game_time_set_speed(saved_speed);
	if (!saved_cinematic.in_progress) cinematic_stop();
	*cinematic_globals = saved_cinematic;
	*hud_scripted_globals = saved_hud;
	player_input_enable(saved_input);
	*director_camera_scripted = saved_scripted;
	if (current_director) *current_director = saved_director;
	current_director = NULL;
	host_photo_mode_overlay(0,0,0,0.5f,0.5f,0);
	error(_error_silent, "photo mode: exited; reload the map to play (AI and projectiles cannot be restored)");
}

static int map_key(char *key, size_t size)
{
	/* a scripted capture loads every map, multiplayer ones too, by the solo map name (map_pictures_render.c) */
	const char *name = global_scenario->type == _scenario_type_multiplayer && !scripted ? main_get_multiplayer_map_name() : main_get_map_name();
	byte hash[32]; char path[256], stock_key[256];
	int custom = custom_edition_level_name(name);
	if (!custom && photo_stock_scenario(name)) return photo_map_key(name,0,NULL,key,size);
	if (custom)
	{
		if (!custom_edition_map_file_path(name,path)) return 0;
	}
	else
	{
		if (!photo_map_key(name,0,NULL,stock_key,sizeof(stock_key)) ||
			snprintf(path,sizeof(path),"d:\\maps\\%s.map",stock_key+6) >= (int)sizeof(path)) return 0;
	}
	if (!map_share_map_hash(path,hash))
	{
		error(_error_silent, "photo mode: cannot hash custom map; framing was not saved or loaded");
		return 0;
	}
	return photo_map_key(name,1,hash,key,size);
}

static int inside_bsp(struct photo_shot *shot)
{
	struct structure_bsp *bsp;
	int i;
	if (shot->bsp != global_structure_bsp_index_get() || !(bsp = global_structure_bsp_get())) return 0;
	for (i=0;i<3;i++) if (shot->position[i] < bsp->world_bounds.n[i*2] || shot->position[i] > bsp->world_bounds.n[i*2+1])
	{
		error(_error_silent, "photo mode: position %c=%.3f outside BSP %d bounds [%.3f, %.3f] by %.3f", "xyz"[i], shot->position[i], shot->bsp,
			bsp->world_bounds.n[i*2], bsp->world_bounds.n[i*2+1],
			shot->position[i] < bsp->world_bounds.n[i*2] ? bsp->world_bounds.n[i*2] - shot->position[i] : shot->position[i] - bsp->world_bounds.n[i*2+1]);
		return 0;
	}
	return 1;
}

/* Scenario vehicles do not retain a placement index. Named objects can be matched
by name; unnamed ones use every placement of the same remapped palette tag. */
static struct scenario_object_datum *vehicle_placement(long index)
{
	return (struct scenario_object_datum *)tag_block_get_element_with_size(&global_scenario->vehicles,index,
		object_type_definition_get(_object_type_vehicle)->placement_tag_block_element_size);
}
static float distance_squared(const float *a, const float *b)
{
	float x=a[0]-b[0], y=a[1]-b[1], z=a[2]-b[2];
	return x*x+y*y+z*z;
}
static int vehicle_moved(const struct object_datum *object)
{
	long i;
	for (i=0;i<global_scenario->vehicles.count;i++)
	{
		struct scenario_object_datum *p=vehicle_placement(i);
		if (object->object.name_index != NONE && p->name_index == object->object.name_index)
			return distance_squared((const float *)&object->object.position,(const float *)&p->position) > 0.25f;
	}
	for (i=0;i<global_scenario->vehicles.count;i++)
	{
		struct scenario_object_datum *p=vehicle_placement(i);
		long tag;
		if (!VALID_INDEX(p->palette_entry_index,global_scenario->vehicle_palette.count)) continue;
		tag=TAG_BLOCK_GET_ELEMENT(&global_scenario->vehicle_palette,p->palette_entry_index,struct scenario_object_palette_entry)->reference.index;
		if (tag != NONE && (tag == object->definition_index || game_engine_remap_vehicle(tag) == object->definition_index) &&
			distance_squared((const float *)&object->object.position,(const float *)&p->position) <= 0.25f) return 0;
	}
	return 1;
}
static void scene_record(struct photo_shot *shot)
{
	struct universal_variant *v=&game_engine_get_variant()->universal_variant;
	const struct game_variant_options *o=game_variant_options_get();
	struct photo_mp_scene *m=&shot->mp;
	struct object_iterator iterator;
	struct object_datum *object;
	int side, i, overflow=0;
	shot->has_mp=0; shot->moved_vehicle_count=0;
	if (global_scenario->type != _scenario_type_multiplayer) return;
	shot->has_mp=1;
	{ static const char *names[]={"none","ctf","slayer","oddball","king","race"};
	  long mode=game_engine_get_variant()->game_engine_index;
	  snprintf(shot->variant,sizeof(shot->variant),"%s",mode >= 0 && mode < 6 ? names[mode] : "none"); }
	m->teams=v->teams; m->weapon_set=v->weapon_set; m->vehicle_set=v->vehicle_set;
	m->starting_equipment=TEST_FLAG(v->flags,_game_variant_generic_starting_equipment_bit);
	m->no_shields=TEST_FLAG(v->flags,_game_variant_no_shields_bit);
	m->always_invisible=TEST_FLAG(v->flags,_game_variant_always_invisible_bit);
	m->infinite_grenades=TEST_FLAG(v->flags,_game_variant_infinite_grenades_bit);
	for (side=0;side<2;side++)
	{
		m->vehicle_sets[side]=o->vehicle_set[side];
		for (i=0;i<PHOTO_VARIANT_VEHICLES;i++) m->vehicle_counts[side][i]=o->vehicle_counts[side][i];
	}
	m->loadout=o->loadout; m->primary_weapon=o->primary_weapon; m->secondary_weapon=o->secondary_weapon;
	m->no_map_weapons=o->no_map_weapons; m->vehicle_respawn_time=o->vehicle_respawn_time;
	object_iterator_new(&iterator,_object_mask_vehicle,0);
	while ((object=object_iterator_next(&iterator)) != NULL)
	{
		float delta[3], distance, dot=0;
		const char *tag;
		struct photo_moved_vehicle *moved;
		for (i=0;i<3;i++) { delta[i]=((float *)&object->object.position)[i]-shot->position[i]; dot+=delta[i]*shot->forward[i]; }
		distance=sqrtf(distance_squared((float *)&object->object.position,shot->position));
		/* A generous cone includes the image corners; distant scenery is not a pose recipe. */
		if (distance > 200.f || (distance > 0.001f && dot/distance < cosf(fminf(3.14159265f,shot->fov*0.5f+0.35f))) || !vehicle_moved(object)) continue;
		if (shot->moved_vehicle_count == PHOTO_MAX_MOVED_VEHICLES) { overflow=1; continue; }
		tag=tag_get_name(object->definition_index);
		if (!tag || !*tag || strlen(tag) >= 256) { error(_error_silent,"photo: cannot record vehicle with an invalid tag path"); continue; }
		for (i=0;tag[i];i++) if ((unsigned char)tag[i] < 32 || (unsigned char)tag[i] > 126) break;
		if (tag[i]) { error(_error_silent,"photo: cannot record vehicle with a non-ASCII tag path"); continue; }
		moved=&shot->moved_vehicles[shot->moved_vehicle_count++];
		strcpy(moved->tag,tag);
		memcpy(moved->position,&object->object.position,sizeof(moved->position));
		memcpy(moved->forward,&object->object.forward,sizeof(moved->forward));
		memcpy(moved->up,&object->object.up,sizeof(moved->up));
	}
	if (overflow) error(_error_silent,"photo: moved vehicle limit reached; recorded first 16 visible vehicles");
}
static int vehicle_occupied(long index)
{
	struct object_iterator iterator;
	struct unit_datum *unit;
	object_iterator_new(&iterator,_object_mask_biped,0);
	while ((unit=object_iterator_next(&iterator)) != NULL)
		if (unit->unit.player_index != NONE && object_get_ultimate_parent(iterator.index) == index) return 1;
	return 0;
}
/* A vehicle still in the object table but already queued for deletion is gone:
the table keeps it until the next objects update. */
static boolean vehicle_live(long index)
{
	struct object_header_datum *header=object_header_try_and_get(index);
	return header && header->datum && !TEST_FLAG(header->flags,_object_header_being_deleted_bit);
}
static long vehicles_live_count(void)
{
	struct object_iterator iterator;
	long count=0;
	object_iterator_new(&iterator,_object_mask_vehicle,0);
	while (object_iterator_next(&iterator)) if (vehicle_live(iterator.index)) count++;
	return count;
}
/* Delete every vehicle now, not at the next objects update, so the rebuild and
the moved-vehicle search see only the new set and a second load starts clean. */
static void vehicles_delete_all(void)
{
	struct object_iterator iterator;
	for (;;)
	{
		long found=NONE;
		object_iterator_new(&iterator,_object_mask_vehicle,0);
		while (object_iterator_next(&iterator)) if (vehicle_live(iterator.index)) { found=iterator.index; break; }
		if (found == NONE) return;
		object_delete_immediately(found);
		if (vehicle_live(found)) { error(_error_silent,"photo: cannot delete vehicle %ld",found); return; }
	}
}
static void scene_restore(const struct photo_shot *shot)
{
	struct universal_variant *v=&game_engine_get_variant()->universal_variant;
	struct game_variant_options o=*game_variant_options_get();
	const struct photo_mp_scene *m=&shot->mp;
	struct object_iterator iterator;
	long i, restored[PHOTO_MAX_MOVED_VEHICLES];
	long before, after_delete=NONE, after_rebuild=NONE;
	int side, k;
	if (!shot->has_mp && !shot->moved_vehicle_count) return;
	if (global_scenario->type != _scenario_type_multiplayer)
	{ error(_error_silent,"photo: cannot restore multiplayer scene in campaign"); return; }
	/* Mode-specific globals and objectives require map initialization. */
	{ static const char *names[]={"none","ctf","slayer","oddball","king","race"};
	  long mode=game_engine_get_variant()->game_engine_index;
	  if (mode < 0 || mode >= 6 || strcmp(shot->variant,names[mode]))
	  { error(_error_silent,"photo: cannot restore game type live; reload the map with variant %s",shot->variant); return; } }
	if (shot->has_mp)
	{
		v->teams=(boolean)m->teams; v->weapon_set=m->weapon_set; v->vehicle_set=m->vehicle_set;
		SET_FLAG(v->flags,_game_variant_generic_starting_equipment_bit,m->starting_equipment);
		SET_FLAG(v->flags,_game_variant_no_shields_bit,m->no_shields);
		SET_FLAG(v->flags,_game_variant_always_invisible_bit,m->always_invisible);
		SET_FLAG(v->flags,_game_variant_infinite_grenades_bit,m->infinite_grenades);
		for (side=0;side<2;side++)
		{
			o.vehicle_set[side]=(byte)m->vehicle_sets[side];
			for (k=0;k<PHOTO_VARIANT_VEHICLES;k++) o.vehicle_counts[side][k]=(byte)m->vehicle_counts[side][k];
		}
		o.loadout=(byte)m->loadout; o.primary_weapon=(byte)m->primary_weapon; o.secondary_weapon=(byte)m->secondary_weapon;
		o.no_map_weapons=(boolean)m->no_map_weapons; o.vehicle_respawn_time=(short)m->vehicle_respawn_time;
		game_set_game_variant_options(&o);
		error(_error_silent,"photo: cannot restore map weapons and equipment live; reload the map with variant %s",shot->variant);
		error(_error_silent,"photo: cannot re-equip existing players or rebuild team objectives live; reload the map with variant %s",shot->variant);
	}
	before=vehicles_live_count();
	/* Do not delete a vehicle carrying a player or leave half a new placement set. */
	object_iterator_new(&iterator,_object_mask_vehicle,0);
	while (object_iterator_next(&iterator)) if (vehicle_live(iterator.index) && vehicle_occupied(iterator.index))
	{ error(_error_silent,"photo: cannot restore vehicles while a player occupies one; exit the vehicle and load again"); return; }
	if (shot->has_mp)
	{
		vehicles_delete_all();
		after_delete=vehicles_live_count();
		game_engine_vehicle_homes_reset();
		game_engine_vehicle_placement_begin();
		for (i=0;i<global_scenario->vehicles.count;i++)
		{
			struct scenario_object_datum *p=vehicle_placement(i);
			if (!VALID_INDEX(p->palette_entry_index,global_scenario->vehicle_palette.count) ||
				TEST_FLAG(p->placement_flags,_scenario_object_placement_not_automatic_bit)) continue;
			if (!custom_edition_vehicle_placement_allowed(p) || !game_engine_vehicle_placement_allowed(p,&global_scenario->vehicle_palette)) continue;
			if (object_new_from_scenario(p,&global_scenario->vehicle_palette) == NONE)
				error(_error_silent,"photo: cannot restore scenario vehicle %ld; tag unavailable or placement refused",i);
		}
		after_rebuild=vehicles_live_count();
	}
	for (i=0;i<shot->moved_vehicle_count;i++)
	{
		const struct photo_moved_vehicle *moved=&shot->moved_vehicles[i];
		struct object_placement_data placement;
		struct object_datum *object;
		long tag=tag_loaded('vehi',moved->tag), nearest=NONE, created;
		float best=0;
		short name=NONE;
		restored[i]=NONE;
		if (tag == NONE || game_engine_remap_vehicle(tag) != tag)
		{ error(_error_silent,"photo: cannot restore moved vehicle %s; tag is not loaded or variant excludes it",moved->tag); continue; }
		/* The format has no spawn identity. Replace the nearest unused vehicle of
		the same tag, so a moved vehicle does not also appear on a spawn pad. */
		object_iterator_new(&iterator,_object_mask_vehicle,0);
		while ((object=object_iterator_next(&iterator)) != NULL)
		{
			float d;
			if (object->definition_index != tag || !vehicle_live(iterator.index)) continue;
			for (k=0;k<i;k++) if (restored[k] == iterator.index) break;
			if (k<i) continue;
			d=distance_squared((float *)&object->object.position,moved->position);
			if (nearest == NONE || d<best) { nearest=iterator.index; best=d; name=object->object.name_index; }
		}
		object_placement_data_new(&placement,tag,NONE);
		memcpy(&placement.position,moved->position,sizeof(moved->position));
		memcpy(&placement.forward,moved->forward,sizeof(moved->forward));
		memcpy(&placement.up,moved->up,sizeof(moved->up));
		if (scenario_leaf_index_from_point(&placement.position) == NONE)
		{ error(_error_silent,"photo: cannot restore moved vehicle %s; position is outside the BSP",moved->tag); continue; }
		created=object_new(&placement);
		if (created == NONE) { error(_error_silent,"photo: cannot create moved vehicle %s",moved->tag); continue; }
		if (nearest != NONE) object_delete_immediately(nearest);
		if (name != NONE) object_claim_scenario_name(created,name);
		restored[i]=created;
	}
	error(_error_silent,"photo: vehicles before restore %ld, after delete %ld, after rebuild %ld, after moved %ld (%d moved)",
		before,after_delete,after_rebuild,vehicles_live_count(),shot->moved_vehicle_count);
}

static void camera_shot(struct flying_camera *camera, struct photo_shot *shot)
{
	real_vector3d forward;
	*shot = metadata;
	memcpy(shot->position,&camera->position,sizeof(shot->position));
	vector3d_from_euler_angles2d(&forward,&camera->facing);
	memcpy(shot->forward,&forward,sizeof(shot->forward));
	photo_roll_up(shot->forward,camera->roll,shot->up);
	shot->fov = camera->field_of_view;
	shot->bsp = global_structure_bsp_index_get();
}

/* Before a map loads: the framing's gametype becomes the one the load starts with, as scene_restore only
applies a scene to a map already running that gametype. Maps with no multiplayer scene get the default again. */
void photo_mode_prepare_variant(const char *key)
{
	char path[1024];
	struct photo_shot shot;
	int revision = 0;
	struct game_variant variant;
	player_ui_clear_game_variant();
	game_set_game_variant(NULL);
	memset(&shot,0,sizeof(shot));
	if (snprintf(path,sizeof(path),"d:\\framings\\%s.json",key) >= (int)sizeof(path)) return;
	if (photo_framing_lookup(path,"d:\\map-framings.json",key,&shot,&revision) == PHOTO_FRAMING_NONE) return;
	if ((!shot.has_mp && !shot.moved_vehicle_count) || !strcmp(shot.variant,"none") || !shot.variant[0]) return;
	game_engine_get_variant_by_name(&variant,shot.variant);
	player_ui_set_game_variant(&variant);
	/* the engine starts from the game's own variant (game_initialize_for_new_map), which only the menu's start path sets */
	game_set_game_variant(&variant);
	error(_error_silent,"photo: map load starts with variant %s",shot.variant);
}

/* After a capture: the next load starts with the default gametype again. */
void photo_mode_clear_variant(void)
{
	player_ui_clear_game_variant();
}

static int file_action(int action, struct flying_camera *camera)
{
	char key[256], folder[768], path[1024], camera_path[1024];
	/* bundled with the app; the host copies it to the data folder at each start */
	const char *bundle_path = "d:\\map-framings.json";
	struct photo_shot shot;
	int revision = 0;
	last_from_fragment = 0; last_revision = 0;
	if (!map_key(key,sizeof(key))) return 0;
	char host_path[1024], host_camera_path[1024];
	/* the game's CRT maps d:\ to platform_data_root(); a host path here would be prefixed with that root again */
	if (snprintf(folder,sizeof(folder),"d:\\framings") >= (int)sizeof(folder) ||
		snprintf(path,sizeof(path),"%s\\%s%s.json",folder,key,action == PHOTO_LOAD_CANDIDATE ? ".solved" : "") >= (int)sizeof(path) ||
		snprintf(camera_path,sizeof(camera_path),"d:\\camera.txt") >= (int)sizeof(camera_path)) return 0;
	platform_translate_path(path,host_path,sizeof(host_path));
	platform_translate_path(camera_path,host_camera_path,sizeof(host_camera_path));
	camera_shot(camera,&shot);
	if (action == PHOTO_SAVE)
	{
		if (!inside_bsp(&shot)) { error(_error_silent, "photo mode: camera is outside the current BSP bounds; save refused"); return 0; }
		scene_record(&shot);
		_mkdir(folder);
		/* a first save over a bundled framing continues its revision, so the menu picture is remade */
		{ struct photo_shot bundled; if (photo_framing_lookup(NULL,bundle_path,key,&bundled,&revision) != PHOTO_FRAMING_BUNDLED) revision = 0; }
		if (!photo_fragment_write_from(path,key,&shot,revision)) { error(_error_silent, "photo mode: fragment save failed: %s", host_path); return 0; }
		director_save_camera();
		FILE *written = fopen(camera_path,"r");
		error(_error_silent, "photo mode: saved framing %s; camera.txt %s %s", host_path, written ? "written" : "MISSING", host_camera_path);
		if (written) fclose(written);
		return 1;
	}
	else
	{
		FILE *file = fopen(path,"r");
		int exists = file != NULL, source;
		if (file) fclose(file);
		int previous_bsp = global_structure_bsp_index_get();
		shot.has_mp=0; shot.moved_vehicle_count=0;
		/* Debug-only: J loads framings/<map>.solved.json, the candidate a tool wrote beside the active framing. Tools never
		write the active <map>.json; choosing a candidate is J, then the save key. */
		if (action == PHOTO_LOAD_CANDIDATE && !exists) { error(_error_silent, "photo mode: no solved framing %s", host_path); return 0; }
		source = photo_framing_lookup(path,action == PHOTO_LOAD_CANDIDATE ? NULL : bundle_path,key,&shot,&revision);
		/* A scripted capture uses only the map's own framing, never the last camera.txt. */
		if (source == PHOTO_FRAMING_NONE && !exists && scripted) return 0;
		if (!(source != PHOTO_FRAMING_NONE || (!exists && photo_camera_read(camera_path,&shot))) ||
			shot.bsp >= global_scenario->structure_bsp_references.count || shot.bsp > 32767)
		{
			error(_error_silent, "photo mode: invalid framing or BSP index; load refused (%s)", exists || source != PHOTO_FRAMING_NONE ? host_path : host_camera_path); return 0;
		}
		if (shot.bsp != previous_bsp && !scenario_switch_structure_bsp((short)shot.bsp))
		{ error(_error_silent, "photo mode: BSP switch failed; load refused"); return 0; }
		if (!inside_bsp(&shot))
		{
			if (previous_bsp != global_structure_bsp_index_get()) scenario_switch_structure_bsp((short)previous_bsp);
			error(_error_silent, "photo mode: position outside BSP bounds; load refused"); return 0;
		}
		if (previous_bsp != shot.bsp)
		{
			ai_scripting_erase_all();
			game_time_set_speed(1);
			settle_until = game_time_get() + shot.settle_ticks;
		}
		scene_restore(&shot);
		memcpy(&camera->position,shot.position,sizeof(shot.position));
		{ real_vector3d forward; memcpy(&forward,shot.forward,sizeof(shot.forward)); euler_angles2d_from_vector3d(&camera->facing,&forward); }
		camera->roll = roll = photo_roll_from_up(shot.forward,shot.up);
		camera->field_of_view = fov = shot.fov;
		metadata = shot;
		/* AI was already erased on entry; retain an honest recipe on the next save. */
		metadata.erase_ai = 1;
		error(_error_silent, "photo mode: loaded first shot (roll %.2f degrees)", roll * 180.f / 3.14159265f);
		last_from_fragment = source != PHOTO_FRAMING_NONE; last_revision = last_from_fragment ? revision : 0;
		return 1;
	}
}

/* The keyboard and mouse never reach the gamepad's sticks in gameplay (xinput_sdl.c sets only
keyboard_actions_held and the pending mouse motion), so photo mode reads them itself and drives
the flying camera from them, in real seconds, while the game's clock stands still. */
static void fly(boolean *controls_active, float seconds, float facing[3], float translation[3], const struct flying_camera *camera)
{
	static unsigned long logged_at;
	unsigned long keys = halo_keyboard_actions(0), now = (unsigned long)(system_milliseconds());
	float mouse_yaw = 0, mouse_pitch = 0, step[3], scale = powf(2.f,(float)speed_step);
	int mouse = halo_linux_mouse_look(0,&mouse_yaw,&mouse_pitch), i;
	float stick[3] = {translation[0], translation[1], translation[2]};
	photo_fly_step(keys,seconds,speed_step,step);
	for (i=0;i<3;i++) translation[i] = stick[i]*scale + step[i];
	facing[0] += mouse_yaw; facing[1] += mouse_pitch; facing[2] = 0;
	*controls_active = TRUE;
	if (now - logged_at >= 500 && (keys || mouse || stick[0] || stick[1] || stick[2] || facing[0] || facing[1]))
	{
		logged_at = now;
		error(_error_silent, "photo fly: dt %.4f keys 0x%lx mouse %d (%.4f,%.4f) stick (%.4f,%.4f,%.4f) move (%.4f,%.4f,%.4f) facing (%.4f,%.4f) speed %d camera (%.2f,%.2f,%.2f) yaw %.3f pitch %.3f",
			seconds, keys, mouse, mouse_yaw, mouse_pitch, stick[0], stick[1], stick[2], translation[0], translation[1], translation[2],
			facing[0], facing[1], speed_step, camera->position.x, camera->position.y, camera->position.z, camera->facing.yaw, camera->facing.pitch);
	}
}

/* Hides an object's attachments (held weapons and the like), then the object. */
static void hide_object_children(long object_index)
{
	while (object_index != NONE)
	{
		struct object_datum *object = object_get(object_index);
		hide_object_children(object->object.first_child_object_index);
		object_set_visibility(object_index, FALSE);
		object_index = object->object.next_object_index;
	}
}

static void hide_local_players(void)
{
	short local;
	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
	{
		long player_index = local_player_get_player_index(local);
		struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
		if (!player || player->unit_index == NONE || !object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit)) continue;
		hide_object_children(object_get(player->unit_index)->object.first_child_object_index);
		object_set_visibility(player->unit_index, FALSE);
	}
}

/* T: carry player 0 to the camera so the level's later structure BSPs load around him. The game
holds one structure BSP at a time, so each other BSP is loaded in turn and asked, with the
game's own leaf lookup, whether it holds the point; the current one is asked first. */
static void teleport_player(const struct flying_camera *camera)
{
	real_point3d point = camera->position;
	real_vector3d forward;
	short original = global_structure_bsp_index_get(), target = NONE, index;
	short count = (short)global_scenario->structure_bsp_references.count;
	long player_index = local_player_get_player_index(0);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct unit_datum *unit;
	if (!player || player->unit_index == NONE)
	{ error(_error_silent, "photo teleport: player 0 has no unit; refused"); return; }
	if (scenario_leaf_index_from_point(&point) != NONE) target = original;
	for (index = 0; target == NONE && index < count; index++)
	{
		if (index == original) continue;
		if (!scenario_switch_structure_bsp(index)) continue;
		if (scenario_leaf_index_from_point(&point) != NONE) target = index;
	}
	if (target == NONE)
	{
		if (global_structure_bsp_index_get() != original) scenario_switch_structure_bsp(original);
		error(_error_silent, "photo teleport: no structure BSP holds (%.2f,%.2f,%.2f); stayed in BSP %d", point.x, point.y, point.z, original);
		return;
	}
	vector3d_from_euler_angles2d(&forward, &camera->facing);
	forward.k = 0; /* the biped stands upright, facing the camera's heading */
	if (!normalize3d(&forward)) { forward.i = 1; forward.j = 0; }
	if (saved_deathless < 0) saved_deathless = cheat.deathless_player;
	cheat.deathless_player = TRUE; /* a drop from the camera's height must not cost a checkpoint revert */
	if (!player_teleport(player_index, NONE, &point))
	{ error(_error_silent, "photo teleport: player_teleport refused (%.2f,%.2f,%.2f)", point.x, point.y, point.z); }
	if ((unit = unit_try_and_get(player->unit_index)) != NULL)
	{
		unit->unit.desired_facing_vector = forward;
		unit->unit.desired_aiming_vector = forward;
		unit->unit.desired_looking_vector = forward;
	}
	if (player->local_player_index != NONE) player_control_set_facing(player->local_player_index, &forward);
	saved_bsp = target; /* leaving photo mode keeps the player's new BSP */
	if (target != original)
	{
		ai_scripting_erase_all();
		game_time_set_speed(1);
		settle_until = game_time_get() + metadata.settle_ticks;
		report_settle = 1;
	}
	error(_error_silent, "photo teleport: BSP %d -> %d; player 0 at (%.2f,%.2f,%.2f) facing (%.3f,%.3f)%s",
		original, target, point.x, point.y, point.z, forward.i, forward.j, target != original ? "; settling, then frozen" : "");
}

/* Automatic camera candidates are checked with a grid of sight rays across the
frame (cheaper than rendering each one and reading back the depth buffer, and
available on the game thread before anything is drawn). Floors are not counted:
ground in the lower half of the picture is expected. */
#define AUTO_GRID_COLUMNS 9
#define AUTO_GRID_ROWS 7
#define AUTO_MAX_CANDIDATES 16
#define AUTO_FLOOR_NORMAL_Z 0.7f
struct auto_candidate { float position[3], forward[3]; int has_eye; float eye[3]; const char *what; };

static int auto_normalize(float *v)
{
	float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (!(length > 0)) return 0;
	v[0] /= length; v[1] /= length; v[2] /= length;
	return 1;
}

/* Fraction of the frame closer than MAP_PICTURES_NEAR_DISTANCE; 1 when the camera
sits behind a wall as seen from the start it was placed from. */
static float auto_near_fraction(const struct auto_candidate *candidate)
{
	const unsigned long flags = FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
		FLAG(_collision_test_ignore_invisible_surfaces_bit) | FLAG(_collision_test_objects_bit) |
		_collision_test_objects_sight_blocking_flags;
	float distances[AUTO_GRID_COLUMNS * AUTO_GRID_ROWS], f[3], right[3], up[3], h, v;
	struct collision_result collision;
	real_point3d origin;
	real_vector3d ray;
	int x, y, n = 0, i;
	memcpy(f, candidate->forward, sizeof(f));
	if (!auto_normalize(f)) return 1.f;
	if (candidate->has_eye)
	{
		origin.x = candidate->eye[0]; origin.y = candidate->eye[1]; origin.z = candidate->eye[2];
		ray.i = candidate->position[0] - origin.x; ray.j = candidate->position[1] - origin.y; ray.k = candidate->position[2] - origin.z;
		if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit), &origin, &ray, NONE, &collision))
			return 1.f;
	}
	right[0] = f[1]; right[1] = -f[0]; right[2] = 0;
	if (!auto_normalize(right)) { right[0] = 1; right[1] = 0; right[2] = 0; }
	up[0] = right[1] * f[2] - right[2] * f[1]; up[1] = right[2] * f[0] - right[0] * f[2]; up[2] = right[0] * f[1] - right[1] * f[0];
	h = tanf(PHOTO_ENTRY_FOV * 0.5f); v = h * 0.75f;
	origin.x = candidate->position[0]; origin.y = candidate->position[1]; origin.z = candidate->position[2];
	for (y = 0; y < AUTO_GRID_ROWS; y++)
		for (x = 0; x < AUTO_GRID_COLUMNS; x++)
		{
			float sx = (2.f * (x + 0.5f) / AUTO_GRID_COLUMNS - 1.f) * h, sy = (1.f - 2.f * (y + 0.5f) / AUTO_GRID_ROWS) * v, d[3];
			for (i = 0; i < 3; i++) d[i] = f[i] + sx * right[i] + sy * up[i];
			auto_normalize(d);
			ray.i = d[0] * MAP_PICTURES_NEAR_DISTANCE; ray.j = d[1] * MAP_PICTURES_NEAR_DISTANCE; ray.k = d[2] * MAP_PICTURES_NEAR_DISTANCE;
			distances[n] = -1.f;
			if (collision_test_vector(flags, &origin, &ray, NONE, &collision) && !(collision.plane.n.k > AUTO_FLOOR_NORMAL_Z))
			{
				float dx = collision.point.x - origin.x, dy = collision.point.y - origin.y, dz = collision.point.z - origin.z;
				distances[n] = sqrtf(dx * dx + dy * dy + dz * dz);
			}
			n++;
		}
	return map_pictures_near_fraction(distances, n, MAP_PICTURES_NEAR_DISTANCE);
}

static void auto_add(struct auto_candidate *list, int *count, const char *what, const float *position, const float *forward, const float *eye)
{
	struct auto_candidate *c;
	if (*count >= AUTO_MAX_CANDIDATES) return;
	c = &list[(*count)++];
	memset(c, 0, sizeof(*c));
	memcpy(c->position, position, sizeof(c->position));
	memcpy(c->forward, forward, sizeof(c->forward));
	if (eye) { c->has_eye = 1; memcpy(c->eye, eye, sizeof(c->eye)); }
	c->what = what;
}

/* A camera for a map without a saved framing. The first candidate is the original one:
above the centroid of the player starts in the current BSP, looking toward the farthest of
them, or behind and above the first start along its facing when they sit close together
(campaign). Candidates whose frame is mostly near geometry are rejected for the next: the
first start's eye along its facing and turned away from it, a higher view behind it, and
other starts' eyes looking toward the centroid. Pure fallback; a saved framing always wins. */
static int auto_camera(struct flying_camera *camera)
{
	static const float turns[] = {0.f, 0.785398f, -0.785398f, 1.570796f, -1.570796f, 3.141593f};
	static const char *const turn_names[] = {"first start eye", "first start eye +45", "first start eye -45",
		"first start eye +90", "first start eye -90", "first start eye reversed"};
	struct scenario *scenario = global_scenario_try_and_get();
	int bsp = global_structure_bsp_index_get(), i, count = 0, any_in_bsp = 0, far_index = -1, n = 0, chosen, all_rejected = 0;
	int valid[AUTO_MAX_CANDIDATES], valid_count = 0;
	float center[3] = {0, 0, 0}, far_distance = 0, position[3], forward[3], eye[3], c, si;
	float fractions[AUTO_MAX_CANDIDATES];
	struct auto_candidate candidates[AUTO_MAX_CANDIDATES];
	struct photo_shot shot;
	struct player_starting_location *first = NULL;
	const char *map = main_get_map_name();
	if (!scenario || scenario->players.count <= 0) return 0;
	for (i = 0; i < scenario->players.count; i++)
		if (TAG_BLOCK_GET_ELEMENT(&scenario->players, i, struct player_starting_location)->structure_bsp_reference_index == bsp) any_in_bsp = 1;
	for (i = 0; i < scenario->players.count && i < 256; i++)
	{
		struct player_starting_location *start = TAG_BLOCK_GET_ELEMENT(&scenario->players, i, struct player_starting_location);
		if (any_in_bsp && start->structure_bsp_reference_index != bsp) continue;
		if (!__builtin_isfinite(start->position.x) || !__builtin_isfinite(start->position.y) || !__builtin_isfinite(start->position.z)) continue;
		if (!first) first = start;
		center[0] += start->position.x; center[1] += start->position.y; center[2] += start->position.z;
		count++;
	}
	if (!count || !first) return 0;
	for (i = 0; i < 3; i++) center[i] /= (float)count;
	for (i = 0; i < scenario->players.count && i < 256; i++)
	{
		struct player_starting_location *start = TAG_BLOCK_GET_ELEMENT(&scenario->players, i, struct player_starting_location);
		float dx, dy, d;
		if (any_in_bsp && start->structure_bsp_reference_index != bsp) continue;
		if (!__builtin_isfinite(start->position.x) || !__builtin_isfinite(start->position.y)) continue;
		dx = start->position.x - center[0]; dy = start->position.y - center[1]; d = dx * dx + dy * dy;
		if (d > far_distance) { far_distance = d; far_index = i; }
	}
	c = cosf(first->facing); si = sinf(first->facing);
	eye[0] = first->position.x; eye[1] = first->position.y; eye[2] = first->position.z + 0.62f;
	if (count > 1 && far_index >= 0 && far_distance >= 16.f)
	{
		struct player_starting_location *far = TAG_BLOCK_GET_ELEMENT(&scenario->players, far_index, struct player_starting_location);
		position[0] = center[0]; position[1] = center[1]; position[2] = center[2] + 3.f;
		forward[0] = far->position.x - center[0]; forward[1] = far->position.y - center[1]; forward[2] = far->position.z - position[2];
		auto_add(candidates, &n, "above the starts", position, forward, NULL);
	}
	else
	{
		/* Campaign: one start, or starts in a cluster. Stand behind and above the first. */
		position[0] = first->position.x - 2.f * c; position[1] = first->position.y - 2.f * si; position[2] = first->position.z + 1.5f;
		forward[0] = c; forward[1] = si; forward[2] = -0.15f;
		auto_add(candidates, &n, "behind the first start", position, forward, eye);
	}
	for (i = 0; i < (int)(sizeof(turns) / sizeof(turns[0])); i++)
	{
		float tc = cosf(first->facing + turns[i]), ts = sinf(first->facing + turns[i]);
		forward[0] = tc; forward[1] = ts; forward[2] = -0.05f;
		auto_add(candidates, &n, turn_names[i], eye, forward, NULL);
	}
	position[0] = first->position.x - 3.f * c; position[1] = first->position.y - 3.f * si; position[2] = first->position.z + 2.5f;
	forward[0] = c; forward[1] = si; forward[2] = -0.25f;
	auto_add(candidates, &n, "high behind the first start", position, forward, eye);
	for (i = 0; i < scenario->players.count && i < 256 && n < AUTO_MAX_CANDIDATES; i++)
	{
		struct player_starting_location *start = TAG_BLOCK_GET_ELEMENT(&scenario->players, i, struct player_starting_location);
		if (start == first || (any_in_bsp && start->structure_bsp_reference_index != bsp)) continue;
		if (!__builtin_isfinite(start->position.x) || !__builtin_isfinite(start->position.y) || !__builtin_isfinite(start->position.z)) continue;
		position[0] = start->position.x; position[1] = start->position.y; position[2] = start->position.z + 0.62f;
		forward[0] = center[0] - position[0]; forward[1] = center[1] - position[1]; forward[2] = -0.05f;
		if (forward[0] * forward[0] + forward[1] * forward[1] < 1.f) { forward[0] = cosf(start->facing); forward[1] = sinf(start->facing); }
		auto_add(candidates, &n, "another start's eye", position, forward, NULL);
	}
	memset(&shot, 0, sizeof(shot));
	shot.bsp = bsp;
	for (i = 0; i < n; i++)
	{
		float f;
		memcpy(shot.position, candidates[i].position, sizeof(shot.position));
		if (!inside_bsp(&shot) || !auto_normalize(candidates[i].forward))
		{
			error(_error_silent, "photo capture: %s auto candidate %d (%s) outside BSP %d; skipped", map, i, candidates[i].what, bsp);
			continue;
		}
		f = auto_near_fraction(&candidates[i]);
		fractions[valid_count] = f;
		valid[valid_count++] = i;
		error(_error_silent, "photo capture: %s auto candidate %d (%s) at (%.2f,%.2f,%.2f) near %.2f %s", map, i, candidates[i].what,
			candidates[i].position[0], candidates[i].position[1], candidates[i].position[2], f,
			map_pictures_view_blocked(f) ? "rejected" : "accepted");
	}
	chosen = map_pictures_pick_view(fractions, valid_count, &all_rejected);
	if (chosen < 0) return 0;
	if (all_rejected)
		error(_error_silent, "photo capture: %s every auto candidate was blocked; keeping the least blocked, %d (near %.2f)", map, valid[chosen], fractions[chosen]);
	{
		const struct auto_candidate *pick = &candidates[valid[chosen]];
		real_vector3d v;
		v.i = pick->forward[0]; v.j = pick->forward[1]; v.k = pick->forward[2];
		memcpy(&camera->position, pick->position, sizeof(pick->position));
		euler_angles2d_from_vector3d(&camera->facing, &v);
		camera->roll = roll = 0;
		camera->field_of_view = fov = PHOTO_ENTRY_FOV;
		error(_error_silent, "photo capture: automatic camera %d (%s) at (%.2f,%.2f,%.2f) from %d starts in BSP %d", valid[chosen], pick->what,
			pick->position[0], pick->position[1], pick->position[2], count, bsp);
	}
	return 1;
}

/* The last resort for a map whose player starts give no camera (d20's sit outside the
loaded BSP's bounds): the player's own view as the capture begins. */
static int view_camera(struct flying_camera *camera)
{
	const struct observer_result *view = observer_get_camera(0);
	real_point3d point;
	if (!view || !__builtin_isfinite(view->position.x) || !__builtin_isfinite(view->position.y) || !__builtin_isfinite(view->position.z)) return 0;
	if (!__builtin_isfinite(view->forward.i) || !__builtin_isfinite(view->forward.j) || !__builtin_isfinite(view->forward.k)) return 0;
	point = view->position;
	if (scenario_leaf_index_from_point(&point) == NONE) return 0;
	flying_camera_new_from_point_and_vector(camera,&view->position,&view->forward);
	camera->roll = roll = 0;
	camera->field_of_view = fov = PHOTO_ENTRY_FOV;
	error(_error_silent, "photo capture: player view camera at (%.2f,%.2f,%.2f) in BSP %d", view->position.x, view->position.y, view->position.z, global_structure_bsp_index_get());
	return 1;
}

/* A scripted capture starts right after the map loads, while the level's
   startup scripts still run during the settle ticks: they can fade the screen,
   shake the camera, switch the BSP away from the framing's or leave subtitles
   and HUD text up. Undo all of it
   on every update so the framing's camera is what gets rendered. */
static void clear_level_start_effects(void)
{
	real_rgb_color color; short ticks; boolean fading_out; long start_time;
	player_effect_port_screen_fade_get(&color,&ticks,&fading_out,&start_time);
	if (ticks != NONE)
	{
		if (!(capture_cleared & 1)) error(_error_silent, "photo capture: cleared a scripted screen fade (%s, %d ticks)", fading_out ? "out" : "in", (int)ticks);
		capture_cleared |= 1;
		player_effect_port_screen_fade_set(&color,NONE,FALSE,start_time);
	}
	if (player_effect_port_scripted_active())
	{
		if (!(capture_cleared & 2)) error(_error_silent, "photo capture: ended a scripted camera effect");
		capture_cleared |= 2;
		player_effect_port_scripted_end();
	}
	/* Cutscene subtitles and chapter titles age only in cinematic_render,
	   which the undrawn fast-forward skips, so they never expire on their own. */
	if (cinematic_globals)
	{
		boolean titles = FALSE; int i;
		for (i = 0; i < 4; i++)
		{
			if (cinematic_globals->queued_titles[i].title_index != NONE) titles = TRUE;
			cinematic_globals->queued_titles[i].title_index = NONE;
			cinematic_globals->queued_titles[i].time = NONE;
		}
		if (titles && !(capture_cleared & 4)) error(_error_silent, "photo capture: cleared cinematic titles (subtitles)");
		if (titles) capture_cleared |= 4;
	}
	if (hud_messaging_port_clear_text())
	{
		if (!(capture_cleared & 8)) error(_error_silent, "photo capture: cleared HUD messages and objective text");
		capture_cleared |= 8;
	}
	if (capture_bsp >= 0 && capture_bsp != global_structure_bsp_index_get())
	{
		error(_error_silent, "photo capture: a script switched to BSP %d; back to BSP %d", global_structure_bsp_index_get(), capture_bsp);
		if (!scenario_switch_structure_bsp((short)capture_bsp)) capture_bsp = -1;
	}
}

void photo_mode_capture_begin(void)
{
	photo_mode_end();
	scripted = 1; capture_failed = 0; capture_framed = 0; capture_revision = 0; capture_hide = 0;
	pending = PHOTO_TOGGLE;
}

int photo_mode_capture_state(int *framed, int *revision)
{
	if (capture_failed) return -1;
	if (!scripted || !active || pending || capture_hide || game_time_get() < settle_until || game_time_get_speed() != 0) return 0;
	if (framed) *framed = capture_framed;
	if (revision) *revision = capture_revision;
	return 1;
}

void photo_mode_update(struct director *director, boolean *controls_active, float seconds, float facing[3], float translation[3])
{
	struct flying_camera *camera = (struct flying_camera *)director->camera_data;
	int action = pending;
	pending = 0;
	if (!active && !action) return;
	if ((!host_photo_mode_available() && !(scripted && host_map_pictures_import_stage())) ||
		(!scripted && !config_boolean("debug.photo_mode")))
	{
		photo_mode_end();
		return;
	}
	if (scripted && action && action != PHOTO_TOGGLE) action = 0; /* keys do nothing during a capture */
	if (action == PHOTO_TOGGLE)
	{
		if (active) { photo_mode_end(); return; }
		if (!game_map_loaded() || !global_scenario || global_scenario->type == _scenario_type_main_menu || (game_connection() != _game_connection_local && game_connection() != _game_connection_network_server))
		{ error(_error_silent, "photo mode: host a map first"); if (scripted) capture_failed = 1; return; }
		{
			const struct observer_result *view = observer_get_camera(0);
			saved_director = *director; current_director = director;
			saved_cinematic = *cinematic_globals; saved_hud = *hud_scripted_globals;
			saved_input = player_input_enabled(); saved_scripted = *director_camera_scripted;
			saved_speed = game_time_get_speed();
			saved_bsp = global_structure_bsp_index_get();
			flying_camera_new_from_point_and_vector(camera,&view->position,&view->forward);
			camera->field_of_view = fov = PHOTO_ENTRY_FOV;
		}
		memset(&metadata,0,sizeof(metadata));
		metadata.focus[0] = metadata.focus[1] = 0.5f;
		metadata.safe[0] = 0.19f; metadata.safe[2] = 0.81f; metadata.safe[3] = 1;
		metadata.settle_ticks = 30; metadata.erase_ai = 1;
		/* Record the active variant rather than changing the map's game rules. */
		{ static const char *names[] = {"none","ctf","slayer","oddball","king","race"};
		  long index = game_engine_get_variant()->game_engine_index;
		  snprintf(metadata.variant,sizeof(metadata.variant),"%s",index >= 0 && index < 6 ? names[index] : "slayer"); }
		active = 1; speed_step = 0; roll = 0;
		saved_dont_timeout = global_connection_dont_timeout; global_connection_dont_timeout = TRUE;
		cinematic_start();
		*director_camera_scripted = FALSE;
		scripted_show_hud(FALSE); scripted_show_hud_help_text(FALSE); player_input_enable(FALSE);
		cinematic_show_letterbox(FALSE); cinematic_globals->letterbox_amount = 0;
		ai_scripting_erase_all();
		game_time_set_speed(1);
		settle_until = game_time_get() + metadata.settle_ticks;
		error(_error_silent, "photo mode: entered; AI erased; reload the map to play");
		if (scripted)
		{
			if (file_action(PHOTO_LOAD,camera) && last_from_fragment) { capture_framed = 1; capture_revision = last_revision; }
			else if (!auto_camera(camera) && !view_camera(camera)) { error(_error_silent, "photo capture: no framing, no usable player start and no player view"); capture_failed = 1; }
			capture_bsp = global_structure_bsp_index_get(); capture_cleared = 0;
			capture_hide = !capture_failed;
		}
		if (global_scenario->type == _scenario_type_multiplayer)
		{
			const struct game_variant_options *o = game_variant_options_get();
			const struct universal_variant *u = &game_engine_get_variant()->universal_variant;
			error(_error_silent, "photo mode: engine %ld teams %d universal vehicle set %d; sets %d/%d; counts %d %d %d %d %d %d / %d %d %d %d %d %d; %ld vehicles live",
				(long)game_engine_get_variant()->game_engine_index, (int)u->teams, (int)u->vehicle_set, (int)o->vehicle_set[0], (int)o->vehicle_set[1],
				(int)o->vehicle_counts[0][0], (int)o->vehicle_counts[0][1], (int)o->vehicle_counts[0][2], (int)o->vehicle_counts[0][3], (int)o->vehicle_counts[0][4], (int)o->vehicle_counts[0][5],
				(int)o->vehicle_counts[1][0], (int)o->vehicle_counts[1][1], (int)o->vehicle_counts[1][2], (int)o->vehicle_counts[1][3], (int)o->vehicle_counts[1][4], (int)o->vehicle_counts[1][5],
				vehicles_live_count());
		}
	}
	if (!active) return;
	/* Scripted cutscenes cannot take the camera back while framing. */
	*director_camera_scripted = FALSE;
	director->camera_mode_index = _camera_flying;
	director->camera_proc = (director_camera_update_proc)flying_camera_update;
	director->perspective = _director_perspective_neutral;
	director->camera_change_pause = 0;
	player_input_enable(FALSE);
	cinematic_globals->show_letterbox = FALSE; cinematic_globals->letterbox_amount = 0;
	if (scripted) clear_level_start_effects();
	if (game_time_get() >= settle_until)
	{
		game_time_set_speed(0);
		if (report_settle)
		{
			report_settle = 0;
			error(_error_silent, "photo teleport: settled and frozen in BSP %d", global_structure_bsp_index_get());
		}
	}
	if (action == PHOTO_GUIDES) guides = !guides;
	if (action == PHOTO_SLOWER && speed_step > -4) speed_step--;
	if (action == PHOTO_FASTER && speed_step < 4) speed_step++;
	if (action == PHOTO_FOV_DOWN || action == PHOTO_FOV_UP) fov = photo_fov_step(fov,action == PHOTO_FOV_UP ? 1 : -1);
	if (action == PHOTO_ROLL_RESET) roll = 0;
	roll = photo_roll_step(roll,(roll_right ? 1 : 0) - (roll_left ? 1 : 0),seconds,speed_step);
	/* fly() zeroes the game's own roll delta; photo mode owns the roll. */
	camera->field_of_view = fov; camera->roll = roll;
	if (scripted && !capture_failed && game_time_get() >= settle_until && game_time_get_speed() == 0)
	{
		/* The framing does not record where the player stood, so at capture he stands at his
		start, often in frame. Every frame of the frozen capture, each local player's unit and
		everything attached to it is hidden, the way the game hides a dead player's body. The
		map is unloaded after the capture, so nothing is shown again. */
		hide_local_players();
		capture_hide = 0;
	}
	if (scripted)
	{
		/* Hold the camera still: no stick, keyboard or mouse input reaches it. */
		facing[0] = facing[1] = facing[2] = 0;
		translation[0] = translation[1] = translation[2] = 0;
		*controls_active = TRUE;
		return;
	}
	fly(controls_active,seconds,facing,translation,camera);
	if (action == PHOTO_SAVE || action == PHOTO_LOAD || action == PHOTO_LOAD_CANDIDATE) file_action(action,camera);
	if (action == PHOTO_TELEPORT) teleport_player(camera);
	if (action == PHOTO_RENDER_CHECK) error(_error_silent, "photo mode: render check not implemented (phase 5)");
	host_photo_mode_overlay(fov,speed_step,guides,metadata.focus[0],metadata.focus[1],roll * 180.f / 3.14159265f);
}
