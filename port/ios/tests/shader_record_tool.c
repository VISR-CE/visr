/* shader-record: check, explain, close, canonical and merge shader record files with the one contract
   (port/runtime/shader_record.h). Built by tools/data_pack.py's helper; see docs/data-packs.md.

     shader-record fields [--markdown]
     shader-record explain FILE|LINE [HASH]
     shader-record check [--lint] FOLDER [BASE]
     shader-record close --out FOLDER [--report FILE] [--base FOLDER]... [--also FOLDER]... DELTA...
     shader-record canonical --out FOLDER FOLDER
     shader-record merge --out FOLDER LISTS FOLDER...

   Folders hold MAP.txt files. Exit 0 on success, 1 when a record is refused or a file cannot be written, 2 on
   usage. Nothing is written to --out unless every map succeeds (merge, canonical). */
#include "../host/host_shader_pack.h"
#include "../../runtime/shader_record.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_MAPS 64
#define MAP_HEADER "# %s: the shaders and pipelines this map's draws make, compiled while it loads\n" \
	"# (d3d8_device.c, halo_shader_list_warm); merged by tools/shader_lists.py from recorded runs\n"

static int usage(void)
{
	fputs("usage: shader-record fields [--markdown] | explain FILE|LINE [HASH] | check [--lint] FOLDER [BASE] |\n"
		"       close --out FOLDER [--report FILE] [--base FOLDER]... [--also FOLDER]... DELTA... |\n"
		"       canonical --out FOLDER FOLDER | merge --out FOLDER LISTS FOLDER...\n", stderr);
	return 2;
}

/* ---- files and folders ---- */

static char *read_text(const char *path, size_t *size)
{
	struct stat st;
	FILE *file;
	char *text;

	if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size > HOST_SHADER_PACK_MAX_FILE_BYTES)
		return NULL;
	if (!(file = fopen(path, "rb")))
		return NULL;
	text = malloc((size_t)st.st_size + 1);
	if (!text || (*size = fread(text, 1, (size_t)st.st_size, file)) != (size_t)st.st_size)
	{
		free(text);
		fclose(file);
		return NULL;
	}
	fclose(file);
	text[*size] = 0;
	return text;
}

static int compare_names(const void *a, const void *b) { return strcmp(a, b); }

/* the MAP names (without .txt) of a folder, sorted; returns how many */
static int list_maps(const char *folder, char names[][64], int max)
{
	DIR *dir = opendir(folder);
	struct dirent *e;
	int n = 0;

	if (!dir)
		return 0;
	while ((e = readdir(dir)))
	{
		size_t length = strlen(e->d_name);

		if (length > 4 && length < 64 + 4 && !strcmp(e->d_name + length - 4, ".txt") && n < max)
		{
			memcpy(names[n], e->d_name, length - 4);
			names[n++][length - 4] = 0;
		}
	}
	closedir(dir);
	qsort(names, (size_t)n, 64, compare_names);
	return n;
}

static void map_path(char *out, size_t size, const char *folder, const char *map)
{
	size_t a = strlen(folder), b = strlen(map);

	if (a + b + 6 > size) { out[0] = 0; return; }
	memcpy(out, folder, a);
	out[a] = '/';
	memcpy(out + a + 1, map, b);
	memcpy(out + a + 1 + b, ".txt", 5);
}

/* Reads the records of a file into SET. Unparseable lines are listed on stderr and counted in *BAD; when ADMIT, a
record the app would refuse is too (a delta is read without it: its lit keys are reduced by close). */
static int read_records(const char *path, struct shader_record_set *set, int admit, int *bad)
{
	size_t size = 0;
	char *text = read_text(path, &size), *cursor;
	int line_no = 0;

	if (!text)
	{
		fprintf(stderr, "%s: cannot read\n", path);
		return 0;
	}
	for (cursor = text; *cursor;)
	{
		char *line = cursor, *next = strchr(cursor, '\n'), message[200];
		struct shader_record record;
		struct shader_record_problem problem;

		line_no++;
		if (next) { *next = 0; cursor = next + 1; } else cursor += strlen(cursor);
		while (*line == ' ' || *line == '\t' || *line == '\r')
			line++;
		if (!*line || *line == '#')
			continue;
		if (shader_record_parse(line, &record, &problem) && record.kind == SHADER_RECORD_PS &&
			shader_record_hash(record.key, record.key_size) != record.hash)
		{
			/* a key must hash to its name at every size: close rewrites the hash from the key */
			memset(&problem, 0, sizeof(problem));
			problem.reason = SHADER_RECORD_BAD_HASH;
			problem.field = "hash";
			problem.index = -1;
			snprintf(problem.detail, sizeof(problem.detail), "the key hashes to another value");
			problem.value = (int64_t)shader_record_hash(record.key, record.key_size);
			fprintf(stderr, "%s:%d: %s\n", path, line_no, shader_record_problem_text(&problem, message, sizeof(message)));
			(*bad)++;
			continue;
		}
		if (!shader_record_parse(line, &record, &problem) || (admit && !shader_record_admit(&record, &problem)))
		{
			fprintf(stderr, "%s:%d: %s\n", path, line_no, shader_record_problem_text(&problem, message, sizeof(message)));
			(*bad)++;
			continue;
		}
		if (!shader_record_set_add(set, &record))
		{
			free(text);
			return 0;
		}
	}
	free(text);
	return 1;
}

static int write_map(const char *folder, const char *map, const struct shader_record_set *set)
{
	char path[1200], line[SHADER_RECORD_LINE_MAX];
	FILE *file;
	size_t i;

	mkdir(folder, 0755);
	map_path(path, sizeof(path), folder, map);
	if (!(file = fopen(path, "wb")))
	{
		fprintf(stderr, "%s: cannot write\n", path);
		return 0;
	}
	fprintf(file, MAP_HEADER, map);
	for (i = 0; i < set->count; i++)
	{
		shader_record_format(&set->items[i], line);
		fprintf(file, "%s\n", line);
	}
	return fclose(file) == 0;
}

/* every record of the set passes admit and lint; the problems go to stderr under NAME */
static int verify(const char *name, const struct shader_record_set *set)
{
	size_t i;
	int bad = 0;

	for (i = 0; i < set->count; i++)
	{
		struct shader_record_problem problem;
		char line[SHADER_RECORD_LINE_MAX], message[200];

		if (!shader_record_admit(&set->items[i], &problem) || !shader_record_lint(&set->items[i], &problem))
		{
			shader_record_format(&set->items[i], line);
			fprintf(stderr, "%s: %s (%.40s)\n", name, shader_record_problem_text(&problem, message, sizeof(message)), line);
			bad++;
		}
	}
	return !bad;
}

/* ---- fields ---- */

static void legal_text(const struct shader_record_field *f, char *out, size_t size)
{
	switch (f->rule)
	{
	case SHADER_RECORD_RANGE: snprintf(out, size, "%u-%u", f->minimum, f->maximum); break;
	case SHADER_RECORD_COMPARE: snprintf(out, size, "0, or 0x200-0x207"); break;
	case SHADER_RECORD_COMBINERS: snprintf(out, size, "not checked by the app; tools: count word at most %d, canonical form", SHADER_RECORD_COMBINER_COUNT_MAX); break;
	case SHADER_RECORD_TEXMODES: snprintf(out, size, "not checked by the app; tools: each mode at most %d", SHADER_RECORD_TEXTURE_MODE_MAX); break;
	case SHADER_RECORD_EXTENDED: snprintf(out, size, "extended keys only; the app takes base keys, tools reduce them"); break;
	default: snprintf(out, size, "not checked"); break;
	}
}

static int command_fields(int argc, char **argv)
{
	size_t i;
	int markdown = argc > 0 && !strcmp(argv[0], "--markdown");

	if (argc > 1 || (argc && !markdown))
		return usage();
	/* the plain form is for scripts: OFFSET SIZE COUNT RULE MINIMUM MAXIMUM NAME, then the pipeline's NAME MAXIMUM */
	if (markdown) printf("| Bytes | Field | Source | Legal values |\n|---|---|---|---|\n");
	for (i = 0; i < shader_record_ps_field_count; i++)
	{
		const struct shader_record_field *f = &shader_record_ps_fields[i];
		char bytes[32], legal[160], name[48];
		uint32_t end = f->offset + f->size * f->count - 1;

		if (f->size * f->count == 1) snprintf(bytes, sizeof(bytes), "%u", f->offset);
		else snprintf(bytes, sizeof(bytes), "%u-%u", f->offset, end);
		if (f->count > 1) snprintf(name, sizeof(name), "`%s[%u]`", f->name, f->count);
		else snprintf(name, sizeof(name), "`%s`", f->name);
		legal_text(f, legal, sizeof(legal));
		if (markdown) printf("| %s | %s | %s | %s |\n", bytes, name, f->source, legal);
		else printf("%u\t%u\t%u\t%s\t%u\t%u\t%s\n", f->offset, f->size, f->count,
			f->rule == SHADER_RECORD_RANGE ? "range" : f->rule == SHADER_RECORD_COMPARE ? "compare" :
			f->rule == SHADER_RECORD_COMBINERS ? "combiners" : f->rule == SHADER_RECORD_TEXMODES ? "texmodes" : f->rule == SHADER_RECORD_EXTENDED ? "extended" : "free",
			f->minimum, f->maximum, f->name);
	}
	if (markdown) printf("\nA pipeline line's fields after its two shader names:\n\n| Field | Source | Legal values |\n|---|---|---|\n");
	for (i = 0; i < shader_record_pipeline_field_count; i++)
	{
		const struct shader_record_pipeline_field *f = &shader_record_pipeline_fields[i];
		int optional = f->default_member != f->member;

		if (i == shader_record_pipeline_required_count())
		{
			if (markdown)
				printf("| `attribute_kinds[%d]` | the vertex attribute format per stream (GPU_ATTRIBUTE_*), 32 hex digits | 0-%d |\n",
					SHADER_RECORD_ATTRIBUTES, SHADER_RECORD_ATTRIBUTE_KIND_MAX);
			else printf("pipeline\tattribute_kinds\t%d\n", SHADER_RECORD_ATTRIBUTE_KIND_MAX);
		}
		if (markdown) printf("| `%s` | %s%s | 0-%u |\n", f->name, f->source,
			optional ? " (absent in the 11-field form, where it equals the matching color field)" : "", f->maximum);
		else printf("pipeline\t%s\t%u\n", f->name, f->maximum);
	}
	return 0;
}

/* ---- explain ---- */

static void explain_record(const struct shader_record *r)
{
	char name[80], message[200];
	struct shader_record_problem problem;
	size_t i;

	if (r->kind == SHADER_RECORD_VS)
		printf("vs %016llx\n  variant=%u\n  mask=0x%x\n", (unsigned long long)r->vertex_hash, r->variant, r->mask);
	else if (r->kind == SHADER_RECORD_PS)
	{
		printf("ps %016llx (%u bytes, key hashes to %016llx)\n", (unsigned long long)r->hash, r->key_size,
			(unsigned long long)shader_record_hash(r->key, r->key_size));
		for (i = 0; i < shader_record_ps_field_count; i++)
		{
			const struct shader_record_field *f = &shader_record_ps_fields[i];
			uint32_t e, b;

			if (f->offset + f->size * f->count > r->key_size)
				continue;
			if (f->rule == SHADER_RECORD_COMBINERS)
			{
				uint32_t count = (uint32_t)r->key[4 * SHADER_RECORD_COMBINER_COUNT];
				printf("  %-22s combiner_count=%u (word %d); %u words, bytes %u-%u\n", f->name, count,
					SHADER_RECORD_COMBINER_COUNT, f->count, f->offset, f->offset + f->size * f->count - 1);
				continue;
			}
			printf("  %-22s", f->name);
			for (e = 0; e < f->count; e++)
			{
				uint32_t value = 0;
				for (b = 0; b < f->size; b++)
					value |= (uint32_t)r->key[f->offset + e * f->size + b] << (8 * b);
				if (f->count > 1) printf(" [%u]=%u", e, value); else printf(" =%u", value);
				if (f->size > 1) printf(" (0x%x)", value);
			}
			if (f->size * f->count == 1) printf("   byte %u: %s\n", f->offset, f->source);
			else printf("   bytes %u-%u: %s\n", f->offset, f->offset + f->size * f->count - 1, f->source);
		}
		if (r->key_size > SHADER_RECORD_KEY_BASE_SIZE)
			printf("  (an extended key: the app admits only the first %d bytes; `close` reduces it)\n", SHADER_RECORD_KEY_BASE_SIZE);
	}
	else
	{
		shader_record_vertex_name(r, name, sizeof(name));
		printf("pipeline %s\n  pixel_hash=%016llx\n", name + 3, (unsigned long long)r->hash);
		for (i = 0; i < shader_record_pipeline_field_count; i++)
			printf("  %s=%u (0-%u)\n", shader_record_pipeline_fields[i].name,
				*(const uint32_t *)((const char *)r + shader_record_pipeline_fields[i].member),
				shader_record_pipeline_fields[i].maximum);
		printf("  attribute_kinds=");
		for (i = 0; i < SHADER_RECORD_ATTRIBUTES; i++)
			printf("%02x", r->attribute_kinds[i]);
		printf("\n");
	}
	if (shader_record_admit(r, &problem) && shader_record_lint(r, &problem))
		printf("  admitted\n");
	else
		printf("  refused: %s\n", shader_record_problem_text(&problem, message, sizeof(message)));
}

static int command_explain(int argc, char **argv)
{
	struct shader_record_set set = {0};
	struct shader_record record;
	struct shader_record_problem problem;
	const char *filter = argc > 1 ? argv[1] : NULL;
	size_t i;
	int bad = 0, shown = 0;
	char message[200];

	if (argc < 1 || argc > 2)
		return usage();
	if (!strncmp(argv[0], "vs ", 3) || !strncmp(argv[0], "ps ", 3) || !strncmp(argv[0], "pipeline ", 9))
	{
		if (!shader_record_parse(argv[0], &record, &problem))
		{
			fprintf(stderr, "%s\n", shader_record_problem_text(&problem, message, sizeof(message)));
			return 1;
		}
		explain_record(&record);
		return 0;
	}
	if (!read_records(argv[0], &set, 0, &bad))
		return 1;
	for (i = 0; i < set.count; i++)
	{
		char hash[20];

		snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)(set.items[i].kind == SHADER_RECORD_VS ? set.items[i].vertex_hash : set.items[i].hash));
		if (filter && strncmp(hash, filter, strlen(filter)))
			continue;
		explain_record(&set.items[i]);
		shown++;
	}
	shader_record_set_free(&set);
	if (!shown)
		fprintf(stderr, "no record matches\n");
	return bad || !shown;
}

/* ---- check ---- */

static void print_problem(void *context, const char *message)
{
	(void)context;
	fprintf(stderr, "%s\n", message);
}

static int command_check(int argc, char **argv)
{
	int lint = argc > 0 && !strcmp(argv[0], "--lint"), valid;

	argc -= lint; argv += lint;
	if (argc < 1 || argc > 2)
		return usage();
	valid = host_shader_pack_check(argv[0], argc == 2 ? argv[1] : NULL, NULL, print_problem, NULL);
	if (valid && lint)
	{
		char maps[MAX_MAPS][64];
		int n = list_maps(argv[0], maps, MAX_MAPS), i;

		for (i = 0; i < n; i++)
		{
			struct shader_record_set set = {0};
			char path[1200];
			int bad = 0;

			map_path(path, sizeof(path), argv[0], maps[i]);
			if (!read_records(path, &set, 1, &bad) || bad || !verify(path, &set))
				valid = 0;
			shader_record_set_free(&set);
		}
	}
	if (!valid)
		fputs("shader-record: refused\n", stderr);
	return valid ? 0 : 1;
}

/* ---- sources for close ---- */

struct source
{
	char map[64];
	int group;
	struct shader_record_set set;
};
struct sources
{
	struct source *items;
	size_t count, capacity;
	char current[64];
	int groups;
};

static struct source *add_source(struct sources *s, const char *map, int group)
{
	if (s->count == s->capacity)
	{
		size_t capacity = s->capacity ? s->capacity * 2 : 16;
		struct source *items = realloc(s->items, capacity * sizeof(*items));

		if (!items)
			return NULL;
		s->items = items;
		s->capacity = capacity;
	}
	memset(&s->items[s->count], 0, sizeof(s->items[0]));
	snprintf(s->items[s->count].map, 64, "%s", map);
	s->items[s->count].group = group;
	return &s->items[s->count++];
}

/* by group in order; within a group, the current map first, then every other map */
static const struct shader_record *lookup(void *context, uint64_t hash)
{
	struct sources *s = context;
	int group, same;
	size_t i;

	for (group = 0; group < s->groups; group++)
		for (same = 1; same >= 0; same--)
			for (i = 0; i < s->count; i++)
			{
				const struct shader_record *found;

				if (s->items[i].group != group || (strcmp(s->items[i].map, s->current) == 0) != same)
					continue;
				if ((found = shader_record_set_find_pixel(&s->items[i].set, hash)))
					return found;
			}
	return NULL;
}

/* loads every MAP.txt of FOLDER as a lookup source of GROUP (records reduced and canonical, their recorded hashes kept) */
static int load_sources(struct sources *s, const char *folder, int group)
{
	char maps[MAX_MAPS][64];
	int n = list_maps(folder, maps, MAX_MAPS), i, bad = 0;

	for (i = 0; i < n; i++)
	{
		struct source *source = add_source(s, maps[i], group);
		char path[1200];
		size_t j;

		if (!source)
			return 0;
		map_path(path, sizeof(path), folder, maps[i]);
		if (!read_records(path, &source->set, 0, &bad))
			return 0;
		for (j = 0; j < source->set.count; j++)
			if (source->set.items[j].kind == SHADER_RECORD_PS)
				shader_record_reduce(&source->set.items[j]);
	}
	if (s->groups <= group)
		s->groups = group + 1;
	return 1;
}

/* ---- close ---- */

struct string_list { const char *items[32]; int count; };

static int command_close(int argc, char **argv)
{
	struct string_list bases = {{0}, 0}, also = {{0}, 0}, deltas = {{0}, 0};
	const char *out = NULL, *report = NULL;
	struct sources s = {0};
	char names[MAX_MAPS][64];
	int i, n = 0, exit_code = 0, group = 0;
	FILE *json = NULL;
	size_t total_unresolved = 0;

	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
		else if (!strcmp(argv[i], "--base") && i + 1 < argc && bases.count < 32) bases.items[bases.count++] = argv[++i];
		else if (!strcmp(argv[i], "--also") && i + 1 < argc && also.count < 32) also.items[also.count++] = argv[++i];
		else if (argv[i][0] != '-' && deltas.count < 32) deltas.items[deltas.count++] = argv[i];
		else return usage();
	}
	if (!out || !deltas.count)
		return usage();
	/* lookup order: the report's other files, its dependency candidates, then the bases */
	for (i = 0; i < deltas.count; i++) if (!load_sources(&s, deltas.items[i], group)) return 1;
	group++;
	for (i = 0; i < also.count; i++) if (!load_sources(&s, also.items[i], group)) return 1;
	group++;
	for (i = 0; i < bases.count; i++) if (!load_sources(&s, bases.items[i], group)) return 1;
	/* the maps of every delta, once each */
	for (i = 0; i < deltas.count; i++)
	{
		char found[MAX_MAPS][64];
		int count = list_maps(deltas.items[i], found, MAX_MAPS), j, k;

		for (j = 0; j < count; j++)
		{
			for (k = 0; k < n && strcmp(names[k], found[j]); k++) ;
			if (k == n && n < MAX_MAPS) memcpy(names[n++], found[j], 64);
		}
	}
	if (report && !(json = fopen(report, "wb")))
	{
		fprintf(stderr, "%s: cannot write\n", report);
		return 1;
	}
	if (json) fputs("{\n  \"maps\": {", json);
	for (i = 0; i < n; i++)
	{
		struct shader_record_set set = {0}, unresolved = {0};
		size_t j, pipelines = 0, ps_before = 0, vs_before = 0, pipelines_out = 0, ps_after = 0, vs_after = 0, k;
		int bad = 0, d;
		char path[1200];

		memcpy(s.current, names[i], 64);
		for (d = 0; d < deltas.count; d++)
		{
			size_t first = set.count;

			map_path(path, sizeof(path), deltas.items[d], names[i]);
			if (access(path, 0) == 0 && !read_records(path, &set, 0, &bad))
				return 1;
			if (bad) exit_code = 1;
			for (j = first; j < set.count; j++)
				set.items[j].origin = (uint32_t)d;
		}
		for (j = 0; j < set.count; j++)
		{
			pipelines += set.items[j].kind == SHADER_RECORD_PIPELINE;
			ps_before += set.items[j].kind == SHADER_RECORD_PS;
			vs_before += set.items[j].kind == SHADER_RECORD_VS;
		}
		if (!shader_record_close(&set, lookup, &s, &unresolved))
			return 1;
		for (j = 0; j < set.count; j++)
		{
			pipelines_out += set.items[j].kind == SHADER_RECORD_PIPELINE;
			ps_after += set.items[j].kind == SHADER_RECORD_PS;
			vs_after += set.items[j].kind == SHADER_RECORD_VS;
		}
		if (!verify(names[i], &set))
			exit_code = 1;
		if (set.count && !write_map(out, names[i], &set))
			return 1;
		total_unresolved += unresolved.count;
		printf("%s: %zu pipelines in, %zu closed, %zu unresolved; vs %zu -> %zu, ps %zu -> %zu\n", names[i], pipelines,
			pipelines_out, unresolved.count, vs_before, vs_after, ps_before, ps_after);
		if (json)
		{
			fprintf(json, "%s\n    \"%s\": {\"pipelines_in\": %zu, \"pipelines_closed\": %zu, \"vs_in\": %zu, \"vs_out\": %zu, "
				"\"ps_in\": %zu, \"ps_out\": %zu, \"unresolved\": [", i ? "," : "", names[i], pipelines, pipelines_out,
				vs_before, vs_after, ps_before, ps_after);
			/* unresolved pixel hashes, each once with how many pipelines name it */
			for (j = 0, k = 0; j < unresolved.count; j++)
			{
				size_t before, count = 0;

				for (before = 0; before < j && unresolved.items[before].hash != unresolved.items[j].hash; before++) ;
				if (before < j)
					continue;
				for (before = j; before < unresolved.count; before++)
					count += unresolved.items[before].hash == unresolved.items[j].hash;
				fprintf(json, "%s{\"hash\": \"%016llx\", \"pipelines\": %zu, \"source\": \"%s\"}", k++ ? ", " : "",
					(unsigned long long)unresolved.items[j].hash, count, deltas.items[unresolved.items[j].origin]);
			}
			fprintf(json, "]}");
		}
		shader_record_set_free(&set);
		shader_record_set_free(&unresolved);
	}
	if (json)
	{
		fprintf(json, "\n  },\n  \"unresolved_pipelines\": %zu\n}\n", total_unresolved);
		fclose(json);
	}
	for (i = 0; i < (int)s.count; i++)
		shader_record_set_free(&s.items[i].set);
	free(s.items);
	return exit_code;
}

/* ---- canonical and merge ---- */

/* Writes every map of FOLDER-or-LISTS merged with ADDITIONS into OUT, after checking all of them. LISTS may be
   NULL (canonical). The result must be closed and pass admit and lint, or nothing is written. */
static int rebuild(const char *out, const char *lists, char **folders, int folder_count, int canonical_only)
{
	char names[MAX_MAPS][64];
	struct shader_record_set results[MAX_MAPS];
	int i, n = 0, ok = 1, f, bad = 0;

	memset(results, 0, sizeof(results));
	if (canonical_only)
		n = list_maps(folders[0], names, MAX_MAPS);
	else
	{
		n = list_maps(lists, names, MAX_MAPS);
		for (f = 0; f < folder_count; f++)
		{
			char found[MAX_MAPS][64];
			int count = list_maps(folders[f], found, MAX_MAPS), j, k;

			for (j = 0; j < count; j++)
			{
				for (k = 0; k < n && strcmp(names[k], found[j]); k++) ;
				if (k == n && n < MAX_MAPS) memcpy(names[n++], found[j], 64);
			}
		}
	}
	for (i = 0; i < n; i++)
	{
		struct shader_record_set *set = &results[i], unresolved = {0}, existing = {0};
		struct sources s = {0};
		char path[1200];
		size_t before;

		if (canonical_only)
		{
			map_path(path, sizeof(path), folders[0], names[i]);
			if (!read_records(path, set, 1, &bad)) bad++;
		}
		else
		{
			map_path(path, sizeof(path), lists, names[i]);
			if (access(path, 0) == 0 && !read_records(path, &existing, 1, &bad)) bad++;
			for (f = 0; f < folder_count; f++)
			{
				map_path(path, sizeof(path), folders[f], names[i]);
				if (access(path, 0) == 0 && !read_records(path, set, 1, &bad)) bad++;
			}
		}
		before = existing.count;
		/* what is added is closed against the list it joins */
		memcpy(s.current, names[i], 64);
		if (before)
		{
			struct source *source = add_source(&s, names[i], 0);

			if (!source) return 1;
			for (size_t j = 0; j < existing.count; j++) shader_record_set_add(&source->set, &existing.items[j]);
			s.groups = 1;
		}
		if (!shader_record_close(set, lookup, &s, &unresolved))
			return 1;
		for (size_t j = 0; j < existing.count; j++) shader_record_set_add(set, &existing.items[j]);
		shader_record_normalize(set);
		if (unresolved.count)
		{
			char line[SHADER_RECORD_LINE_MAX];

			for (size_t j = 0; j < unresolved.count; j++)
			{
				shader_record_format(&unresolved.items[j], line);
				fprintf(stderr, "%s: unresolved pipeline, pixel shader %016llx is in no list: %.60s\n", names[i],
					(unsigned long long)unresolved.items[j].hash, line);
			}
			ok = 0;
		}
		{
			struct shader_record_problem problems[8];
			size_t found = shader_record_closed_problems(set, problems, 8), j;

			for (j = 0; j < found && j < 8; j++)
				fprintf(stderr, "%s: %s\n", names[i], problems[j].detail);
			if (found) ok = 0;
		}
		if (!verify(names[i], set)) ok = 0;
		for (size_t j = 0; j < s.count; j++) shader_record_set_free(&s.items[j].set);
		free(s.items);
		shader_record_set_free(&existing);
		shader_record_set_free(&unresolved);
	}
	if (bad) ok = 0;
	if (ok)
		for (i = 0; i < n; i++)
			if (results[i].count && !write_map(out, names[i], &results[i]))
				ok = 0;
	for (i = 0; i < n; i++)
		shader_record_set_free(&results[i]);
	if (!ok)
		fputs("shader-record: refused; nothing written\n", stderr);
	return ok ? 0 : 1;
}

static int command_rebuild(int argc, char **argv, int canonical_only)
{
	const char *out;

	if (argc < (canonical_only ? 3 : 4) || strcmp(argv[0], "--out"))
		return usage();
	out = argv[1];
	return canonical_only ? rebuild(out, NULL, argv + 2, argc - 2, 1) : rebuild(out, argv[2], argv + 3, argc - 3, 0);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return usage();
	if (!strcmp(argv[1], "fields")) return command_fields(argc - 2, argv + 2);
	if (!strcmp(argv[1], "explain")) return command_explain(argc - 2, argv + 2);
	if (!strcmp(argv[1], "check")) return command_check(argc - 2, argv + 2);
	if (!strcmp(argv[1], "close")) return command_close(argc - 2, argv + 2);
	if (!strcmp(argv[1], "canonical")) return command_rebuild(argc - 2, argv + 2, 1);
	if (!strcmp(argv[1], "merge")) return command_rebuild(argc - 2, argv + 2, 0);
	return usage();
}
