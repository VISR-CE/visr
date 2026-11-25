/*
SHADER_RECORD.H

The one definition of a shader record: the line that the recorder writes, the warm-up reads, the app
admits from a feed, the upload writer sends, and the tools merge. A record is one of

    vs HASH VARIANT MASK
    ps HASH KEY
    pipeline VERTEX_HASH.VARIANT.MASK PIXEL_HASH BLEND SOURCE DESTINATION OPERATION WRITE_MASK EXACT_BORDERS
        DEPTH KINDS ALPHA_SOURCE ALPHA_DESTINATION

(the pipeline line is one line; the form before the alpha factors has 11 fields and reads as the alpha factors
equal to the color factors). Everything below is fixed-width and depends on <stdint.h> alone, so the 32-bit
guest, the 64-bit host and the test probes compile it the same way. The key's layout is the table
SHADER_RECORD_PS_FIELDS; xgpu.h asserts that struct nv2a_pixel_shader_key matches it, so a field added to the
struct without a row here does not compile.

Two forms of a file, used by the tools and the app (docs/data-packs.md):

  Closed: every pipeline's vertex and pixel record is in the same file. Bundled lists, feed files and upload
          batches are closed, and so is every file an app admits.
  Delta:  the records a device made beyond what it had warmed. Run intervals and shader-lists-missed files
          are deltas; shader-record close turns one into a closed file.

Three levels of rule, from what the app enforces to what only tools do:

  parse     the text is a well-formed record (token counts, hex, digits). Never looks at ranges.
  admit     exactly what the app's validator has always enforced (host_shader_pack.c): the ranges of the
            key's bytes 232-251, the hash, the variant and mask, the pipeline's ranges. The app and
            shader-pack-probe call this and nothing else, so a rule added here changes what a released
            app's copy of this code admits.
  lint      the further rules tools apply before a record enters the bundled lists. Never linked into
            admission. A rule moves from lint to admit only by a deliberate, separately reviewed change with
            the released-build gate (tools/data_pack.py) green.
*/
#ifndef SHADER_RECORD_H
#define SHADER_RECORD_H

#include <stddef.h>
#include <stdint.h>

#define SHADER_RECORD_FORMAT "shader-lists-2"

/* sizes of a pixel shader key: the base key every list holds, the key with alpha_test_samples or soft_depth
(never recorded; the warm-up derives its shader from the base), and the lit key (lighting_program set) */
#define SHADER_RECORD_KEY_BASE_SIZE 252
#define SHADER_RECORD_KEY_SAMPLES_SIZE 256
#define SHADER_RECORD_KEY_LIT_SIZE 264
#define SHADER_RECORD_KEY_MAX SHADER_RECORD_KEY_LIT_SIZE

/* the combiner state: D3DRS_PS_MAX words first, the combiner count (D3DRS_PSCOMBINERCOUNT) among them, and
four register banks of eight stages (D3DRS_PSALPHAINPUTS0, ...OUTPUTS0, PSRGBINPUTS0, ...OUTPUTS0) */
#define SHADER_RECORD_COMBINER_WORDS 57
#define SHADER_RECORD_COMBINER_COUNT 53
#define SHADER_RECORD_STAGES 8
#define SHADER_RECORD_BANK_ALPHA_INPUTS 0
#define SHADER_RECORD_BANK_ALPHA_OUTPUTS 26
#define SHADER_RECORD_BANK_RGB_INPUTS 34
#define SHADER_RECORD_BANK_RGB_OUTPUTS 45
#define SHADER_RECORD_TEXTURE_MODE_MAX 18 /* PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST */
#define SHADER_RECORD_COMBINER_COUNT_MAX 8

/* a pipeline's ranges (the gpu.h enums they mirror are asserted there) */
#define SHADER_RECORD_FACTOR_MAX 14 /* GPU_BLEND_ONE_MINUS_CONSTANT_ALPHA */
#define SHADER_RECORD_OPERATION_MAX 4 /* GPU_BLEND_OP_MAX */
#define SHADER_RECORD_ATTRIBUTE_KIND_MAX 19 /* GPU_ATTRIBUTE_NORMPACKED3 */
#define SHADER_RECORD_ATTRIBUTES 16 /* GPU_ATTRIBUTE_COUNT */
#define SHADER_RECORD_VARIANT_MAX 1 /* a recorded vertex variant: 0 or 1 (2 and 3 are the lit twins) */
#define SHADER_RECORD_MASK_MAX 65535

/* how a field's value is checked by admit */
enum shader_record_rule
{
	SHADER_RECORD_FREE,     /* not checked */
	SHADER_RECORD_COMBINERS, /* the combiner registers: admit does not check them; lint requires the count word
	                           (index SHADER_RECORD_COMBINER_COUNT) at most SHADER_RECORD_COMBINER_COUNT_MAX and
	                           the canonical form (registers past the count zero) */
	SHADER_RECORD_RANGE,    /* each element in MIN..MAX */
	SHADER_RECORD_COMPARE,  /* 0, or a D3DCMP_* function (Xbox values 0x200..0x207, not desktop D3D's 1..8) */
	SHADER_RECORD_TEXMODES, /* not checked by admit; lint: each of the four 5-bit modes at most
	                           SHADER_RECORD_TEXTURE_MODE_MAX */
	SHADER_RECORD_EXTENDED  /* only in extended keys (past the base size); not checked by admit, which
	                           takes base keys alone */
};

/* The key's fields, in byte order: X(name, offset, size, count, minimum, maximum, rule, source). A field is
COUNT elements of SIZE bytes; a multi-byte element is little-endian. */
#define SHADER_RECORD_PS_FIELDS(X) \
	X(combiner_state, 0, 4, 57, 0, 0, SHADER_RECORD_COMBINERS, \
		"D3DRS_PS* render states; constants zeroed, stages past the combiner count zeroed") \
	X(texture_modes, 228, 4, 1, 0, 0, SHADER_RECORD_TEXMODES, "D3DRS_PSTEXTUREMODES: four 5-bit modes") \
	X(sampler_type, 232, 1, 4, 0, 5, SHADER_RECORD_RANGE, \
		"the bound texture's kind per stage: 0 none ... 5 _xgpu_sampler_2d_content (a dynamic-resolution pooled target)") \
	X(alpha_kill, 236, 1, 4, 0, 1, SHADER_RECORD_RANGE, "D3DTSS_ALPHAKILL per stage") \
	X(color_sign, 240, 1, 4, 0, 15, SHADER_RECORD_RANGE, "D3DTSS_COLORSIGN's channel bits per stage") \
	X(alpha_test_function, 244, 4, 1, 0, 0, SHADER_RECORD_COMPARE, \
		"D3DRS_ALPHAFUNC when the alpha test is on, else 0") \
	X(fog_enable, 248, 1, 1, 0, 1, SHADER_RECORD_RANGE, "D3DRS_FOGENABLE") \
	X(fog_table_mode, 249, 1, 1, 0, 3, SHADER_RECORD_RANGE, "D3DRS_FOGTABLEMODE") \
	X(count_samples, 250, 1, 1, 0, 1, SHADER_RECORD_RANGE, "a visibility test with a shader counter") \
	X(coverage_alpha, 251, 1, 1, 0, 3, SHADER_RECORD_RANGE, "high-resolution HUD meter flags") \
	X(alpha_test_samples, 252, 1, 1, 0, 0, SHADER_RECORD_EXTENDED, "a multisampled target's alpha test") \
	X(soft_depth, 253, 1, 1, 0, 0, SHADER_RECORD_EXTENDED, "a soft-edged particle sprite's mode") \
	X(lighting_highlight, 254, 1, 1, 0, 0, SHADER_RECORD_EXTENDED, "display.point_light_highlight's preset") \
	X(lighting_point_lights, 255, 1, 1, 0, 0, SHADER_RECORD_EXTENDED, "display.point_light_limit's extra lights") \
	X(lighting_program, 256, 4, 2, 0, 0, SHADER_RECORD_EXTENDED, \
		"the lit vertex program's hash, low word first; 0 unlit") \
	X(traced_shadow, 264, 4, 1, 0, 0, SHADER_RECORD_EXTENDED, "a traced object shadow's apply draw; never recorded") \
	X(light_visibility, 268, 4, 1, 0, 0, SHADER_RECORD_EXTENDED, \
		"what blocks a dynamic light's draw: 1 traced, 2 shadow map; never recorded")

/* A pipeline's numeric fields, required ones first: X(name, maximum, default, source). Each is 0..MAXIMUM. A
field whose DEFAULT is another field is optional in the line (the 13-field form) and takes that field's value
when absent; a required field names itself. On the line they follow the two shader names (columns 3 on), with
ATTRIBUTE_KINDS (SHADER_RECORD_ATTRIBUTES bytes, each 0..SHADER_RECORD_ATTRIBUTE_KIND_MAX, as 32 hex digits)
after the required ones. */
#define SHADER_RECORD_PIPELINE_FIELDS(X) \
	X(blend, 1, blend, "blending on") \
	X(source, SHADER_RECORD_FACTOR_MAX, source, "the color source factor (GPU_BLEND_*)") \
	X(destination, SHADER_RECORD_FACTOR_MAX, destination, "the color destination factor") \
	X(operation, SHADER_RECORD_OPERATION_MAX, operation, "the blend operation (GPU_BLEND_OP_*)") \
	X(write_mask, 15, write_mask, "the color channels written") \
	X(exact_borders, 15, exact_borders, "the borders drawn exactly") \
	X(depth, 1, depth, "depth testing on") \
	X(alpha_source, SHADER_RECORD_FACTOR_MAX, source, "the alpha source factor") \
	X(alpha_destination, SHADER_RECORD_FACTOR_MAX, destination, "the alpha destination factor")

enum shader_record_kind { SHADER_RECORD_VS, SHADER_RECORD_PS, SHADER_RECORD_PIPELINE };

struct shader_record
{
	int kind;
	/* vs, and a pipeline's vertex key */
	uint64_t vertex_hash;
	uint32_t variant, mask;
	/* ps: the hash as it was written and as it is now (they differ once the key is canonicalized or
	reduced), and the key. A pipeline: the pixel shader's hash in HASH (RECORDED_HASH is the same) */
	uint64_t hash, recorded_hash;
	uint32_t key_size;
	uint8_t key[SHADER_RECORD_KEY_MAX];
	/* the caller's tag for where the record came from (tools only; nothing here reads it) */
	uint32_t origin;
	/* pipeline */
	uint32_t blend, source, destination, operation, write_mask, exact_borders, depth, alpha_source, alpha_destination;
	uint8_t attribute_kinds[SHADER_RECORD_ATTRIBUTES];
};

enum shader_record_reason
{
	SHADER_RECORD_SYNTAX,     /* parse: DETAIL says what is malformed */
	SHADER_RECORD_OUT_OF_RANGE, /* admit or lint: FIELD=VALUE outside MINIMUM..MAXIMUM */
	SHADER_RECORD_BAD_COMPARE, /* alpha_test_function is neither 0 nor 0x200..0x207 */
	SHADER_RECORD_BAD_HASH,   /* the key does not hash to the hash written (VALUE is the key's hash) */
	SHADER_RECORD_BAD_SIZE,   /* key_size is not the base size (VALUE is the size) */
	SHADER_RECORD_BAD_VARIANT, /* variant 1 with a mask */
	SHADER_RECORD_NOT_CANONICAL, /* lint: combiner registers past the combiner count are not zero */
	SHADER_RECORD_MISSING,    /* a pipeline names a record the file does not hold (DETAIL names it) */
	SHADER_RECORD_CONFLICT,   /* two records of one name with different lines */
	SHADER_RECORD_FILE        /* the file as a whole: size, length, records */
};

struct shader_record_problem
{
	int line;              /* 1-based line in its file; 0 when the caller did not set it */
	int reason;
	const char *field;     /* a static name, from the table */
	int index;             /* the element of an array field, else -1 */
	int64_t value, minimum, maximum;
	char detail[96];
};

#define SHADER_RECORD_LINE_MAX 1100

/* canonical form: the combiner registers of the stages at or past the combiner count (at most 8) are zeroed,
because the translation reads only the stages below it and the rest hold whatever an earlier shader left.
One shader, one key. COMBINER is the key's D3DRS_PS_MAX words, of any unsigned integer type that is 32 bits wide
(the guest's DWORD is not uint32_t), so this is a macro. */
#define SHADER_RECORD_CANONICALIZE_COMBINERS(combiner) \
	do { \
		unsigned shader_record_count = (unsigned)((combiner)[SHADER_RECORD_COMBINER_COUNT] & 0xff), shader_record_stage; \
		for (shader_record_stage = shader_record_count < SHADER_RECORD_STAGES ? shader_record_count : SHADER_RECORD_STAGES; \
			shader_record_stage < SHADER_RECORD_STAGES; shader_record_stage++) \
		{ \
			(combiner)[SHADER_RECORD_BANK_ALPHA_INPUTS + shader_record_stage] = 0; \
			(combiner)[SHADER_RECORD_BANK_ALPHA_OUTPUTS + shader_record_stage] = 0; \
			(combiner)[SHADER_RECORD_BANK_RGB_INPUTS + shader_record_stage] = 0; \
			(combiner)[SHADER_RECORD_BANK_RGB_OUTPUTS + shader_record_stage] = 0; \
		} \
	} while (0)

static inline void shader_record_canonicalize_combiners(uint32_t *combiner)
{
	SHADER_RECORD_CANONICALIZE_COMBINERS(combiner);
}

/* one row of SHADER_RECORD_PS_FIELDS */
struct shader_record_field
{
	const char *name;
	uint32_t offset, size, count;
	uint32_t minimum, maximum;
	int rule;
	const char *source;
};
extern const struct shader_record_field shader_record_ps_fields[];
extern const size_t shader_record_ps_field_count;

/* one row of SHADER_RECORD_PIPELINE_FIELDS */
struct shader_record_pipeline_field
{
	const char *name;
	uint32_t maximum;
	size_t member;         /* offsetof the field in struct shader_record */
	size_t default_member; /* the same for DEFAULT; equal to MEMBER when the field is required */
	const char *source;
};
/* how many of the table's fields a line must carry (the rest may be absent), and a field's column on the line */
size_t shader_record_pipeline_required_count(void);
size_t shader_record_pipeline_column(size_t index);
extern const struct shader_record_pipeline_field shader_record_pipeline_fields[];
extern const size_t shader_record_pipeline_field_count;

/* FNV-1a 64: a ps record's hash is this of its key */
uint64_t shader_record_hash(const uint8_t *bytes, size_t size);

/* Reads one record line (no newline; leading blanks allowed). 1 on success; 0 with *PROBLEM filled. */
int shader_record_parse(const char *line, struct shader_record *record, struct shader_record_problem *problem);

/* The record's line in its current form (a pipeline always with the alpha factors), at most
SHADER_RECORD_LINE_MAX bytes with the terminator. Returns its length. */
size_t shader_record_format(const struct shader_record *record, char *line);

/* the "vs HASH.VARIANT.MASK" and "ps HASH" names records are told apart by (a pipeline has none) */
void shader_record_name(const struct shader_record *record, char *name, size_t size);
/* the vertex name a pipeline depends on, and the pixel name */
void shader_record_vertex_name(const struct shader_record *record, char *name, size_t size);
void shader_record_pixel_name(const struct shader_record *record, char *name, size_t size);

/* 1 if the record passes the app's rules, else 0 with *PROBLEM filled (the first rule it breaks) */
int shader_record_admit(const struct shader_record *record, struct shader_record_problem *problem);
/* 1 if the record also passes the rules tools add */
int shader_record_lint(const struct shader_record *record, struct shader_record_problem *problem);

/* canonicalizes a ps key and rehashes it; no-op for other kinds. Returns 1 if the key changed. */
int shader_record_canonicalize(struct shader_record *record);
/* The unlit base: a ps key loses its lighting, multisample and soft-particle fields (it is cut to the base
size), is canonicalized and rehashed; a vs, or a pipeline's vertex key, drops variant bit 1 (the lit twin's
bit). Returns 1 if the record changed. A pipeline's pixel_hash is the caller's to remap, from the ps record's
recorded_hash to its hash. */
int shader_record_reduce(struct shader_record *record);

/* "FIELD=VALUE, allowed MIN-MAX" and the like; returns OUT */
const char *shader_record_problem_text(const struct shader_record_problem *problem, char *out, size_t size);

/* ---- sets and closing (shader_record_close.c) ---- */

struct shader_record_set
{
	struct shader_record *items;
	size_t count, capacity;
};
void shader_record_set_free(struct shader_record_set *set);
/* appends a copy; 0 if memory ran out */
int shader_record_set_add(struct shader_record_set *set, const struct shader_record *record);
/* the ps whose recorded or current hash is HASH, or NULL */
const struct shader_record *shader_record_set_find_pixel(const struct shader_record_set *set, uint64_t hash);

/* finds a ps by hash (recorded or canonical) among records outside the set being closed */
typedef const struct shader_record *(*shader_record_lookup)(void *context, uint64_t hash);

/* Turns a delta into a closed file, in place:
  - every ps is reduced (a lit key to its base), canonicalized and deduplicated, and each pipeline's pixel
    hash and vertex variant follow it;
  - a pipeline's missing vs is synthesized from its vertex key, which holds the whole vs line;
  - a pipeline's missing ps is taken from LOOKUP (called with the hash as the pipeline wrote it);
  - a pipeline whose ps is found nowhere is moved to *UNRESOLVED (nothing is guessed).
Returns 1, or 0 if memory ran out. The set is left sorted (vs, ps, pipelines, each by line) and without
duplicates. */
int shader_record_close(struct shader_record_set *set, shader_record_lookup lookup, void *context,
	struct shader_record_set *unresolved);
/* Sorts and deduplicates a set of records already in current form (the union, ordering and canonical form the
bundled lists use), without closing it */
int shader_record_normalize(struct shader_record_set *set);
/* Every problem keeping the set from being closed (a pipeline's missing vs or ps; conflicting records of one name are reported by shader_record_normalize's caller
through SHADER_RECORD_CONFLICT, not here),
at most MAX of them into OUT; returns how many there are in all. A pipeline's line is not set (the caller knows
it); DETAIL names what is missing. */
size_t shader_record_closed_problems(const struct shader_record_set *set, struct shader_record_problem *out,
	size_t max);

#endif
