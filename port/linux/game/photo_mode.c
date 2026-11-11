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
#include "networking/network_connection.h"
#include "interface/hud.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "main/main.h"
#include "ai/ai_script.h"
#include "custom_edition_cache.h"
#include "map_share.h"
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
const char *platform_data_root(void);
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
#ifdef HALO_ILP32
int host_photo_mode_available(void);
void host_photo_mode_overlay(float fov, int speed, int guides, float focus_x, float focus_y, float roll_degrees);
#else
static int host_photo_mode_available(void) { return 0; }
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
static int saved_input, saved_scripted, saved_bsp, saved_dont_timeout, saved_deathless = -1, report_settle;
static float saved_speed, fov;
static struct photo_shot metadata;

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
	photo_roll_up(shot->forward,camera->roll,shot->up);
	shot->fov = camera->field_of_view;
	shot->bsp = global_structure_bsp_index_get();
}

static void file_action(int action, struct flying_camera *camera)
{
	char key[256], folder[768], path[1024], camera_path[1024];
	struct photo_shot shot;
	int revision;
	if (!map_key(key,sizeof(key))) return;
	char host_path[1024], host_camera_path[1024];
	/* the game's CRT maps d:\ to platform_data_root(); a host path here would be prefixed with that root again */
	if (snprintf(folder,sizeof(folder),"d:\\framings") >= (int)sizeof(folder) ||
		snprintf(path,sizeof(path),"%s\\%s.json",folder,key) >= (int)sizeof(path) ||
		snprintf(camera_path,sizeof(camera_path),"d:\\camera.txt") >= (int)sizeof(camera_path)) return;
	platform_translate_path(path,host_path,sizeof(host_path));
	platform_translate_path(camera_path,host_camera_path,sizeof(host_camera_path));
	camera_shot(camera,&shot);
	if (action == PHOTO_SAVE)
	{
		if (!inside_bsp(&shot)) { error(_error_silent, "photo mode: camera is outside the current BSP bounds; save refused"); return; }
		_mkdir(folder);
		if (!photo_fragment_write(path,key,&shot)) { error(_error_silent, "photo mode: fragment save failed: %s", host_path); return; }
		director_save_camera();
		FILE *written = fopen(camera_path,"r");
		error(_error_silent, "photo mode: saved framing %s; camera.txt %s %s", host_path, written ? "written" : "MISSING", host_camera_path);
		if (written) fclose(written);
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
			error(_error_silent, "photo mode: invalid framing or BSP index; load refused (%s)", exists ? host_path : host_camera_path); return;
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
		camera->roll = roll = photo_roll_from_up(shot.forward,shot.up);
		camera->field_of_view = fov = shot.fov;
		metadata = shot;
		/* AI was already erased on entry; retain an honest recipe on the next save. */
		metadata.erase_ai = 1;
		error(_error_silent, "photo mode: loaded first shot (roll %.2f degrees)", roll * 180.f / 3.14159265f);
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

void photo_mode_update(struct director *director, boolean *controls_active, float seconds, float facing[3], float translation[3])
{
	struct flying_camera *camera = (struct flying_camera *)director->camera_data;
	int action = pending;
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
	fly(controls_active,seconds,facing,translation,camera);
	if (action == PHOTO_SAVE || action == PHOTO_LOAD) file_action(action,camera);
	if (action == PHOTO_TELEPORT) teleport_player(camera);
	if (action == PHOTO_RENDER_CHECK) error(_error_silent, "photo mode: render check not implemented (phase 5)");
	host_photo_mode_overlay(fov,speed_step,guides,metadata.focus[0],metadata.focus[1],roll * 180.f / 3.14159265f);
}
