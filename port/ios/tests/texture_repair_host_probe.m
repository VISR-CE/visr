/* The texture repair's host side without a game: its records and finished maps follow a delete of the cache folder. */
#import "host_texture_repair.m"
#include <assert.h>

static char cache_root[512];
static int walks;
/* a measured run (host_texture_measured_run in host_texture_upscale.m, which the upscaler's driver probe covers) */
static const char *measured;
const char *host_texture_measured_run(void) { return measured; }
void host_logf(int level, const char *format, ...) { (void)level; (void)format; }
void host_config_string(const char *name, const char *fallback, char *out, size_t size) { (void)name; snprintf(out, size, "%s", fallback); }
int host_device_profile_peek(const char *root, const char *name, char *text, size_t size) { (void)root; (void)name; (void)text; (void)size; return 0; }
void host_texture_cache_path(const char *root, char *text, size_t size) { (void)root; snprintf(text, size, "%s", cache_root); }
/* one map with no textures: walking it finishes it */
int host_texture_source_scan(const char *root, host_texture_map_callback callback, void *context) {
    char path[600];
    snprintf(path, sizeof(path), "%s/maps/a10.map", root);
    return callback(context, path);
}
int host_texture_source_walk_map(const char *path, host_texture_source_callback callback, void *context) {
    (void)path; (void)callback; (void)context; walks++; return 1;
}
enum texture_upscale_encode_result texture_upscale_encode_raw(const struct texture_upscale_rgba_level *levels, unsigned count,
    uint32_t width, uint32_t height, uint64_t hash, uint64_t recipe, void **bytes, size_t *size) {
    (void)levels; (void)count; (void)width; (void)height; (void)hash; (void)recipe; (void)bytes; (void)size;
    assert(!"no texture to encode"); return TEXTURE_UPSCALE_ENCODE_ERROR;
}
static void settle(void) { for (int i = 0; i < 4; i++) dispatch_sync(worker, ^{}); }

int main(void) { @autoreleasepool {
    char pattern[] = "/tmp/visr-repair-host-XXXXXX", *root = mkdtemp(pattern), maps[600];
    assert(root);
    snprintf(cache_root, sizeof(cache_root), "%s/cache", root);
    snprintf(maps, sizeof(maps), "%s/maps", root);
    assert(!mkdir(cache_root, 0700) && !mkdir(maps, 0700));
    strlcat(maps, "/a10.map", sizeof(maps));
    FILE *map = fopen(maps, "w"); assert(map); fputs("map", map); fclose(map);
    /* a record from an earlier launch, current recipe */
    struct texture_cache_record held = { .hash = 0x71, .width = 4, .height = 4, .recipe_key = REPAIR_RECIPE, .bytes = 16 };
    assert(!texture_cache_manifest_save_named(cache_root, MANIFEST_NAME, &held, 1));
    /* a measured run (bug class: background work running behind a benchmark's numbers): no walk at launch, none at a
       level's load, none at the menus; the records already made stay listed */
    measured = "benchmark suite";
    host_texture_repair_init(root);
    settle();
    host_texture_repair_map_will_load("a10");
    host_texture_repair_game_started(0);
    host_texture_repair_scan();
    settle();
    assert(walks == 0 && records.count == 1 && map_done.count == 0);
    /* not measured: the next scan does the work */
    measured = NULL;
    host_texture_repair_scan();
    settle();
    assert(walks == 1 && records.count == 1 && map_done.count == 1);
    /* Settings' delete (bug class: a delete that leaves another owner's in-memory copy of the folder's contents): the
       folder empties, the repair forgets what it held instead of saving it back, and walks the map again */
    assert(!texture_cache_delete_all(cache_root));
    host_texture_repair_cache_deleted();
    settle();
    assert(walks == 2 && records.count == 0 && map_done.count == 1);
    struct texture_cache_record *loaded = NULL; size_t count = 9;
    assert(!texture_cache_manifest_load_named(cache_root, MANIFEST_NAME, &loaded, &count) && count == 0);
    free(loaded);
    [NSFileManager.defaultManager removeItemAtPath:@(root) error:nil];
    puts("texture repair host: no work in a measured run; records and finished maps follow a cache delete PASS");
} return 0; }
