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

/* Raised when the filter or its limit changes: every record is made again. */
#define REPAIR_VERSION 1u
#define REPAIR_RECIPE (0x5245504149520000ull | REPAIR_VERSION)
#define MANIFEST_NAME "manifest-repair.tsv"
#define MAPS_NAME "repair-maps.tsv"
/* A level's repair, run as it loads, is held to this long: past it only the lightmaps are made and the rest waits. */
#define LOAD_BUDGET_SECONDS 2.0
#define MAXIMUM_TEXELS (1024u * 1024u)
#define SAVE_EVERY 48

enum { MAP_PENDING = 0, MAP_LIGHTMAPS = 1, MAP_COMPLETE = 2 };

static NSString *data_root;
static dispatch_queue_t worker;
static NSMutableDictionary<NSString *, NSData *> *records;   /* key -> struct texture_cache_record */
static NSMutableDictionary<NSString *, NSString *> *map_done; /* path -> "stamp level" */
static NSMutableSet<NSString *> *upscaled_keys;               /* textures the upscaler made a record for */
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
        if (parts.count == 3 && [parts[0] unsignedIntValue] == REPAIR_VERSION) map_done[parts[2]] = parts[1];
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
        if (parts.count == 2) [text appendFormat:@"%u\t%@\t%@\n", REPAIR_VERSION, value, path];
    }
    [text writeToFile:[@(folder) stringByAppendingPathComponent:@MAPS_NAME] atomically:YES encoding:NSUTF8StringEncoding error:nil];
}
static int map_level(NSString *path) {
    NSString *stamp = stamp_of(path), *value = map_done[path];
    NSArray *parts = [value componentsSeparatedByString:@":"];
    return stamp && parts.count == 2 && [parts[0] isEqual:stamp] ? [parts[1] intValue] : MAP_PENDING;
}
static void upscaled_listing(void) {
    upscaled_keys = [NSMutableSet new];
    struct texture_cache_record *loaded = NULL;
    size_t count = 0;
    if (!texture_cache_manifest_load(folder, &loaded, &count)) {
        for (size_t i = 0; i < count; i++)
            if (loaded[i].result != TEXTURE_POLICY_ORIGINAL) [upscaled_keys addObject:key_of(loaded[i].hash, loaded[i].width, loaded[i].height)];
        free(loaded);
    }
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
    if (atomic_load(&stop_requested) || atomic_load(&playing)) { w->stopped = YES; return 0; }
    if (s->format != 6 && s->format != 14) return 1;
    if ((uint64_t)s->width * s->height > MAXIMUM_TEXELS) return 1;
    NSString *key = key_of(s->hash, s->width, s->height);
    if (records[key] || [upscaled_keys containsObject:key]) return 1;
    if (w->deadline && now() > w->deadline) w->lightmaps_only = YES;
    if (w->lightmaps_only && s->format != 6) { w->skipped_other = YES; return 1; }
    double start = now();
    struct texture_upscale_rgba_level *levels = calloc(s->level_count, sizeof(*levels));
    uint8_t **repaired = calloc(s->level_count, sizeof(*repaired));
    struct texture_repair_stats sum = { 0 };
    BOOL ok = levels && repaired;
    for (unsigned level = 0; ok && level < s->level_count; level++) {
        uint32_t lw = s->width >> level ? s->width >> level : 1, lh = s->height >> level ? s->height >> level : 1;
        repaired[level] = malloc((size_t)lw * lh * 4);
        struct texture_repair_stats stats = { 0 };
        int status = -1;
        if (!repaired[level]) { ok = NO; break; }
        if (s->format == 6) status = texture_repair_r5g6b5(s->rgba + s->level_offsets[level], lw, lh, 0, repaired[level], &stats);
        else if (lw >= 4 && lh >= 4) status = texture_repair_dxt1(s->stored + s->level_stored_offsets[level], lw, lh, 0, repaired[level], &stats);
        else { memcpy(repaired[level], s->rgba + s->level_offsets[level], (size_t)lw * lh * 4); status = 0; }
        if (status) { ok = NO; break; }
        sum.texels += stats.texels; sum.flagged += stats.flagged; sum.changed += stats.changed; sum.stepped_pairs += stats.stepped_pairs;
        levels[level] = (struct texture_upscale_rgba_level){ repaired[level], lw, lh };
    }
    if (ok && texture_repair_worth_storing(&sum, s->format == 6)) {
        void *bytes = NULL;
        size_t size = 0;
        struct texture_cache_record record = { s->hash, s->width, s->height, TEXTURE_POLICY_BPF, (enum texture_policy_treatment)0,
            REPAIR_RECIPE, 0, 0 };
        if (texture_upscale_encode_raw(levels, s->level_count, s->width, s->height, s->hash, REPAIR_RECIPE, &bytes, &size) == TEXTURE_UPSCALE_ENCODE_OK &&
            !texture_cache_write_entry(folder, &record, bytes, size)) {
            note(w, s->hash, s->width, s->height, TEXTURE_POLICY_BPF, size);
            w->made++;
        }
        else host_logf(HOST_LOG_WARN, "Texture repair %016llx: could not write the record", (unsigned long long)s->hash);
        free(bytes);
    } else if (ok) {
        note(w, s->hash, s->width, s->height, TEXTURE_POLICY_ORIGINAL, 0);
        w->unchanged++;
    }
    for (unsigned level = 0; repaired && level < s->level_count; level++) free(repaired[level]);
    free(repaired);
    free(levels);
    w->seconds += now() - start;
    return 1;
}

static void process_map(NSString *path, double budget) {
    @autoreleasepool {
        if (!enabled_now) return;
        upscaled_listing();
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
            if (atomic_load(&playing) || atomic_load(&stop_requested)) break;
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
        dispatch_async(worker, ^{ load_state(); });
        queue_background();
    });
}
int host_texture_repair_enabled(void) { return enabled_now; }
void host_texture_repair_scan(void) {
    if (!worker) return;
    atomic_store(&stop_requested, false);
    dispatch_async(worker, ^{ enabled_now = read_enabled(); });
    queue_background();
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
