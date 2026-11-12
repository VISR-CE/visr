/* Production menu persistence with config, profile, and host I/O replaced. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#define HALO_IOS 1
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define _stricmp strcasecmp
typedef int boolean;
struct widget_instance
{
	struct widget_instance *parent;
	char const *name;
	struct { struct { short selected_index; } list; } parameters;
};
struct pc_menu_setting
{
	char const *setting;
	short value_count;
	char const *values[3];
	short loaded_index;
};
struct player_profile { int name, color; struct { int sensitivity; } controller_settings; wchar_t player_name[12]; };
#define primary_color_index color
static struct { short color; } color_list;
static char const *menu_style = "xbox";
static int menu_pad = 1;
static int platform_menu_pad(void) { return menu_pad; }
static char const *config_string(char const *name);
static struct player_profile disk_profile = { 1, 2, { 3 }, L"Player" }, edit_profile;
static long edit_index = 7;
static int writes, applies, saves, asks, messages, reloads, capture_result, write_failure;
static int online, running, cloud;
static char binding_values[17][128];
static struct widget_instance *screen_of(struct widget_instance *w) { while (w->parent) w = w->parent; return w; }
static void platform_log(char const *format, ...) { (void)format; }
static void platform_show_message(char const *title, char const *text) { messages++; }
static void platform_display_apply(void) { applies++; }
static int p2p_running(void) { return running; }
static void platform_internet_play_request(void) { asks++; }
static void xnet_start_internet_play(void) { running = 1; }
static boolean setting_write(char const *name, char const *value)
{
	if (write_failure) return FALSE;
	writes++;
	if (!strcmp(name, "profile.look_sensitivity")) edit_profile.controller_settings.sensitivity = atoi(value);
	if (!strcmp(name, "network.online")) online = !strcmp(value, "true");
	if (!strcmp(name, "host.cloud_saves")) cloud = !strcmp(value, "true");
	return TRUE;
}
static boolean setting_load(struct widget_instance *spinner) { reloads++; spinner->parameters.list.selected_index = cloud; return TRUE; }
static boolean setting_text(char const *name, char *text, unsigned int size, boolean defaults)
{ snprintf(text, size, "%s", "false"); return TRUE; }
static short setting_value_index(struct pc_menu_setting *setting, char const *text) { return 0; }
static struct player_profile *player_ui_get_edit_player_profile(void) { return edit_index == NONE ? NULL : &edit_profile; }
static long player_ui_get_edit_profile_index(void) { return edit_index; }
static boolean player_ui_edit_profile_is_dirty(void) { return memcmp(&disk_profile, &edit_profile, sizeof(edit_profile)) != 0; }
static boolean player_ui_save_profile(void) { disk_profile = edit_profile; edit_index = NONE; saves++; return TRUE; }
static void player_ui_begin_editing_profile(long index) { edit_index = index; edit_profile = disk_profile; }
/* SETTINGS */
static int binding_index(char const *name);
static char const *config_string(char const *name) { return !strcmp(name, "display.menus") ? menu_style : binding_values[binding_index(name)]; }
static int config_default(char const *name, char *text, unsigned int size) { snprintf(text, size, "D%d", binding_index(name)); return TRUE; }
static int config_write(char const *name, char const *text) { snprintf(binding_values[binding_index(name)], 128, "%s", text); writes++; return TRUE; }
static int platform_binding_capture_poll(int *input) { *input = 5; return capture_result; }
static void halo_input_name(int input, char *text, unsigned int size) { snprintf(text, size, "K"); }
/* CONTROLS */
static int binding_index(char const *name)
{
	int i;
	for (i = 0; i < NUMBEROF(controls); i++) if (!strcmp(name, controls[i].setting)) return i;
	abort();
}
int main(void)
{
	struct widget_instance screen = { NULL, "audio_settings_screen", { { 0 } } };
	struct widget_instance spinner = { &screen, "spinner", { { 1 } } };
	struct pc_menu_setting setting = { "audio.master", 2, { "false", "true" }, 0 };
	assert(setting_apply_now(&spinner, &setting));
	assert(writes == 1 && applies == 1 && setting.loaded_index == 1);
	assert(setting_apply_now(&spinner, &setting) && writes == 1 && applies == 1);
	screen.name = "video_settings_screen"; setting.loaded_index = 0;
	assert(setting_apply_now(&spinner, &setting) && writes == 2 && applies == 1);
	setting.setting = "network.online"; setting.loaded_index = 0;
	assert(setting_apply_now(&spinner, &setting));
	assert(asks == 1 && !online && spinner.parameters.list.selected_index == 0 && writes == 2);
	assert(setting_apply_now(&spinner, &setting) && asks == 1);
	running = online = 1; setting.loaded_index = 1;
	assert(setting_apply_now(&spinner, &setting) && !online && messages == 1);
	setting.setting = "host.cloud_saves"; spinner.parameters.list.selected_index = 1; setting.loaded_index = 0;
	assert(setting_apply_now(&spinner, &setting) && reloads == 1 && cloud);
	setting.setting = "profile.look_sensitivity"; setting.loaded_index = 0;
	setting.values[1] = "5"; edit_profile = disk_profile; edit_profile.name = 99; edit_profile.color = 88;
	assert(setting_apply_now(&spinner, &setting));
	assert(saves == 1 && disk_profile.controller_settings.sensitivity == 5);
	assert(disk_profile.name == 99 && disk_profile.color == 88);
	assert(edit_index == 7 && edit_profile.name == 99 && edit_profile.color == 88);
	assert(setting_apply_now(&spinner, &setting) && saves == 1);
	/* A confirmed name saves the entire edit, then another edit remains possible. */
	wcscpy(edit_profile.player_name, L"Renamed");
	pc_menu_profile_name_saved(edit_profile.player_name);
	assert(saves == 2 && !wcscmp(disk_profile.player_name, L"Renamed") && edit_index == 7);
	pc_menu_profile_name_saved(edit_profile.player_name);
	assert(saves == 2);
	edit_profile.name = 123;
	pc_menu_profile_name_saved(L"Other dialog");
	assert(saves == 2 && disk_profile.name == 99);
	color_list.color = 4;
	assert(color_choose() && saves == 3 && disk_profile.color == 4 && disk_profile.name == 123);
	edit_profile.name = 456;
	assert(pc_menu_profile_save_on_back() && saves == 4 && disk_profile.name == 456);
	assert(pc_menu_profile_save_on_back() && saves == 4);
	/* PC menus retain explicit save, and a gametype is never treated as a player. */
	menu_style = "pc"; edit_profile.name = 789;
	pc_menu_profile_name_saved(edit_profile.player_name);
	assert(pc_menu_profile_save_on_back() && saves == 4 && disk_profile.name == 456);
	menu_style = "auto"; menu_pad = 0;
	assert(pc_menu_profile_save_on_back() && saves == 4);
	menu_pad = 1;
	assert(pc_menu_profile_save_on_back() && saves == 5 && disk_profile.name == 789);
	edit_index = NONE;
	pc_menu_profile_name_saved(L"Gametype");
	assert(pc_menu_profile_save_on_back() && saves == 5);
	player_ui_begin_editing_profile(7);
	setting.setting = "audio.master"; setting.loaded_index = 0; write_failure = 1;
	assert(!setting_apply_now(&spinner, &setting) && setting.loaded_index == 0);
	write_failure = 0;
	assert(setting_default_show(&spinner, &setting) && spinner.parameters.list.selected_index == 0);
	setting.loaded_index = 1;
	assert(setting_apply_now(&spinner, &setting) && setting.loaded_index == 0);

	controls_load(FALSE); controls_screen.apply_now = TRUE;
	strcpy(controls_screen.bindings[1][0], "K");
	strcpy(binding_values[1], "K");
	controls_screen.capturing_control = 0; controls_screen.capturing_slot = 0; capture_result = 1;
	control_capture_poll();
	assert(!strcmp(binding_values[0], "K") && !*binding_values[1]);
	controls_screen.capturing_control = 0; capture_result = 2; control_capture_poll();
	assert(!*binding_values[0]);
	int before = writes;
	controls_screen.capturing_control = 0; capture_result = 3; control_capture_poll();
	assert(writes == before);
	controls_load(TRUE); controls_save(); assert(!strcmp(binding_values[0], "D0"));
	controls_screen.apply_now = FALSE; controls_screen.capturing_control = 0; capture_result = 1;
	control_capture_poll(); assert(!strcmp(binding_values[0], "D0"));
	puts("apply-now persistence passed");
	return 0;
}
