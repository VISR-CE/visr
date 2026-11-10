/* Developer framing controls; scene and camera ownership stays on the game thread. */
#include "cseries/cseries.h"
#include "photo_mode.h"
#include "photo_framing.h"
#include "camera/director.h"
#include "camera/flying_camera.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "main/main.h"
#include "ai/ai_script.h"
#include "custom_edition_cache.h"
#include "map_share.h"
#include "../src/port_config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

void platform_log(const char *format, ...);
const char *platform_data_root(void);
#ifdef HALO_ILP32
int host_photo_mode_available(void);
void host_photo_mode_overlay(float fov, int speed, int guides, float focus_x, float focus_y);
#else
static int host_photo_mode_available(void) { return 0; }
static void host_photo_mode_overlay(float fov, int speed, int guides, float focus_x, float focus_y) { (void)fov; (void)speed; (void)guides; (void)focus_x; (void)focus_y; }
#endif

static int active, pending, guides = 1, speed_step;
static long settle_until;
static struct director saved_director, *current_director;
static struct cinematic_global_data saved_cinematic;
static struct hud_scripted_globals_definition saved_hud;
static int saved_input, saved_scripted, saved_bsp;
static float saved_speed, fov;
static struct photo_shot metadata;

int photo_mode_active(void) { return active; }

void photo_mode_trace_key(int scancode, int action)
{
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
	if (!active && (!game_map_loaded() || !global_scenario ||
		global_scenario->type == _scenario_type_main_menu || (game_connection() != _game_connection_local && game_connection() != _game_connection_network_server)))
	{
		error(_error_silent, "photo mode: key ignored (no map is running on this machine as the host)");
		return 0;
	}
	pending = action;
	return 1;
}

void photo_mode_end(void)
{
	pending = 0;
	if (!active) return;
	active = 0;
	if (saved_bsp != global_structure_bsp_index_get()) scenario_switch_structure_bsp((short)saved_bsp);
	game_time_set_speed(saved_speed);
	if (!saved_cinematic.in_progress) cinematic_stop();
	*cinematic_globals = saved_cinematic;
	*hud_scripted_globals = saved_hud;
	player_input_enable(saved_input);
	*director_camera_scripted = saved_scripted;
	if (current_director) *current_director = saved_director;
	current_director = NULL;
	host_photo_mode_overlay(0,0,0,0.5f,0.5f);
	error(_error_silent, "photo mode: exited; reload the map to play (AI and projectiles cannot be restored)");
}

static int map_key(char *key, size_t size)
{
	const char *name = global_scenario->type == _scenario_type_multiplayer ? main_get_multiplayer_map_name() : main_get_map_name();
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
	for (i=0;i<3;i++) if (shot->position[i] < bsp->world_bounds.n[i*2] || shot->position[i] > bsp->world_bounds.n[i*2+1]) return 0;
	return 1;
}

static void camera_shot(struct flying_camera *camera, struct photo_shot *shot)
{
	real_vector3d forward;
	*shot = metadata;
	memcpy(shot->position,&camera->position,sizeof(shot->position));
	vector3d_from_euler_angles2d(&forward,&camera->facing);
	memcpy(shot->forward,&forward,sizeof(shot->forward));
	photo_level_up(shot->forward,shot->up);
	shot->fov = camera->field_of_view;
	shot->bsp = global_structure_bsp_index_get();
}

static void file_action(int action, struct flying_camera *camera)
{
	char key[256], folder[768], path[1024], camera_path[1024];
	struct photo_shot shot;
	int revision;
	if (!map_key(key,sizeof(key))) return;
	if (snprintf(folder,sizeof(folder),"%s/framings",platform_data_root()) >= (int)sizeof(folder) ||
		snprintf(path,sizeof(path),"%s/%s.json",folder,key) >= (int)sizeof(path) ||
		snprintf(camera_path,sizeof(camera_path),"%s/camera.txt",platform_data_root()) >= (int)sizeof(camera_path)) return;
	camera_shot(camera,&shot);
	if (action == PHOTO_SAVE)
	{
		if (!inside_bsp(&shot)) { error(_error_silent, "photo mode: camera is outside the current BSP bounds; save refused"); return; }
		_mkdir(folder);
		if (!photo_fragment_write(path,key,&shot)) { error(_error_silent, "photo mode: fragment save failed"); return; }
		director_save_camera();
		error(_error_silent, "photo mode: saved framing and camera.txt");
	}
	else
	{
		FILE *file = fopen(path,"r");
		int exists = file != NULL;
		if (file) fclose(file);
		int previous_bsp = global_structure_bsp_index_get();
		if (!(exists ? photo_fragment_read(path,key,&shot,&revision) : photo_camera_read(camera_path,&shot)) ||
			shot.bsp >= global_scenario->structure_bsp_references.count || shot.bsp > 32767)
		{
			error(_error_silent, "photo mode: invalid framing or BSP index; load refused"); return;
		}
		if (shot.bsp != previous_bsp && !scenario_switch_structure_bsp((short)shot.bsp))
		{ error(_error_silent, "photo mode: BSP switch failed; load refused"); return; }
		if (!inside_bsp(&shot))
		{
			if (previous_bsp != global_structure_bsp_index_get()) scenario_switch_structure_bsp((short)previous_bsp);
			error(_error_silent, "photo mode: position outside BSP bounds; load refused"); return;
		}
		if (previous_bsp != shot.bsp)
		{
			ai_scripting_erase_all();
			game_time_set_speed(1);
			settle_until = game_time_get() + shot.settle_ticks;
		}
		memcpy(&camera->position,shot.position,sizeof(shot.position));
		{ real_vector3d forward; memcpy(&forward,shot.forward,sizeof(shot.forward)); euler_angles2d_from_vector3d(&camera->facing,&forward); }
		camera->roll = 0;
		camera->field_of_view = fov = shot.fov;
		metadata = shot;
		/* AI was already erased on entry; retain an honest recipe on the next save. */
		metadata.erase_ai = 1;
		error(_error_silent, "photo mode: loaded first shot (roll lock on)");
	}
}

void photo_mode_update(struct director *director, float *translation, float *roll_delta)
{
	struct flying_camera *camera = (struct flying_camera *)director->camera_data;
	int action = pending, i;
	pending = 0;
	if (!active && !action) return;
	if (!host_photo_mode_available() || !config_boolean("debug.photo_mode"))
	{
		photo_mode_end();
		return;
	}
	if (action == PHOTO_TOGGLE)
	{
		if (active) { photo_mode_end(); return; }
		if (!game_map_loaded() || !global_scenario || global_scenario->type == _scenario_type_main_menu || (game_connection() != _game_connection_local && game_connection() != _game_connection_network_server))
		{ error(_error_silent, "photo mode: host a map first"); return; }
		{
			const struct observer_result *view = observer_get_camera(0);
			saved_director = *director; current_director = director;
			saved_cinematic = *cinematic_globals; saved_hud = *hud_scripted_globals;
			saved_input = player_input_enabled(); saved_scripted = *director_camera_scripted;
			saved_speed = game_time_get_speed();
			saved_bsp = global_structure_bsp_index_get();
			flying_camera_new_from_point_and_vector(camera,&view->position,&view->forward);
			camera->field_of_view = fov = photo_fov_step(view->field_of_view,0);
		}
		memset(&metadata,0,sizeof(metadata));
		metadata.focus[0] = metadata.focus[1] = 0.5f;
		metadata.safe[0] = 0.19f; metadata.safe[2] = 0.81f; metadata.safe[3] = 1;
		metadata.settle_ticks = 30; metadata.erase_ai = 1;
		/* Record the active variant rather than changing the map's game rules. */
		{ static const char *names[] = {"none","ctf","slayer","oddball","king","race"};
		  long index = game_engine_get_variant()->game_engine_index;
		  snprintf(metadata.variant,sizeof(metadata.variant),"%s",index >= 0 && index < 6 ? names[index] : "slayer"); }
		active = 1; speed_step = 0;
		cinematic_start();
		*director_camera_scripted = FALSE;
		scripted_show_hud(FALSE); scripted_show_hud_help_text(FALSE); player_input_enable(FALSE);
		cinematic_show_letterbox(FALSE); cinematic_globals->letterbox_amount = 0;
		ai_scripting_erase_all();
		game_time_set_speed(1);
		settle_until = game_time_get() + metadata.settle_ticks;
		error(_error_silent, "photo mode: entered; AI erased; reload the map to play");
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
	if (game_time_get() >= settle_until) game_time_set_speed(0);
	if (action == PHOTO_GUIDES) guides = !guides;
	if (action == PHOTO_SLOWER && speed_step > -4) speed_step--;
	if (action == PHOTO_FASTER && speed_step < 4) speed_step++;
	if (action == PHOTO_FOV_DOWN || action == PHOTO_FOV_UP) fov = photo_fov_step(fov,action == PHOTO_FOV_UP ? 1 : -1);
	camera->field_of_view = fov; camera->roll = 0; *roll_delta = 0;
	for (i=0;i<3;i++) translation[i] *= powf(2.f,(float)speed_step);
	if (action == PHOTO_SAVE || action == PHOTO_LOAD) file_action(action,camera);
	if (action == PHOTO_RENDER_CHECK) error(_error_silent, "photo mode: render check not implemented (phase 5)");
	host_photo_mode_overlay(fov,speed_step,guides,metadata.focus[0],metadata.focus[1]);
}
