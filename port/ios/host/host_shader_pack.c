/* Strict admission for online shader descriptors, shared by the host and publisher. */
#include "host_shader_pack.h"
#include "../../runtime/shader_record.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A record as the loader keeps it: its names (for dependencies and conflicts) and its line in current form. The
   record's definition, its ranges and its text form are shader_record.h's; this file adds the file, merge and
   limit rules. */
struct record { int kind, line_no; char key[64], vertex[64], pixel[64], line[640]; };
/* where a problem goes when a caller wants every one of them, not only whether the folder is admitted */
struct reporter { host_shader_report fn; void *context; int reported; };
static void report(struct reporter *r, const char *file, int line_no, const char *message)
{
    char text[1400];
    if (!r || !r->fn) return;
    r->reported++;
    if (line_no) snprintf(text, sizeof(text), "%s:%d: %s", file, line_no, message);
    else snprintf(text, sizeof(text), "%s: %s", file, message);
    r->fn(r->context, text);
}
static int parse(const char *line, struct record *r, struct shader_record_problem *problem)
{
    struct shader_record s;
    char text[SHADER_RECORD_LINE_MAX];
    if (!shader_record_parse(line, &s, problem) || !shader_record_admit(&s, problem)) return 0;
    memset(r, 0, sizeof(*r));
    r->kind = s.kind;
    if (s.kind == SHADER_RECORD_PIPELINE) {
        shader_record_vertex_name(&s, r->vertex, sizeof(r->vertex));
        shader_record_pixel_name(&s, r->pixel, sizeof(r->pixel));
    } else shader_record_name(&s, r->key, sizeof(r->key));
    {
        size_t length = shader_record_format(&s, text);
        if (length >= sizeof(r->line)) return 0;
        memcpy(r->line, text, length + 1);
    }
    return 1;
}
static int compare_key(const void *a, const void *b) { return strcmp(((const struct record *)a)->key, ((const struct record *)b)->key); }
static int compare_line(const void *a, const void *b)
{
    const struct record *x = a, *y = b;
    return x->kind != y->kind ? x->kind - y->kind : strcmp(x->line, y->line);
}
static int load(const char *text, size_t size, struct record *records, size_t *count, const char *file,
    struct reporter *rep)
{
    char *copy, *cursor; size_t first = *count; int line_no = 0, valid = 1;
    if (!text || size > HOST_SHADER_PACK_MAX_FILE_BYTES || memchr(text, 0, size)) {
        report(rep, file, 0, "empty, too large, or holds a NUL byte");
        return 0;
    }
    copy = malloc(size + 1); if (!copy) return 0;
    memcpy(copy, text, size); copy[size] = 0; cursor = copy;
    while (*cursor) {
        char *line = cursor, *next = strchr(cursor, '\n');
        struct shader_record_problem problem;
        line_no++;
        if (next) { *next = 0; cursor = next+1; } else cursor += strlen(cursor);
        if (strlen(line) > 1023) {
            report(rep, file, line_no, "line longer than 1023 bytes");
            if (!rep) { free(copy); return 0; }
            valid = 0; continue;
        }
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (!*line || *line == '#') continue;
        if (*count-first == HOST_SHADER_LIST_MAX_RECORDS) {
            report(rep, file, line_no, "more than 8192 records");
            free(copy); return 0;
        }
        if (!parse(line, &records[*count], &problem)) {
            char message[200];
            report(rep, file, line_no, shader_record_problem_text(&problem, message, sizeof(message)));
            if (!rep) { free(copy); return 0; }
            valid = 0; continue;
        }
        records[*count].line_no = line_no;
        (*count)++;
    }
    free(copy);
    size_t n = *count-first;
    qsort(records+first,n,sizeof(*records),compare_key);
    for (size_t i = first; i < *count; i++) {
        struct record *r = records+i;
        if (r->kind < 2 && i > first && !strcmp(r->key,records[i-1].key) && strcmp(r->line,records[i-1].line)) {
            report(rep, file, r->line_no, "two different records named the same");
            if (!rep) return 0;
            valid = 0;
        }
        if (r->kind == 2) {
            struct record key = {0};
            char message[120];
            snprintf(key.key,sizeof(key.key),"%s",r->vertex);
            if (!bsearch(&key,records+first,n,sizeof(*records),compare_key)) {
                snprintf(message,sizeof(message),"missing dependency %s",r->vertex);
                report(rep, file, r->line_no, message);
                if (!rep) return 0;
                valid = 0;
            }
            snprintf(key.key,sizeof(key.key),"%s",r->pixel);
            if (!bsearch(&key,records+first,n,sizeof(*records),compare_key)) {
                snprintf(message,sizeof(message),"missing dependency %s",r->pixel);
                report(rep, file, r->line_no, message);
                if (!rep) return 0;
                valid = 0;
            }
        }
    }
    return valid && n > 0;
}
/* Ordinary-play batches contain only a few descriptors. Reserve their actual
   record count instead of the maximum catalog size on every worker checkpoint. */
static int record_capacity(const char *text, size_t size, size_t *count)
{
    size_t at = 0, lines = 0;
    if (!text) return 1;
    if (size > HOST_SHADER_PACK_MAX_FILE_BYTES || memchr(text, 0, size)) return 0;
    while (at < size) {
        size_t first = at;
        while (at < size && text[at] != '\n') at++;
        while (first < at && (text[first] == ' ' || text[first] == '\t' || text[first] == '\r')) first++;
        if (first < at && text[first] != '#' && ++lines > HOST_SHADER_LIST_MAX_RECORDS) return 0;
        if (at < size) at++;
    }
    *count += lines;
    return 1;
}
static int merge(const char *base, size_t base_size, const char *base_name, const char *additions, size_t additions_size,
    const char *additions_name, struct reporter *rep, char **merged, size_t *merged_size)
{
    size_t capacity = 0;
    struct record *r = NULL;
    size_t n = 0, length = 0; char *output = NULL; int valid = 0;
    if (merged) *merged = NULL;
    if (merged_size) *merged_size = 0;
    if (!record_capacity(base,base_size,&capacity) || !record_capacity(additions,additions_size,&capacity) || !capacity) goto done;
    r = calloc(capacity,sizeof(*r));
    if (!r || (base && !load(base,base_size,r,&n,base_name,rep)) || (additions && !load(additions,additions_size,r,&n,additions_name,rep)) || !n) goto done;
    qsort(r,n,sizeof(*r),compare_key);
    for (size_t i = 1; i < n; i++) if (r[i].kind < 2 && !strcmp(r[i].key,r[i-1].key) && strcmp(r[i].line,r[i-1].line)) goto done;
    qsort(r,n,sizeof(*r),compare_line);
    for (size_t i = 0; i < n; i++) if (!i || strcmp(r[i].line,r[i-1].line)) length += strlen(r[i].line)+1;
    if (merged) {
        output = malloc(length+1); if (!output) goto done;
        size_t at = 0;
        for (size_t i = 0; i < n; i++) if (!i || strcmp(r[i].line,r[i-1].line)) {
            size_t bytes = strlen(r[i].line); memcpy(output+at,r[i].line,bytes); at += bytes; output[at++] = '\n';
        }
        output[at] = 0; *merged = output;
    }
    if (merged_size) *merged_size = length;
    valid = 1;
done:
    free(r); return valid;
}
int host_shader_list_merge(const char *base, size_t base_size, const char *additions, size_t additions_size,
    char **merged, size_t *merged_size)
{
    return merge(base,base_size,"base",additions,additions_size,"additions",NULL,merged,merged_size);
}
int host_shader_map_name_valid(const char *name)
{
    size_t n = strlen(name);
    return n > 0 && n <= 63 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= '0' && name[0] <= '9')) &&
        strspn(name,"abcdefghijklmnopqrstuvwxyz0123456789_-") == n;
}
char *host_shader_list_read_file(const char *path, size_t *size)
{
    int fd = open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC); struct stat st; char *bytes = NULL;
    if (fd < 0) return NULL;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size < 1 || st.st_size > HOST_SHADER_PACK_MAX_FILE_BYTES) goto done;
    bytes = malloc((size_t)st.st_size+1); if (!bytes) goto done;
    *size = 0;
    while (*size < (size_t)st.st_size) {
        ssize_t n = read(fd,bytes+*size,(size_t)st.st_size-*size);
        if (n <= 0) { free(bytes); bytes = NULL; goto done; }
        *size += (size_t)n;
    }
    char extra;
    if (read(fd,&extra,1) != 0) { free(bytes); bytes = NULL; }
done:
    close(fd); return bytes;
}
int host_shader_pack_check(const char *folder, const char *bundled, void (*log)(const char *),
    host_shader_report report_problem, void *context)
{
    struct stat st; DIR *dir = NULL; struct dirent *e; int valid = 0, bad = 0; size_t total = 0, files = 0;
    struct reporter rep = { report_problem, context, 0 }, *reporter = report_problem ? &rep : NULL;
    if (lstat(folder,&st) || !S_ISDIR(st.st_mode) || !(dir = opendir(folder))) goto done;
    while ((e = readdir(dir))) {
        const char *name = e->d_name; size_t length = strlen(name), size = 0, base_size = 0;
        char map[64], path[1200], base_path[1200];
        if (!strcmp(name,".") || !strcmp(name,"..")) continue;
        if (length < 5 || length >= sizeof(map)+4 || strcmp(name+length-4,".txt")) {
            report(reporter,name,0,"not a MAP.txt file name");
            if (!reporter) goto done;
            bad = 1; continue;
        }
        memcpy(map,name,length-4); map[length-4] = 0;
        if (!host_shader_map_name_valid(map) || ++files > HOST_SHADER_PACK_MAX_MAPS) {
            report(reporter,name,0,"not a valid map name, or more than 32 maps");
            if (!reporter) goto done;
            bad = 1; continue;
        }
        snprintf(path,sizeof(path),"%s/%s",folder,name);
        char *bytes = host_shader_list_read_file(path,&size), *base = NULL;
        base_path[0] = 0;
        if (bundled) { snprintf(base_path,sizeof(base_path),"%s/%s",bundled,name); base = host_shader_list_read_file(base_path,&base_size); }
        int before = reporter ? reporter->reported : 0;
        int accepted = bytes && size <= HOST_SHADER_PACK_MAX_TOTAL_BYTES-total &&
            merge(base,base_size,base_path,bytes,size,path,reporter,NULL,NULL);
        if (!bytes) report(reporter,path,0,"cannot be read as a regular file of 1 byte to 4 MiB");
        else if (size > HOST_SHADER_PACK_MAX_TOTAL_BYTES-total) report(reporter,path,0,"the snapshot exceeds 16 MiB");
        free(bytes); free(base);
        if (!accepted && reporter && reporter->reported == before) report(reporter,path,0,"records not admitted");
        if (!accepted) { if (!reporter) goto done; bad = 1; continue; }
        total += size;
    }
    valid = files > 0 && !bad;
done:
    if (dir) closedir(dir);
    if (!valid && log) log("shader update: invalid map descriptors or dependencies");
    return valid;
}
int host_shader_pack_validate(const char *folder, const char *bundled, void (*log)(const char *))
{
    return host_shader_pack_check(folder,bundled,log,NULL,NULL);
}
