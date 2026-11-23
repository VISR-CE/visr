/* The shader record contract (shader_record.h): parsing, formatting, admission, lint, canonical form. */
#include "shader_record.h"
#include <stdio.h>
#include <string.h>

#define ROW(name, offset, size, count, minimum, maximum, rule, source) \
	{#name, offset, size, count, minimum, maximum, rule, source},
const struct shader_record_field shader_record_ps_fields[] = { SHADER_RECORD_PS_FIELDS(ROW) };
#undef ROW
const size_t shader_record_ps_field_count = sizeof(shader_record_ps_fields) / sizeof(shader_record_ps_fields[0]);

#define ROW(name, maximum, default, source) \
	{#name, maximum, offsetof(struct shader_record, name), offsetof(struct shader_record, default), source},
const struct shader_record_pipeline_field shader_record_pipeline_fields[] = { SHADER_RECORD_PIPELINE_FIELDS(ROW) };
#undef ROW
const size_t shader_record_pipeline_field_count =
	sizeof(shader_record_pipeline_fields) / sizeof(shader_record_pipeline_fields[0]);

size_t shader_record_pipeline_required_count(void)
{
	size_t i, n = 0;

	for (i = 0; i < shader_record_pipeline_field_count; i++)
		if (shader_record_pipeline_fields[i].default_member == shader_record_pipeline_fields[i].member)
			n++;
	return n;
}

/* columns: "pipeline VERTEX PIXEL" is 0-2, the required fields follow, then the attribute kinds, then the optional ones */
size_t shader_record_pipeline_column(size_t index)
{
	return index < shader_record_pipeline_required_count() ? 3 + index : 4 + index;
}

static uint32_t *pipeline_member(struct shader_record *r, size_t member)
{
	return (uint32_t *)((char *)r + member);
}

uint64_t shader_record_hash(const uint8_t *bytes, size_t size)
{
	uint64_t hash = UINT64_C(14695981039346656037);

	while (size--)
		hash = (hash ^ *bytes++) * UINT64_C(1099511628211);
	return hash;
}

static void note(struct shader_record_problem *problem, int reason, const char *field, int index,
	int64_t value, int64_t minimum, int64_t maximum, const char *detail)
{
	if (!problem)
		return;
	memset(problem, 0, sizeof(*problem));
	problem->reason = reason;
	problem->field = field;
	problem->index = index;
	problem->value = value;
	problem->minimum = minimum;
	problem->maximum = maximum;
	if (detail)
		snprintf(problem->detail, sizeof(problem->detail), "%s", detail);
}

/* ---- parse ---- */

static int hex_digit(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* exactly 2 * COUNT lowercase hex digits */
static int hex_bytes(const char *s, size_t count, uint8_t *bytes)
{
	size_t i;

	if (strlen(s) != count * 2)
		return 0;
	for (i = 0; i < count; i++)
	{
		int high = hex_digit(s[2 * i]), low = hex_digit(s[2 * i + 1]);

		if (high < 0 || low < 0)
			return 0;
		bytes[i] = (uint8_t)(high * 16 + low);
	}
	return 1;
}

/* 1 to 10 digits, a value that fits 32 bits (a longer or larger one is out of every range anyway) */
static int number(const char *s, int radix, uint32_t *out)
{
	uint64_t n = 0;

	if (!*s || strlen(s) > 10)
		return 0;
	for (; *s; s++)
	{
		int v = radix == 16 ? hex_digit(*s) : *s >= '0' && *s <= '9' ? *s - '0' : -1;

		if (v < 0)
			return 0;
		n = n * (uint64_t)radix + (uint64_t)v;
	}
	if (n > UINT32_MAX)
		return 0;
	*out = (uint32_t)n;
	return 1;
}

static int hash_value(const char *s, uint64_t *out)
{
	uint8_t bytes[8];
	int i;

	if (!hex_bytes(s, 8, bytes))
		return 0;
	*out = 0;
	for (i = 0; i < 8; i++)
		*out = (*out << 8) | bytes[i];
	return 1;
}

/* HASH.VARIANT.MASK, each part non-empty with no dot */
static int vertex_key(const char *s, struct shader_record *r, struct shader_record_problem *p)
{
	char hash[17], variant[12], mask[12];
	const char *first = strchr(s, '.'), *second = first ? strchr(first + 1, '.') : NULL;
	size_t a, b, c;

	if (!second)
	{
		note(p, SHADER_RECORD_SYNTAX, "vertex", -1, 0, 0, 0, "not HASH.VARIANT.MASK");
		return 0;
	}
	a = (size_t)(first - s); b = (size_t)(second - first - 1); c = strlen(second + 1);
	if (a != 16 || !b || b > 11 || !c || c > 11 || strchr(second + 1, '.'))
	{
		note(p, SHADER_RECORD_SYNTAX, "vertex", -1, 0, 0, 0, "not HASH.VARIANT.MASK");
		return 0;
	}
	memcpy(hash, s, a); hash[a] = 0;
	memcpy(variant, first + 1, b); variant[b] = 0;
	memcpy(mask, second + 1, c); mask[c] = 0;
	if (!hash_value(hash, &r->vertex_hash))
	{
		note(p, SHADER_RECORD_SYNTAX, "vertex_hash", -1, 0, 0, 0, "not 16 lowercase hex digits");
		return 0;
	}
	if (!number(variant, 10, &r->variant))
	{
		note(p, SHADER_RECORD_SYNTAX, "variant", -1, 0, 0, 0, "not a decimal number");
		return 0;
	}
	if (!number(mask, 16, &r->mask))
	{
		note(p, SHADER_RECORD_SYNTAX, "mask", -1, 0, 0, 0, "not a hex number");
		return 0;
	}
	return 1;
}

int shader_record_parse(const char *text, struct shader_record *r, struct shader_record_problem *p)
{
	char copy[1024], *fields[14], *save = NULL, *token;
	int n = 0;

	memset(r, 0, sizeof(*r));
	if (strlen(text) > 1023)
	{
		note(p, SHADER_RECORD_SYNTAX, "line", -1, 0, 0, 0, "longer than 1023 bytes");
		return 0;
	}
	snprintf(copy, sizeof(copy), "%s", text);
	for (token = strtok_r(copy, " \t\r", &save); token && n < 14; token = strtok_r(NULL, " \t\r", &save))
		fields[n++] = token;
	if (token || !n)
	{
		note(p, SHADER_RECORD_SYNTAX, "line", -1, 0, 0, 0, token ? "more than 13 fields" : "empty");
		return 0;
	}
	if (!strcmp(fields[0], "vs"))
	{
		r->kind = SHADER_RECORD_VS;
		if (n != 4)
		{
			note(p, SHADER_RECORD_SYNTAX, "vs", -1, n, 4, 4, "a vs line has 4 fields");
			return 0;
		}
		if (!hash_value(fields[1], &r->vertex_hash))
		{
			note(p, SHADER_RECORD_SYNTAX, "hash", -1, 0, 0, 0, "not 16 lowercase hex digits");
			return 0;
		}
		if (!number(fields[2], 10, &r->variant))
		{
			note(p, SHADER_RECORD_SYNTAX, "variant", -1, 0, 0, 0, "not a decimal number");
			return 0;
		}
		if (!number(fields[3], 16, &r->mask))
		{
			note(p, SHADER_RECORD_SYNTAX, "mask", -1, 0, 0, 0, "not a hex number");
			return 0;
		}
	}
	else if (!strcmp(fields[0], "ps"))
	{
		size_t length = n == 3 ? strlen(fields[2]) : 0;

		r->kind = SHADER_RECORD_PS;
		if (n != 3)
		{
			note(p, SHADER_RECORD_SYNTAX, "ps", -1, n, 3, 3, "a ps line has 3 fields");
			return 0;
		}
		if (!hash_value(fields[1], &r->hash))
		{
			note(p, SHADER_RECORD_SYNTAX, "hash", -1, 0, 0, 0, "not 16 lowercase hex digits");
			return 0;
		}
		if (!length || length % 2 || length > 2 * SHADER_RECORD_KEY_MAX ||
			!hex_bytes(fields[2], length / 2, r->key))
		{
			note(p, SHADER_RECORD_SYNTAX, "key", -1, (int64_t)length, 0, 2 * SHADER_RECORD_KEY_MAX,
				"not an even number of lowercase hex digits");
			return 0;
		}
		r->key_size = (uint32_t)(length / 2);
		r->recorded_hash = r->hash;
	}
	else if (!strcmp(fields[0], "pipeline"))
	{
		size_t required = shader_record_pipeline_required_count(), i;

		r->kind = SHADER_RECORD_PIPELINE;
		if ((size_t)n != 4 + required && (size_t)n != 4 + shader_record_pipeline_field_count)
		{
			note(p, SHADER_RECORD_SYNTAX, "pipeline", -1, n, (int64_t)(4 + required),
				(int64_t)(4 + shader_record_pipeline_field_count), "a pipeline line has 11 or 13 fields");
			return 0;
		}
		if (!vertex_key(fields[1], r, p))
			return 0;
		if (!hash_value(fields[2], &r->hash))
		{
			note(p, SHADER_RECORD_SYNTAX, "pixel_hash", -1, 0, 0, 0, "not 16 lowercase hex digits");
			return 0;
		}
		r->recorded_hash = r->hash;
		if (!hex_bytes(fields[3 + required], SHADER_RECORD_ATTRIBUTES, r->attribute_kinds))
		{
			note(p, SHADER_RECORD_SYNTAX, "attribute_kinds", -1, 0, 0, 0, "not 32 lowercase hex digits");
			return 0;
		}
		for (i = 0; i < shader_record_pipeline_field_count; i++)
		{
			const struct shader_record_pipeline_field *f = &shader_record_pipeline_fields[i];
			size_t column = shader_record_pipeline_column(i);

			if (column >= (size_t)n)
				continue;
			if (!number(fields[column], 10, pipeline_member(r, f->member)))
			{
				note(p, SHADER_RECORD_SYNTAX, f->name, -1, 0, 0, 0, "not a decimal number");
				return 0;
			}
		}
		for (i = 0; i < shader_record_pipeline_field_count; i++)
			if (shader_record_pipeline_column(i) >= (size_t)n)
				*pipeline_member(r, shader_record_pipeline_fields[i].member) =
					*pipeline_member(r, shader_record_pipeline_fields[i].default_member);
	}
	else
	{
		note(p, SHADER_RECORD_SYNTAX, "kind", -1, 0, 0, 0, "not vs, ps or pipeline");
		return 0;
	}
	return 1;
}

/* ---- format ---- */

void shader_record_name(const struct shader_record *r, char *name, size_t size)
{
	if (r->kind == SHADER_RECORD_VS)
		snprintf(name, size, "vs %016llx.%u.%x", (unsigned long long)r->vertex_hash, r->variant, r->mask);
	else if (r->kind == SHADER_RECORD_PS)
		snprintf(name, size, "ps %016llx", (unsigned long long)r->hash);
	else
		snprintf(name, size, "pipeline");
}

void shader_record_vertex_name(const struct shader_record *r, char *name, size_t size)
{
	snprintf(name, size, "vs %016llx.%u.%x", (unsigned long long)r->vertex_hash, r->variant, r->mask);
}

void shader_record_pixel_name(const struct shader_record *r, char *name, size_t size)
{
	snprintf(name, size, "ps %016llx", (unsigned long long)r->hash);
}

size_t shader_record_format(const struct shader_record *r, char *line)
{
	static const char digits[] = "0123456789abcdef";
	size_t length, i, j;

	if (r->kind == SHADER_RECORD_VS)
		return (size_t)snprintf(line, SHADER_RECORD_LINE_MAX, "vs %016llx %u %x",
			(unsigned long long)r->vertex_hash, r->variant, r->mask);
	if (r->kind == SHADER_RECORD_PS)
	{
		length = (size_t)snprintf(line, SHADER_RECORD_LINE_MAX, "ps %016llx ", (unsigned long long)r->hash);
		for (i = 0; i < r->key_size && i < SHADER_RECORD_KEY_MAX; i++)
		{
			line[length++] = digits[r->key[i] >> 4];
			line[length++] = digits[r->key[i] & 15];
		}
		line[length] = 0;
		return length;
	}
	length = (size_t)snprintf(line, SHADER_RECORD_LINE_MAX, "pipeline %016llx.%u.%x %016llx",
		(unsigned long long)r->vertex_hash, r->variant, r->mask, (unsigned long long)r->hash);
	for (i = 0; i < shader_record_pipeline_field_count; i++)
	{
		if (i == shader_record_pipeline_required_count())
		{
			line[length++] = ' ';
			for (j = 0; j < SHADER_RECORD_ATTRIBUTES; j++)
			{
				line[length++] = digits[r->attribute_kinds[j] >> 4];
				line[length++] = digits[r->attribute_kinds[j] & 15];
			}
		}
		length += (size_t)snprintf(line + length, SHADER_RECORD_LINE_MAX - length, " %u",
			*pipeline_member((struct shader_record *)r, shader_record_pipeline_fields[i].member));
	}
	return length;
}

/* ---- admit and lint ---- */

static uint32_t element(const uint8_t *key, const struct shader_record_field *f, uint32_t i)
{
	uint32_t value = 0, b;

	for (b = 0; b < f->size; b++)
		value |= (uint32_t)key[f->offset + i * f->size + b] << (8 * b);
	return value;
}

static int admit_vertex(const struct shader_record *r, struct shader_record_problem *p)
{
	if (r->variant > SHADER_RECORD_VARIANT_MAX)
	{
		note(p, SHADER_RECORD_OUT_OF_RANGE, "variant", -1, r->variant, 0, SHADER_RECORD_VARIANT_MAX, NULL);
		return 0;
	}
	if (r->mask > SHADER_RECORD_MASK_MAX)
	{
		note(p, SHADER_RECORD_OUT_OF_RANGE, "mask", -1, r->mask, 0, SHADER_RECORD_MASK_MAX, NULL);
		return 0;
	}
	if (r->variant && r->mask)
	{
		note(p, SHADER_RECORD_BAD_VARIANT, "mask", -1, r->mask, 0, 0, "variant 1 has no mask");
		return 0;
	}
	return 1;
}

int shader_record_admit(const struct shader_record *r, struct shader_record_problem *p)
{
	size_t i;
	uint32_t e;

	if (r->kind == SHADER_RECORD_VS)
		return admit_vertex(r, p);
	if (r->kind == SHADER_RECORD_PS)
	{
		if (r->key_size != SHADER_RECORD_KEY_BASE_SIZE)
		{
			note(p, SHADER_RECORD_BAD_SIZE, "key_size", -1, r->key_size, SHADER_RECORD_KEY_BASE_SIZE,
				SHADER_RECORD_KEY_BASE_SIZE,
				r->key_size > SHADER_RECORD_KEY_BASE_SIZE ? "an extended or lit key: reduce it to its base" : NULL);
			return 0;
		}
		if (shader_record_hash(r->key, r->key_size) != r->hash)
		{
			note(p, SHADER_RECORD_BAD_HASH, "hash", -1, (int64_t)shader_record_hash(r->key, r->key_size), 0, 0,
				"the key hashes to another value");
			return 0;
		}
		for (i = 0; i < shader_record_ps_field_count; i++)
		{
			const struct shader_record_field *f = &shader_record_ps_fields[i];

			if (f->rule != SHADER_RECORD_RANGE && f->rule != SHADER_RECORD_COMPARE)
				continue;
			for (e = 0; e < f->count; e++)
			{
				uint32_t v = element(r->key, f, e);

				if (f->rule == SHADER_RECORD_COMPARE ? v && (v < 0x200 || v > 0x207) : v < f->minimum || v > f->maximum)
				{
					note(p, f->rule == SHADER_RECORD_COMPARE ? SHADER_RECORD_BAD_COMPARE : SHADER_RECORD_OUT_OF_RANGE,
						f->name, f->count > 1 ? (int)e : -1, v, f->minimum, f->maximum, NULL);
					return 0;
				}
			}
		}
		return 1;
	}
	if (!admit_vertex(r, p))
		return 0;
	for (i = 0; i < shader_record_pipeline_field_count; i++)
	{
		const struct shader_record_pipeline_field *f = &shader_record_pipeline_fields[i];
		uint32_t value = *pipeline_member((struct shader_record *)r, f->member);

		if (value > f->maximum)
		{
			note(p, SHADER_RECORD_OUT_OF_RANGE, f->name, -1, value, 0, f->maximum, NULL);
			return 0;
		}
	}
	for (i = 0; i < SHADER_RECORD_ATTRIBUTES; i++)
		if (r->attribute_kinds[i] > SHADER_RECORD_ATTRIBUTE_KIND_MAX)
		{
			note(p, SHADER_RECORD_OUT_OF_RANGE, "attribute_kinds", (int)i, r->attribute_kinds[i], 0,
				SHADER_RECORD_ATTRIBUTE_KIND_MAX, NULL);
			return 0;
		}
	return 1;
}

static const struct shader_record_field *combiner_field(void)
{
	size_t k;

	for (k = 0; k < shader_record_ps_field_count; k++)
		if (shader_record_ps_fields[k].rule == SHADER_RECORD_COMBINERS)
			return &shader_record_ps_fields[k];
	return NULL; /* the field-table test requires exactly one */
}

static void load_combiners(const struct shader_record *r, const struct shader_record_field *f, uint32_t *words)
{
	uint32_t i;

	for (i = 0; i < f->count; i++)
		words[i] = element(r->key, f, i);
}

int shader_record_lint(const struct shader_record *r, struct shader_record_problem *p)
{
	size_t k;

	if (r->kind != SHADER_RECORD_PS)
		return 1;
	for (k = 0; k < shader_record_ps_field_count; k++)
	{
		const struct shader_record_field *f = &shader_record_ps_fields[k];

		if (f->rule == SHADER_RECORD_COMBINERS)
		{
			uint32_t words[SHADER_RECORD_COMBINER_WORDS], canonical[SHADER_RECORD_COMBINER_WORDS];

			if (r->key_size < f->offset + f->size * f->count + 4)
			{
				note(p, SHADER_RECORD_BAD_SIZE, "key_size", -1, r->key_size, SHADER_RECORD_KEY_BASE_SIZE,
					SHADER_RECORD_KEY_BASE_SIZE, NULL);
				return 0;
			}
			load_combiners(r, f, words);
			if ((words[SHADER_RECORD_COMBINER_COUNT] & 0xff) > SHADER_RECORD_COMBINER_COUNT_MAX)
			{
				note(p, SHADER_RECORD_OUT_OF_RANGE, "combiner_count", -1, words[SHADER_RECORD_COMBINER_COUNT] & 0xff,
					0, SHADER_RECORD_COMBINER_COUNT_MAX, NULL);
				return 0;
			}
			memcpy(canonical, words, sizeof(words));
			shader_record_canonicalize_combiners(canonical);
			if (memcmp(canonical, words, sizeof(words)))
			{
				note(p, SHADER_RECORD_NOT_CANONICAL, f->name, -1, 0, 0, 0,
					"registers past the combiner count are not zero (canonical form)");
				return 0;
			}
		}
		else if (f->rule == SHADER_RECORD_TEXMODES)
		{
			uint32_t modes = element(r->key, f, 0), i;

			for (i = 0; i < 4; i++)
				if (((modes >> (5 * i)) & 31) > SHADER_RECORD_TEXTURE_MODE_MAX)
				{
					note(p, SHADER_RECORD_OUT_OF_RANGE, f->name, (int)i, (modes >> (5 * i)) & 31, 0,
						SHADER_RECORD_TEXTURE_MODE_MAX, NULL);
					return 0;
				}
		}
	}
	return 1;
}

int shader_record_canonicalize(struct shader_record *r)
{
	const struct shader_record_field *f = combiner_field();
	uint32_t words[SHADER_RECORD_COMBINER_WORDS], i, b;
	uint8_t before[4 * SHADER_RECORD_COMBINER_WORDS];
	uint64_t hash;

	if (r->kind != SHADER_RECORD_PS || r->key_size < f->offset + f->size * f->count)
		return 0;
	memcpy(before, r->key + f->offset, sizeof(before));
	load_combiners(r, f, words);
	shader_record_canonicalize_combiners(words);
	for (i = 0; i < f->count; i++)
		for (b = 0; b < f->size; b++)
			r->key[f->offset + i * f->size + b] = (uint8_t)(words[i] >> (8 * b));
	hash = shader_record_hash(r->key, r->key_size);
	if (!memcmp(before, r->key + f->offset, sizeof(before)) && hash == r->hash)
		return 0;
	r->hash = hash;
	return 1;
}

int shader_record_reduce(struct shader_record *r)
{
	if (r->kind == SHADER_RECORD_PS)
	{
		uint32_t size = r->key_size;
		uint64_t hash = r->hash;

		if (r->key_size > SHADER_RECORD_KEY_BASE_SIZE)
		{
			memset(r->key + SHADER_RECORD_KEY_BASE_SIZE, 0, r->key_size - SHADER_RECORD_KEY_BASE_SIZE);
			r->key_size = SHADER_RECORD_KEY_BASE_SIZE;
		}
		shader_record_canonicalize(r);
		if (r->key_size == size && r->hash == hash)
			return 0;
		r->hash = shader_record_hash(r->key, r->key_size);
		return 1;
	}
	if (r->variant & 2)
	{
		r->variant &= 1;
		return 1;
	}
	return 0;
}

/* ---- problem text ---- */

const char *shader_record_problem_text(const struct shader_record_problem *p, char *out, size_t size)
{
	char field[64];

	if (p->index >= 0)
		snprintf(field, sizeof(field), "%s[%d]", p->field ? p->field : "?", p->index);
	else
		snprintf(field, sizeof(field), "%s", p->field ? p->field : "?");
	switch (p->reason)
	{
	case SHADER_RECORD_OUT_OF_RANGE:
		snprintf(out, size, "%s=%lld, allowed %lld-%lld", field, (long long)p->value, (long long)p->minimum,
			(long long)p->maximum);
		break;
	case SHADER_RECORD_BAD_COMPARE:
		snprintf(out, size, "%s=0x%llx, allowed 0 or 0x200-0x207", field, (unsigned long long)p->value);
		break;
	case SHADER_RECORD_BAD_HASH:
		snprintf(out, size, "%s: %s (%016llx)", field, p->detail, (unsigned long long)p->value);
		break;
	case SHADER_RECORD_BAD_SIZE:
		snprintf(out, size, "%s=%lld, allowed %lld%s%s", field, (long long)p->value, (long long)p->minimum,
			p->detail[0] ? "; " : "", p->detail);
		break;
	default:
		snprintf(out, size, "%s: %s", field, p->detail);
		break;
	}
	return out;
}
