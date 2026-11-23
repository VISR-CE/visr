/* Apache-2.0. Runs the texture repair filter over every texture of every map under a data root and checks the repair's
properties texel by texel (texture_repair_verify_*): each repaired texel is one the stored value could have been rounded
from, and a texel outside a flagged area is byte-identical. Prints one row per changed texture and a summary; exits 1
on any violation. Stock maps exist only on the runner hosts, so tools/texture_repair_sweep.py builds and runs this
there (run-job.sh); no map data is committed. */
#import "host_astc_store.m"
#include "texture_repair.h"
#include <stdarg.h>
#include <time.h>
void host_texture_upscale_init(const char *root) {(void)root;}
void host_texture_upscale_scan(void) {}
void host_texture_upscale_map_will_load(const char *name) {(void)name;}
void host_texture_upscale_game_started(int is_game) {(void)is_game;}
void host_config_string(const char *name,const char *fallback,char *out,size_t size) {(void)name;snprintf(out,size,"%s",fallback);}
double host_config_real(const char *name,double fallback) {(void)name;return fallback;}
void host_logf(int level,const char *format,...) {(void)level;(void)format;}

static struct {
    unsigned long maps,textures,r565,dxt1,changed_textures,changed_texels,texels,violations,listed;
    double seconds,seconds_565_max_page;
} total;
static const char *map_name;

static double wall(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }

static int visit(void *context,const struct host_texture_source *s) {
    (void)context;
    if(s->format!=6&&s->format!=14)return 1;
    total.textures++;
    struct texture_repair_stats sum={0};long bad=0;
    for(unsigned level=0;level<s->level_count;level++) {
        unsigned w=MAX(1u,s->width>>level),h=MAX(1u,s->height>>level);
        if(s->format==14&&(w<4||h<4))continue;
        uint8_t *out=malloc((size_t)w*h*4);struct texture_repair_stats stats={0};
        double start=wall();int status;
        if(s->format==6)status=texture_repair_r5g6b5(s->rgba+s->level_offsets[level],w,h,0,out,&stats);
        else status=texture_repair_dxt1(s->stored+s->level_stored_offsets[level],w,h,0,out,&stats);
        double took=wall()-start;total.seconds+=took;
        if(status){fprintf(stderr,"%s %016llx level %u: filter refused (%d)\n",map_name,(unsigned long long)s->hash,level,status);bad++;free(out);continue;}
        long violations=s->format==6?texture_repair_verify_r5g6b5(s->rgba+s->level_offsets[level],out,w,h,0):
            texture_repair_verify_dxt1(s->stored+s->level_stored_offsets[level],out,w,h,0);
        if(violations)bad+=violations;
        sum.texels+=stats.texels;sum.flagged+=stats.flagged;sum.changed+=stats.changed;sum.stepped_pairs+=stats.stepped_pairs;
        if(level==0&&s->format==6&&took>total.seconds_565_max_page)total.seconds_565_max_page=took;
        free(out);
    }
    if(s->format==6)total.r565++;else total.dxt1++;
    total.texels+=sum.texels;total.changed_texels+=sum.changed;total.violations+=bad;
    if(sum.changed)total.changed_textures++;
    /* a texture more than half flagged that is not 5:6:5 (lightmap-like) is listed for a look */
    int look=sum.texels&&sum.flagged*2>sum.texels&&s->format!=6;
    if(look)total.listed++;
    if(sum.changed||bad)
        printf("%s\t%016llx\t%ux%u\tfmt%u\tflagged %.1f%%\tchanged %lu\tstepped %lu\tviolations %ld%s\n",map_name,(unsigned long long)s->hash,
            s->width,s->height,s->format,100.0*sum.flagged/(sum.texels?sum.texels:1),sum.changed,sum.stepped_pairs,bad,look?"\tLOOK":"");
    return 1;
}
static int visit_map(void *context,const char *path) {
    (void)context;map_name=strrchr(path,'/')?strrchr(path,'/')+1:path;total.maps++;
    int status=host_texture_source_walk_map(path,visit,NULL);
    if(status<0){fprintf(stderr,"%s: could not read\n",map_name);total.violations++;}
    return 1;
}
int main(int argc,char **argv) {
    if(argc<2){fprintf(stderr,"usage: %s DATA_ROOT\n",argv[0]);return 2;}
    @autoreleasepool {host_texture_source_scan(argv[1],visit_map,NULL);}
    printf("SUMMARY maps %lu textures %lu (r5g6b5 %lu, dxt1 %lu) changed textures %lu, changed texels %lu of %lu, violations %lu, look %lu, "
        "filter %.2f s total, slowest r5g6b5 level 0 %.1f ms\n",total.maps,total.textures,total.r565,total.dxt1,total.changed_textures,
        total.changed_texels,total.texels,total.violations,total.listed,total.seconds,total.seconds_565_max_page*1000);
    return total.violations?1:0;
}
