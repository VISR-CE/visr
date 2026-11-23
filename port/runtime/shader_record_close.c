/* Sets of shader records: ordering, deduplication and closing a delta (shader_record.h). */
#include "shader_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shader_record_set_free(struct shader_record_set *set)
{
	free(set->items);
	memset(set, 0, sizeof(*set));
}

int shader_record_set_add(struct shader_record_set *set, const struct shader_record *record)
{
	if (set->count == set->capacity)
	{
		size_t capacity = set->capacity ? set->capacity * 2 : 256;
		struct shader_record *items = realloc(set->items, capacity * sizeof(*items));

		if (!items)
			return 0;
		set->items = items;
		set->capacity = capacity;
	}
	set->items[set->count++] = *record;
	return 1;
}

const struct shader_record *shader_record_set_find_pixel(const struct shader_record_set *set, uint64_t hash)
{
	const struct shader_record *recorded = NULL;
	size_t i;

	for (i = 0; i < set->count; i++)
	{
		const struct shader_record *r = &set->items[i];

		if (r->kind != SHADER_RECORD_PS)
			continue;
		if (r->hash == hash)
			return r;
		if (!recorded && r->recorded_hash == hash)
			recorded = r;
	}
	return recorded;
}

static int compare(const void *a, const void *b)
{
	const struct shader_record *x = a, *y = b;
	char one[SHADER_RECORD_LINE_MAX], two[SHADER_RECORD_LINE_MAX];

	if (x->kind != y->kind)
		return x->kind - y->kind;
	shader_record_format(x, one);
	shader_record_format(y, two);
	return strcmp(one, two);
}

int shader_record_normalize(struct shader_record_set *set)
{
	size_t i, kept = 0;

	qsort(set->items, set->count, sizeof(*set->items), compare);
	for (i = 0; i < set->count; i++)
		if (!kept || compare(&set->items[i], &set->items[kept - 1]))
			set->items[kept++] = set->items[i];
	set->count = kept;
	return 1;
}

static int has_vertex(const struct shader_record_set *set, const struct shader_record *pipeline)
{
	size_t i;

	for (i = 0; i < set->count; i++)
	{
		const struct shader_record *r = &set->items[i];

		if (r->kind == SHADER_RECORD_VS && r->vertex_hash == pipeline->vertex_hash && r->variant == pipeline->variant &&
			r->mask == pipeline->mask)
			return 1;
	}
	return 0;
}

int shader_record_close(struct shader_record_set *set, shader_record_lookup lookup, void *context,
	struct shader_record_set *unresolved)
{
	size_t i, kept = 0, count;

	for (i = 0; i < set->count; i++)
		shader_record_reduce(&set->items[i]); /* also a pipeline's vertex variant */
	/* A pipeline follows its pixel shader to the canonical hash now: normalizing folds ps records that reduce to
	one key and keeps only one recorded hash, so the others would be lost */
	for (i = 0; i < set->count; i++)
		if (set->items[i].kind == SHADER_RECORD_PIPELINE)
		{
			const struct shader_record *found = shader_record_set_find_pixel(set, set->items[i].hash);

			if (found)
				set->items[i].hash = found->hash;
		}
	shader_record_normalize(set);
	count = set->count;
	for (i = 0; i < count; i++)
	{
		struct shader_record *r = &set->items[i];
		const struct shader_record *found;

		if (r->kind != SHADER_RECORD_PIPELINE)
			continue;
		found = shader_record_set_find_pixel(set, r->hash);
		if (!found && lookup && (found = lookup(context, r->hash)) != NULL)
		{
			struct shader_record copy = *found;

			shader_record_reduce(&copy);
			copy.recorded_hash = r->hash;
			if (!shader_record_set_add(set, &copy))
				return 0;
			r = &set->items[i]; /* the add may have moved the items */
			found = &set->items[set->count - 1];
		}
		if (!found)
		{
			if (unresolved && !shader_record_set_add(unresolved, r))
				return 0;
			r->kind = -1; /* dropped below */
			continue;
		}
		r->hash = found->hash;
		if (!has_vertex(set, r))
		{
			struct shader_record vertex;

			memset(&vertex, 0, sizeof(vertex));
			vertex.kind = SHADER_RECORD_VS;
			vertex.vertex_hash = r->vertex_hash; vertex.variant = r->variant; vertex.mask = r->mask;
			if (!shader_record_set_add(set, &vertex))
				return 0;
		}
	}
	for (i = 0; i < set->count; i++)
		if (set->items[i].kind >= 0)
			set->items[kept++] = set->items[i];
	set->count = kept;
	return shader_record_normalize(set);
}

size_t shader_record_closed_problems(const struct shader_record_set *set, struct shader_record_problem *out,
	size_t max)
{
	size_t i, found = 0;

	for (i = 0; i < set->count; i++)
	{
		const struct shader_record *r = &set->items[i];
		char name[64];
		int vertex_missing, pixel_missing;
		const struct shader_record *pixel;

		if (r->kind != SHADER_RECORD_PIPELINE)
			continue;
		vertex_missing = !has_vertex(set, r);
		pixel = shader_record_set_find_pixel(set, r->hash);
		pixel_missing = !pixel || pixel->hash != r->hash;
		if (vertex_missing && found++ < max)
		{
			shader_record_vertex_name(r, name, sizeof(name));
			memset(&out[found - 1], 0, sizeof(*out));
			out[found - 1].reason = SHADER_RECORD_MISSING;
			out[found - 1].field = "vertex";
			out[found - 1].index = -1;
			snprintf(out[found - 1].detail, sizeof(out[found - 1].detail), "missing dependency %s", name);
		}
		if (pixel_missing && found++ < max)
		{
			shader_record_pixel_name(r, name, sizeof(name));
			memset(&out[found - 1], 0, sizeof(*out));
			out[found - 1].reason = SHADER_RECORD_MISSING;
			out[found - 1].field = "pixel_hash";
			out[found - 1].index = -1;
			snprintf(out[found - 1].detail, sizeof(out[found - 1].detail), "missing dependency %s", name);
		}
	}
	return found;
}
