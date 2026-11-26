/* The Metal GPU-family gate (port/ios/host/metal_gate.h) answers as the checks it replaced did.

Each GPU the app can meet, Apple families 2 to 11, a simulator that reports no family, Apple silicon Macs and a Mac
that reports no Apple family, with every runtime query answered as Apple's table says, is resolved by the gate and by
a copy of the scattered checks gpu_metal.m, host_stereo.m, host_astc_store.m, host_texture_upscale.m, host_device.m,
host_benchmark.m and host_perf_metal.m made before the gate (the truth table below), and every answer must agree.
It also checks the tables themselves: no pass's floor is below the families of the features it needs, and a query
that answers yes below its documented family is overruled and reported. tools/test_metal_gate.py compiles and runs it;
"blocks" prints the log block for the fleet's oldest GPUs. */
#include "metal_gate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct metal_gate probe_gate;

const struct metal_gate *metal_gate(void)
{
	return &probe_gate;
}

void metal_gate_adopt_facts(const struct metal_gate_facts *facts)
{
	metal_gate_resolve(facts, &probe_gate);
}

static int failures;

#define EXPECT(name, today, gated) do { \
		int expected_ = !!(today), got_ = !!(gated); \
		if (expected_ != got_) { \
			fprintf(stderr, "%s: %s is %d before the gate and %d through it\n", label, name, expected_, got_); \
			failures++; \
		} \
	} while (0)

#define Q(name) (UINT32_C(1) << METAL_QUERY_##name)

/* the device: Apple families up to `family` (each family's device also reports every earlier one), the Mac flag, and
the queries' answers */
static struct metal_gate_facts device(int family, int mac, uint32_t queries)
{
	struct metal_gate_facts facts = { 0 };
	int index;

	for (index = 1; index <= family; index++)
		facts.apple_families |= UINT32_C(1) << index;
	facts.mac = (uint8_t)mac;
	facts.queries = queries;
	return facts;
}

/* the queries as Apple's table answers them for a family (and on a Mac), plus the counters, which vary by device */
static uint32_t documented_queries(int family, int mac, int bc, uint32_t counters)
{
	uint32_t queries = counters;

	if (bc || family >= 9 || mac)
		queries |= Q(BC);
	if (family >= 9 || mac)
		queries |= Q(FLOAT32_FILTER);
	if (family >= 6 || mac)
		queries |= Q(RATE_MAP);
	if (family >= 3 || mac)
		queries |= Q(METALFX_SPATIAL);
	if (family >= 6 || mac)
		queries |= Q(RAYTRACING_RENDER);
	return queries;
}

static void compare(const char *label, struct metal_gate_facts facts)
{
	/* the checks as they were: `supportsFamily:` per use, the queries as asked */
	int apple[32], mac = facts.mac, highest = 0, index;
	int bc = !!(facts.queries & Q(BC));

	for (index = 0; index < 32; index++)
	{
		apple[index] = (int)((facts.apple_families >> index) & 1);
		if (index >= 1 && index <= 20 && apple[index])
			highest = index;
	}
	metal_gate_adopt_facts(&facts);

	/* gpu_metal.m */
	EXPECT("msaa_stencil_export", apple[5] || mac, metal_pass(METAL_PASS_MSAA_STENCIL_FILL));
	EXPECT("pipeline archive", apple[3] || mac, metal_feature(METAL_FEATURE_BINARY_ARCHIVES));
	EXPECT("ssao_supported", apple[6] || mac, metal_pass(METAL_PASS_AMBIENT_OCCLUSION));
	EXPECT("relaxed math", apple[4] || mac, metal_feature(METAL_FEATURE_RELAXED_MATH));
	EXPECT("fast math below Apple3", !apple[3], metal_gate_family() < 3);
	EXPECT("BC textures", bc, metal_feature(METAL_FEATURE_BC_TEXTURES));
	EXPECT("ASTC textures", apple[2], metal_feature(METAL_FEATURE_ASTC_TEXTURES));
	EXPECT("base vertex draws", apple[3] || mac, metal_feature(METAL_FEATURE_BASE_VERTEX_DRAWS));
	EXPECT("counting occlusion", apple[3] || mac, metal_feature(METAL_FEATURE_COUNTING_OCCLUSION));
	EXPECT("border colors", apple[7] || mac, metal_feature(METAL_FEATURE_BORDER_COLOR));
	EXPECT("max_texture_size 16384", apple[3] || mac, metal_limit(METAL_LIMIT_TEXTURE_2D_SIZE) >= 16384);
	EXPECT("max_texture_size 8192", !(apple[3] || mac), metal_limit(METAL_LIMIT_TEXTURE_2D_SIZE) == 8192);
	EXPECT("rate map test", facts.queries & Q(RATE_MAP), metal_pass(METAL_PASS_RATE_MAP));
	EXPECT("MetalFX spatial", facts.queries & Q(METALFX_SPATIAL), metal_pass(METAL_PASS_METALFX_SPATIAL));
	EXPECT("counters at stage boundaries", facts.queries & Q(COUNTERS_STAGE),
		metal_feature(METAL_FEATURE_COUNTER_SAMPLING_STAGE));
	EXPECT("counters at draw boundaries", facts.queries & Q(COUNTERS_DRAW),
		metal_feature(METAL_FEATURE_COUNTER_SAMPLING_DRAW));
	/* gpu_metal.m's soft_particles_supported() reads this row: Apple4 or a Mac */
	EXPECT("soft particles", apple[4] || mac, (metal_gate()->passes >> METAL_PASS_SOFT_PARTICLES) & 1);
	/* gpu_metal.m's traced_shadows_supported(): hardware ray tracing, Apple9 and later, and an Apple silicon Mac only
	by its Apple family (an M1 or M2 Mac traces in software, as an A14 does) */
	EXPECT("traced shadows", (facts.queries & Q(RAYTRACING_RENDER)) && (apple[9] || (mac && !highest)),
		metal_pass(METAL_PASS_TRACED_SHADOWS));
	/* host_stereo.m */
	EXPECT("stereo HUD depth", apple[7] || mac, metal_pass(METAL_PASS_STEREO_HUD_DEPTH));
	/* host_device.m and host_benchmark.m */
	if (highest != metal_gate_family())
	{
		fprintf(stderr, "%s: gpu_family is %d before the gate and %d through it\n", label, highest, metal_gate_family());
		failures++;
	}
	if (metal_gate()->overruled)
	{
		fprintf(stderr, "%s: a documented answer was overruled (0x%x)\n", label, (unsigned)metal_gate()->overruled);
		failures++;
	}
}

/* Every family and Mac shape against every subset of the queries' answers: a query-backed answer is the old check's
(the query alone) unless the query is in `overruled`, and `overruled` holds exactly the non-footnoted queries that
answered yes below their documented family. This is the whole set of behavior changes, asserted, not assumed. */
static void check_every_answer(void)
{
	static const struct { int query, family, mac; enum metal_feature feature; } rows[] = {
		{ METAL_QUERY_RATE_MAP, 6, 1, METAL_FEATURE_RASTERIZATION_RATE_MAPS },
		{ METAL_QUERY_METALFX_SPATIAL, 3, 1, METAL_FEATURE_METALFX_SPATIAL },
		{ METAL_QUERY_COUNTERS_STAGE, 2, 1, METAL_FEATURE_COUNTER_SAMPLING_STAGE },
		{ METAL_QUERY_COUNTERS_DRAW, 2, 1, METAL_FEATURE_COUNTER_SAMPLING_DRAW },
		{ METAL_QUERY_RAYTRACING_RENDER, 6, 1, METAL_FEATURE_RAYTRACING_RENDER },
		{ METAL_QUERY_BC, 9, 1, METAL_FEATURE_BC_TEXTURES },
		{ METAL_QUERY_FLOAT32_FILTER, 9, 1, METAL_FEATURE_FLOAT32_FILTERING },
	};
	int family, mac, row;
	uint32_t subset;

	for (family = 0; family <= 11; family++)
		for (mac = 0; mac <= 1; mac++)
			for (subset = 0; subset < (UINT32_C(1) << METAL_QUERY_COUNT); subset++)
			{
				struct metal_gate_facts facts = device(family, mac, subset);
				uint32_t expected = 0;

				metal_gate_adopt_facts(&facts);
				for (row = 0; row < (int)(sizeof(rows) / sizeof(*rows)); row++)
				{
					int answer = (int)((subset >> rows[row].query) & 1);
					int documented = family >= rows[row].family || (mac && rows[row].mac);
					int raises = rows[row].query == METAL_QUERY_BC || rows[row].query == METAL_QUERY_FLOAT32_FILTER;
					int overruled = (int)((metal_gate()->overruled >> rows[row].query) & 1);

					if (answer && !documented && !raises)
						expected |= UINT32_C(1) << rows[row].query;
					if (!overruled && metal_feature(rows[row].feature) != answer)
					{
						fprintf(stderr, "Apple%d mac %d queries 0x%x: %s is %d, its query said %d\n", family, mac,
							(unsigned)subset, metal_feature_name(rows[row].feature), metal_feature(rows[row].feature),
							answer);
						failures++;
					}
				}
				if (metal_gate()->overruled != expected)
				{
					fprintf(stderr, "Apple%d mac %d queries 0x%x: overruled 0x%x, expected 0x%x\n", family, mac,
						(unsigned)subset, (unsigned)metal_gate()->overruled, (unsigned)expected);
					failures++;
				}
			}
}

static void check_tables(void)
{
	static const struct { const char *name; uint64_t needs; int floor; } passes[] = {
#define ROW(name, needs, blocked, floor, state, enforced, label, without) { #name, needs, floor },
		METAL_PASSES(ROW)
#undef ROW
	};
	int pass, feature;

	for (pass = 0; pass < METAL_PASS_COUNT; pass++)
		for (feature = 0; feature < METAL_FEATURE_COUNT; feature++)
			if (((passes[pass].needs >> feature) & 1) &&
				metal_feature_documented_family((enum metal_feature)feature) > passes[pass].floor)
			{
				fprintf(stderr, "pass %s: floor Apple%d is below %s's Apple%d\n", passes[pass].name, passes[pass].floor,
					metal_feature_name((enum metal_feature)feature),
					metal_feature_documented_family((enum metal_feature)feature));
				failures++;
			}
	/* a device that says yes below the documented family: the family wins, and the block says so */
	{
		struct metal_gate_facts facts = device(2, 0, Q(METALFX_SPATIAL) | Q(RATE_MAP));
		char block[2048];

		metal_gate_adopt_facts(&facts);
		if (metal_pass(METAL_PASS_METALFX_SPATIAL) || metal_pass(METAL_PASS_RATE_MAP) ||
			metal_gate()->overruled != (Q(METALFX_SPATIAL) | Q(RATE_MAP)))
		{
			fprintf(stderr, "a query's yes below the documented family was not overruled\n");
			failures++;
		}
		metal_gate_describe_features(metal_gate(), block, sizeof(block));
		if (!strstr(block, "documented family wins"))
		{
			fprintf(stderr, "the overruled query is missing from the block: %s\n", block);
			failures++;
		}
	}
	/* the footnoted queries raise: BC on an Apple7 iPad that has it */
	{
		struct metal_gate_facts facts = device(7, 0, Q(BC));

		metal_gate_adopt_facts(&facts);
		if (!metal_feature(METAL_FEATURE_BC_TEXTURES) || metal_gate()->overruled)
		{
			fprintf(stderr, "BC on an Apple7 iPad that answers yes is off\n");
			failures++;
		}
	}
	/* the block fits a log line and truncates safely */
	{
		struct metal_gate_facts facts = device(2, 0, 0);
		char small[16], block[2048];
		int wanted;

		metal_gate_adopt_facts(&facts);
		wanted = metal_gate_describe(metal_gate(), small, sizeof(small));
		if (wanted < (int)sizeof(small) || strlen(small) != sizeof(small) - 1 ||
			metal_gate_describe(metal_gate(), block, sizeof(block)) != wanted || (int)strlen(block) != wanted ||
			strncmp(block, small, sizeof(small) - 1))
		{
			fprintf(stderr, "metal_gate_describe truncates wrongly: %d %s\n", wanted, small);
			failures++;
		}
	}
}

static void print_block(const char *name, struct metal_gate_facts facts)
{
	char block[2048];

	metal_gate_adopt_facts(&facts);
	metal_gate_describe(metal_gate(), block, sizeof(block));
	printf("%s\n%s\n", name, block);
	metal_gate_describe_features(metal_gate(), block, sizeof(block));
	printf("%s\n", block);
	/* platform_log formats into 1024 bytes */
	if (metal_gate_describe(metal_gate(), NULL, 0) >= 1024 || metal_gate_describe_features(metal_gate(), NULL, 0) >= 1024)
	{
		fprintf(stderr, "%s: the gate's log lines are longer than platform_log takes\n", name);
		failures++;
	}
}

int main(int argc, char **argv)
{
	uint32_t counter_sets[] = { 0, Q(COUNTERS_STAGE), Q(COUNTERS_STAGE) | Q(COUNTERS_DRAW) };
	int family, counters, bc;
	char label[96];

	if (argc > 1 && !strcmp(argv[1], "blocks"))
	{
		print_block("Apple TV HD (A8)", device(2, 0, documented_queries(2, 0, 0, 0)));
		print_block("iPad Pro A10X", device(3, 0, documented_queries(3, 0, 0, 0)));
		print_block("iPad Air 4 (A14)", device(7, 0, documented_queries(7, 0, 0, Q(COUNTERS_STAGE))));
		print_block("Apple silicon Mac (M1)", device(7, 1, documented_queries(7, 1, 0, Q(COUNTERS_STAGE))));
		print_block("Mac with no Apple family", device(0, 1, documented_queries(0, 1, 0, Q(COUNTERS_STAGE))));
		print_block("no Apple family", device(0, 0, 0));
		return failures != 0;
	}
	for (counters = 0; counters < 3; counters++)
	{
		uint32_t set = counter_sets[counters];

		/* no family: a simulator, which has no counters either */
		if (!counters)
			compare("no Apple family", device(0, 0, documented_queries(0, 0, 0, 0)));
		for (family = 2; family <= 11; family++)
			for (bc = 0; bc <= (family == 7 || family == 8); bc++)
			{
				snprintf(label, sizeof(label), "Apple%d%s, counters %d", family, bc ? " with BC" : "", counters);
				compare(label, device(family, 0, documented_queries(family, 0, bc, set)));
			}
		/* Apple silicon Macs report Apple families and Mac2 */
		for (family = 7; family <= 11; family++)
		{
			snprintf(label, sizeof(label), "Apple silicon Mac, Apple%d, counters %d", family, counters);
			compare(label, device(family, 1, documented_queries(family, 1, 0, set)));
		}
		snprintf(label, sizeof(label), "Mac with no Apple family, counters %d", counters);
		compare(label, device(0, 1, documented_queries(0, 1, 0, set)));
	}
	/* iOS before 16.4 has no BC query: the answer is no */
	compare("Apple9 on an OS without the BC query", device(9, 0, documented_queries(9, 0, 0, 0) & ~Q(BC)));
	check_tables();
	check_every_answer();
	if (failures)
	{
		fprintf(stderr, "metal gate: %d disagreements\n", failures);
		return 1;
	}
	printf("metal gate: every family agrees with the checks it replaced\n");
	return 0;
}
