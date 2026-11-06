/* host_config_write_boolean: one key set, every other line kept, the section added when missing; and the graphics
mode over the settings it replaces, as the game reads them (port/linux/src/port_graphics_modes.h) */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host_config.h"
#include "../../linux/src/port_graphics_modes.h"

static int head;
static int head_now(void) { return head; }

static char *slurp(const char *path)
{
	static char text[4096];
	FILE *file = fopen(path, "r");
	size_t n = fread(text, 1, sizeof(text) - 1, file);

	fclose(file);
	text[n] = 0;
	return text;
}

int main(void)
{
	char directory[] = "/tmp/host-config-XXXXXX";
	FILE *file;

	assert(mkdtemp(directory) && chdir(directory) == 0);   /* host_config reads config.toml in the working directory */
	file = fopen("config.toml", "w");
	fputs("# mine\n[display]\nhigh_res_hud = true\nupscaled_textures = true\n[debug]\ntexture_log = false\n", file);
	fclose(file);
	assert(host_config_write_boolean("display.upscaled_textures", 0) == 1);
	assert(strstr(slurp("config.toml"), "# mine\n[display]\nhigh_res_hud = true\nupscaled_textures = false\n[debug]\ntexture_log = false\n"));
	file = fopen("config.toml", "w");
	fputs("[debug]\ntexture_log = false\n", file);
	fclose(file);
	assert(host_config_write_boolean("display.upscaled_textures", 1) == 1);
	assert(strstr(slurp("config.toml"), "[debug]\ntexture_log = false\n\n[display]\nupscaled_textures = true\n"));
	file = fopen("config.toml", "w");
	fputs("[display]\nhigh_res_hud = false\n[debug]\n", file);
	fclose(file);
	assert(host_config_write_boolean("display.upscaled_textures", 0) == 1);
	assert(strstr(slurp("config.toml"), "[display]\nhigh_res_hud = false\nupscaled_textures = false\n[debug]\n"));
	file = fopen("config.toml", "w");   /* a last line without its newline keeps its value */
	fputs("[display]\nhigh_res_hud = true", file);
	fclose(file);
	assert(host_config_write_boolean("display.upscaled_textures", 0) == 1);
	assert(!strcmp(slurp("config.toml"), "[display]\nhigh_res_hud = true\nupscaled_textures = false\n"));
	unlink("config.toml");                /* no file yet: one with the section */
	assert(host_config_write_boolean("display.upscaled_textures", 0) == 1);
	assert(!strcmp(slurp("config.toml"), "\n[display]\nupscaled_textures = false\n"));
	assert(access("config.toml.tmp", F_OK) != 0);
	unlink("config.toml");

	/* the graphics mode: Enhanced when unset, and a mode's values over the file's */
	{
		char text[64];

		assert(host_config_graphics_mode() == GRAPHICS_MODE_ENHANCED);
		file = fopen("config.toml", "w");
		fputs("[display]\ngraphics = \"hardcore\"\nrender_scale = 0.5\nanti_aliasing = \"smaa\"\nupscaled_textures = true\n"
			"vsync = true\n", file);
		fclose(file);
		assert(host_config_graphics_mode() == GRAPHICS_MODE_HARDCORE);
		assert(host_config_real("display.render_scale", 0.25) == 1.0);
		host_config_string("display.anti_aliasing", "fxaa", text, sizeof(text));
		assert(!strcmp(text, "off"));
		host_config_string("display.upscaled_textures", "true", text, sizeof(text));
		assert(!strcmp(text, "false"));
		host_config_string("display.vsync", "false", text, sizeof(text));
		assert(!strcmp(text, "true"));
		/* head-tracked stereo: Hardcore acts as Classic, which leaves these alone */
		host_config_set_head_stereo(head_now);
		head = 1;
		assert(host_config_graphics_mode() == GRAPHICS_MODE_CLASSIC && host_config_real("display.render_scale", 1.0) == 0.5);
		head = 0;
		/* a benchmark suite measures Enhanced unless its environment names a mode */
		setenv("HALO_BENCHMARK_SUITE", "quick", 1);
		assert(host_config_graphics_mode() == GRAPHICS_MODE_ENHANCED);
		setenv("HALO_GRAPHICS", "classic", 1);
		assert(host_config_graphics_mode() == GRAPHICS_MODE_CLASSIC);
		unsetenv("HALO_GRAPHICS");
		unsetenv("HALO_BENCHMARK_SUITE");
		/* a mode a newer build has: Enhanced */
		file = fopen("config.toml", "w");
		fputs("[display]\ngraphics = \"rt\"\nrender_scale = 0.5\n", file);
		fclose(file);
		assert(host_config_graphics_mode() == GRAPHICS_MODE_ENHANCED && host_config_real("display.render_scale", 1.0) == 0.5);
		unlink("config.toml");
	}
	rmdir(directory);
	puts("host_config_write_probe: ok");
	return 0;
}
