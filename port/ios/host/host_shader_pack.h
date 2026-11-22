/* The shader-list ABI is independent of replay guest-code identity. */
#pragma once
#include <stddef.h>
#include "../../runtime/shader_record.h"
#define HOST_SHADER_PACK_COMPATIBILITY SHADER_RECORD_FORMAT
#define HOST_SHADER_PACK_MAX_MAPS 32
#define HOST_SHADER_PACK_MAX_FILE_BYTES (4 * 1024 * 1024)
#define HOST_SHADER_PACK_MAX_TOTAL_BYTES (16 * 1024 * 1024)
#define HOST_SHADER_PACK_MAX_ENVELOPE_BYTES (16 * 1024 * 1024)
#define HOST_SHADER_PACK_MAX_INDEX_BYTES 65536
#define HOST_SHADER_PACK_MAX_COMPRESSED_FILE_BYTES (4 * 1024 * 1024)
#define HOST_SHADER_PACK_MAX_COMPRESSED_TOTAL_BYTES (16 * 1024 * 1024)
#define HOST_SHADER_LIST_MAX_RECORDS 8192
/* Both lists require complete map-local dependencies; the union rejects conflicting definitions.
   Returned canonical, deduplicated text belongs to the caller. NULL additions validates the base only. */
int host_shader_list_merge(const char *base, size_t base_size, const char *additions, size_t additions_size,
    char **merged, size_t *merged_size);
int host_shader_pack_validate(const char *folder, const char *bundled, void (*log)(const char *));
/* The same admission, reporting every problem instead of stopping at the first: REPORT_PROBLEM gets one line each,
   "FILE:LINE: FIELD=VALUE, allowed MIN-MAX" or "FILE:LINE: missing dependency ...". Returns what validate returns for
   the same folders. (tools/shader-record check) */
typedef void (*host_shader_report)(void *context, const char *message);
int host_shader_pack_check(const char *folder, const char *bundled, void (*log)(const char *),
    host_shader_report report_problem, void *context);
int host_shader_map_name_valid(const char *name);

char *host_shader_list_read_file(const char *path, size_t *size);
