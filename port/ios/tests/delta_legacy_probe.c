/* Sign every parser case with a disposable test key so malformed documents
   exercise the authenticated parser as well as the signature boundary. */
#include "halo_delta_legacy.h"
#include "halo_network_version.h"
#include "monocypher-ed25519.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct delta_legacy_key key;
static unsigned char secret[64];
static unsigned char signed_table[DELTA_LEGACY_SIGNED_MAX + 1];
static const char valid[] = "{\"delta_legacy\":1,\"serial\":1,\"issued\":1791331200,"
	"\"wires\":{\"chupa-38a\":{\"minimum\":29,\"announce\":42,\"maximum\":42,\"follows\":\"build-145.A_b\"}},"
	"\"disabled_capabilities\":[\"unknown\",\"platform\"],\"platform_policy\":{\"pc_windows\":{\"players\":4096}}}";
static size_t sign_document(const char *document)
{
	static const char digits[] = "0123456789abcdef";
	unsigned char signature[64];
	size_t i, size = strlen(document);
	assert(size <= DELTA_LEGACY_DOCUMENT_MAX);
	crypto_ed25519_sign(signature, secret, (const unsigned char *)document, size);
	for (i = 0; i < 64; i++)
	{
		signed_table[i * 2] = digits[signature[i] >> 4];
		signed_table[i * 2 + 1] = digits[signature[i] & 15];
	}
	signed_table[128] = '\n'; memcpy(signed_table + 129, document, size);
	return size + 129;
}
static int verify(const char *document)
{
	struct delta_legacy_table table;
	return delta_legacy_verify(signed_table, sign_document(document), &key, 1, &table);
}
static void row_document(char *out, size_t capacity, unsigned serial, const char *wire,
	unsigned minimum, unsigned announce, unsigned maximum, const char *extra)
{
	snprintf(out, capacity, "{\"delta_legacy\":1,\"serial\":%u,\"wires\":{\"%s\":{"
		"\"minimum\":%u,\"announce\":%u,\"maximum\":%u%s}}}", serial, wire, minimum, announce, maximum, extra);
}
static void parser_checks(void)
{
	struct delta_legacy_table table;
	struct delta_legacy_key wrong = {{1}, 255};
	size_t size, i;
	char document[2000];
	char padded[DELTA_LEGACY_DOCUMENT_MAX + 1];
	assert(verify(valid)); size = sign_document(valid);
	assert(delta_legacy_verify(signed_table, size, &key, 1, &table));
	assert(table.row.serial == 1 && table.row.minimum == 29 && table.row.announce == 42 && table.row.maximum == 43);
	assert(table.disabled_count == 1 && !strcmp(table.disabled[0], "platform"));
	assert(!strcmp(table.row.follows, "build-145.A_b"));
	assert(!delta_legacy_verify(signed_table, size, &wrong, 1, &table));
	signed_table[128] = '\r';
	assert(!delta_legacy_verify(signed_table, size, &key, 1, &table));
	size = sign_document(valid);

	signed_table[0] = signed_table[0] == '0' ? '1' : '0';
	assert(!delta_legacy_verify(signed_table, size, &key, 1, &table));
	size = sign_document(valid); signed_table[130] ^= 1;
	assert(!delta_legacy_verify(signed_table, size, &key, 1, &table));
	size = sign_document(valid);
	for (i = 0; i < size; i++) assert(!delta_legacy_verify(signed_table, i, &key, 1, &table));
	assert(!delta_legacy_verify(signed_table, DELTA_LEGACY_SIGNED_MAX + 1, &key, 1, &table));
	/* Re-sign truncated documents to reach the parser past authentication. */
	for (i = 0; i < strlen(valid); i++)
	{
		memcpy(document, valid, i); document[i] = 0;
		assert(!verify(document));
	}
	memset(padded, ' ', sizeof(padded) - 1); memcpy(padded, valid, strlen(valid));
	padded[sizeof(padded) - 1] = 0; assert(verify(padded));

	row_document(document, sizeof(document), 0x01000001, "chupa-38a", 29, 42, 42, "");
	assert(!verify(document)); key.last_epoch = 1; assert(verify(document)); key.last_epoch = 0;
	row_document(document, sizeof(document), 0, "chupa-38a", 29, 42, 42, ""); assert(!verify(document));
	row_document(document, sizeof(document), 0xFFFFFFFFu, "chupa-38a", 29, 42, 42, ""); assert(!verify(document));
	key.last_epoch = 255;
	row_document(document, sizeof(document), 0xFFFFFFFEu, "chupa-38a", 29, 42, 42, ""); assert(verify(document));
	key.last_epoch = 0;
	row_document(document, sizeof(document), 1, "chupa-38a", 43, 42, 43, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 0, 42, 42, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 0, 42, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 42, 0, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 65536, 65536, 65536, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 42, 41, ""); assert(!verify(document));

	row_document(document, sizeof(document), 1, "chupa-38a", 29, 65536, 65536, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 42, 65536, ""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 42, 42, ",\"follows\":\"bad/name\""); assert(!verify(document));
	row_document(document, sizeof(document), 1, "chupa-38a", 29, 42, 42, ",\"follows\":\"12345678901234567890123456789012\""); assert(!verify(document));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"serial\":2,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"s\\u0065rial\":2,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"unknown\":{\"x\":0,\"x\":1}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"unknown\":[[[[[[[[]]]]]]]]}"));
	assert(verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"unknown\":[[[[[[[]]]]]]]}"));
	assert(verify("{\"unknown\":{\"a\":[null,true,false,-1.25e+2]},\"delta_legacy\":1,\"serial\":1,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":2,\"serial\":1,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1.0,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":01,\"wires\":{}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"platform_policy\":{\"Bad\":{}}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"platform_policy\":{\"pc\":{\"players\":4097}}}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"platform_policy\":[]}"));
	assert(!verify("{\"delta_legacy\":1,\"serial\":1,\"wires\":{},\"unknown\":nonsense}"));
}
static void offer_checks(void)
{
	unsigned version;
	size_t size;
	char document[600];
	unsigned char copy[DELTA_LEGACY_SIGNED_MAX];
	delta_legacy_initialize(1, NULL, &key, 1);
	for (version = 0; version < 100; version++)
		assert(halo_network_version_joinable(38, version) == ((version >= 29 && version <= 32) ||
			(version >= 34 && version <= 36) || (version >= 38 && version <= 41)));
	assert(delta_legacy_announce_version() == 38 && !delta_legacy_serial());
	row_document(document, sizeof(document), 1, "another-wire", 29, 42, 42, "");
	size = sign_document(document); assert(delta_legacy_offer(signed_table, size));
	assert(!halo_network_version_joinable(38, 42) && delta_legacy_announce_version() == 38);
	size = sign_document(valid); assert(!delta_legacy_offer(signed_table, size));
	row_document(document, sizeof(document), 2, "chupa-38a", 29, 42, 42, "");
	size = sign_document(document); assert(delta_legacy_offer(signed_table, size));
	assert(delta_legacy_serial() == 2 && delta_legacy_signed(copy, sizeof(copy)) == size);
	assert(!memcmp(copy, signed_table, size) && !delta_legacy_signed(copy, size - 1));
	assert(!delta_legacy_offer(signed_table, size));
	assert(halo_network_version_joinable(38, 42) && halo_network_version_joinable(38, 43));
	assert(!halo_network_version_joinable(38, 44) && !halo_network_version_joinable(38, 33) && !halo_network_version_joinable(38, 37));
	assert(!delta_legacy_presumed(42) && delta_legacy_presumed(43));
	assert(delta_legacy_announce_version() == 42);
	row_document(document, sizeof(document), 3, "chupa-38a", 29, 44, 44, ",\"requires\":[\"protection\"]");
	size = sign_document(document); assert(delta_legacy_offer(signed_table, size));
	assert(delta_legacy_announce_version() == 38 && halo_network_version_joinable(38, 44));
	row_document(document, sizeof(document), 4, "chupa-38a", 29, 38, 38, "");
	size = sign_document(document); assert(delta_legacy_offer(signed_table, size));
	assert(!halo_network_version_joinable(38, 42) && halo_network_version_joinable(38, 41));
	row_document(document, sizeof(document), 5, "chupa-38a", 45, 45, 45, "");
	size = sign_document(document); assert(delta_legacy_offer(signed_table, size));
	assert(!halo_network_version_joinable(38, 42) && halo_network_version_joinable(38, 45));
	assert(halo_network_version_joinable(38, 29) && halo_network_version_joinable(38, 38));
	delta_legacy_initialize(1, NULL, &key, 1);
	size = sign_document(valid); assert(delta_legacy_offer(signed_table, size));
	assert(delta_legacy_capability_disabled("platform") && !delta_legacy_capability_disabled("unknown"));
	delta_legacy_initialize(0, NULL, &key, 1);
	assert(!delta_legacy_offer(signed_table, size) && !delta_legacy_serial() && delta_legacy_announce_version() == 38);
}
static void cache_checks(void)
{
	char directory[] = "/tmp/delta-legacy-XXXXXX", path[256];
	unsigned char bytes[DELTA_LEGACY_SIGNED_MAX];
	struct dirent *entry;
	DIR *dir;
	FILE *file;
	size_t size;
	assert(mkdtemp(directory));
	snprintf(path, sizeof(path), "%s/delta_legacy.signed", directory);
	delta_legacy_initialize(1, directory, &key, 1);
	size = sign_document(valid); assert(delta_legacy_offer(signed_table, size));
	assert(delta_legacy_cache_write());
	file = fopen(path, "rb"); assert(file);
	assert(fread(bytes, 1, sizeof(bytes), file) == size && !memcmp(bytes, signed_table, size)); fclose(file);
	dir = opendir(directory); assert(dir);
	while ((entry = readdir(dir))) assert(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") || !strcmp(entry->d_name, "delta_legacy.signed"));
	closedir(dir);
	delta_legacy_initialize(1, directory, &key, 1); assert(delta_legacy_serial() == 1);
	assert(!delta_legacy_cache_read());
	delta_legacy_initialize(0, directory, &key, 1); assert(!delta_legacy_serial() && !delta_legacy_cache_read());
	file = fopen(path, "r+b"); assert(file); assert(fputc('z', file) == 'z'); fclose(file);
	delta_legacy_initialize(1, directory, &key, 1); assert(!delta_legacy_serial());
	assert(unlink(path) == 0 && rmdir(directory) == 0);
}
int main(void)
{
	unsigned char seed[32] = {7};
	crypto_ed25519_key_pair(secret, key.public_key, seed);
	parser_checks(); offer_checks(); cache_checks();
	assert(delta_legacy_fetch_due(0, 0, 0));
	assert(!delta_legacy_fetch_due(100, 101, 1) && delta_legacy_fetch_due(101, 101, 1));
	assert(delta_legacy_fetch_next(0, 1) == 14400000 && delta_legacy_fetch_next(0, 0) == 1800000);
	assert(!delta_legacy_fetch_due(UINT32_MAX - 1, 100, 1) && delta_legacy_fetch_due(100, 100, 1));
	assert(halo_network_version_has_join_pass_field(42));
	puts("delta legacy probe ok");
	return 0;
}
