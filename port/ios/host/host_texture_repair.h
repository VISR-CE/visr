/* Apache-2.0. The texture repair at import (spec 2026-11-23-halo-ce-ios-image-precision-design.md, Part 1): the maps'
lightmaps and gradient textures, rebuilt without the banding their 5:6:5 values carry (port/runtime/texture_repair.h),
kept as records beside the upscaler's (texture-cache/manifest-repair.tsv, a texture's own size, uncompressed) and drawn
by the same replacement cache. A CPU step: it runs on every device, with or without upscaled textures, behind its own
setting display.repaired_textures (a profile key once device profiles set it), and never during gameplay (a pause is fine). */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* the data root, once (the upscaler's init calls it); the key is read here and again at each level load */
void host_texture_repair_init(const char *root);
/* maps arrived or the app returned: the background pass looks at every map that is not done */
void host_texture_repair_scan(void);
/* a level begins to load: its map is repaired first, here, within a time budget (the lightmaps first); what does not
   fit waits for the background pass */
void host_texture_repair_map_will_load(const char *name);
/* gameplay began (1: stop the background pass) or the menus opened (0: go on) */
void host_texture_repair_game_started(int is_game);
void host_texture_repair_paused(int paused);
int host_texture_repair_enabled(void);
/* the cache folder was emptied: forget every record and finished map, and repair again in the background */
void host_texture_repair_cache_deleted(void);
#ifdef __cplusplus
}
#endif
