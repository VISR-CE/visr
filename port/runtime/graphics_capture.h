/* Public performance configuration only. Do not add paths or user/account/network data. */
#pragma once
#define CONFIG_GRAPHICS_MAXIMUM_BYTES (32 * 1024)
#define CONFIG_GRAPHICS_MAXIMUM_STRING_BYTES 256
/* Keep equal to GAME_PERF_GRAPHICS_MAX_KEYS (a test checks); raise both when the key-count test fails. */
#define CONFIG_GRAPHICS_MAXIMUM_KEYS 192
/* Every display.* setting, and these: debug settings that change what is drawn or how it is timed, and
   game.enhanced_animations, which the graphics mode (display.graphics) turns off in Hardcore */
#define GRAPHICS_CAPTURE_DEBUG_KEYS(X) \
    X("game.enhanced_animations") \
    X("debug.foveation_eye_passes") \
    X("debug.anti_aliasing_folded") \
    X("debug.rate_map_test") \
    X("debug.fixed_timestep") \
    X("debug.null_renderer") \
    X("debug.fixed_timestep_paced") \
    X("debug.visibility_any_sample") \
    X("debug.frame_load_cpu") \
    X("debug.frame_load_every") \
    X("debug.frame_load_gpu") \
    X("debug.frame_counter") \
    X("debug.gl_debug") \
    X("debug.render_validation") \
    X("debug.metal_state_cache") \
    X("debug.shader_list_warm") \
    X("debug.metal_pipeline_archive") \
    X("debug.metal_specialize") \
    X("debug.metal_vertex_bgra") \
    X("debug.metal_base_vertex") \
    X("debug.gpu_flush_draws") \
    X("debug.gpu_stats") \
    X("debug.texture_no_cache")

/* The renderer_state object of a schema 2 snapshot: what was drawn, as the suite writes it (benchmark_suite.c's
   benchmark_graphics_snapshot_json) and the report validator reads it (host_benchmark.m). Keep one list here;
   a test compares the suite's real output with it and runs the validator on that output. */
#define GRAPHICS_CAPTURE_RENDERER_STATE_KEYS(X) \
    X("shadow_target_size") \
    X("anisotropic_sampler_max") \
    X("model_lod") \
    X("texture_replacement_selections") \
    X("texture_replacement_source") \
    X("upscaled_texture_assets")
/* Settings whose drawn answer can differ from the configured one (a GPU that cannot run the pass, a mode, stereo).
   Each is a true/false renderer_state key, named after the display.* setting it reports (the benchmark tools rely on
   that), with the front end's getter for what was drawn. Performance reports replace the configured value of
   display.<key> with the getter's answer (d3d8_device.c), so no report records a setting that was not drawn. */
#define GRAPHICS_CAPTURE_DRAWN_FLAGS(X) \
    X("ambient_occlusion", halo_ambient_occlusion_effective) \
    X("soft_particles", halo_soft_particles_effective) \
    X("traced_shadows", halo_traced_shadows_effective)
