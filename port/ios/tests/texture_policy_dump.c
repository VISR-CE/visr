/* texture_policy.c over a folder of .map files, in sorted name order, with the embedded policy: one line per key,
"hash<TAB>width<TAB>height<TAB>result<TAB>kind", the format of the reference implementation's (not in this repository) classify's results.tsv */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "texture_policy.h"

/* texture-policy.py sorts the maps by name without ".map" (a10 before a10b, whatever follows) */
static int by_name(const void *a, const void *b)
{
	const char *x = *(char *const *)a, *y = *(char *const *)b;
	size_t m = strlen(x) - 4, n = strlen(y) - 4;
	int order = memcmp(x, y, m < n ? m : n);

	return order ? order : (m > n) - (m < n);
}
static void log_line(void *context, const char *line) { fprintf(stderr, "%s\n", line); (void)context; }

int main(int argc, char **argv)
{
	struct texture_policy_catalog *catalog = texture_policy_catalog_new();
	struct texture_policy_decision *decisions;
	char *names[64], path[2048], error[256] = "";
	size_t count = 0, i, n;
	struct dirent *item;
	DIR *folder;
	clock_t start = clock();

	if (argc != 2 || !(folder = opendir(argv[1])))
		return fprintf(stderr, "usage: texture-policy-dump MAPS_FOLDER\n"), 2;
	while ((item = readdir(folder)) && count < 64)
	{
		size_t length = strlen(item->d_name);

		if (length > 4 && !strcmp(item->d_name + length - 4, ".map"))
			names[count++] = strdup(item->d_name);
	}
	closedir(folder);
	qsort(names, count, sizeof(*names), by_name);
	for (i = 0; i < count; i++)
	{
		snprintf(path, sizeof(path), "%s/%s", argv[1], names[i]);
		if (texture_policy_add_map_file(catalog, path, error, sizeof(error)))
			return fprintf(stderr, "%s\n", error), 1;
	}
	n = texture_policy_classify(catalog, &texture_policy_embedded, &decisions, log_line, NULL);
	for (i = 0; i < n; i++)
		printf("%016llx\t%u\t%u\t%s\t%s\n", (unsigned long long)decisions[i].hash, decisions[i].width,
			decisions[i].height, texture_policy_result_name(decisions[i].result),
			texture_policy_kind_name(decisions[i].kind));
	fprintf(stderr, "%zu maps, %zu keys, %.1f s\n", count, n, (double)(clock() - start) / CLOCKS_PER_SEC);
	free(decisions);
	texture_policy_catalog_free(catalog);
	return 0;
}
