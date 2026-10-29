/* Exercise durable publication and lifecycle handoffs without a model or game process. */
#define HALO_DEVELOPMENT_HOOKS 1
#import "host_texture_upscale.m"
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
const struct texture_policy_table texture_policy_embedded={.low=12,.high=30,.sigma=1,.iterations=3,.damage_share=.1f,
    .damage_structure_loss=-1,.damage_bpf_structure_loss=-1,.damage_measure_version=1};
void host_config_string(const char *name,const char *fallback,char *out,size_t size) {
    snprintf(out,size,"%s",!strcmp(name,"debug.texture_upscale_generate")?"true":fallback);
}
static int retry_logs;
void host_logf(int level,const char *format,...) {
    (void)level;if(strstr(format,"Upscale import: Try Again:"))retry_logs++;
}
int texture_upscale_model_available(void) {return 0;}
const char *texture_upscale_model_unavailable_reason(void) {return "no compiled model in this build";}
static int continued_begins,continued_ends;
void host_texture_upscale_continued_phase(int began) {if(began)continued_begins++;else continued_ends++;}
void texture_refine_set_cancel(const volatile int *flag) {(void)flag;}
int host_device_profile_peek(const char *root,const char *name,char *text,size_t size) {(void)root;(void)name;snprintf(text,size,"true");return 1;}
struct texture_upscale_generator *texture_upscale_generate_create(char *error,size_t size) {
    snprintf(error,size,"synthetic model failure");return NULL;
}
void texture_upscale_generate_stats(struct texture_upscale_generator *g,struct texture_upscale_model_stats *s) {(void)g;memset(s,0,sizeof(*s));}
int texture_upscale_generate(struct texture_upscale_generator *g,const struct texture_upscale_generate_input *in,
    struct texture_upscale_generate_output *out,char *error,size_t size) {(void)g;(void)in;(void)out;(void)error;(void)size;return -1;}
void texture_upscale_generate_free_output(struct texture_upscale_generate_output *out) {(void)out;}
int texture_upscale_generate_raw4x(struct texture_upscale_generator *g,const struct texture_upscale_generate_input *in,
    float **pixels,unsigned *channels,char *error,size_t size) {(void)g;(void)in;(void)pixels;(void)channels;(void)error;(void)size;return -1;}
const char *texture_upscale_encoder_name(uint32_t format) {(void)format;return "bc7-v1";}
enum texture_upscale_encode_result texture_upscale_encode_record(const struct texture_upscale_rgba_level *levels,unsigned count,
    uint32_t w,uint32_t h,uint64_t hash,uint64_t recipe,uint32_t format,unsigned alpha_mode,unsigned threads,void **bytes,size_t *size,
    struct texture_upscale_encode_metrics *metrics) {
    (void)alpha_mode;(void)levels;(void)count;(void)w;(void)h;(void)hash;(void)recipe;(void)format;(void)threads;(void)bytes;(void)size;(void)metrics;
    assert(!"unexpected encoder call");return TEXTURE_UPSCALE_ENCODE_ERROR;
}
/* the fake map b30: one key, 0x77 at 4x4 (fake_maps on) */
static int fake_maps;
int host_texture_source_scan(const char *root,host_texture_map_callback callback,void *context) {
    (void)root;if(fake_maps)callback(context,"/fake/maps/b30.map");return 1;}
int host_texture_source_walk_map(const char *path,host_texture_source_callback callback,void *context) {(void)path;(void)callback;(void)context;return 1;}
FILE *host_texture_source_open_policy_map(const char *path,uint64_t *size,char *error,size_t capacity) {
    (void)path;(void)error;(void)capacity;if(!fake_maps)return NULL;FILE *f=tmpfile();fputs("map",f);fflush(f);*size=3;return f;}
struct texture_policy_catalog *texture_policy_catalog_new(void) {return fake_maps?(struct texture_policy_catalog *)calloc(1,8):NULL;}
void texture_policy_catalog_free(struct texture_policy_catalog *catalog) {free(catalog);}
int texture_policy_add_map(struct texture_policy_catalog *catalog,const unsigned char *data,size_t size,char *error,size_t capacity) {
    (void)catalog;(void)data;(void)size;(void)error;(void)capacity;return fake_maps?0:-1;
}
size_t texture_policy_classify(struct texture_policy_catalog *catalog,const struct texture_policy_table *policy,
    struct texture_policy_decision **out,texture_policy_log_proc log,void *context) {
    (void)catalog;(void)policy;(void)log;(void)context;
    if(!fake_maps)return 0;
    *out=calloc(1,sizeof(**out));(*out)[0]=(struct texture_policy_decision){.hash=0x77,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR,.scale=2};
    return 1;
}
const char *texture_policy_kind_name(enum texture_policy_kind kind) {(void)kind;return "graphic";}
const char *texture_policy_result_name(enum texture_policy_result result) {
    static const char *names[]={"s4g","bpf","original"};return names[result];
}
struct retry_test {
    NSString *capacity_path;
    int presses,disabled,resumed,saw_note,start_after_resume;
    double note_began;
    double fraction;
};
static int retry_progress(void *opaque,const struct host_texture_upscale_import_status *status) {
    struct retry_test *test=opaque;
    assert(status->fraction>=test->fraction);test->fraction=status->fraction;
    if(!status->message){
        if(test->presses==2)test->resumed=1;
        /* after a passing retry, Start playing is offered again once the main menu's textures are made (here at once:
           this set has no menu textures) */
        if(test->resumed&&!status->menu_phase){test->start_after_resume=1;return HOST_TEXTURE_UPSCALE_START_PLAYING;}
        return HOST_TEXTURE_UPSCALE_WAIT;
    }
    assert(status->needed_bytes>=TEXTURE_CACHE_FALLBACK_HEADROOM);
    assert(status->found_bytes==(64ull<<20));
    if(!status->can_retry){test->disabled++;return HOST_TEXTURE_UPSCALE_WAIT;}
    if(test->presses==0){test->presses++;return HOST_TEXTURE_UPSCALE_TRY_AGAIN;}
    if(status->retry_note){
        assert(!strcmp(status->retry_note,"Still not enough free storage."));
        if(!test->saw_note)test->note_began=now();test->saw_note=1;
        return HOST_TEXTURE_UPSCALE_WAIT;
    }
    assert(test->saw_note&&now()-test->note_began>=2.8);
    assert([@"9000,9000" writeToFile:test->capacity_path atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    test->presses++;return HOST_TEXTURE_UPSCALE_TRY_AGAIN;
}
int main(void) {@autoreleasepool {
    char pattern[]="/tmp/visr-upscale-driver-XXXXXX";char *directory=mkdtemp(pattern);assert(directory);
    /* "auto" (the setting here) stays out of a measured run */
    setenv("HALO_INPUT_REPLAY","a10-bench",1);
    host_texture_upscale_init(directory);
    unsetenv("HALO_INPUT_REPLAY");
    assert(upscale_mode==TEXTURE_UPSCALE_MODE_OFF);
    assert(!upscale_enabled&&!upscale_worker&&!generator);
    upscale_root=@(directory);texture_log=[NSMutableDictionary new];thermal_log=[NSMutableArray new];started=now();
    assert([NSFileManager.defaultManager createDirectoryAtPath:cache_directory() withIntermediateDirectories:YES attributes:nil error:nil]);
    struct texture_cache_record initial={.hash=1,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR,
        .recipe_key=11,.global_key=22,.bytes=4};
    assert(publish_record(&initial)&&manifest_count==1&&manifest[0].recipe_key==11);
    NSString *manifest_path=[cache_directory() stringByAppendingPathComponent:@"manifest.tsv"];
    assert(!unlink(manifest_path.fileSystemRepresentation));assert(!mkdir(manifest_path.fileSystemRepresentation,0700));
    struct texture_cache_record changed=initial;changed.recipe_key=33;
    assert(!publish_record(&changed)&&manifest_count==1&&manifest[0].recipe_key==11);
    NSString *entry=[cache_directory() stringByAppendingPathComponent:@"0000000000000001-4x4.bin"];
    assert([[@"test" dataUsingEncoding:NSUTF8StringEncoding] writeToFile:entry atomically:YES]);
    decisions=calloc(1,sizeof(*decisions));decision_count=1;
    decisions[0]=(struct texture_policy_decision){.hash=1,.width=4,.height=4,.result=TEXTURE_POLICY_ORIGINAL};
    assert(!reconcile_manifest()&&manifest_count==1&&[NSFileManager.defaultManager fileExistsAtPath:entry]);
    assert(!rmdir(manifest_path.fileSystemRepresentation));assert(reconcile_manifest()&&manifest_count==0);
    assert(![NSFileManager.defaultManager fileExistsAtPath:entry]);
    struct texture_policy_decision d={.hash=2,.width=4,.height=4,.kind=TEXTURE_POLICY_GRAPHIC,.result=TEXTURE_POLICY_S4G,
        .treatment=TEXTURE_POLICY_COLOR,.scale=2};
    log_texture_error(&d,"generate","synthetic failure",.25);
    NSData *log=[NSData dataWithContentsOfFile:[upscale_root stringByAppendingPathComponent:@"texture-upscale-generate.json"]];
    NSDictionary *json=[NSJSONSerialization JSONObjectWithData:log options:0 error:nil];
    assert([json[@"textures"] count]==1&&[json[@"textures"][0][@"failed_stage"] isEqual:@"generate"]);
    /* A menu wake arriving while a pump is exiting must survive its queued flag. */
    upscale_enabled=YES;atomic_store(&pump_queued,true);atomic_store(&pump_requested,false);
    host_texture_upscale_game_started(0);assert(atomic_load(&pump_requested));
    /* Rules 4 and 6: a level's game stops the work at once; a pause lets it run only after two seconds; the game
       resuming stops it again; the next map's load frees it and makes that map first. */
    work_group=dispatch_group_create();activity_token=[NSObject new];
    host_texture_upscale_game_started(1);assert(stop_flag&&atomic_load(&upscale_playing));
    host_texture_upscale_paused(1);assert(stop_flag);
    activity.paused_since-=TEXTURE_UPSCALE_PAUSE_SECONDS;host_texture_upscale_paused(1);assert(stop_flag);
    resume_if_allowed();assert(!stop_flag&&!atomic_load(&upscale_playing));
    host_texture_upscale_paused(0);assert(stop_flag);
    unsigned serial=atomic_load(&priority_serial);
    host_texture_upscale_map_will_load("levels\\b30\\b30");
    assert(!stop_flag&&[priority_snapshot() isEqual:@"b30"]&&atomic_load(&priority_serial)==serial+1);
    /* the loading screen's wait: a level whose textures are not listed is checked, then plays original; a skip
       always does */
    assert(host_texture_upscale_level_begin("levels\\a10\\a10")&&[level_name isEqual:@"a10"]);
    assert(host_texture_upscale_level_poll(NULL,NULL)==TEXTURE_UPSCALE_LEVEL_CHECKING);
    /* a queue that stops without listing the level (here: no catalog) ends the wait */
    atomic_store(&pump_queued,false);atomic_fetch_add(&pumps_ended,1);
    assert(host_texture_upscale_level_poll(NULL,NULL)==TEXTURE_UPSCALE_LEVEL_DONE);
    host_texture_upscale_level_end(1);assert(atomic_load(&urgent)==0);
    assert(!host_texture_upscale_level_ready());
    /* no catalog yet (a launch straight into the level): the level reads its own list and the saved cache from disk,
       never "not listed yet"; with no data root there is no cache, so nothing upscaled is drawn */
    assert(!catalog_ready&&host_texture_upscale_level_ready());
    upscale_mode=TEXTURE_UPSCALE_MODE_AUTO;profile_upscales=0;
    char status[64];host_texture_upscale_status(status,sizeof(status));assert(!strcmp(status,"Not used on this device"));
    upscale_enabled=NO;
    assert(host_texture_upscale_level_ready()&&host_texture_upscale_level_poll(NULL,NULL)==TEXTURE_UPSCALE_LEVEL_NONE);
    /* the no-mix rule with the generator off: no cache plays (nothing upscaled is drawn); a cache without its decisions,
       or missing one of the level's textures, or with that texture's file gone, plays original; complete, upscaled */
    any_root=@(directory);fake_maps=1;
    host_texture_upscale_map_will_load("levels\\b30\\b30");
    NSString *cache=[@(directory) stringByAppendingPathComponent:@TEXTURE_CACHE_DIRECTORY];
    [NSFileManager.defaultManager removeItemAtPath:cache error:nil];
    assert(host_texture_upscale_level_ready());
    assert([NSFileManager.defaultManager createDirectoryAtPath:cache withIntermediateDirectories:YES attributes:nil error:nil]);
    struct texture_cache_record other={.hash=0x99,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR,.recipe_key=1,.bytes=4};
    other.global_key=texture_recipe_global_key(&(struct texture_recipe_globals){0},TEXTURE_POLICY_COLOR,TEXTURE_POLICY_S4G);
    assert(!texture_cache_manifest_save(cache.fileSystemRepresentation,&other,1));
    assert(!host_texture_upscale_level_ready());                         /* no decisions */
    struct texture_policy_decision saved[2]={{.hash=0x77,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR},
        {.hash=0x99,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR}};
    assert(!texture_cache_decisions_save(cache.fileSystemRepresentation,saved,2));
    assert(!host_texture_upscale_level_ready());                         /* b30's 0x77 is not made: original */
    struct texture_recipe_globals globals=recipe_globals();
    struct texture_cache_record mine={.hash=0x77,.width=4,.height=4,.result=TEXTURE_POLICY_S4G,.treatment=TEXTURE_POLICY_COLOR,.recipe_key=2,
        .global_key=texture_recipe_global_key(&globals,TEXTURE_POLICY_COLOR,TEXTURE_POLICY_S4G),.bytes=4};
    struct texture_cache_record both[2]={other,mine};
    assert(!texture_cache_manifest_save(cache.fileSystemRepresentation,both,2));
    assert(!host_texture_upscale_level_ready());                         /* listed, but its file is missing */
    assert([[@"test" dataUsingEncoding:NSUTF8StringEncoding] writeToFile:[cache stringByAppendingPathComponent:@"0000000000000077-4x4.bin"] atomically:YES]);
    assert(host_texture_upscale_level_ready());                          /* complete */
    fake_maps=0;
    assert(!host_texture_upscale_prepare_import(directory,NULL,NULL));
    free(decisions);free(manifest);decisions=NULL;manifest=NULL;decision_count=manifest_count=0;
    state_token=[NSObject new];
    /* Found/needed before the catalog: use saved decisions, counting only entries on the current recipe. */
    saved[0].scale=4;saved[1].scale=2;
    assert(!texture_cache_decisions_save(cache.fileSystemRepresentation,saved,2));
    assert(import_storage_needed()==TEXTURE_CACHE_FALLBACK_HEADROOM+texture_cache_projected_entry(&saved[1]));
    assert(!texture_cache_delete_all(cache.fileSystemRepresentation));
    assert(import_storage_needed()==TEXTURE_CACHE_FALLBACK_HEADROOM+IMPORT_FULL_SET_BYTES);
    /* The file is read on every check; unlike an environment value it can change during the phase. */
    NSString *capacity=[@(directory) stringByAppendingPathComponent:@"capacity.txt"];
    assert([@"0,64" writeToFile:capacity atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    setenv("HALO_TEXTURE_UPSCALE_CAPACITY_FILE",capacity.fileSystemRepresentation,1);
    uint64_t found=0;
    assert(storage_available(cache,&found)==0&&found==(64ull<<20));
    upscale_worker=dispatch_queue_create("test.upscale.retry",DISPATCH_QUEUE_SERIAL);
    finisher=dispatch_queue_create("test.upscale.finish",DISPATCH_QUEUE_SERIAL);
    map_keys=[NSMutableDictionary new];generator_releases=[NSMutableArray new];
    upscale_enabled=YES;upscale_mode=TEXTURE_UPSCALE_MODE_FORCE;atomic_store(&pump_queued,false);
    atomic_store(&loading_picture_failed,true);atomic_store(&retry_scheduled,true);
    struct retry_test retry={.capacity_path=capacity};
    assert(host_texture_upscale_prepare_import(directory,retry_progress,&retry));
    dispatch_sync(upscale_worker,^{});
    assert(retry.presses==2&&retry.disabled==2&&retry.resumed&&retry.saw_note&&retry_logs==2&&retry.start_after_resume);
    assert(continued_begins==2&&continued_ends==2&&atomic_load(&import_shown)==-1);
    assert(atomic_load(&retry_scheduled)); /* manual retry never replaces or cancels the timer */
    assert(storage_available(cache,&found)>0&&found==(9000ull<<20));
    unsetenv("HALO_TEXTURE_UPSCALE_CAPACITY_FILE");upscale_enabled=NO;
    free(decisions);free(manifest);decisions=NULL;manifest=NULL;
    assert([NSFileManager.defaultManager removeItemAtPath:upscale_root error:nil]);
    puts("texture upscale driver: publication rollback, original reconciliation, failure JSON, unavailable model, menu wake, gameplay stop, pause, level wait, readiness, the no-mix rule, storage estimates and import retry PASS");
}}
