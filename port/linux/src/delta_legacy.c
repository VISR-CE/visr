/* The published Delta document is untrusted even after signature verification.
   Keep parsing bounded independently of the game-list reader's permissive rules. */
#include "halo_delta_legacy.h"
#include "halo_delta_key.h"
#include "monocypher-ed25519.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { NODE_LIMIT = 4096, STRING_LIMIT = 255, KEY_LIMIT = 8 };
struct node { int type, child, next; const char *start; size_t size; };
struct parser
{
	const char *p, *end;
	unsigned count;
	struct node nodes[NODE_LIMIT];
};
static void space(struct parser *p)
{
	while (p->p < p->end && strchr(" \t\r\n", *p->p) && *p->p) p->p++;
}
static int hex(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}
static int code_unit(const char **at, const char *end, unsigned *value)
{
	unsigned n = 0, i;
	for (i = 0; i < 4; i++)
	{
		int digit;
		if (*at == end || (digit = hex((unsigned char)*(*at)++)) < 0) return 0;
		n = n * 16 + (unsigned)digit;
	}
	*value = n;
	return 1;
}
/* Decode keys before comparing them: "serial" and "s\u0065rial" are duplicates.
   Strings are bounded by decoded UTF-8 bytes, including fields we do not use. */
static int string(const char **at, const char *end, char out[STRING_LIMIT + 1])
{
	unsigned length = 0;
	if (*at == end || *(*at)++ != '"') return 0;
	while (*at < end)
	{
		unsigned c = (unsigned char)*(*at)++, value, more = 0, minimum = 0;
		if (c == '"') { out[length] = 0; return 1; }
		if (c < 32) return 0;
		if (c == '\\')
		{
			if (*at == end) return 0;
			c = (unsigned char)*(*at)++;
			switch (c)
			{
			case '"': case '\\': case '/': break;
			case 'b': c = 8; break;
			case 'f': c = 12; break;
			case 'n': c = 10; break;
			case 'r': c = 13; break;
			case 't': c = 9; break;
			case 'u':
				if (!code_unit(at, end, &c)) return 0;
				if (c >= 0xD800 && c <= 0xDBFF)
				{
					if (end - *at < 2 || (*at)[0] != '\\' || (*at)[1] != 'u') return 0;
					*at += 2;
					if (!code_unit(at, end, &value) || value < 0xDC00 || value > 0xDFFF) return 0;
					c = 0x10000 + ((c - 0xD800) << 10) + value - 0xDC00;
				}
				else if (c >= 0xDC00 && c <= 0xDFFF) return 0;
				break;
			default: return 0;
			}
		}
		else if (c >= 128)
		{
			if (c >= 0xC2 && c <= 0xDF) { c &= 31; more = 1; minimum = 128; }
			else if (c >= 0xE0 && c <= 0xEF) { c &= 15; more = 2; minimum = 0x800; }
			else if (c >= 0xF0 && c <= 0xF4) { c &= 7; more = 3; minimum = 0x10000; }
			else return 0;
			while (more--)
			{
				if (*at == end || ((value = (unsigned char)*(*at)++) & 0xC0) != 0x80) return 0;
				c = (c << 6) | (value & 63);
			}
			if (c < minimum || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0;
		}
		/* Embedded NUL would make field and duplicate-key comparisons ambiguous. */
		if (!c || length + (c < 128 ? 1u : c < 0x800 ? 2u : c < 0x10000 ? 3u : 4u) > STRING_LIMIT) return 0;
		if (c < 128) out[length++] = (char)c;
		else
		{
			if (c >= 0x10000) out[length++] = (char)(0xF0 | (c >> 18));
			if (c >= 0x800) out[length++] = (char)((c >= 0x10000 ? 0x80 : 0xE0) | ((c >> 12) & 63));
			out[length++] = (char)((c >= 0x800 ? 0x80 : 0xC0) | ((c >> 6) & 63));
			out[length++] = (char)(0x80 | (c & 63));
		}
	}
	return 0;
}
static int text(const struct node *n, char out[STRING_LIMIT + 1])
{
	const char *p = n->start;
	return n->type == '"' && string(&p, n->start + n->size, out);
}
static int parse(struct parser *p, unsigned depth)
{
	int index, last = 0;
	struct node *n;
	char key[STRING_LIMIT + 1], previous[STRING_LIMIT + 1];
	space(p);
	if (p->p == p->end || p->count + 1 >= NODE_LIMIT) return 0;
	index = (int)++p->count;
	n = &p->nodes[index]; n->start = p->p; n->type = (unsigned char)*p->p;
	if (n->type == '{' || n->type == '[')
	{
		char close = n->type == '{' ? '}' : ']';
		if (depth > 8) return 0;
		p->p++; space(p);
		if (p->p < p->end && *p->p == close) { p->p++; return index; }
		for (;;)
		{
			int child;
			if (n->type == '{')
			{
				int k;
				space(p);
				child = parse(p, depth + 1);
				if (!child || !text(&p->nodes[child], key)) return 0;
				for (k = n->child; k; k = p->nodes[k].next)
				{
					if (!text(&p->nodes[k], previous) || !strcmp(key, previous)) return 0;
				}
				space(p);
				if (p->p == p->end || *p->p++ != ':') return 0;
				p->nodes[child].child = parse(p, depth + 1);
				if (!p->nodes[child].child) return 0;
			}
			else child = parse(p, depth + 1);
			if (!child) return 0;
			if (last) p->nodes[last].next = child;
			else n->child = child;
			last = child;
			space(p);
			if (p->p == p->end) return 0;
			if (*p->p == close) { p->p++; break; }
			if (*p->p++ != ',') return 0;
		}
	}
	else if (n->type == '"')
	{
		if (!string(&p->p, p->end, key)) return 0;
	}
	else if (n->type == 't' || n->type == 'f' || n->type == 'n')
	{
		const char *word = n->type == 't' ? "true" : n->type == 'f' ? "false" : "null";
		size_t size = strlen(word);
		if ((size_t)(p->end - p->p) < size || memcmp(p->p, word, size)) return 0;
		p->p += size;
	}
	else
	{
		if (*p->p == '-') p->p++;
		if (p->p == p->end || *p->p < '0' || *p->p > '9') return 0;
		if (*p->p++ != '0') while (p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
		if (p->p < p->end && *p->p == '.')
		{
			p->p++;
			if (p->p == p->end || *p->p < '0' || *p->p > '9') return 0;
			while (p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
		}
		if (p->p < p->end && (*p->p == 'e' || *p->p == 'E'))
		{
			p->p++;
			if (p->p < p->end && (*p->p == '+' || *p->p == '-')) p->p++;
			if (p->p == p->end || *p->p < '0' || *p->p > '9') return 0;
			while (p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
		}
		n->type = '0';
	}
	n->size = (size_t)(p->p - n->start);
	return index;
}
static int field(struct parser *p, int object, const char *name)
{
	int i;
	char key[STRING_LIMIT + 1];
	if (p->nodes[object].type != '{') return 0;
	for (i = p->nodes[object].child; i; i = p->nodes[i].next)
		if (text(&p->nodes[i], key) && !strcmp(key, name)) return p->nodes[i].child;
	return 0;
}
static int integer(struct parser *p, int index, uint32_t limit, uint32_t *out)
{
	struct node *n = &p->nodes[index];
	size_t i;
	uint32_t value = 0;
	if (!index || n->type != '0' || !n->size) return 0;
	for (i = 0; i < n->size; i++)
	{
		unsigned digit = (unsigned char)n->start[i] - '0';
		if (digit > 9 || value > limit / 10 || (value == limit / 10 && digit > limit % 10)) return 0;
		value = value * 10 + digit;
	}
	*out = value;
	return 1;
}
static int name_valid(const char *name, int follows)
{
	const unsigned char *s = (const unsigned char *)name;
	if (!*s) return 0;
	for (; *s; s++)
		if (!(*s >= 'a' && *s <= 'z') && !(*s >= '0' && *s <= '9') && *s != '_' &&
			!(follows && ((*s >= 'A' && *s <= 'Z') || *s == '.' || *s == '-'))) return 0;
	return 1;
}
static int document(struct parser *p, struct delta_legacy_table *out)
{
	uint32_t value;
	int wires, i, index;
	char key[STRING_LIMIT + 1];
	memset(out, 0, sizeof(*out));
	if (!parse(p, 1)) return 0;
	space(p);
	if (p->p != p->end || p->nodes[1].type != '{' ||
		!integer(p, field(p, 1, "delta_legacy"), 1, &value) || value != 1 ||
		!integer(p, field(p, 1, "serial"), UINT32_MAX - 1, &out->row.serial) || !out->row.serial) return 0;
	index = field(p, 1, "issued");
	/* issued is informational, but must still be a nonnegative whole number. */
	if (index)
	{
		struct node *n = &p->nodes[index]; size_t j;
		if (n->type != '0') return 0;
		for (j = 0; j < n->size; j++) if (n->start[j] < '0' || n->start[j] > '9') return 0;
	}
	wires = field(p, 1, "wires");
	if (!wires || p->nodes[wires].type != '{') return 0;
	for (i = p->nodes[wires].child; i; i = p->nodes[i].next)
	{
		uint32_t minimum, announce, maximum;
		int row = p->nodes[i].child, follows;
		if (p->nodes[row].type != '{' ||
			!integer(p, field(p, row, "minimum"), 65535, &minimum) || !minimum ||
			!integer(p, field(p, row, "announce"), 65535, &announce) || !announce ||
			!integer(p, field(p, row, "maximum"), 65535, &maximum) || !maximum ||
			minimum > announce || announce > maximum) return 0;
		follows = field(p, row, "follows");
		if (follows && (!text(&p->nodes[follows], key) || strlen(key) > 31 || !name_valid(key, 1))) return 0;
		text(&p->nodes[i], key);
		if (!strcmp(key, "chupa-38a"))
		{
			out->row.present = 1; out->row.minimum = minimum; out->row.announce = announce;
			out->row.proven_maximum = maximum;
			if (maximum < announce + DELTA_LEGACY_PRESUMED && announce < 65535) maximum = announce + DELTA_LEGACY_PRESUMED;
			out->row.maximum = maximum < 41 ? 41 : maximum;
			out->row.requires = field(p, row, "requires") != 0;
			if (follows) { text(&p->nodes[follows], key); strcpy(out->row.follows, key); }
		}
	}
	index = field(p, 1, "disabled_capabilities");
	if (index)
	{
		if (p->nodes[index].type != '[') return 0;
		for (i = p->nodes[index].child; i; i = p->nodes[i].next)
		{
			if (!text(&p->nodes[i], key)) return 0;
			/* Only platform is implemented by this build; unknown registry names are ignored. */
			if (!strcmp(key, "platform") && out->disabled_count < 16)
				strcpy(out->disabled[out->disabled_count++], key);
		}
	}
	index = field(p, 1, "platform_policy");
	if (index)
	{
		if (p->nodes[index].type != '{') return 0;
		for (i = p->nodes[index].child; i; i = p->nodes[i].next)
		{
			int j, limits = p->nodes[i].child;
			if (!text(&p->nodes[i], key) || !name_valid(key, 0) || p->nodes[limits].type != '{') return 0;
			for (j = p->nodes[limits].child; j; j = p->nodes[j].next)
				if (!text(&p->nodes[j], key) || !name_valid(key, 0) ||
					!integer(p, p->nodes[j].child, 4096, &value)) return 0;
		}
	}
	return 1;
}
int delta_legacy_verify(const void *bytes, size_t size, const struct delta_legacy_key *keys,
	size_t key_count, struct delta_legacy_table *table)
{
	const unsigned char *data = bytes;
	unsigned char signature[64], nonzero;
	struct parser *p;
	struct delta_legacy_table parsed;
	size_t i, j;
	int valid = 0;
	if (!data || !keys || !table || size <= 129 || size > DELTA_LEGACY_SIGNED_MAX || data[128] != '\n') return 0;
	for (i = 0; i < 64; i++)
	{
		int a = hex(data[i * 2]), b = hex(data[i * 2 + 1]);
		if (a < 0 || b < 0) return 0;
		signature[i] = (unsigned char)((a << 4) | b);
	}
	/* Authenticate before allocating or parsing any JSON. */
	for (i = 0; i < key_count; i++)
	{
		nonzero = 0;
		for (j = 0; j < 32; j++) nonzero |= keys[i].public_key[j];
		if (nonzero && !crypto_ed25519_check(signature, keys[i].public_key, data + 129, size - 129)) break;
	}
	if (i == key_count) return 0;
	p = calloc(1, sizeof(*p));
	if (!p) return 0;
	p->p = (const char *)data + 129; p->end = (const char *)data + size;
	if (document(p, &parsed))
	{
		/* More than one key may match, with different epoch ceilings. */
		for (; i < key_count; i++)
			if ((parsed.row.serial >> 24) <= keys[i].last_epoch &&
				!crypto_ed25519_check(signature, keys[i].public_key, data + 129, size - 129))
			{ *table = parsed; valid = 1; break; }
	}
	free(p);
	return valid;
}

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static struct delta_legacy_table active;
static struct delta_legacy_key accepted[KEY_LIMIT];
static size_t accepted_count, signed_size;
static unsigned char signed_bytes[DELTA_LEGACY_SIGNED_MAX];
static int enabled;
static char cache_path[1200];

static int cache_write_locked(void)
{
#ifdef _WIN32
	/* The Windows CRT cannot atomically replace an existing cache with rename.
	   Keep verified tables in memory until a native replacement helper exists. */
	return 0;
#else
	char temporary[1232];
	FILE *file;
	int ok;
	if (!enabled || !signed_size || !cache_path[0]) return 0;
	if (snprintf(temporary, sizeof(temporary), "%s.tmp", cache_path) >= (int)sizeof(temporary)) return 0;
	file = fopen(temporary, "wb");
	if (!file) return 0;
	ok = fwrite(signed_bytes, 1, signed_size, file) == signed_size;
	if (fclose(file)) ok = 0;
	if (ok) ok = rename(temporary, cache_path) == 0;
	if (!ok) unlink(temporary);
	return ok;
#endif
}
void delta_legacy_initialize(int on, const char *root, const struct delta_legacy_key *keys, size_t count)
{
	pthread_mutex_lock(&table_lock);
	enabled = on; signed_size = 0; memset(&active, 0, sizeof(active));
	cache_path[0] = 0;
	if (root && *root && snprintf(cache_path, sizeof(cache_path), "%s/delta_legacy.signed", root) >= (int)sizeof(cache_path))
		cache_path[0] = 0;
	if (!keys) { keys = delta_legacy_keys; count = sizeof(delta_legacy_keys) / sizeof(delta_legacy_keys[0]); }
	accepted_count = count <= KEY_LIMIT ? count : 0;
	if (accepted_count) memcpy(accepted, keys, accepted_count * sizeof(*keys));
	/* The row joins already read stays in force until the new state replaces it, so there is no window with none. */
	if (!on) delta_legacy_runtime_set(NULL);
	pthread_mutex_unlock(&table_lock);
	if (on && !delta_legacy_cache_read())
	{
		pthread_mutex_lock(&table_lock);
		if (!active.row.serial) delta_legacy_runtime_set(NULL);
		pthread_mutex_unlock(&table_lock);
	}
}
int delta_legacy_offer(const void *bytes, size_t size)
{
	struct delta_legacy_table table;
	int taken = 0;
	pthread_mutex_lock(&table_lock);
	if (enabled && delta_legacy_verify(bytes, size, accepted, accepted_count, &table) && table.row.serial > active.row.serial)
	{
		active = table; signed_size = size; memcpy(signed_bytes, bytes, size);
		delta_legacy_runtime_set(&active.row);
		cache_write_locked();
		taken = 1;
	}
	pthread_mutex_unlock(&table_lock);
	return taken;
}
uint32_t delta_legacy_serial(void)
{
	uint32_t serial;
	pthread_mutex_lock(&table_lock); serial = active.row.serial; pthread_mutex_unlock(&table_lock);
	return serial;
}
size_t delta_legacy_signed(void *bytes, size_t capacity)
{
	size_t size = 0;
	pthread_mutex_lock(&table_lock);
	if (bytes && capacity >= signed_size) { size = signed_size; memcpy(bytes, signed_bytes, size); }
	pthread_mutex_unlock(&table_lock);
	return size;
}
int delta_legacy_capability_disabled(const char *name)
{
	unsigned i; int found = 0;
	pthread_mutex_lock(&table_lock);
	for (i = 0; name && i < active.disabled_count; i++) if (!strcmp(name, active.disabled[i])) found = 1;
	pthread_mutex_unlock(&table_lock);
	return found;
}
int delta_legacy_cache_write(void)
{
	int ok;
	pthread_mutex_lock(&table_lock); ok = cache_write_locked(); pthread_mutex_unlock(&table_lock);
	return ok;
}
int delta_legacy_cache_read(void)
{
	unsigned char bytes[DELTA_LEGACY_SIGNED_MAX + 1];
	FILE *file; size_t size;
	pthread_mutex_lock(&table_lock);
	file = enabled && cache_path[0] ? fopen(cache_path, "rb") : NULL;
	pthread_mutex_unlock(&table_lock);
	if (!file) return 0;
	size = fread(bytes, 1, sizeof(bytes), file);
	if (ferror(file)) size = 0;
	fclose(file);
	return delta_legacy_offer(bytes, size);
}
