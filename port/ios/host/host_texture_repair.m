/* Apache-2.0. See host_texture_repair.h. */
#import <Foundation/Foundation.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <pthread/qos.h>
#import "host_texture_repair.h"
#import "host_texture_upscale.h"
#import "host_astc_store.h"
#include "host.h"
#include "host_config.h"
#include "host_profile.h"
#include "texture_cache.h"
#include "texture_record.h"
#include "texture_repair.h"
#include "texture_upscale_encode.h"

/* texture_repair.h's version: raised when the filter or its limits change, and every record is made again. The byte
above it is the records' layout: 1 since they have a file name of their own (<key>.repair.bin; before, they shared the
upscaled texture's), so earlier records and finished maps are dropped and done again. The finished-maps list is
stamped with the recipe's low half, so it follows both. */
#define REPAIR_RECIPE (0x5245504149520100ull | TEXTURE_REPAIR_VERSION)
#define MAPS_STAMP ((unsigned)(REPAIR_RECIPE & 0xffffffffu))
#define MANIFEST_NAME "manifest-repair.tsv"
#define MAPS_NAME "repair-maps.tsv"
/* A level's repair, run as it loads, is held to this long: past it only the lightmaps are made and the rest waits. */
#define LOAD_BUDGET_SECONDS 2.0
#define SAVE_EVERY 48

enum { MAP_PENDING = 0, MAP_LIGHTMAPS = 1, MAP_COMPLETE = 2 };

static NSString *data_root;
static dispatch_queue_t worker;
static NSMutableDictionary<NSString *, NSData *> *records;   /* key -> struct texture_cache_record */
static NSMutableDictionary<NSString *, NSString *> *map_done; /* path -> "stamp level" */
static atomic_bool playing, stop_requested, background_queued, enabled_now;
static atomic_uint load_serial;
static char folder[1024];

static double now(void) { return NSProcessInfo.processInfo.systemUptime; }
static NSString *key_of(uint64_t hash, uint32_t width, uint32_t height) {
    return [NSString stringWithFormat:@"%016llx-%ux%u", (unsigned long long)hash, width, height];
}
static NSString *stamp_of(NSString *path) {
    struct stat info;
    return stat(path.fileSystemRepresentation, &info) ? nil : [NSString stringWithFormat:@"%lld-%lld", (long long)info.st_size, (long long)info.st_mtime];
}
static BOOL text_true(const char *text) { return !strcmp(text, "true") || !strcmp(text, "1"); }

/* display.repaired_textures as this launch has it: the profile's value and the player's (the graphics mode's layer is in
   host_config_string), either off turning it off */
static BOOL read_enabled(void) {
    /* a measured run: no repair work behind the numbers (records already made are still drawn) */
    if (host_texture_measured_run()) return NO;
    char text[32] = "true", profile[32] = "true";
    if (data_root.length && host_device_profile_peek(data_root.fileSystemRepresentation, "display.repaired_textures", profile, sizeof(profile)) && !text_true(profile))
        return NO;
    host_config_string("display.repaired_textures", "true", text, sizeof(text));
    return text_true(text);
}

/* ---------- persistence */
static void load_state(void) {
    records = [NSMutableDictionary new];
    map_done = [NSMutableDictionary new];
    struct texture_cache_record *loaded = NULL;
    size_t count = 0;
    if (!texture_cache_manifest_load_named(folder, MANIFEST_NAME, &loaded, &count)) {
        for (size_t i = 0; i < count; i++)
            if (loaded[i].recipe_key == REPAIR_RECIPE)
                records[key_of(loaded[i].hash, loaded[i].width, loaded[i].height)] = [NSData dataWithBytes:&loaded[i] length:sizeof(loaded[i])];
        free(loaded);
    }
    NSString *text = [NSString stringWithContentsOfFile:[@(folder) stringByAppendingPathComponent:@MAPS_NAME] encoding:NSUTF8StringEncoding error:nil];
    for (NSString *line in [text componentsSeparatedByString:@"\n"]) {
        NSArray *parts = [line componentsSeparatedByString:@"\t"];
        if (parts.count == 3 && [parts[0] unsignedIntValue] == MAPS_STAMP) map_done[parts[2]] = parts[1];
    }
}
static void save_state(void) {
    NSArray<NSData *> *rows = records.allValues;
    struct texture_cache_record *list = calloc(rows.count ? rows.count : 1, sizeof(*list));
    if (!list) return;
    for (NSUInteger i = 0; i < rows.count; i++) memcpy(&list[i], rows[i].bytes, sizeof(*list));
    if (texture_cache_manifest_save_named(folder, MANIFEST_NAME, list, rows.count))
        host_logf(HOST_LOG_WARN, "Texture repair: could not save the manifest");
    free(list);
    NSMutableString *text = [NSMutableString new];
    for (NSString *path in map_done) {
        NSString *value = map_done[path];   /* "stamp:level" */
        NSArray *parts = [value componentsSeparatedByString:@":"];
        if (parts.count == 2) [text appendFormat:@"%u\t%@\t%@\n", MAPS_STAMP, value, path];
    }
    [text writeToFile:[@(folder) stringByAppendingPathComponent:@MAPS_NAME] atomically:YES encoding:NSUTF8StringEncoding error:nil];
}
static int map_level(NSString *path) {
    NSString *stamp = stamp_of(path), *value = map_done[path];
    NSArray *parts = [value componentsSeparatedByString:@":"];
    return stamp && parts.count == 2 && [parts[0] isEqual:stamp] ? [parts[1] intValue] : MAP_PENDING;
}

/* ---------- one map */
struct walk {
    BOOL lightmaps_only;     /* past the load budget: format 6 only, the rest left for the background pass */
    double deadline;         /* 0: none */
    BOOL stopped, skipped_other;
    unsigned made, unchanged, since_save;
    double seconds;
};

static void note(struct walk *w, uint64_t hash, uint32_t width, uint32_t height, int result, uint64_t bytes) {
    struct texture_cache_record record = { hash, width, height, (enum texture_policy_result)result, (enum texture_policy_treatment)0,
        REPAIR_RECIPE, 0, bytes };
    records[key_of(hash, width, height)] = [NSData dataWithBytes:&record length:sizeof(record)];
    if (++w->since_save >= SAVE_EVERY) { save_state(); w->since_save = 0; }
}

static int repair_texture(void *context, const struct host_texture_source *s) {
    struct walk *w = context;
    /* (off now, too: a measured run that began mid-pass, whatever cleared the stop since) */
    if (atomic_load(&stop_requested) || atomic_load(&playing) || !atomic_load(&enabled_now)) { w->stopped = YES; return 0; }
    if (!texture_repair_applies(s->format, s->width, s->height)) return 1;
    NSString *key = key_of(s->hash, s->width, s->height);
    if (records[key]) return 1;
    if (w->deadline && now() > w->deadline) w->lightmaps_only = YES;
    if (w->lightmaps_only && s->format != TEXTURE_REPAIR_FORMAT_R5G6B5) { w->skipped_other = YES; return 1; }
    double start = now();
    size_t bytes_all = 0;
    for (unsigned level = 0; level < s->level_count; level++)
        bytes_all += (size_t)(s->width >> level ? s->width >> level : 1) * (s->height >> level ? s->height >> level : 1) * 4;
    uint8_t *repaired = malloc(bytes_all ? bytes_all : 1);
    struct texture_upscale_rgba_level *levels = calloc(s->level_count ? s->level_count : 1, sizeof(*levels));
    int status = repaired && levels ? texture_repair_texture(s->format, s->width, s->height, s->level_count, s->rgba, s->level_offsets,
        s->stored, s->level_stored_offsets, repaired, NULL) : -1;
    if (status == 1) {
        void *bytes = NULL;
        size_t size = 0, offset = 0;
        for (unsigned level = 0; level < s->level_count; level++) {
            uint32_t lw = s->width >> level ? s->width >> level : 1, lh = s->height >> level ? s->height >> level : 1;
            levels[level] = (struct texture_upscale_rgba_level){ repaired + offset, lw, lh };
            offset += (size_t)lw * lh * 4;
        }
        struct texture_cache_record record = { s->hash, s->width, s->height, TEXTURE_POLICY_BPF, (enum texture_policy_treatment)0,
            REPAIR_RECIPE, 0, 0 };
        if (texture_upscale_encode_raw(levels, s->level_count, s->width, s->height, s->hash, REPAIR_RECIPE, &bytes, &size) == TEXTURE_UPSCALE_ENCODE_OK &&
            !texture_cache_write_entry_kind(folder, &record, TEXTURE_RECORD_REPAIRED, bytes, size)) {
            note(w, s->hash, s->width, s->height, TEXTURE_POLICY_BPF, size);
            w->made++;
        }
        else host_logf(HOST_LOG_WARN, "Texture repair %016llx: could not write the record", (unsigned long long)s->hash);
        free(bytes);
    } else if (status == 0) {
        note(w, s->hash, s->width, s->height, TEXTURE_POLICY_ORIGINAL, 0);
        w->unchanged++;
    }
    free(repaired);
    free(levels);
    w->seconds += now() - start;
    return 1;
}

static void process_map(NSString *path, double budget) {
    @autoreleasepool {
        if (!enabled_now) return;
        struct walk w = { .deadline = budget > 0 ? now() + budget : 0 };
        double start = now();
        int status = host_texture_source_walk_map(path.fileSystemRepresentation, repair_texture, &w);
        BOOL complete = status == 1 && !w.stopped;
        if (complete) map_done[path] = [NSString stringWithFormat:@"%@:%d", stamp_of(path) ?: @"", w.skipped_other ? MAP_LIGHTMAPS : MAP_COMPLETE];
        save_state();
        host_logf(HOST_LOG_INFO, "Texture repair %s: %u repaired, %u left as they were, %.2f s (%.2f s in the filter)%s%s",
            path.lastPathComponent.UTF8String, w.made, w.unchanged, now() - start, w.seconds,
            w.skipped_other ? ", lightmaps only" : "", w.stopped ? ", stopped" : "");
    }
}

/* ---------- scheduling */
static int collect(void *context, const char *path) { [(__bridge NSMutableArray *)context addObject:@(path)]; return 1; }

static void background_pass(void) {
    @autoreleasepool {
        if (!enabled_now || atomic_load(&playing)) { atomic_store(&background_queued, false); return; }
        NSMutableArray<NSString *> *paths = [NSMutableArray new];
        host_texture_source_scan(data_root.fileSystemRepresentation, collect, (__bridge void *)paths);
        for (NSString *path in paths) {
            if (atomic_load(&playing) || atomic_load(&stop_requested) || !atomic_load(&enabled_now)) break;
            if (map_level(path) == MAP_COMPLETE) continue;
            process_map(path, 0);
        }
        atomic_store(&background_queued, false);
    }
}
static void queue_background(void) {
    if (!worker || !enabled_now || atomic_load(&playing)) return;
    bool expected = false;
    if (atomic_compare_exchange_strong(&background_queued, &expected, true)) dispatch_async(worker, ^{ background_pass(); });
}

void host_texture_repair_init(const char *root) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (!root || !*root) return;
        data_root = [@(root) stringByStandardizingPath];
        host_texture_cache_path(root, folder, sizeof(folder));
        if (!folder[0]) return;
        [NSFileManager.defaultManager createDirectoryAtPath:@(folder) withIntermediateDirectories:YES attributes:nil error:nil];
        worker = dispatch_queue_create("org.visr.texture-repair", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
        enabled_now = read_enabled();
        const char *measured = host_texture_measured_run();
        if (measured) host_logf(HOST_LOG_INFO, "Texture repair: no repair work in this measured run (%s)", measured);
        dispatch_async(worker, ^{ load_state(); });
        queue_background();
    });
}
int host_texture_repair_enabled(void) { return enabled_now; }
/* the cache folder was emptied (Settings' delete, texture_cache_delete_all): the records and finished maps held here
name files that are gone, so they are dropped before anything saves them back, and every map is done again */
void host_texture_repair_cache_deleted(void) {
    if (!worker) return;
    atomic_store(&stop_requested, true);
    dispatch_async(worker, ^{
        records = [NSMutableDictionary new];
        map_done = [NSMutableDictionary new];
        /* (a pass the delete stopped saved its lists after the files went: write the empty ones over them) */
        save_state();
        atomic_store(&stop_requested, false);
        host_logf(HOST_LOG_INFO, "Texture repair: the cache was deleted; every map is repaired again");
        /* queued from here, after the pass the delete stopped has ended and given up its place */
        queue_background();
    });
}
/* host_texture_measured_run_began: the run became a measured one after init; a pass under way stops at its next
texture, and nothing starts again (every entry point reads the setting again, now off) */
void host_texture_repair_measured_run_began(void) {
    if (!worker) return;
    atomic_store(&enabled_now, false);
    atomic_store(&stop_requested, true);
    host_logf(HOST_LOG_INFO, "Texture repair: no repair work in this measured run (%s)", host_texture_measured_run() ?: "measured");
}
void host_texture_repair_scan(void) {
    if (!worker) return;
    atomic_store(&stop_requested, false);
    /* queued after the setting is read again, so a scan that turns the repair on starts the work */
    dispatch_async(worker, ^{ enabled_now = read_enabled(); queue_background(); });
}
static NSString *resolve(NSString *name) {
    NSString *base = [[name stringByReplacingOccurrencesOfString:@"\\" withString:@"/"] lastPathComponent];
    if (base.pathExtension.length) base = base.stringByDeletingPathExtension;
    NSMutableArray<NSString *> *paths = [NSMutableArray new];
    host_texture_source_scan(data_root.fileSystemRepresentation, collect, (__bridge void *)paths);
    for (NSString *path in paths)
        if ([[path.lastPathComponent.stringByDeletingPathExtension lowercaseString] isEqual:base.lowercaseString]) return path;
    return nil;
}
void host_texture_repair_map_will_load(const char *name) {
    if (!worker || !name || !*name) return;
    /* (a measured run's load does no repair, and does not wait on the worker) */
    if (!enabled_now && host_texture_measured_run()) return;
    unsigned serial = atomic_fetch_add(&load_serial, 1) + 1;
    NSString *requested = @(name);
    atomic_store(&playing, false);
    /* The level's own map first, held to the budget: the loading screen is already up, and nothing new may wait on it.
       A background pass in the way stops at its next texture; it goes on when the menus open or the game pauses. */
    atomic_store(&stop_requested, true);
    dispatch_semaphore_t finished = dispatch_semaphore_create(0);
    dispatch_async(worker, ^{
        atomic_store(&stop_requested, false);
        enabled_now = read_enabled();
        NSString *path = enabled_now && serial == atomic_load(&load_serial) ? resolve(requested) : nil;
        if (path && map_level(path) != MAP_COMPLETE) process_map(path, LOAD_BUDGET_SECONDS);
        dispatch_semaphore_signal(finished);
    });
    dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, (int64_t)((LOAD_BUDGET_SECONDS + 1.0) * NSEC_PER_SEC)));
}
void host_texture_repair_game_started(int is_game) {
    if (!worker) return;
    if (is_game) { atomic_store(&playing, true); return; }
    atomic_store(&playing, false);
    atomic_store(&stop_requested, false);
    queue_background();
}
void host_texture_repair_paused(int paused) {
    if (!worker) return;
    if (paused) { atomic_store(&playing, false); queue_background(); }
    else atomic_store(&playing, true);
}
