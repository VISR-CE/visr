/* Compiled by test_graphics_modes.py with the production consumer functions extracted into a header, so the
 * Xbox SDK and GPU are not needed. The settings resolver and display-size math are the production code too. */
#define main config_origins_main
#include "config_origins_probe.c"
#undef main
#include "halo_display.h"
#include "texture_upscale_state.h"

typedef int boolean;
typedef int BOOL;
#define SCREEN_HEIGHT 480
#define GPU_INITIALIZE_FRAME_RATE_SHIFT 16
#define GPU_INITIALIZE_FRAME_RATE_UNLIMITED 65535
#define D3DFMT_LIN_A8R8G8B8 1
#define D3DFMT_LIN_D24S8 2
typedef unsigned int UINT;
struct probe_surface { unsigned long width, height; };
static struct {
	int gl_ready; unsigned long frame; int created;
	struct { UINT BackBufferWidth; } presentation;
	struct probe_surface back_buffer, depth_buffer;
} device = { .gl_ready = 1, .frame = 10, .created = 1 };
static long screen_width = 1280, screen_maximum_texture_size = 8192;
static float screen_scale[2] = { 2, 2 }, screen_scale_ceiling[2] = { 2, 2 };
static float screen_scale_mono[2], screen_scale_native[2];
static int screen_dynamic = 1, released, release_pooled;
static double render_scale_live = -1;
static long benchmark_render_lines, knob_render_lines;
static struct probe_stereo { int foveated; } probe_stereo;
static const struct probe_stereo *halo_stereo_frame(void) { return &probe_stereo; }
static int head_eyes_frame(void) { return 0; }
static int halo_scene_class(void) { return CONFIG_SCENE_GAMEPLAY; }
static int halo_external_pause_allowed(void) { return 0; }
static void host_external_commit(int allowed) { (void)allowed; }
static int dynamic_resolution_refused(void) { return 2; }
static int host_dynamic_resolution_lines(int ceiling) { (void)ceiling; return 0; }
static void platform_video_drawable_size(int *w, int *h) { *w = 2560; *h = 1440; }
static void anti_aliasing_read(void) {}
static long halo_screen_width(void) { return screen_width; }
static void screen_targets_release(int pooled) { released++; release_pooled = pooled; }
static void d3d8_surface_resize(struct probe_surface *surface, int format, unsigned long w, unsigned long h)
{ (void)format; surface->width = w; surface->height = h; }
static struct { int upscale; } device_capabilities = { 1 };
void xgpu_texture_cache_settings_read(void);
static int native_hud_wanted = -1, hud_split_layout_forced;
static int model_lod_setting = -1, model_lod_override = -1;
static unsigned long model_lod_revision, knob_release_frame;
static int forgotten, applied, applied_upscaler, applied_crt, applied_frame_rate;
static int local_player_count(void) { return 2; }
static void recent_render_targets_forget(void) { forgotten++; }
static void host_graphics_apply(int metalfx, int crt, int frame_rate)
{
	applied++;
	applied_upscaler = metalfx;
	applied_crt = crt;
	applied_frame_rate = frame_rate;
}
/* Only the GPU uploads and readiness query are stubbed for the live texture selector. */
#define TEXTURE_BUCKET_COUNT 2
static struct texture_upscale_state texture_upscale;
static unsigned long texture_upscale_read_at = (unsigned long)-1;
static unsigned long menu_picture_refresh = 1000;
static struct texture_entry {
	struct texture_entry *next;
	unsigned long generation, address;
	struct { unsigned long width, height; } description;
} texture_entry = { .generation = 7 };
static unsigned long hud_changed;
static unsigned long hud_hires_settings_changed(void)
{ unsigned long changed = hud_changed; hud_changed = 0; return changed; }
static int hud_hires_bitmap_affected(unsigned long address, long width, long height, unsigned long changed)
{ (void)address; (void)width; (void)height; return (changed & 1) != 0; }
static struct texture_entry *texture_buckets[TEXTURE_BUCKET_COUNT] = { &texture_entry, NULL };
static int recent_textures[2] = { 1, 1 };
static int texture_ready = 1, prepared;
static void texture_upscale_map_loaded(void)
{
	prepared++;
	if (!texture_ready) texture_upscale_state_request_original(&texture_upscale);
	texture_upscale_state_map_loaded(&texture_upscale, config_boolean("display.upscaled_textures"));
	texture_upscale_read_at = config_changes();
}
#include "graphics_live_consumers.h"

/* the additions each mode keeps: limits and soft edges in Classic and Enhanced, per-pixel lighting in Enhanced */
static void check_additions(int mode)
{
	assert(halo_effect_limits_raised() == (mode != GRAPHICS_MODE_HARDCORE));
	assert(!strcmp(config_string("game.garbage_limit"), mode == GRAPHICS_MODE_HARDCORE ? "xbox" : "raised"));
	assert(config_boolean("display.soft_particles") == (mode != GRAPHICS_MODE_HARDCORE));
	assert(config_boolean("display.per_pixel_lighting") == (mode == GRAPHICS_MODE_ENHANCED));
	assert(config_integer("display.point_light_highlight") == 2);
}

static void check_consumers(int hardcore)
{
	struct halo_display_size size;
	int before, before_forgotten;

	graphics_settings_read();
	assert(native_hud_wanted == !hardcore);
	assert(applied_upscaler == !hardcore && applied_crt == hardcore);
	assert(applied_frame_rate == (hardcore ? 30 : 60));
	assert(hud_split_screen_layout() == hardcore);
	assert(halo_interpolation_enabled() == !hardcore);
	assert(text_hires_enabled() == !hardcore && ui_redraw_enabled() == !hardcore);
	assert(model_lod_is_max() == !hardcore);
	assert(knob_release_frame == device.frame + 120);
	before = applied;
	before_forgotten = forgotten;
	graphics_settings_read();
	assert(applied == before && forgotten == before_forgotten);
	if (hardcore)
	{
		long width = halo_display_layout_width(2560, 1440, config_integer("display.screen_width"), 1280);
		assert(config_real("display.render_scale") == 1.0);
		size = halo_display_render_size(2560, 1440, width, config_integer("display.screen_width"),
			config_scene_integer("display.render_height", CONFIG_SCENE_GAMEPLAY), 8192);
		assert(width == 640 && size.width == 640 && size.height == 480);
	}
}

int main(void)
{
	assert(config_origins_main() == 0);
	snprintf(directory, sizeof(directory), "/tmp/graphics-live-probe-XXXXXX");
	assert(mkdtemp(directory));
	setenv("HALO_DATA_ROOT", directory, 1);
	snprintf(path, sizeof(path), "%s/config.toml", directory);
	snprintf(origins, sizeof(origins), "%s/config.origins", directory);
	launch("display.upscaled_textures\ttrue\tprofile\n" "display.native_hud\ttrue\tprofile\n" "display.upscaler\tmetalfx\tprofile\n");
	assert(config_write("display.native_hud", "true"));
	assert(config_write("display.frame_rate", "60"));
	assert(config_write("display.high_res_text", "true"));
	assert(config_write("display.ui_redraw", "true"));
	assert(config_write("display.crt_effect", "true"));
	assert(config_write("display.graphics", "enhanced"));
	check_consumers(0);
	check_additions(GRAPHICS_MODE_ENHANCED);
	assert(config_graphics_try("classic"));
	check_additions(GRAPHICS_MODE_CLASSIC);
	assert(config_graphics_try("hardcore"));
	check_additions(GRAPHICS_MODE_HARDCORE);
	assert(halo_screen_commit() == 640);
	assert(screen_scale[0] == 1 && screen_scale[1] == 1 && !screen_dynamic);
	assert(released == 1 && release_pooled == 1);
	assert(device.back_buffer.width == 640 && device.depth_buffer.width == 640);
	assert(device.back_buffer.height == 480 && device.depth_buffer.height == 480);
	check_consumers(1);
	assert(config_graphics_try("enhanced"));
	check_consumers(0);
	check_additions(GRAPHICS_MODE_ENHANCED);
	assert(config_write("display.graphics", "hardcore"));
	check_consumers(1);
	config_graphics_head_stereo(1);
	check_consumers(0);
	config_graphics_head_stereo(0);
	check_consumers(1);
	assert(config_write("display.graphics", "enhanced"));
	check_consumers(0);
	/* A level started in Hardcore acquires replacements on its next enabled frame. */
	texture_upscale_state_map_loaded(&texture_upscale, 0);
	texture_upscale_read_at = (unsigned long)-1;
	prepared = 0;
	assert(config_write("display.upscaled_textures", "true"));
	texture_upscale_settings_read();
	assert(prepared == 1 && texture_upscale.level_enabled && !texture_upscale.show_original);
	assert(!texture_entry.generation && !recent_textures[0] && !menu_picture_refresh);
	texture_entry.generation = 9;
	assert(config_graphics_try("hardcore"));
	texture_upscale_settings_read();
	assert(texture_upscale.show_original && texture_entry.generation == 9);
	assert(config_graphics_try("enhanced"));
	texture_upscale_settings_read();
	assert(!texture_upscale.show_original && prepared == 1 && texture_entry.generation == 9);
	assert(config_write("display.crt_effect", "false")); /* unrelated changes keep the debug selection */
	texture_upscale_state_toggle(&texture_upscale);
	texture_upscale_settings_read();
	assert(texture_upscale.show_original && prepared == 1);
	/* Incomplete assets stay original; a later map load checks readiness again. */
	texture_upscale_state_map_loaded(&texture_upscale, 0);
	texture_upscale_read_at = (unsigned long)-1;
	texture_ready = 0;
	texture_upscale_settings_read();
	assert(!texture_upscale.level_enabled && prepared == 2);
	texture_upscale_settings_read();
	assert(prepared == 2);
	texture_entry.generation = 11;
	recent_textures[0] = 1;
	hud_changed = 1;
	xgpu_texture_cache_settings_read();
	assert(!texture_entry.generation && !recent_textures[0]);
	fresh();
	rmdir(directory);
	puts("graphics_live_probe: ok (saved modes, previews, stereo fallback, cached consumers, 640x480)");
	return 0;
}
