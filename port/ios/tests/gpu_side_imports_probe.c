/* The guest's side-operation imports (port/ios/host/gpu_side_imports.h): a call with no values is a level's warm-up,
which reaches only the warm operations (they compile the programs and touch no render target), so the current view's
soft-depth state is left alone; a call with values reaches the operation itself, with the same arguments, and the
game gets its answer. Built and run by tools/test_shader_host_prepare.py. */

#include "gpu_backend.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned ssao_warms, ssao_applies, soft_warms, soft_prepares, soft_finishes;
static uint32_t soft_warm_answer;
static gpu_texture seen_color, seen_depth;
static const float *seen_values;
static uint32_t seen_count;

void host_gpu_ssao_warm(void);
void host_gpu_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count);
uint32_t host_gpu_soft_depth_warm(void);
uint32_t host_gpu_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count);
void host_gpu_soft_depth_finish(void);

void host_gpu_ssao_warm(void)
{
	ssao_warms++;
}

void host_gpu_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count)
{
	ssao_applies++;
	seen_color = color;
	seen_depth = depth;
	seen_values = values;
	seen_count = count;
}

uint32_t host_gpu_soft_depth_warm(void)
{
	soft_warms++;
	return soft_warm_answer;
}

uint32_t host_gpu_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count)
{
	soft_prepares++;
	seen_depth = depth;
	seen_values = values;
	seen_count = count;
	return 7;
}

void host_gpu_soft_depth_finish(void)
{
	soft_finishes++;
}

static unsigned traced_readies, traced_datas, traced_resolves;
static uint32_t seen_kind, seen_id, seen_term;
static const void *seen_bytes;

uint32_t host_gpu_traced_shadows_ready(uint32_t any_gpu);
void host_gpu_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size);
void host_gpu_traced_shadows_resolve(gpu_texture color, gpu_texture depth, gpu_texture term, const float *values,
	uint32_t count);

uint32_t host_gpu_traced_shadows_ready(uint32_t any_gpu)
{
	traced_readies++;
	return any_gpu + 1;
}

void host_gpu_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size)
{
	traced_datas++;
	seen_kind = kind;
	seen_id = id;
	seen_bytes = bytes;
	seen_count = size;
}

void host_gpu_traced_shadows_resolve(gpu_texture color, gpu_texture depth, gpu_texture term, const float *values,
	uint32_t count)
{
	traced_resolves++;
	seen_color = color;
	seen_depth = depth;
	seen_term = term;
	seen_values = values;
	seen_count = count;
}

uint32_t host_traced_shadows_ready(uint32_t any_gpu);
void host_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size);
void host_traced_shadows_resolve(gpu_texture color, gpu_texture depth, gpu_texture term, const float *values,
	uint32_t count);
void host_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count);
uint32_t host_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count);
void host_soft_depth_finish(void);
#include "gpu_side_imports.h"

#define CHECK(condition, message) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_side_imports_probe: %s\n", message); \
			exit(1); \
		} \
	} while (0)

int main(void)
{
	static const float values[27] = { 1.0f };
	uint32_t answer;

	/* warming: the programs only, whatever they answer */
	host_ssao_apply(0, 0, NULL, 0);
	CHECK(ssao_warms == 1 && !ssao_applies, "an SSAO warm-up reached more than the warm operation");
	for (soft_warm_answer = 0; soft_warm_answer < 2; soft_warm_answer++)
	{
		answer = host_soft_depth_prepare(0, NULL, 0);
		CHECK(answer == soft_warm_answer, "a soft-depth warm-up did not answer whether the program compiled");
	}
	CHECK(soft_warms == 2 && !soft_prepares && !soft_finishes,
		"a soft-depth warm-up reached more than the warm operation (the view's soft-depth state must stay)");

	/* a call with values: the operation itself, with the same arguments */
	host_ssao_apply(3, 4, values, 27);
	CHECK(ssao_applies == 1 && ssao_warms == 1 && seen_color == 3 && seen_depth == 4 && seen_values == values &&
		seen_count == 27, "SSAO with values did not reach ssao_apply unchanged");
	answer = host_soft_depth_prepare(5, values, 6);
	CHECK(answer == 7 && soft_prepares == 1 && soft_warms == 2 && seen_depth == 5 && seen_values == values &&
		seen_count == 6, "soft depth with values did not reach soft_depth_prepare unchanged, or lost its answer");
	host_soft_depth_finish();
	CHECK(soft_finishes == 1, "host_soft_depth_finish did not reach soft_depth_finish");
	/* traced object shadows: each import is its operation, arguments and answer unchanged */
	CHECK(host_traced_shadows_ready(1) == 2 && traced_readies == 1, "traced_shadows_ready lost its argument or answer");
	host_traced_shadows_data(3, 9, values, 12);
	CHECK(traced_datas == 1 && seen_kind == 3 && seen_id == 9 && seen_bytes == values && seen_count == 12,
		"a traced shadows record did not reach traced_shadows_data unchanged");
	host_traced_shadows_resolve(3, 4, 5, values, 11);
	CHECK(traced_resolves == 1 && seen_color == 3 && seen_depth == 4 && seen_term == 5 && seen_values == values &&
		seen_count == 11, "the traced shadows resolve did not reach traced_shadows_resolve unchanged");
	puts("gpu_side_imports_probe: ok");
	return 0;
}
