/* SPDX-License-Identifier: Apache-2.0 */
#include "map_pictures.h"

#include <stdio.h>
#include <string.h>

static int identifier_valid(const char *text, size_t size)
{
	size_t i;
	for (i = 0; i < size && text[i]; i++)
	{
		unsigned char c = (unsigned char)text[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == ':' || c == '_' || c == '-')) return 0;
	}
	return i > 0 && i < size;
}

static int metadata_valid(const struct map_pictures_metadata *m)
{
	size_t i;
	if (!m || !identifier_valid(m->map_key, sizeof(m->map_key)) ||
		!m->renderer_version || !m->width || !m->height ||
		m->width > MAP_PICTURES_MAX_DIMENSION || m->height > MAP_PICTURES_MAX_DIMENSION ||
		(m->texture_set != MAP_PICTURES_ORIGINAL && m->texture_set != MAP_PICTURES_UPSCALED) ||
		(m->pre_game != 0 && m->pre_game != 1)) return 0;
	if (m->framing == MAP_PICTURES_CURATED)
	{
		if (m->auto_version || !m->framing_revision) return 0;
	}
	else if (m->framing != MAP_PICTURES_AUTO || !m->auto_version) return 0;
	for (i = 0; i < 64; i++)
		if (!((m->map_hash[i] >= '0' && m->map_hash[i] <= '9') ||
			(m->map_hash[i] >= 'a' && m->map_hash[i] <= 'f'))) return 0;
	return m->map_hash[64] == 0;
}

int map_pictures_meta_write(char *buf, size_t size, const struct map_pictures_metadata *m)
{
	char framing[32];
	int length;
	if (!buf || !size) return 0;
	buf[0] = 0;
	if (!metadata_valid(m)) return 0;
	if (m->framing == MAP_PICTURES_AUTO)
		snprintf(framing, sizeof(framing), "auto:%lu", (unsigned long)m->auto_version);
	else strcpy(framing, "curated");
	length = snprintf(buf, size,
		"{\"schema\":1,\"map_key\":\"%s\",\"framing_revision\":%lu,"
		"\"renderer_version\":%lu,\"texture_set\":\"%s\",\"map_hash\":\"%s\","
		"\"width\":%lu,\"height\":%lu,\"framing\":\"%s\",\"pre_game\":%s}",
		m->map_key, (unsigned long)m->framing_revision, (unsigned long)m->renderer_version,
		m->texture_set == MAP_PICTURES_UPSCALED ? "upscaled" : "original", m->map_hash,
		(unsigned long)m->width, (unsigned long)m->height, framing, m->pre_game ? "true" : "false");
	if (length < 0 || (size_t)length >= size || length >= MAP_PICTURES_META_SIZE)
	{
		buf[0] = 0;
		return 0;
	}
	return 1;
}

/* photo_framing.c's JSON reader is private and accepts nested shot data.
   This fixed schema needs only bounded strings, unsigned integers and booleans. */
struct reader { const char *p, *end; };

static void whitespace(struct reader *r)
{
	while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\r' || *r->p == '\n')) r->p++;
}

static int take(struct reader *r, char c)
{
	whitespace(r);
	if (r->p == r->end || *r->p != c) return 0;
	r->p++;
	return 1;
}

static int string_read(struct reader *r, char *buf, size_t size)
{
	size_t n = 0;
	if (!take(r, '"')) return 0;
	while (r->p < r->end && *r->p != '"')
	{
		unsigned char c = (unsigned char)*r->p++;
		/* Identifiers never need escapes or non-ASCII bytes. */
		if (c < 32 || c > 126 || c == '\\' || n + 1 >= size) return 0;
		buf[n++] = (char)c;
	}
	buf[n] = 0;
	return r->p < r->end && *r->p++ == '"';
}

static int unsigned_read(struct reader *r, uint32_t *out)
{
	uint32_t value = 0;
	const char *start;
	whitespace(r);
	start = r->p;
	while (r->p < r->end && *r->p >= '0' && *r->p <= '9')
	{
		unsigned int digit = (unsigned int)(*r->p++ - '0');
		if (value > (UINT32_MAX - digit) / 10) return 0;
		value = value * 10 + digit;
	}
	if (r->p == start || (r->p - start > 1 && *start == '0')) return 0;
	*out = value;
	return 1;
}

int map_pictures_meta_parse(const char *json, size_t length, struct map_pictures_metadata *out)
{
	static const char *fields[] = {"schema", "map_key", "framing_revision", "renderer_version",
		"texture_set", "map_hash", "width", "height", "framing", "pre_game"};
	struct map_pictures_metadata m = {0};
	struct reader r;
	unsigned int seen = 0;
	if (!json || !out || !length || length >= MAP_PICTURES_META_SIZE) return 0;
	r.p = json; r.end = json + length;
	if (!take(&r, '{')) return 0;
	for (;;)
	{
		char key[32], text[32];
		uint32_t number;
		size_t field;
		if (!string_read(&r, key, sizeof(key)) || !take(&r, ':')) return 0;
		for (field = 0; field < sizeof(fields) / sizeof(fields[0]); field++)
			if (!strcmp(key, fields[field])) break;
		if (field == sizeof(fields) / sizeof(fields[0]) || (seen & (1u << field))) return 0;
		seen |= 1u << field;
		switch (field)
		{
		case 0: if (!unsigned_read(&r, &number) || number != 1) return 0; break;
		case 1: if (!string_read(&r, m.map_key, sizeof(m.map_key))) return 0; break;
		case 2: if (!unsigned_read(&r, &m.framing_revision)) return 0; break;
		case 3: if (!unsigned_read(&r, &m.renderer_version)) return 0; break;
		case 4:
			if (!string_read(&r, text, sizeof(text))) return 0;
			if (!strcmp(text, "original")) m.texture_set = MAP_PICTURES_ORIGINAL;
			else if (!strcmp(text, "upscaled")) m.texture_set = MAP_PICTURES_UPSCALED;
			else return 0;
			break;
		case 5: if (!string_read(&r, m.map_hash, sizeof(m.map_hash))) return 0; break;
		case 6: if (!unsigned_read(&r, &m.width)) return 0; break;
		case 7: if (!unsigned_read(&r, &m.height)) return 0; break;
		case 8:
			if (!string_read(&r, text, sizeof(text))) return 0;
			if (!strcmp(text, "curated")) m.framing = MAP_PICTURES_CURATED;
			else
			{
				struct reader version;
				if (strncmp(text, "auto:", 5)) return 0;
				version.p = text + 5; version.end = text + strlen(text);
				if (version.p == version.end || *version.p < '0' || *version.p > '9' ||
					!unsigned_read(&version, &m.auto_version) || version.p != version.end) return 0;
				m.framing = MAP_PICTURES_AUTO;
			}
			break;
		case 9:
			whitespace(&r);
			if (r.end - r.p >= 4 && !memcmp(r.p, "true", 4)) { m.pre_game = 1; r.p += 4; }
			else if (r.end - r.p >= 5 && !memcmp(r.p, "false", 5)) r.p += 5;
			else return 0;
			break;
		}
		if (take(&r, '}')) break;
		if (!take(&r, ',')) return 0;
	}
	whitespace(&r);
	if (r.p != r.end || seen != (1u << 10) - 1 || !metadata_valid(&m)) return 0;
	*out = m;
	return 1;
}

int map_pictures_is_stale(const struct map_pictures_metadata *cached, const struct map_pictures_request *wanted)
{
	const struct map_pictures_metadata *m;
	if (!wanted || !metadata_valid(cached) || !metadata_valid(&wanted->metadata)) return 1;
	m = &wanted->metadata;
	return strcmp(cached->map_key, m->map_key) || strcmp(cached->map_hash, m->map_hash) ||
		cached->framing_revision != m->framing_revision || cached->renderer_version != m->renderer_version ||
		cached->texture_set != m->texture_set || cached->width != m->width || cached->height != m->height ||
		cached->framing != m->framing || cached->auto_version != m->auto_version ||
		(cached->pre_game && (wanted->settle_ticks || wanted->erase_ai));
}

int map_pictures_cache_path(char *buf, size_t size, const char *root, const char *map_key)
{
	static const char hex[] = "0123456789ABCDEF";
	char key[(MAP_PICTURES_KEY_SIZE - 1) * 3 + 1];
	size_t i, n = 0, root_length;
	int length;
	if (!buf || !size) return 0;
	buf[0] = 0;
	if (!root || !*root || !map_key || !*map_key) return 0;
	for (i = 0; i < MAP_PICTURES_KEY_SIZE && map_key[i]; i++)
	{
		unsigned char c = (unsigned char)map_key[i];
		if (i == MAP_PICTURES_KEY_SIZE - 1) return 0;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_') key[n++] = (char)c;
		else { key[n++] = '%'; key[n++] = hex[c >> 4]; key[n++] = hex[c & 15]; }
	}
	key[n] = 0;
	root_length = strlen(root);
	length = snprintf(buf, size, "%s%smap-pictures/%s/", root, root[root_length - 1] == '/' ? "" : "/", key);
	if (length < 0 || (size_t)length >= size) { buf[0] = 0; return 0; }
	return 1;
}

enum map_pictures_source map_pictures_lookup(struct map_pictures_availability a)
{
	if (!a.graphics_mode_hardcore && a.rendered) return MAP_PICTURES_RENDERED;
	if (a.is_custom_map && a.custom_bmp) return MAP_PICTURES_CUSTOM_BMP;
	if (!a.graphics_mode_hardcore && a.is_custom_map && a.auto_picture) return MAP_PICTURES_AUTO_PICTURE;
	if (a.stock) return MAP_PICTURES_STOCK;
	return MAP_PICTURES_NAME_TILE;
}

int map_pictures_render_allowed(enum map_pictures_state state)
{
	return state == MAP_PICTURES_IMPORT || state == MAP_PICTURES_PRE_GAME ||
		state == MAP_PICTURES_NETWORK_CLIENT_PRE_GAME;
}

/* Reject overlong encodings, surrogates and code points above U+10FFFF. */
static size_t utf8_length(const unsigned char *s)
{
	size_t n, i;
	if (*s < 0x80) return *s ? 1 : 0;
	if (*s >= 0xc2 && *s <= 0xdf) n = 2;
	else if (*s >= 0xe0 && *s <= 0xef) n = 3;
	else if (*s >= 0xf0 && *s <= 0xf4) n = 4;
	else return 0;
	for (i = 1; i < n; i++) if (s[i] < 0x80 || s[i] > 0xbf) return 0;
	if ((*s == 0xe0 && s[1] < 0xa0) || (*s == 0xed && s[1] > 0x9f) ||
		(*s == 0xf0 && s[1] < 0x90) || (*s == 0xf4 && s[1] > 0x8f)) return 0;
	return n;
}

size_t map_pictures_name_tile(char *buf, size_t size, const char *name, size_t max_length)
{
	size_t bytes = 0, count = 0;
	const unsigned char *s = (const unsigned char *)name;
	if (!buf || !size) return 0;
	buf[0] = 0;
	if (!s) return 0;
	while (*s && count < max_length)
	{
		size_t n = utf8_length(s);
		if (!n || n > size - bytes - 1) break;
		if (n == 1 && *s >= 'a' && *s <= 'z') buf[bytes] = (char)(*s - 'a' + 'A');
		else memcpy(buf + bytes, s, n);
		bytes += n; count++; s += n;
	}
	buf[bytes] = 0;
	return bytes;
}
