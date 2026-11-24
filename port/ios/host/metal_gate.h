/* The Metal GPU-family gate: one place that answers "may this run on this GPU, and if not, what happens instead".

Three tables and a resolver, in plain C with no Metal headers, so tools/test_metal_gate.py compiles and checks them on
Linux and macOS (port/ios/tests/metal_gate_probe.c):

- METAL_FEATURES: what Apple's "Metal Feature Set Tables" document, one row per feature VISR uses: the first Apple GPU
  family with it, whether Macs have it, and the runtime query that also decides it, if any. The numbers are from the
  edition named in each row; tools/data/metal-feature-families.tsv is a copy of that edition's table, and the test
  compares the two.
- METAL_QUIRKS: what devices showed and the documentation does not say. Empty today.
- METAL_PASSES: what the backend optionally does: the features it needs, the quirks that block it, and the lowest
  family it may run on (its floor), either verified on a device or taken from the documentation and today's checks.

The device's facts (port/ios/host/metal_gate.m) are gathered once and resolved once; metal_feature(), metal_pass() and
metal_gate_family() are bit tests on the result, with no per-frame cost. */
#ifndef METAL_GATE_H
#define METAL_GATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* the edition of Apple's table the rows were transcribed from */
#define METAL_GATE_EDITION 20261023

/* Runtime queries the device answers itself: (name, whether its answer can turn a feature on below the feature's
documented family). Only Apple's footnotes allow that (BC textures and 32-bit float filtering on some Apple7 and Apple8
GPUs); every other query can only turn a feature off. */
#define METAL_QUERIES(X) \
	X(BC, 1) \
	X(FLOAT32_FILTER, 1) \
	X(RATE_MAP, 0) \
	X(METALFX_SPATIAL, 0) \
	X(COUNTERS_STAGE, 0) \
	X(COUNTERS_DRAW, 0) \
	X(RAYTRACING_RENDER, 0)

/* (name, first Apple family, on Macs, runtime query or NONE, edition) */
#define METAL_FEATURES(X) \
	X(NONUNIFORM_THREADGROUPS, 4, 1, NONE, 20261023) \
	X(MSAA_DEPTH_RESOLVE, 3, 1, NONE, 20261023) \
	X(MSAA_STORE_AND_RESOLVE, 3, 1, NONE, 20261023) \
	X(MSAA_STENCIL_RESOLVE, 5, 1, NONE, 20261023) \
	X(STENCIL_FEEDBACK, 5, 1, NONE, 20261023) \
	X(STENCIL_TEXTURE_VIEWS, 2, 1, NONE, 20261023) \
	X(BINARY_ARCHIVES, 3, 1, NONE, 20261023) \
	X(BASE_VERTEX_DRAWS, 3, 1, NONE, 20261023) \
	X(COUNTING_OCCLUSION, 3, 1, NONE, 20261023) \
	X(BORDER_COLOR, 7, 1, NONE, 20261023) \
	X(RELAXED_MATH, 4, 1, NONE, 20261023) \
	X(QUAD_PERMUTES, 4, 1, NONE, 20261023) \
	X(SIMD_PERMUTES, 6, 1, NONE, 20261023) \
	X(SIMD_REDUCTIONS, 7, 1, NONE, 20261023) \
	X(READ_WRITE_TEXTURES, 4, 1, NONE, 20261023) \
	X(LAYERED_RENDERING, 5, 1, NONE, 20261023) \
	X(VERTEX_AMPLIFICATION, 6, 1, NONE, 20261023) \
	X(RASTERIZATION_RATE_MAPS, 6, 1, RATE_MAP, 20261023) \
	X(METALFX_SPATIAL, 3, 1, METALFX_SPATIAL, 20261023) \
	X(BC_TEXTURES, 9, 1, BC, 20261023) \
	X(ASTC_TEXTURES, 2, 0, NONE, 20261023) \
	X(FLOAT32_FILTERING, 9, 1, FLOAT32_FILTER, 20261023) \
	X(COUNTER_SAMPLING_STAGE, 2, 1, COUNTERS_STAGE, 20261023) \
	X(COUNTER_SAMPLING_DRAW, 2, 1, COUNTERS_DRAW, 20261023) \
	X(RAYTRACING_RENDER, 6, 1, RAYTRACING_RENDER, 20261023)

/* Limits that vary by family: (name, Apple2, Apple3, ..., Apple11, Macs without an Apple family). A family below
Apple2 (a simulator that reports none) takes Apple2's. */
#define METAL_LIMITS(X) \
	X(TEXTURE_2D_SIZE, 8192, 16384, 16384, 16384, 16384, 16384, 16384, 16384, 32768, 32768, 16384) \
	X(THREADS_PER_THREADGROUP, 512, 512, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024) \
	X(FRAGMENT_INPUT_COMPONENTS, 60, 60, 124, 124, 124, 124, 124, 124, 124, 124, 124) \
	X(TEXTURE_TABLE_ENTRIES, 31, 31, 96, 96, 128, 128, 128, 128, 128, 128, 128)

/* (name, first family, last family, what was seen). A quirk blocks every pass that names it on the families from
first to last. */
#define METAL_QUIRKS(X) \
	X(COMPUTE_DISPATCH_FAULT, 2, 3, "A8 and A10X compute dispatch faults")

/* How a pass's floor is known: a fleet device of that family ran the pass clean, or the family comes from Apple's
table and today's checks and no fleet device has run it there. Lowering any floor needs a device run. */
#define METAL_FLOOR_DOCUMENTED 0
#define METAL_FLOOR_VERIFIED 1

/* (name, features it needs, quirks that block it, floor family, floor state, enforced, label, without it).
"enforced" is 0 for a row the backend does not read yet: its answer is logged and changes nothing (MSAA until the
resolve calls read it).
Soft particles: the A8 (Apple TV HD) faults its GPU on the pass's compute dispatch whatever the kernel reads
(known-bugs, Nov 21, 2026), and the A10X iPad Pro (Apple3) had failed command buffers in 11 of 24 latency-lane arms
with the pass on and none in 24 arms with it off (Nov 22), so the floor is Apple4. No fleet device of Apple4 (A11) has
run it; the Apple7 Air 4 (A14) is the fleet device that confirms it runs clean above the floor.
Traced shadows: Metal traces rays in render pipelines from Apple6, but only Apple9 and later GPUs (A17 Pro, M3) have
hardware for it; earlier ones run the same API in software at about a quarter of the speed, so the floor is Apple9 and
they keep the shadow maps. No fleet device of Apple9 or later has run it; the M3 and M6 Macs have. */
#define METAL_PASSES(X) \
	X(SOFT_PARTICLES, METAL_NEEDS(STENCIL_TEXTURE_VIEWS), METAL_BLOCKED_BY(COMPUTE_DISPATCH_FAULT), 4, \
		METAL_FLOOR_DOCUMENTED, 1, "soft particles", "hard particle edges") \
	X(AMBIENT_OCCLUSION, METAL_NEEDS(SIMD_PERMUTES) | METAL_NEEDS(STENCIL_TEXTURE_VIEWS), 0, 6, \
		METAL_FLOOR_DOCUMENTED, 1, "ambient occlusion", "no ambient occlusion") \
	X(MSAA, METAL_NEEDS(MSAA_DEPTH_RESOLVE) | METAL_NEEDS(MSAA_STORE_AND_RESOLVE) | METAL_NEEDS(MSAA_STENCIL_RESOLVE), \
		0, 5, METAL_FLOOR_DOCUMENTED, 0, "multisampling", "one sample a pixel") \
	X(MSAA_STENCIL_FILL, METAL_NEEDS(STENCIL_FEEDBACK), 0, 5, METAL_FLOOR_DOCUMENTED, 1, \
		"multisampled stencil fill", "stencil is not refilled by a shader") \
	X(METALFX_SPATIAL, METAL_NEEDS(METALFX_SPATIAL), 0, 3, METAL_FLOOR_DOCUMENTED, 1, \
		"MetalFX spatial", "bilinear scaling") \
	X(RATE_MAP, METAL_NEEDS(RASTERIZATION_RATE_MAPS), 0, 6, METAL_FLOOR_DOCUMENTED, 1, \
		"rate map test", "no rate map") \
	X(STEREO_HUD_DEPTH, METAL_NEEDS(SIMD_REDUCTIONS), 0, 7, METAL_FLOOR_DOCUMENTED, 1, \
		"stereo HUD depth", "the HUD rests at its distance") \
	X(TRACED_SHADOWS, METAL_NEEDS(RAYTRACING_RENDER), 0, 9, METAL_FLOOR_DOCUMENTED, 1, \
		"traced object shadows", "shadow maps")

enum metal_query
{
#define METAL_GATE_ENUM(name, raises) METAL_QUERY_##name,
	METAL_QUERIES(METAL_GATE_ENUM)
#undef METAL_GATE_ENUM
	METAL_QUERY_COUNT,
	METAL_QUERY_NONE = 63
};

enum metal_feature
{
#define METAL_GATE_ENUM(name, family, mac, query, edition) METAL_FEATURE_##name,
	METAL_FEATURES(METAL_GATE_ENUM)
#undef METAL_GATE_ENUM
	METAL_FEATURE_COUNT
};

enum metal_limit
{
#define METAL_GATE_ENUM(name, ...) METAL_LIMIT_##name,
	METAL_LIMITS(METAL_GATE_ENUM)
#undef METAL_GATE_ENUM
	METAL_LIMIT_COUNT
};

enum metal_quirk
{
#define METAL_GATE_ENUM(name, first, last, seen) METAL_QUIRK_##name,
	METAL_QUIRKS(METAL_GATE_ENUM)
#undef METAL_GATE_ENUM
	METAL_QUIRK_COUNT
};

enum metal_pass
{
#define METAL_GATE_ENUM(name, needs, blocked, floor, state, enforced, label, without) METAL_PASS_##name,
	METAL_PASSES(METAL_GATE_ENUM)
#undef METAL_GATE_ENUM
	METAL_PASS_COUNT
};

#define METAL_NEEDS(feature) (UINT64_C(1) << METAL_FEATURE_##feature)
#define METAL_BLOCKED_BY(quirk) (UINT32_C(1) << METAL_QUIRK_##quirk)

_Static_assert(METAL_QUERY_COUNT <= 32, "queries fit a 32-bit set");
_Static_assert(METAL_FEATURE_COUNT <= 64, "features fit a 64-bit set");
_Static_assert(METAL_QUIRK_COUNT <= 32 && METAL_PASS_COUNT <= 32, "quirks and passes fit 32-bit sets");

/* What the device says about itself, gathered once (metal_gate.m). */
struct metal_gate_facts
{
	/* bit N: the device supports MTLGPUFamilyAppleN (asked by number, so a family newer than the SDK counts) */
	uint32_t apple_families;
	/* MTLGPUFamilyMac2 */
	uint8_t mac;
	/* bit per metal_query: the device answered yes */
	uint32_t queries;
};

struct metal_gate
{
	struct metal_gate_facts facts;
	/* the highest Apple family, 0 for none */
	int family;
	uint64_t features;
	uint32_t quirks;
	uint32_t passes;
	/* bit per metal_query whose yes the documented family overruled (a device disagreeing with Apple's table) */
	uint32_t overruled;
	int limits[METAL_LIMIT_COUNT];
};

/* the gate for the system's Metal device, resolved on first use (metal_gate.m) */
const struct metal_gate *metal_gate(void);
/* tests: resolve these facts instead of asking the device; call before the first metal_gate() */
void metal_gate_adopt_facts(const struct metal_gate_facts *facts);

static inline int metal_feature(enum metal_feature feature)
{
	return (int)((metal_gate()->features >> feature) & 1);
}

static inline int metal_pass(enum metal_pass pass)
{
	return (int)((metal_gate()->passes >> pass) & 1);
}

static inline int metal_limit(enum metal_limit limit)
{
	return metal_gate()->limits[limit];
}

static inline int metal_gate_family(void)
{
	return metal_gate()->family;
}

/* ---------- the resolver: pure, no device ---------- */

static inline int metal_gate_has_family(const struct metal_gate_facts *facts, int family)
{
	return family >= 0 && family < 32 && ((facts->apple_families >> family) & 1);
}

static inline int metal_gate_highest_family(const struct metal_gate_facts *facts)
{
	int family;

	for (family = 31; family > 0; family--)
		if (metal_gate_has_family(facts, family))
			return family;
	return 0;
}

static inline void metal_gate_resolve(const struct metal_gate_facts *facts, struct metal_gate *gate)
{
	static const struct { uint8_t raises; } queries[] = {
#define METAL_GATE_ROW(name, raises) { raises },
		METAL_QUERIES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	static const struct { uint8_t family, mac, query; } features[] = {
#define METAL_GATE_ROW(name, family, mac, query, edition) { family, mac, METAL_QUERY_##query },
		METAL_FEATURES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	static const struct { uint8_t first, last; } quirks[METAL_QUIRK_COUNT + 1] = {
#define METAL_GATE_ROW(name, first, last, seen) { first, last },
		METAL_QUIRKS(METAL_GATE_ROW)
#undef METAL_GATE_ROW
		{ 0, 0 }
	};
	static const struct { uint64_t needs; uint32_t blocked; uint8_t floor; } passes[] = {
#define METAL_GATE_ROW(name, needs, blocked, floor, state, enforced, label, without) { needs, blocked, floor },
		METAL_PASSES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	static const int limits[][11] = {
#define METAL_GATE_ROW(name, ...) { __VA_ARGS__ },
		METAL_LIMITS(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	int index;
	/* a Mac that reports no Apple family (an Intel or AMD GPU): no floors, Mac flags and Mac limits */
	int mac_only;

	*gate = (struct metal_gate){ .facts = *facts };
	gate->family = metal_gate_highest_family(facts);
	mac_only = facts->mac && !gate->family;
	for (index = 0; index < METAL_FEATURE_COUNT; index++)
	{
		int documented = metal_gate_has_family(facts, features[index].family) || (facts->mac && features[index].mac);
		int present = documented;

		if (features[index].query != METAL_QUERY_NONE)
		{
			int answer = (int)((facts->queries >> features[index].query) & 1);

			present = queries[features[index].query].raises ? answer : documented && answer;
			if (answer && !present)
				gate->overruled |= UINT32_C(1) << features[index].query;
		}
		if (present)
			gate->features |= UINT64_C(1) << index;
	}
	for (index = 0; index < METAL_QUIRK_COUNT; index++)
	{
		if (gate->family >= quirks[index].first && gate->family <= quirks[index].last)
			gate->quirks |= UINT32_C(1) << index;
	}
	for (index = 0; index < METAL_PASS_COUNT; index++)
	{
		int floor_met = mac_only || metal_gate_has_family(facts, passes[index].floor);

		if ((gate->features & passes[index].needs) == passes[index].needs && !(gate->quirks & passes[index].blocked) &&
			floor_met)
			gate->passes |= UINT32_C(1) << index;
	}
	for (index = 0; index < METAL_LIMIT_COUNT; index++)
	{
		int column = mac_only ? 10 : gate->family < 2 ? 0 : gate->family > 11 ? 9 : gate->family - 2;

		gate->limits[index] = limits[index][column];
	}
}

/* ---------- names, for the log block and the probe ---------- */

static inline const char *metal_feature_name(enum metal_feature feature)
{
	static const char *const names[] = {
#define METAL_GATE_ROW(name, family, mac, query, edition) #name,
		METAL_FEATURES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	return (unsigned)feature < METAL_FEATURE_COUNT ? names[feature] : "?";
}

static inline int metal_feature_documented_family(enum metal_feature feature)
{
	static const uint8_t families[] = {
#define METAL_GATE_ROW(name, family, mac, query, edition) family,
		METAL_FEATURES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	return (unsigned)feature < METAL_FEATURE_COUNT ? families[feature] : 0;
}

/* the GPUs of each family, from Apple's table */
static inline const char *metal_gate_family_gpus(int family)
{
	static const char *const gpus[] = { "no Apple family", "A7", "A8", "A9-A10", "A11", "A12", "A13", "A14/M1",
		"A15-A16/M2", "A17-A18/M3-M4", "A19/M5", "M6" };
	return family >= 0 && family < (int)(sizeof(gpus) / sizeof(*gpus)) ? gpus[family] : "newer than the table";
}

/* snprintf at the end of text, keeping what was wanted in *wanted and stopping at the end of the buffer */
#define METAL_GATE_APPEND(...) do { \
		size_t used_ = text && size ? strlen(text) : 0; \
		int written_ = snprintf(text ? text + used_ : NULL, size - used_, __VA_ARGS__); \
		if (written_ > 0) wanted += written_; \
	} while (0)

/* The log's first line: the family, then each pass on or off with why. Returns the length wanted. */
static inline int metal_gate_describe(const struct metal_gate *gate, char *text, size_t size)
{
	static const struct { const char *label; uint64_t needs; uint32_t blocked; uint8_t floor, state, enforced; }
		passes[] = {
#define METAL_GATE_ROW(name, needs, blocked, floor, state, enforced, label, without) \
		{ label, needs, blocked, floor, state, enforced },
		METAL_PASSES(METAL_GATE_ROW)
#undef METAL_GATE_ROW
	};
	static const char *const quirk_names[METAL_QUIRK_COUNT + 1] = {
#define METAL_GATE_ROW(name, first, last, seen) seen,
		METAL_QUIRKS(METAL_GATE_ROW)
#undef METAL_GATE_ROW
		""
	};
	int index, wanted = 0;

	if (text && size)
		text[0] = 0;
	else
		text = NULL, size = 0;
	if (gate->family)
		METAL_GATE_APPEND("Metal gate: Apple%d (%s)%s.", gate->family, metal_gate_family_gpus(gate->family),
			gate->facts.mac ? ", a Mac" : "");
	else
		METAL_GATE_APPEND("Metal gate: %s.", gate->facts.mac ? "a Mac with no Apple family" : "no Apple family");
	METAL_GATE_APPEND(" Passes:");
	for (index = 0; index < METAL_PASS_COUNT; index++)
	{
		int on = (int)((gate->passes >> index) & 1);
		uint64_t missing = passes[index].needs & ~gate->features;
		uint32_t quirks = passes[index].blocked & gate->quirks;

		METAL_GATE_APPEND("%s %s %s", index ? "," : "", passes[index].label, on ? "on" : "off");
		if (!on && missing)
		{
			int feature = 0;

			while (!((missing >> feature) & 1))
				feature++;
			METAL_GATE_APPEND(" (needs %s, Apple%d)", metal_feature_name((enum metal_feature)feature),
				metal_feature_documented_family((enum metal_feature)feature));
		}
		else if (!on && quirks)
		{
			int quirk = 0;

			while (!((quirks >> quirk) & 1))
				quirk++;
			METAL_GATE_APPEND(" (%s)", quirk_names[quirk]);
		}
		else if (!on)
			METAL_GATE_APPEND(" (floor Apple%d, %s)", passes[index].floor,
				passes[index].state == METAL_FLOOR_VERIFIED ? "device-verified" : "documented only");
		if (!passes[index].enforced)
			METAL_GATE_APPEND(" [not enforced yet]");
	}
	METAL_GATE_APPEND(".");
	return wanted;
}

/* The log's second line: the features that are off, and any query the documented family overruled. */
static inline int metal_gate_describe_features(const struct metal_gate *gate, char *text, size_t size)
{
	int index, any = 0, wanted = 0;

	if (text && size)
		text[0] = 0;
	else
		text = NULL, size = 0;
	METAL_GATE_APPEND("Metal gate: features off:");
	for (index = 0; index < METAL_FEATURE_COUNT; index++)
		if (!((gate->features >> index) & 1))
			METAL_GATE_APPEND("%s %s", any++ ? "," : "", metal_feature_name((enum metal_feature)index));
	if (!any)
		METAL_GATE_APPEND(" none");
	METAL_GATE_APPEND(".");
	if (gate->overruled)
		METAL_GATE_APPEND(" The device answered yes to a query below its documented family (set 0x%x); the "
			"documented family wins.", (unsigned)gate->overruled);
	return wanted;
}
#undef METAL_GATE_APPEND

#endif
