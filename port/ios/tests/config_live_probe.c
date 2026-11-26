/* config_set_live against the real settings resolver (built by tools/test_console_config.py). The class it guards:
 * a live set that reports success without changing the value in force. Each case sets a value through the one
 * live entry point and checks what the game's own readers return afterward, never only the reply. */
#define main config_origins_main
#include "config_origins_probe.c"
#undef main

static char hooked[256];
static int hook_calls, hook_checks;
/* a setting the probe's renderer refuses, as a benchmark's override makes the real one */
static const char *hook_refuses = "";

static int probe_hook(const char *name, const char *value, int apply, char *why, size_t why_size)
{
	if (!apply)
	{
		hook_checks++;
		if (!strcmp(name, hook_refuses))
		{
			snprintf(why, why_size, "a benchmark sets it");
			return 0;
		}
		return 1;
	}
	snprintf(hooked, sizeof(hooked), "%s=%s", name, value);
	hook_calls++;
	return 1;
}

static int live(const char *name, const char *value, char *note, size_t size)
{
	return config_set_live(name, value, note, size);
}

/* display.graphics is its own layer: the set goes through config_graphics_try and reads back as the mode in force */
static void check_graphics(void)
{
	char note[200];
	unsigned long before;

	fresh();
	launch("");
	assert(config_graphics_mode() == GRAPHICS_MODE_ENHANCED);
	before = config_changes();
	assert(live("display.graphics", "classic", note, sizeof(note)));
	assert(!strcmp(note, "display.graphics = classic"));
	assert(config_graphics_mode() == GRAPHICS_MODE_CLASSIC);
	assert(!strcmp(config_string("display.graphics"), "classic"));
	assert(config_changes() != before);
	assert(!live("display.graphics", "ultra", note, sizeof(note)));
	assert(strstr(note, "is not one of"));
	assert(config_graphics_mode() == GRAPHICS_MODE_CLASSIC);
	/* (head-tracked stereo draws Hardcore as Classic: refused before the mode changes, from any mode) */
	assert(live("display.graphics", "enhanced", note, sizeof(note)));
	config_graphics_head_stereo(1);
	assert(!live("display.graphics", "hardcore", note, sizeof(note)));
	assert(!strcmp(note, "display.graphics stays enhanced: hardcore draws as classic here"));
	assert(config_graphics_mode() == GRAPHICS_MODE_ENHANCED);
	config_graphics_head_stereo(0);
	assert(config_graphics_mode() == GRAPHICS_MODE_ENHANCED && !strcmp(config_string("display.graphics"), "enhanced"));
	/* (for this run only: nothing written) */
	assert(!file_has(path, "graphics = \"classic\""));
}

/* a setting the graphics mode replaces keeps the mode's value: the set reports that, and fails */
static void check_mode_replaced(void)
{
	char note[200];

	fresh();
	launch("");
	assert(config_graphics_try("hardcore"));
	assert(!config_boolean("display.soft_particles"));
	hook_calls = 0;
	assert(!live("display.soft_particles", "true", note, sizeof(note)));
	assert(!strcmp(note, "display.soft_particles stays false: hardcore graphics replaces it"));
	assert(!config_boolean("display.soft_particles"));
	/* (a refusal changes nothing: the renderer is not told, and the stored value does not wait for the mode to go) */
	assert(hook_calls == 0);
	assert(config_graphics_try("enhanced"));
	assert(config_boolean("display.soft_particles"));
	{
		long enhanced = config_integer("display.shadow_map_size"), wanted = enhanced == 256 ? 512 : 256;
		char text[16];

		snprintf(text, sizeof(text), "%ld", wanted);
		assert(config_graphics_try("hardcore"));
		assert(config_graphics_replaces("display.shadow_map_size"));
		assert(!live("display.shadow_map_size", text, note, sizeof(note)) && strstr(note, "hardcore graphics replaces it"));
		assert(config_graphics_try("enhanced"));
		assert(config_integer("display.shadow_map_size") == enhanced);
	}
	assert(hook_calls == 0);
}

/* a profile's variable-workload value would beat a bare stored value; a live set wins over it, as a player's does */
static void check_profile_shadowed(void)
{
	char note[200], text[32];

	fresh();
	launch(TV_MODEL "display.shadow_map_size\t256\tmodel Apple TV HD\tvariable\n"
		"display.anti_aliasing\tfxaa\tmodel Apple TV HD\tvariable\n");
	config_workload_set(1, "multiplayer map");
	assert(config_scene_integer("display.shadow_map_size", CONFIG_SCENE_GAMEPLAY) == 256);
	hook_calls = 0;
	assert(live("display.shadow_map_size", "512", note, sizeof(note)));
	assert(!strcmp(note, "display.shadow_map_size = 512"));
	assert(config_scene_integer("display.shadow_map_size", CONFIG_SCENE_GAMEPLAY) == 512);
	assert(hook_calls == 1 && !strcmp(hooked, "display.shadow_map_size=512"));
	assert(live("display.anti_aliasing", "smaa", note, sizeof(note)));
	/* (0: the stored value, not the profile's variable one) */
	assert(!config_scene_string("display.anti_aliasing", CONFIG_SCENE_GAMEPLAY, text, sizeof(text)));
	assert(!strcmp(text, "smaa"));
	assert(!file_has(path, "shadow_map_size = 512") && !file_has(path, "anti_aliasing = \"smaa\""));
	config_workload_set(0, "campaign");
}

/* values that are not the setting's type or choices, unknown and launch-only settings: refused, nothing changes */
static void check_refusals(void)
{
	char note[200];
	unsigned long before;

	fresh();
	launch("");
	before = config_changes();
	hook_calls = 0;
	assert(!live("display.shadow_map_size", "big", note, sizeof(note)) && strstr(note, "not a whole number"));
	assert(!live("display.shadow_map_size", "256x", note, sizeof(note)));
	assert(!live("display.anti_aliasing", "smaaa", note, sizeof(note)) && strstr(note, "is not one of"));
	assert(!live("display.soft_particles", "maybe", note, sizeof(note)) && strstr(note, "not true or false"));
	assert(!live("display.render_quality", "fast", note, sizeof(note)) && strstr(note, "not a number"));
	assert(!live("display.no_such_setting", "1", note, sizeof(note)) && !strcmp(note, "no setting display.no_such_setting"));
	assert(!live("display.hud_scale", "1.5", note, sizeof(note)) && strstr(note, "read at start"));
	assert(!config_setting_is_live("display.hud_scale") && config_setting_is_live("display.shadow_map_size"));
	assert(!live("display.shadow_map_size", "-7", note, sizeof(note)) && strstr(note, "outside 128 to 1024"));
	assert(!live("display.shadow_map_size", "4096", note, sizeof(note)) && strstr(note, "outside"));
	assert(!live("display.render_quality", "9", note, sizeof(note)) && strstr(note, "outside"));
	/* (the renderer's refusal, a benchmark's override in the real one, comes before anything changes) */
	hook_refuses = "display.anisotropic_filtering";
	assert(!live("display.anisotropic_filtering", "4", note, sizeof(note)));
	assert(!strcmp(note, "display.anisotropic_filtering: a benchmark sets it"));
	hook_refuses = "";
	assert(config_changes() == before && hook_calls == 0);
	assert(config_integer("display.shadow_map_size") != 0);
	/* (a profile setting's choices are port_profile_keys.h's, as tools/set_check.py checks --set: multisampling is
	   not one of display.anti_aliasing's there) */
	assert(!live("display.anti_aliasing", "msaa2x", note, sizeof(note)) && strstr(note, "off|fxaa|smaa"));
	assert(live("display.anti_aliasing", "fxaa", note, sizeof(note)));
	assert(!strcmp(config_string("display.anti_aliasing"), "fxaa"));
	assert(live("display.soft_particles", "off", note, sizeof(note)) && !strcmp(note, "display.soft_particles = false"));
}

int main(void)
{
	assert(config_origins_main() == 0);
	assert(test_mkdtemp(directory, sizeof(directory), "config-live-probe"));
	setenv("HALO_DATA_ROOT", directory, 1);
	snprintf(path, sizeof(path), "%s/config.toml", directory);
	snprintf(origins, sizeof(origins), "%s/config.origins", directory);
	config_set_live_hook(probe_hook);
	check_graphics();
	check_mode_replaced();
	check_profile_shadowed();
	check_refusals();
	puts("config_live_probe: ok");
	return 0;
}
