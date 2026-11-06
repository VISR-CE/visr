/*
NV2A_VSH.C

Translation of NV2A vertex programs (Xbox vertex shader microcode) into
GLSL.

Each instruction is four little-endian words. Word 1 to 3 hold a MAC
(vector) operation and an ILU (scalar) operation that execute in parallel
on the same three operands A, B and C, and the destinations of both; the
bit positions of every field are spelled out in operand() and in
nv2a_vertex_shader_translate(). When both units run, the ILU result goes to
temporary r1, whatever the instruction's temporary register field says.

Xbox vertex programs finish by converting their clip-space position to
screen space with the viewport constants c[-38] and c[-37], which Direct3D
maintains. The generated shader inverts that transform to hand OpenGL a
clip-space position again.
*/

#include "xgpu.h"
#include "gpu_uniforms.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------- instruction fields */

static unsigned long field(const DWORD *instruction, int word, int low_bit, int bit_count)
{
	return (instruction[word] >> low_bit) & ((1UL << bit_count) - 1);
}

enum
{
	_mac_nop, _mac_mov, _mac_mul, _mac_add, _mac_mad, _mac_dp3, _mac_dph, _mac_dp4,
	_mac_dst, _mac_min, _mac_max, _mac_slt, _mac_sge, _mac_arl,
};

enum
{
	_ilu_nop, _ilu_mov, _ilu_rcp, _ilu_rcc, _ilu_rsq, _ilu_exp, _ilu_log, _ilu_lit,
};

enum
{
	_mux_unknown, _mux_temporary, _mux_input, _mux_constant,
};

/* output register addresses (o[]) */
static const char *output_name(unsigned long address)
{
	switch (address)
	{
	case 0: return "oPos";
	case 3: return "oD0";
	case 4: return "oD1";
	case 5: return "oFog";
	case 6: return "oPts";
	case 7: return "oB0";
	case 8: return "oB1";
	case 9: return "oT0";
	case 10: return "oT1";
	case 11: return "oT2";
	case 12: return "oT3";
	default: return "oUnused";
	}
}

static void write_mask(unsigned long mask, char *out)
{
	/* bit 3 is x */
	int count = 0;

	if (mask & 8) out[count++] = 'x';
	if (mask & 4) out[count++] = 'y';
	if (mask & 2) out[count++] = 'z';
	if (mask & 1) out[count++] = 'w';
	out[count] = 0;
}

static void operand(struct xgpu_text *text, const DWORD *instruction, char which, int relative)
{
	static const char swizzle_names[] = "xyzw";
	unsigned long negate, swizzle[4], index, mux;

	switch (which)
	{
	case 'A':
		negate = field(instruction, 1, 8, 1);
		swizzle[0] = field(instruction, 1, 6, 2);
		swizzle[1] = field(instruction, 1, 4, 2);
		swizzle[2] = field(instruction, 1, 2, 2);
		swizzle[3] = field(instruction, 1, 0, 2);
		index = field(instruction, 2, 28, 4);
		mux = field(instruction, 2, 26, 2);
		break;
	case 'B':
		negate = field(instruction, 2, 25, 1);
		swizzle[0] = field(instruction, 2, 23, 2);
		swizzle[1] = field(instruction, 2, 21, 2);
		swizzle[2] = field(instruction, 2, 19, 2);
		swizzle[3] = field(instruction, 2, 17, 2);
		index = field(instruction, 2, 13, 4);
		mux = field(instruction, 2, 11, 2);
		break;
	default:
		negate = field(instruction, 2, 10, 1);
		swizzle[0] = field(instruction, 2, 8, 2);
		swizzle[1] = field(instruction, 2, 6, 2);
		swizzle[2] = field(instruction, 2, 4, 2);
		swizzle[3] = field(instruction, 2, 2, 2);
		index = (field(instruction, 2, 0, 2) << 2) | field(instruction, 3, 30, 2);
		mux = field(instruction, 3, 28, 2);
		break;
	}

	xgpu_text_append(text, "%s", negate ? "-" : "");
	switch (mux)
	{
	case _mux_temporary:
		/* r12 reads back the position output */
		if (index == 12)
			xgpu_text_append(text, "oPos");
		else
			xgpu_text_append(text, "r%lu", index);
		break;
	case _mux_input:
		xgpu_text_append(text, "v%lu", field(instruction, 1, 9, 4));
		break;
	case _mux_constant:
		if (relative)
			xgpu_text_append(text, "c[clamp(a0 + %lu, 0, %d)]", field(instruction, 1, 13, 8), XGPU_VERTEX_CONSTANT_COUNT - 1);
		else
			xgpu_text_append(text, "c[%lu]", field(instruction, 1, 13, 8));
		break;
	default:
		xgpu_text_append(text, "vec4(0.0)");
		break;
	}
	xgpu_text_append(text, ".%c%c%c%c",
		swizzle_names[swizzle[0]], swizzle_names[swizzle[1]], swizzle_names[swizzle[2]], swizzle_names[swizzle[3]]);
}

/* ---------- uniforms */

void nv2a_uniform_declarations(struct xgpu_text *text, const struct nv2a_dialect *dialect, int stage)
{
#define DECLARE_UNIFORM(name, glsl_type, count, uniform_stage) \
	if ((uniform_stage) == stage || \
		((uniform_stage) == GPU_UNIFORM_VERTEX_CONSTANTS && stage == GPU_UNIFORM_VERTEX) || \
		((uniform_stage) == GPU_UNIFORM_PIXEL_LOD_BIAS && stage == GPU_UNIFORM_PIXEL && dialect->shader_lod_bias)) \
	{ \
		if ((count) > 1) \
			xgpu_text_append(text, "uniform " #glsl_type " " #name "[%d];\n", (int)(count)); \
		else \
			xgpu_text_append(text, "uniform " #glsl_type " " #name ";\n"); \
	}
	GPU_UNIFORMS(DECLARE_UNIFORM)
#undef DECLARE_UNIFORM
}
static const char shader_outputs[] =
	"out vec4 xD0;\n"
	"out vec4 xD1;\n"
	"out vec4 xB0;\n"
	"out vec4 xB1;\n"
	"out vec4 xT0;\n"
	"out vec4 xT1;\n"
	"out vec4 xT2;\n"
	"out vec4 xT3;\n"
	"out float xFog;\n"
	"invariant gl_Position;\n";
/* GLSL and MSL alike */
static const char shader_helpers[] =
	"vec4 unpack_normpacked3(uint p)\n"
	"{\n"
	"	int x = int(p << 21) >> 21;\n"
	"	int y = int(p << 10) >> 21;\n"
	"	int z = int(p) >> 22;\n"
	"	return vec4(float(x) / 1023.0, float(y) / 1023.0, float(z) / 511.0, 1.0);\n"
	"}\n"
	"vec4 nv2a_rcc(float x)\n"
	"{\n"
	"	float r = 1.0 / x;\n"
	"	if (r > 0.0) r = clamp(r, 5.42101e-20, 1.884467e+19);\n"
	"	else r = clamp(r, -1.884467e+19, -5.42101e-20);\n"
	"	return vec4(r);\n"
	"}\n"
	"vec4 nv2a_exp(float x)\n"
	"{\n"
	"	return vec4(exp2(floor(x)), fract(x), exp2(x), 1.0);\n"
	"}\n"
	"vec4 nv2a_log(float x)\n"
	"{\n"
	"	x = abs(x);\n"
	"	if (x == 0.0) return vec4(-1.0e30, 1.0, -1.0e30, 1.0);\n"
	"	float e = floor(log2(x));\n"
	"	return vec4(e, x / exp2(e), log2(x), 1.0);\n"
	"}\n"
	"vec4 nv2a_lit(vec4 s)\n"
	"{\n"
	"	float specular = s.x > 0.0 ? pow(max(s.y, 0.0), clamp(s.w, -127.9961, 127.9961)) : 0.0;\n"
	"	return vec4(1.0, max(s.x, 0.0), specular, 1.0);\n"
	"}\n";

/* the clip position's z and w (r12) as the program writes them, for MSL's
reversed depth: after a dot product into one of them, keep its operands
(depth_za and depth_zb for z, depth_wa and depth_wb for w) and remember the
operation; any other write forgets it. Halo's programs make both with the
same position and two rows of the projection, as dph v0, c[2] and
dph v0, c[3]. The writes come after both units have read their operands, so
A and B are this instruction's. */
static void depth_rows_track(struct xgpu_text *text, const DWORD *instruction, unsigned long mac, unsigned long ilu,
	unsigned long *z_op, unsigned long *w_op)
{
	unsigned long temporary = field(instruction, 3, 20, 4);
	int mac_dot = mac == _mac_dp3 || mac == _mac_dph || mac == _mac_dp4;
	/* components of r12 this instruction writes with a dot product, and with
	anything else */
	unsigned long dot = 0, other = 0;

	if (mac != _mac_nop && mac != _mac_arl && temporary == 12)
		*(mac_dot ? &dot : &other) |= field(instruction, 3, 24, 4);
	if (ilu != _ilu_nop && (mac != _mac_nop ? 1 : temporary) == 12)
		other |= field(instruction, 3, 16, 4);
	if (field(instruction, 3, 11, 1) && field(instruction, 3, 3, 8) == 0 &&
		(field(instruction, 3, 2, 1) ? ilu : mac) != 0)
	{
		*(!field(instruction, 3, 2, 1) && mac_dot ? &dot : &other) |= field(instruction, 3, 12, 4);
	}
	if ((dot | other) & 2)
	{
		*z_op = (dot & 2) && !(other & 2) ? mac : 0;
		if (*z_op)
			xgpu_text_append(text, "\tdepth_za = A;\n\tdepth_zb = B;\n");
	}
	if ((dot | other) & 1)
	{
		*w_op = (dot & 1) && !(other & 1) ? mac : 0;
		if (*w_op)
			xgpu_text_append(text, "\tdepth_wa = A;\n\tdepth_wb = B;\n");
	}
}

/* at the clip position's capture: clip.w - clip.z made without the
cancellation. When z and w were the same dot product with one operand in
common, w - z is the dot product of the common operand with the difference
of the others; the rows of a projection differ in every element by much less
than either, and the difference of two floats that close is exact. The
result is a value near the clip position's w - z with the rounding of a
small dot product, not of two large ones. Returns whether it wrote one. */
static int reversed_depth_operands(struct xgpu_text *text, unsigned long z_op, unsigned long w_op)
{
	const char *form;

	if (!z_op || z_op != w_op)
	{
		xgpu_text_append(text, "\tclip_reversed_kept = false;\n");
		return 0;
	}
	/* dph(a, b) is dot(a.xyz, b.xyz) + b.w: with a in common its rows'
	difference keeps the w, with b in common the b.w cancels */
	form = z_op == _mac_dp4 ? "dot(%s, %s)" : "dot((%s).xyz, (%s).xyz)";
	xgpu_text_append(text, "\tclip_reversed_kept = true;\n\tif (all(depth_za == depth_wa))\n\t\tclip_reversed = ");
	if (z_op == _mac_dph)
		xgpu_text_append(text, "dot(depth_wa.xyz, (depth_wb - depth_zb).xyz) + (depth_wb.w - depth_zb.w)");
	else
		xgpu_text_append(text, form, "depth_wa", "depth_wb - depth_zb");
	xgpu_text_append(text, ";\n\telse if (all(depth_zb == depth_wb))\n\t\tclip_reversed = ");
	xgpu_text_append(text, form, "depth_wa - depth_za", "depth_wb");
	xgpu_text_append(text, ";\n\telse\n\t\tclip_reversed_kept = false;\n");
	return 1;
}

/* an instruction's operands and both units' results (mac, ilu), written by
instruction_writes */
static void instruction_compute(struct xgpu_text *text, const DWORD *instruction, unsigned long index)
{
	unsigned long mac = field(instruction, 1, 21, 4);
	unsigned long ilu = field(instruction, 1, 25, 3);
	int relative = (int)field(instruction, 3, 1, 1);

	xgpu_text_append(text, "\t/* %lu */\n", index);
	xgpu_text_append(text, "\tA = "); operand(text, instruction, 'A', relative); xgpu_text_append(text, ";\n");
	xgpu_text_append(text, "\tB = "); operand(text, instruction, 'B', relative); xgpu_text_append(text, ";\n");
	xgpu_text_append(text, "\tC = "); operand(text, instruction, 'C', relative); xgpu_text_append(text, ";\n");

	switch (mac)
	{
	case _mac_nop: break;
	case _mac_mov: xgpu_text_append(text, "\tmac = A;\n"); break;
	case _mac_mul: xgpu_text_append(text, "\tmac = A * B;\n"); break;
	case _mac_add: xgpu_text_append(text, "\tmac = A + C;\n"); break;
	case _mac_mad: xgpu_text_append(text, "\tmac = A * B + C;\n"); break;
	case _mac_dp3: xgpu_text_append(text, "\tmac = vec4(dot(A.xyz, B.xyz));\n"); break;
	case _mac_dph: xgpu_text_append(text, "\tmac = vec4(dot(A.xyz, B.xyz) + B.w);\n"); break;
	case _mac_dp4: xgpu_text_append(text, "\tmac = vec4(dot(A, B));\n"); break;
	case _mac_dst: xgpu_text_append(text, "\tmac = vec4(1.0, A.y * B.y, A.z, B.w);\n"); break;
	case _mac_min: xgpu_text_append(text, "\tmac = min(A, B);\n"); break;
	case _mac_max: xgpu_text_append(text, "\tmac = max(A, B);\n"); break;
	case _mac_slt: xgpu_text_append(text, "\tmac = vec4(lessThan(A, B));\n"); break;
	case _mac_sge: xgpu_text_append(text, "\tmac = vec4(greaterThanEqual(A, B));\n"); break;
	case _mac_arl: xgpu_text_append(text, "\tmac = A;\n"); break;
	default: xgpu_text_append(text, "\tmac = vec4(0.0);\n"); break;
	}
	switch (ilu)
	{
	case _ilu_nop: break;
	case _ilu_mov: xgpu_text_append(text, "\tilu = C;\n"); break;
	case _ilu_rcp: xgpu_text_append(text, "\tilu = vec4(1.0 / C.x);\n"); break;
	case _ilu_rcc: xgpu_text_append(text, "\tilu = nv2a_rcc(C.x);\n"); break;
	case _ilu_rsq: xgpu_text_append(text, "\tilu = vec4(inversesqrt(abs(C.x)));\n"); break;
	case _ilu_exp: xgpu_text_append(text, "\tilu = nv2a_exp(C.x);\n"); break;
	case _ilu_log: xgpu_text_append(text, "\tilu = nv2a_log(C.x);\n"); break;
	case _ilu_lit: xgpu_text_append(text, "\tilu = nv2a_lit(C);\n"); break;
	default: xgpu_text_append(text, "\tilu = vec4(0.0);\n"); break;
	}
}

/* an instruction's results into its registers and outputs, after both units
have read their operands; outputs (a mask of 1 oD0, 2 oD1, 4 oT3) limits the
outputs written to those, 0 all (the pixel shader's slice, nv2a_lighting_slice,
writes only the ones it replaces) */
static void instruction_writes(struct xgpu_text *text, const DWORD *instruction, unsigned long outputs)
{
	unsigned long mac = field(instruction, 1, 21, 4);
	unsigned long ilu = field(instruction, 1, 25, 3);
	unsigned long mac_mask = field(instruction, 3, 24, 4);
	unsigned long temporary = field(instruction, 3, 20, 4);
	unsigned long ilu_mask = field(instruction, 3, 16, 4);
	unsigned long output_mask = field(instruction, 3, 12, 4);
	unsigned long output_is_register = field(instruction, 3, 11, 1);
	unsigned long output_address = field(instruction, 3, 3, 8);
	unsigned long output_from_ilu = field(instruction, 3, 2, 1);
	char mask[5];

	/* results are written only after both units have read their inputs */
	if (mac == _mac_arl)
	{
		xgpu_text_append(text, "\ta0 = int(floor(mac.x + 0.001));\n");
	}
	else if (mac != _mac_nop && mac_mask)
	{
		write_mask(mac_mask, mask);
		if (temporary == 12)
			xgpu_text_append(text, "\toPos.%s = mac.%s;\n", mask, mask);
		else
			xgpu_text_append(text, "\tr%lu.%s = mac.%s;\n", temporary, mask, mask);
	}
	if (ilu != _ilu_nop && ilu_mask)
	{
		unsigned long ilu_temporary = mac != _mac_nop ? 1 : temporary;

		write_mask(ilu_mask, mask);
		if (ilu_temporary == 12)
			xgpu_text_append(text, "\toPos.%s = ilu.%s;\n", mask, mask);
		else
			xgpu_text_append(text, "\tr%lu.%s = ilu.%s;\n", ilu_temporary, mask, mask);
	}
	if (outputs && output_is_register && !((output_address == 3 && (outputs & 1)) ||
		(output_address == 4 && (outputs & 2)) || (output_address == 12 && (outputs & 4))))
	{
		output_mask = 0;
	}
	if (output_mask && (output_from_ilu ? ilu : mac) != 0)
	{
		const char *source = output_from_ilu ? "ilu" : "mac";

		write_mask(output_mask, mask);
		if (output_is_register)
			xgpu_text_append(text, "\t%s.%s = %s.%s;\n", output_name(output_address), mask, source, mask);
		/* writes to constant memory are not used by Halo's shaders */
	}
}

/* ---------- per-pixel lighting (nv2a_lighting_slice, xgpu.h) */

/* a register in the slice's numbering: 0-11 r0-r11, 12 oPos (r12), 16-31
v0-v15 */
#define LIGHTING_REGISTERS 32
/* operand_register's constant for a relatively addressed one */
#define LIGHTING_RELATIVE 1000

/* an operand's register in that numbering (-1 for none: *constant is then
the constant it reads, or -1) and its swizzle */
static int operand_register(const DWORD *instruction, char which, unsigned long *swizzle, int *constant)
{
	unsigned long index, mux;

	switch (which)
	{
	case 'A':
		swizzle[0] = field(instruction, 1, 6, 2);
		swizzle[1] = field(instruction, 1, 4, 2);
		swizzle[2] = field(instruction, 1, 2, 2);
		swizzle[3] = field(instruction, 1, 0, 2);
		index = field(instruction, 2, 28, 4);
		mux = field(instruction, 2, 26, 2);
		break;
	case 'B':
		swizzle[0] = field(instruction, 2, 23, 2);
		swizzle[1] = field(instruction, 2, 21, 2);
		swizzle[2] = field(instruction, 2, 19, 2);
		swizzle[3] = field(instruction, 2, 17, 2);
		index = field(instruction, 2, 13, 4);
		mux = field(instruction, 2, 11, 2);
		break;
	default:
		swizzle[0] = field(instruction, 2, 8, 2);
		swizzle[1] = field(instruction, 2, 6, 2);
		swizzle[2] = field(instruction, 2, 4, 2);
		swizzle[3] = field(instruction, 2, 2, 2);
		index = (field(instruction, 2, 0, 2) << 2) | field(instruction, 3, 30, 2);
		mux = field(instruction, 3, 28, 2);
		break;
	}
	*constant = -1;
	switch (mux)
	{
	case _mux_temporary:
		return index <= 12 ? (int)index : -1;
	case _mux_input:
		return 16 + (int)field(instruction, 1, 9, 4);
	case _mux_constant:
		*constant = field(instruction, 3, 1, 1) ? LIGHTING_RELATIVE : (int)field(instruction, 1, 13, 8);
		return -1;
	default:
		return -1;
	}
}

/* a hardware write mask (bit 3 x) as components (bit 0 x) */
static unsigned long mask_components(unsigned long mask)
{
	return ((mask >> 3) & 1) | (((mask >> 2) & 1) << 1) | (((mask >> 1) & 1) << 2) | ((mask & 1) << 3);
}

/* the backward walk's state: each register's components still needed, and
what the latest step read */
struct lighting_reads
{
	unsigned long needed[LIGHTING_REGISTERS];
	int reads_lighting;
	int relative;
};

/* the operand's components that components (of the unit's result) read,
marked needed */
static void lighting_read(const DWORD *instruction, char which, unsigned long components, struct lighting_reads *reads)
{
	unsigned long swizzle[4], read = 0;
	int reg, constant, axis;

	if (!components)
		return;
	reg = operand_register(instruction, which, swizzle, &constant);
	for (axis = 0; axis < 4; axis++)
		if (components & (1UL << axis))
			read |= 1UL << swizzle[axis];
	if (reg >= 0)
		reads->needed[reg] |= read;
	else if (constant == LIGHTING_RELATIVE)
		reads->relative = 1;
	else if (constant == 4 || (constant >= 17 && constant <= 27))
		reads->reads_lighting = 1;
}

/* one step back over an instruction: whether it writes anything still
needed (registers in reads; outputs, the components of oD0, oD1 and oT3 still
to be written, in outputs), and if so its operands become needed */
static int lighting_step(const DWORD *instruction, unsigned long *outputs, struct lighting_reads *reads)
{
	unsigned long mac = field(instruction, 1, 21, 4);
	unsigned long ilu = field(instruction, 1, 25, 3);
	unsigned long mac_mask = mask_components(field(instruction, 3, 24, 4));
	unsigned long temporary = field(instruction, 3, 20, 4);
	unsigned long ilu_mask = mask_components(field(instruction, 3, 16, 4));
	unsigned long output_mask = mask_components(field(instruction, 3, 12, 4));
	unsigned long output_address = field(instruction, 3, 3, 8);
	unsigned long output_from_ilu = field(instruction, 3, 2, 1);
	unsigned long mac_needed = 0, ilu_needed = 0;
	int output = output_address == 3 ? 0 : output_address == 4 ? 1 : output_address == 12 ? 2 : -1;

	/* the writes, last first (instruction_writes' order) */
	if (output_mask && field(instruction, 3, 11, 1) && output >= 0 && (output_from_ilu ? ilu : mac) != 0)
	{
		unsigned long wanted = outputs[output] & output_mask;

		outputs[output] &= ~wanted;
		*(output_from_ilu ? &ilu_needed : &mac_needed) |= wanted;
	}
	if (ilu != _ilu_nop && ilu_mask)
	{
		unsigned long reg = mac != _mac_nop ? 1 : temporary;

		if (reg <= 12)
		{
			ilu_needed |= reads->needed[reg] & ilu_mask;
			reads->needed[reg] &= ~ilu_mask;
		}
	}
	if (mac != _mac_nop && mac != _mac_arl && mac_mask && temporary <= 12)
	{
		mac_needed |= reads->needed[temporary] & mac_mask;
		reads->needed[temporary] &= ~mac_mask;
	}
	if (!mac_needed && !ilu_needed)
		return 0;
	switch (mac)
	{
	case _mac_nop:
		break;
	case _mac_mov:
		lighting_read(instruction, 'A', mac_needed, reads);
		break;
	case _mac_add:
		lighting_read(instruction, 'A', mac_needed, reads);
		lighting_read(instruction, 'C', mac_needed, reads);
		break;
	case _mac_mad:
		lighting_read(instruction, 'C', mac_needed, reads);
		/* fall through */
	case _mac_mul: case _mac_min: case _mac_max: case _mac_slt: case _mac_sge:
		lighting_read(instruction, 'A', mac_needed, reads);
		lighting_read(instruction, 'B', mac_needed, reads);
		break;
	case _mac_dp3:
		if (mac_needed) { lighting_read(instruction, 'A', 7, reads); lighting_read(instruction, 'B', 7, reads); }
		break;
	case _mac_dph:
		if (mac_needed) { lighting_read(instruction, 'A', 7, reads); lighting_read(instruction, 'B', 15, reads); }
		break;
	case _mac_dp4:
		if (mac_needed) { lighting_read(instruction, 'A', 15, reads); lighting_read(instruction, 'B', 15, reads); }
		break;
	case _mac_dst:
		if (mac_needed) { lighting_read(instruction, 'A', 6, reads); lighting_read(instruction, 'B', 10, reads); }
		break;
	default:
		/* (arl: a0 is not carried to the pixel shader) */
		if (mac_needed)
			reads->relative = 1;
		break;
	}
	switch (ilu)
	{
	case _ilu_nop:
		break;
	case _ilu_mov:
		lighting_read(instruction, 'C', ilu_needed, reads);
		break;
	case _ilu_lit:
		if (ilu_needed)
			lighting_read(instruction, 'C', 11, reads);
		break;
	default:
		if (ilu_needed)
			lighting_read(instruction, 'C', 1, reads);
		break;
	}
	return 1;
}

/* the instructions that run: up to the one marked final */
static unsigned long lighting_length(const DWORD *instructions, unsigned long instruction_count)
{
	unsigned long index;

	for (index = 0; index < instruction_count; index++)
		if (field(instructions + index * 4, 3, 0, 1))
			return index + 1;
	return instruction_count;
}

int nv2a_lighting_slice(const DWORD *instructions, unsigned long instruction_count, struct nv2a_lighting_slice *slice)
{
	struct lighting_reads reads;
	unsigned long outputs[3], length = lighting_length(instructions, instruction_count), index;
	long cut = -1, scan;
	int reg, ambient = 0;

	memset(slice, 0, sizeof(*slice));
	slice->normal = -1;
	slice->position = -1;
	slice->normal_scale = -1;
	if (!length || length > NV2A_LIGHTING_MAX_INSTRUCTIONS)
		return 0;
	/* a model lighting program reads the ambient color, c[27] */
	for (index = 0; index < length; index++)
	{
		unsigned long swizzle[4];
		int constant;
		char which;

		for (which = 'A'; which <= 'C'; which++)
		{
			operand_register(instructions + index * 4, which, swizzle, &constant);
			ambient |= constant == 27;
		}
	}
	if (!ambient)
		return 0;
	/* first the whole program's slice, to find where it starts: its first
	instruction that reads the camera position (c[4]) or a light (c[17] to
	c[27]), by which the world position and normal are made */
	memset(&reads, 0, sizeof(reads));
	outputs[0] = outputs[1] = outputs[2] = 15;
	for (scan = (long)length - 1; scan >= 0; scan--)
	{
		reads.reads_lighting = 0;
		if (lighting_step(instructions + scan * 4, outputs, &reads) && reads.reads_lighting)
			cut = scan;
	}
	if (cut < 0)
		return 0;
	/* then the slice from there on, and what it reads from before */
	memset(&reads, 0, sizeof(reads));
	outputs[0] = outputs[1] = outputs[2] = 15;
	for (scan = (long)length - 1; scan >= cut; scan--)
		slice->in_slice[scan] = (unsigned char)lighting_step(instructions + scan * 4, outputs, &reads);
	if (reads.relative)
		return 0;
	for (index = 0; index < 3; index++)
		if (outputs[index] != 15)
			slice->outputs |= 1UL << index;
	if (!slice->outputs)
		return 0;
	for (reg = 0; reg < LIGHTING_REGISTERS; reg++)
	{
		if (!reads.needed[reg])
			continue;
		if (slice->live_count == NV2A_LIGHTING_MAX_VARYINGS)
			return 0;
		slice->live[slice->live_count++] = (unsigned char)reg;
	}
	slice->cut = (unsigned long)cut;
	/* the normal: what the first distant light's dot product (with c[23])
	takes, back through scalings and moves to a register from before the cut */
	for (index = (unsigned long)cut; index < length; index++)
	{
		const DWORD *instruction = instructions + index * 4;
		unsigned long swizzle[4], live;
		int a, b, constant_a, constant_b;

		if (!slice->in_slice[index] || field(instruction, 1, 21, 4) != _mac_dp3)
			continue;
		a = operand_register(instruction, 'A', swizzle, &constant_a);
		b = operand_register(instruction, 'B', swizzle, &constant_b);
		reg = constant_b == 23 ? a : constant_a == 23 ? b : -1;
		if (reg < 0)
			continue;
		for (scan = (long)index - 1; scan >= cut && reg >= 0; scan--)
		{
			const DWORD *writer = instructions + scan * 4;
			unsigned long mac = field(writer, 1, 21, 4);
			int constant;

			/* (a write of its x, y or z) */
			if (!slice->in_slice[scan] || mac == _mac_nop || mac == _mac_arl ||
				(int)field(writer, 3, 20, 4) != reg || !(field(writer, 3, 24, 4) & 0xe))
			{
				continue;
			}
			reg = mac == _mac_mov || mac == _mac_mul ? operand_register(writer, 'A', swizzle, &constant) : -1;
			if (mac == _mac_mul && reg >= 0)
			{
				unsigned long scale_swizzle[4];
				int scale_constant;

				operand_register(writer, 'B', scale_swizzle, &scale_constant);
				slice->normal_scale = scale_constant >= 0 && scale_constant < LIGHTING_RELATIVE ?
					scale_constant * 4 + (int)scale_swizzle[0] : -1;
			}
		}
		for (live = 0; reg >= 0 && live < slice->live_count; live++)
			if (slice->live[live] == reg)
				slice->normal = (int)live;
		break;
	}
	/* the world position: the register both the first point light's vector
	(c[17] - P) and the camera's (c[4] - P) subtract, read as it stands at
	the cut */
	{
		int light_from = -1, camera_from = -1;

		for (index = (unsigned long)cut; index < length; index++)
		{
			const DWORD *instruction = instructions + index * 4;
			unsigned long swizzle[4];
			int a, c, constant_a, constant_c;

			if (field(instruction, 1, 21, 4) != _mac_add)
				continue;
			a = operand_register(instruction, 'A', swizzle, &constant_a);
			c = operand_register(instruction, 'C', swizzle, &constant_c);
			/* (the register operand is the negated one) */
			if (constant_a == 17 && c >= 0 && field(instruction, 2, 10, 1))
				light_from = c;
			else if (constant_c == 4 && a >= 0 && field(instruction, 1, 8, 1))
				camera_from = a;
			else if (constant_a == 4 && c >= 0 && field(instruction, 2, 10, 1))
				camera_from = c;
		}
		if (light_from >= 0 && light_from == camera_from)
		{
			unsigned long live;

			for (live = 0; live < slice->live_count; live++)
				if (slice->live[live] == light_from)
					slice->position = (int)live;
		}
	}
	return 1;
}

static void lighting_register_name(struct xgpu_text *text, int reg)
{
	if (reg < 12)
		xgpu_text_append(text, "r%d", reg);
	else if (reg == 12)
		xgpu_text_append(text, "oPos");
	else
		xgpu_text_append(text, "v%d", reg - 16);
}

/* the vertex shader's side: the live registers as they stand at the cut */
static void lighting_capture(struct xgpu_text *text, const struct nv2a_lighting_slice *slice)
{
	unsigned long index;

	xgpu_text_append(text, "\t/* per-pixel lighting: what the pixel shader's lighting reads */\n");
	for (index = 0; index < slice->live_count; index++)
	{
		xgpu_text_append(text, "\txL%lu = ", index);
		lighting_register_name(text, slice->live[index]);
		xgpu_text_append(text, ";\n");
	}
	if (slice->normal >= 0)
		xgpu_text_append(text, "\txLn = length(xL%d.xyz);\n", slice->normal);
}

void nv2a_lighting_pixel_helpers(struct xgpu_text *text)
{
	xgpu_text_append(text, "%s", shader_helpers);
}

/* display.point_light_highlight's presets: brightness, Blinn-Phong exponent */
static const float highlight_presets[NV2A_HIGHLIGHT_PRESETS][2] =
{
	{ 0.35f, 96.0f }, { 0.5f, 64.0f }, { 0.75f, 48.0f }, { 1.0f, 32.0f },
};

/* the point-light highlight: for each of the program's two point lights, the
game's own color, distance falloff and spot cone (as the vertex program's
lighting computes them), times a Blinn-Phong lobe toward the camera; the
pixel shader adds it to the reflection (nv2a_pixel_shader_key
lighting_highlight) */
static void lighting_highlight(struct xgpu_text *text, const struct nv2a_lighting_slice *slice, int preset)
{
	int light;

	xgpu_text_append(text, "\t{\n\tvec3 hp = in.xL%d.xyz, hn = normalize(in.xL%d.xyz);\n"
		"\tvec3 hv = normalize(c[4].xyz - hp);\n", slice->position, slice->normal);
	for (light = 0; light < 2; light++)
	{
		int base = 17 + 3 * light;

		xgpu_text_append(text,
			"\t{\n"
			"\t\tvec3 l = c[%d].xyz - hp;\n"
			"\t\tfloat d2 = dot(l, l);\n"
			"\t\tvec3 ln = l * rsqrt(max(d2, 1.0e-8));\n"
			"\t\tfloat falloff = clamp(1.0 - d2 * c[%d].w, 0.0, 1.0);\n"
			"\t\tfloat cone = clamp(dot(ln, -c[%d].xyz) * c[%d].w + c[%d].w, 0.0, 1.0);\n"
			"\t\tfloat facing = clamp(dot(hn, ln), 0.0, 1.0);\n"
			"\t\tfloat lobe = pow(clamp(dot(hn, normalize(ln + hv)), 0.0, 1.0), %.1f);\n"
			"\t\tppl_highlight += c[%d].xyz * (falloff * cone * (facing > 0.0 ? lobe : 0.0) * %.3f);\n"
			"\t}\n",
			base, base, base + 1, base + 1, base + 2, highlight_presets[preset - 1][1], base + 2,
			highlight_presets[preset - 1][0]);
	}
	xgpu_text_append(text, "\t}\n");
}

/* the longer point light list's lights past the game's two (vertex
constants 39 to 57, rasterizer_set_model_lighting): each lit as the vertex
program lights its two (color, distance falloff, spot cone and the clamped
dot product with the normal it uses), added to the diffuse light, and to the
highlight when there is one */
static void lighting_more_point_lights(struct xgpu_text *text, const struct nv2a_lighting_slice *slice,
	int point_lights, int highlight)
{
	static const char components[] = "xyzw";

	/* (from the varyings: the slice has used its registers by now) */
	xgpu_text_append(text, "\t{\n\tvec3 ep = in.xL%d.xyz, en = normalize(in.xL%d.xyz) * in.xLn", slice->position,
		slice->normal);
	if (slice->normal_scale >= 0)
		xgpu_text_append(text, " * c[%d].%c;\n", slice->normal_scale / 4, components[slice->normal_scale % 4]);
	else
		xgpu_text_append(text, ";\n");
	xgpu_text_append(text,
		"\tvec3 ev = normalize(c[4].xyz - ep), eh = normalize(en);\n"
		"\tint count = min(int(c[39].x + 0.5), %d);\n"
		"\tfor (int i = 0; i < count; i++)\n"
		"\t{\n"
		"\t\tint b = 40 + 3 * i;\n"
		"\t\tvec3 l = c[b].xyz - ep;\n"
		"\t\tfloat d2 = dot(l, l);\n"
		"\t\tvec3 ln = l * rsqrt(max(d2, 1.0e-8));\n"
		"\t\tfloat falloff = max(1.0 - d2 * c[b].w, 0.0);\n"
		"\t\tfloat cone = clamp(dot(ln, -c[b + 1].xyz) * c[b + 1].w + c[b + 2].w, 0.0, 1.0);\n"
		"\t\toD0.xyz += c[b + 2].xyz * (falloff * clamp(dot(ln, en), 0.0, 1.0) * cone);\n",
		point_lights);
	if (highlight > 0 && highlight <= NV2A_HIGHLIGHT_PRESETS)
		xgpu_text_append(text,
			"\t\tfloat lobe = pow(clamp(dot(eh, normalize(ln + ev)), 0.0, 1.0), %.1f);\n"
			"\t\tppl_highlight += c[b + 2].xyz * (min(falloff, 1.0) * cone * (dot(eh, ln) > 0.0 ? lobe : 0.0) * %.3f);\n",
			highlight_presets[highlight - 1][1], highlight_presets[highlight - 1][0]);
	xgpu_text_append(text, "\t}\n\t}\n");
}

void nv2a_lighting_pixel_block(struct xgpu_text *text, const DWORD *instructions, unsigned long instruction_count,
	const struct nv2a_lighting_slice *slice, int highlight, int point_lights)
{
	unsigned long index, length = lighting_length(instructions, instruction_count);

	xgpu_text_append(text, "\tvec3 ppl_highlight = vec3(0.0);\n");
	if (highlight > 0 && highlight <= NV2A_HIGHLIGHT_PRESETS && slice->position >= 0 && slice->normal >= 0)
		lighting_highlight(text, slice, highlight);
	xgpu_text_append(text,
		"\t/* per-pixel lighting: the vertex program's lighting again, for this pixel */\n"
		"\t{\n"
		"\tvec4 r0 = vec4(0.0), r1 = vec4(0.0), r2 = vec4(0.0), r3 = vec4(0.0);\n"
		"\tvec4 r4 = vec4(0.0), r5 = vec4(0.0), r6 = vec4(0.0), r7 = vec4(0.0);\n"
		"\tvec4 r8 = vec4(0.0), r9 = vec4(0.0), r10 = vec4(0.0), r11 = vec4(0.0);\n"
		"\tvec4 v0 = vec4(0.0, 0.0, 0.0, 1.0), v1 = v0, v2 = v0, v3 = v0, v4 = v0, v5 = v0, v6 = v0, v7 = v0;\n"
		"\tvec4 v8 = v0, v9 = v0, v10 = v0, v11 = v0, v12 = v0, v13 = v0, v14 = v0, v15 = v0;\n"
		"\tvec4 oPos = vec4(0.0, 0.0, 0.0, 1.0), oD0 = xD0, oD1 = xD1, oT3 = xT3;\n"
		"\tvec4 oB0 = oPos, oB1 = oPos, oT0 = oPos, oT1 = oPos, oT2 = oPos;\n"
		"\tvec4 oFog = vec4(1.0), oPts = vec4(1.0), oUnused = vec4(0.0);\n"
		"\tint a0 = 0;\n"
		"\tvec4 A, B, C, mac, ilu;\n");
	for (index = 0; index < slice->live_count; index++)
	{
		xgpu_text_append(text, "\t");
		lighting_register_name(text, slice->live[index]);
		xgpu_text_append(text, " = in.xL%lu;\n", index);
	}
	/* interpolated, the normal is shorter between vertices: as long as the
	vertices' normals again, in its own direction */
	if (slice->normal >= 0)
	{
		xgpu_text_append(text, "\tif (length(");
		lighting_register_name(text, slice->live[slice->normal]);
		xgpu_text_append(text, ".xyz) > 0.0)\n\t\t");
		lighting_register_name(text, slice->live[slice->normal]);
		xgpu_text_append(text, ".xyz *= in.xLn / length(");
		lighting_register_name(text, slice->live[slice->normal]);
		xgpu_text_append(text, ".xyz);\n");
	}
	for (index = slice->cut; index < length; index++)
	{
		if (!slice->in_slice[index])
			continue;
		instruction_compute(text, instructions + index * 4, index);
		instruction_writes(text, instructions + index * 4, slice->outputs);
	}
	if (point_lights > 0 && (slice->outputs & 1) && slice->position >= 0 && slice->normal >= 0)
		lighting_more_point_lights(text, slice, point_lights, highlight);
	/* as the vertex shader hands them over */
	if (slice->outputs & 1)
		xgpu_text_append(text, "\txD0 = clamp(oD0, 0.0, 1.0);\n");
	if (slice->outputs & 2)
		xgpu_text_append(text, "\txD1 = clamp(oD1, 0.0, 1.0);\n");
	if (slice->outputs & 4)
		xgpu_text_append(text, "\txT3 = oT3;\n");
	xgpu_text_append(text, "\t}\n");
}

/* the lit programs, by program hash (nv2a_lighting_register) */
struct lighting_program
{
	uint64_t hash;
	DWORD *instructions;
	unsigned long instruction_count;
	struct lighting_program *next;
};

static struct lighting_program *lighting_programs;

void nv2a_lighting_register(uint64_t program_hash, const DWORD *instructions, unsigned long instruction_count)
{
	struct lighting_program *program;

	for (program = lighting_programs; program; program = program->next)
		if (program->hash == program_hash)
			return;
	program = calloc(1, sizeof(*program));
	if (!program)
		return;
	program->instructions = malloc(instruction_count * 4 * sizeof(DWORD));
	if (!program->instructions)
	{
		free(program);
		return;
	}
	memcpy(program->instructions, instructions, instruction_count * 4 * sizeof(DWORD));
	program->hash = program_hash;
	program->instruction_count = instruction_count;
	program->next = lighting_programs;
	lighting_programs = program;
}

int nv2a_lighting_find(uint64_t program_hash, const DWORD **instructions, unsigned long *instruction_count,
	struct nv2a_lighting_slice *slice)
{
	struct lighting_program *program;

	for (program = lighting_programs; program; program = program->next)
	{
		if (program->hash != program_hash)
			continue;
		*instructions = program->instructions;
		*instruction_count = program->instruction_count;
		return nv2a_lighting_slice(program->instructions, program->instruction_count, slice);
	}
	return 0;
}

char *nv2a_vertex_shader_translate(const struct nv2a_dialect *dialect, const DWORD *instructions,
	unsigned long instruction_count, unsigned long packed_attribute_mask)
{
	return nv2a_vertex_shader_translate_lit(dialect, instructions, instruction_count, packed_attribute_mask, 0);
}

char *nv2a_vertex_shader_translate_lit(const struct nv2a_dialect *dialect, const DWORD *instructions,
	unsigned long instruction_count, unsigned long packed_attribute_mask, int lit)
{
	struct xgpu_text text = { 0 };
	unsigned long index;
	struct nv2a_lighting_slice slice;
	const struct nv2a_lighting_slice *lighting = NULL;
	/* the dot products (_mac_dp3, _mac_dph, _mac_dp4; 0 none) that last
	wrote the clip position's z and w (r12), for reversed_depth_operands */
	unsigned long depth_z_op = 0, depth_w_op = 0;
	int reversed_kept = 0;


	if (!dialect->version)
	{
		platform_log("vertex shader translated before the dialect was set");
		return NULL;
	}
	/* (Metal only: the pixel shader reads the vertex constants) */
	if (lit && dialect->msl && nv2a_lighting_slice(instructions, instruction_count, &slice))
		lighting = &slice;
	if (dialect->msl)
	{
		nv2a_msl_prelude(&text, GPU_UNIFORM_VERTEX);
		nv2a_msl_vertex_outputs(&text, lighting);
		xgpu_text_append(&text, "%s", shader_helpers);
		nv2a_msl_vertex_main(&text, dialect, packed_attribute_mask, lighting);
	}
	else
	{
		if (dialect->es)
			xgpu_text_append(&text, "#version %u es\nprecision highp float;\nprecision highp int;\n", (unsigned)dialect->version);
		else
			xgpu_text_append(&text, "#version %u core\n", (unsigned)dialect->version);
		nv2a_uniform_declarations(&text, dialect, GPU_UNIFORM_VERTEX);
		xgpu_text_append(&text, "%s%s", shader_outputs, shader_helpers);
		for (index = 0; index < XGPU_VERTEX_ATTRIBUTE_COUNT; index++)
		{
			if (packed_attribute_mask & (1UL << index))
				xgpu_text_append(&text, "layout(location = %lu) in uint v%lu_packed;\n", index, index);
			else
				xgpu_text_append(&text, "layout(location = %lu) in vec4 v%lu_in;\n", index, index);
		}

		xgpu_text_append(&text, "void main()\n{\n");
	}
	for (index = 0; index < XGPU_VERTEX_ATTRIBUTE_COUNT; index++)
	{
		if (packed_attribute_mask & (1UL << index))
			xgpu_text_append(&text, "\tvec4 v%lu = unpack_normpacked3(v%lu_packed);\n", index, index);
		else
			xgpu_text_append(&text, "\tvec4 v%lu = v%lu_in;\n", index, index);
	}
	xgpu_text_append(&text,
		"\tvec4 r0 = vec4(0.0), r1 = vec4(0.0), r2 = vec4(0.0), r3 = vec4(0.0);\n"
		"\tvec4 r4 = vec4(0.0), r5 = vec4(0.0), r6 = vec4(0.0), r7 = vec4(0.0);\n"
		"\tvec4 r8 = vec4(0.0), r9 = vec4(0.0), r10 = vec4(0.0), r11 = vec4(0.0);\n"
		"\tvec4 oPos = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"\tvec4 oD0 = vec4(0.0, 0.0, 0.0, 1.0), oD1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"\tvec4 oB0 = vec4(0.0, 0.0, 0.0, 1.0), oB1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"\tvec4 oT0 = vec4(0.0, 0.0, 0.0, 1.0), oT1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"\tvec4 oT2 = vec4(0.0, 0.0, 0.0, 1.0), oT3 = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"\tvec4 oFog = vec4(1.0), oPts = vec4(point_size), oUnused = vec4(0.0);\n"
		"\tint a0 = 0;\n"
		"\tvec4 A, B, C, mac, ilu;\n");
	if (dialect->clip_capture)
		xgpu_text_append(&text, "\tvec4 clip_position = vec4(0.0);\n\tbool clip_captured = false;\n");
	if (dialect->msl)
		xgpu_text_append(&text,
			"\tvec4 depth_za = vec4(0.0), depth_zb = vec4(0.0), depth_wa = vec4(0.0), depth_wb = vec4(0.0);\n"
			"\tfloat clip_reversed = 0.0;\n\tbool clip_reversed_kept = false;\n");

	for (index = 0; index < instruction_count; index++)
	{
		const DWORD *instruction = instructions + index * 4;
		unsigned long mac = field(instruction, 1, 21, 4);
		unsigned long ilu = field(instruction, 1, 25, 3);

		if (lighting && index == lighting->cut)
			lighting_capture(&text, lighting);
		instruction_compute(&text, instruction, index);
		/* the screen-space conversion takes the reciprocal of the clip-space
		position's w (rcc of r12.w); keep the position it converts */
		if (dialect->clip_capture && ilu == _ilu_rcc && field(instruction, 3, 28, 2) == _mux_temporary &&
			((field(instruction, 2, 0, 2) << 2) | field(instruction, 3, 30, 2)) == 12)
		{
			xgpu_text_append(&text, "\tclip_position = oPos;\n\tclip_captured = true;\n");
			if (dialect->msl)
				reversed_kept = reversed_depth_operands(&text, depth_z_op, depth_w_op);
		}
		if (dialect->msl)
			depth_rows_track(&text, instruction, mac, ilu, &depth_z_op, &depth_w_op);

		instruction_writes(&text, instruction, 0);
		if (field(instruction, 3, 0, 1))
			break;
	}

	xgpu_text_append(&text,
		"\t/* undo the screen-space conversion done with c[-38] and c[-37] */\n"
		"\tvec3 scale = vec3(viewport_scale.x != 0.0 ? viewport_scale.x : 1.0,\n"
		"\t\tviewport_scale.y != 0.0 ? viewport_scale.y : 1.0,\n"
		"\t\tviewport_scale.z != 0.0 ? viewport_scale.z : 1.0);\n");
	/* Direct3D 8 puts pixel centers on integer screen coordinates (the game
	offsets its screen-space quads by -0.5 to match), OpenGL on half-integers */
	if (dialect->clip_capture)
	{
		/* The conversion is screen = clip * c[-38] * rcc(w) + c[-37]; undoing
		it by multiplying by w again is lossy near the camera plane, where rcc
		clamps and 1/w rounds differently on each GPU (Mali put vertices of
		the first-person weapon at the vanishing point). Where the clip
		position was kept, the same result is computed without dividing. */
		xgpu_text_append(&text,
			"\tvec4 position;\n"
			"\tif (clip_captured)\n"
			"\t\tposition = vec4((clip_position.xyz * c[%d].xyz + (c[%d].xyz + vec3(0.5 + screen_offset, 0.5, 0.0)\n"
			"\t\t\t- viewport_offset.xyz) * clip_position.w) / scale, clip_position.w);\n"
			"\telse\n"
			"\t\tposition = vec4((vec3(oPos.xy + vec2(0.5 + screen_offset, 0.5), oPos.z) - viewport_offset.xyz) / scale * oPos.w, oPos.w);\n",
			XGPU_VERTEX_CONSTANT_BIAS - 38, XGPU_VERTEX_CONSTANT_BIAS - 37);
	}
	else
	{
		xgpu_text_append(&text,
			"\tvec3 ndc = (vec3(oPos.xy + vec2(0.5 + screen_offset, 0.5), oPos.z) - viewport_offset.xyz) / scale;\n"
			"\tvec4 position = vec4(ndc * oPos.w, oPos.w);\n");
	}
	/* A position whose w is zero, or is not a number, is the clip-space
	origin: the screen conversion's reciprocal is clamped rather than
	infinite, so a large position times a w of zero is exactly zero, and the
	origin is inside the frustum. Nothing then clips the triangle away and
	the divide by w puts the vertex in the middle of the screen: the triangle
	is drawn out to it from the first-person weapon, whose pose follows the
	camera and so reaches the camera plane. The divide on the Xbox sends such
	a vertex to infinity and the clipper takes the triangle; put it behind the
	camera instead, which the clipper also takes (upstream's 3d2c04d6). */
	xgpu_text_append(&text,
		"\tif (!(abs(position.w) > 0.0))\n"
		"\t\tposition = vec4(0.0, 0.0, 0.0, -1.0);\n"
		"\tgl_Position = position;\n");
	/* what glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE) does on desktop GL:
	rows from the top, depth 0..1 */
	if (dialect->clip_y_flip)
		xgpu_text_append(&text, "\tgl_Position.y = -gl_Position.y;\n");
	if (dialect->clip_z_remap)
		xgpu_text_append(&text, "\tgl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n");
	/* Metal draws reversed-Z (gpu_metal.m): depth 1 at the near plane, 0 at
	the far, so a float depth buffer keeps its precision at a distance. w - z
	of the finished position does not: at a distance z is within a few
	thousandths of w, so the difference keeps only the rounding of the two
	dot products that made them, different at every vertex, and a decal a
	millimeter off its surface lost whole triangles to it (Blood Gulch's base
	emblems at a scope's zoom). Where the dot products' operands were kept,
	the difference of the projection's w and z rows is taken first, which is
	exact, and the vertex's dot product with it is as precise as any other
	(reversed_depth_operands). The screen conversion's z, undone as above, is
	(clip.z * c[-38].z + (c[-37].z - viewport_offset.z) * clip.w) / scale.z,
	so its w - z is written with clip.w - clip.z in place of clip.z. */
	if (dialect->msl && reversed_kept)
		xgpu_text_append(&text,
			"\tif (clip_captured && clip_reversed_kept && abs(clip_position.w) > 0.0)\n"
			"\t\tgl_Position.z = (clip_reversed * c[%d].z + clip_position.w * ((scale.z - c[%d].z)\n"
			"\t\t\t- (c[%d].z - viewport_offset.z))) / scale.z;\n"
			"\telse\n\t",
			XGPU_VERTEX_CONSTANT_BIAS - 38, XGPU_VERTEX_CONSTANT_BIAS - 38, XGPU_VERTEX_CONSTANT_BIAS - 37);
	if (dialect->msl)
		xgpu_text_append(&text, "\tgl_Position.z = gl_Position.w - gl_Position.z;\n");
	xgpu_text_append(&text,
		"\tgl_PointSize = oPts.x;\n"
		"\txD0 = clamp(oD0, 0.0, 1.0);\n"
		"\txD1 = clamp(oD1, 0.0, 1.0);\n"
		"\txB0 = clamp(oB0, 0.0, 1.0);\n"
		"\txB1 = clamp(oB1, 0.0, 1.0);\n"
		"\txT0 = oT0;\n"
		"\txT1 = oT1;\n"
		"\txT2 = oT2;\n"
		"\txT3 = oT3;\n"
		"\txFog = oFog.x;\n");
	if (dialect->msl)
		nv2a_msl_vertex_return(&text, lighting);
	xgpu_text_append(&text, "}\n");
	return text.buffer;
}

/* ---------- the program on the CPU (nv2a_vertex_program_position) */

/* an operand as operand() reads it, with its swizzle and negation */
static void operand_value(const DWORD *instruction, char which, int relative, int a0, const float (*constants)[4],
	const float (*inputs)[4], const float (*temporaries)[4], const float position[4], float value[4])
{
	unsigned long negate, swizzle[4], index, mux;
	const float *from = NULL;
	static const float zero[4];
	int axis;

	switch (which)
	{
	case 'A':
		negate = field(instruction, 1, 8, 1);
		swizzle[0] = field(instruction, 1, 6, 2);
		swizzle[1] = field(instruction, 1, 4, 2);
		swizzle[2] = field(instruction, 1, 2, 2);
		swizzle[3] = field(instruction, 1, 0, 2);
		index = field(instruction, 2, 28, 4);
		mux = field(instruction, 2, 26, 2);
		break;
	case 'B':
		negate = field(instruction, 2, 25, 1);
		swizzle[0] = field(instruction, 2, 23, 2);
		swizzle[1] = field(instruction, 2, 21, 2);
		swizzle[2] = field(instruction, 2, 19, 2);
		swizzle[3] = field(instruction, 2, 17, 2);
		index = field(instruction, 2, 13, 4);
		mux = field(instruction, 2, 11, 2);
		break;
	default:
		negate = field(instruction, 2, 10, 1);
		swizzle[0] = field(instruction, 2, 8, 2);
		swizzle[1] = field(instruction, 2, 6, 2);
		swizzle[2] = field(instruction, 2, 4, 2);
		swizzle[3] = field(instruction, 2, 2, 2);
		index = (field(instruction, 2, 0, 2) << 2) | field(instruction, 3, 30, 2);
		mux = field(instruction, 3, 28, 2);
		break;
	}
	switch (mux)
	{
	case _mux_temporary:
		from = index == 12 ? position : temporaries[index < 12 ? index : 0];
		break;
	case _mux_input:
		from = inputs[field(instruction, 1, 9, 4)];
		break;
	case _mux_constant:
		{
			long constant = (long)field(instruction, 1, 13, 8) + (relative ? a0 : 0);

			constant = constant < 0 ? 0 : constant > XGPU_VERTEX_CONSTANT_COUNT - 1 ? XGPU_VERTEX_CONSTANT_COUNT - 1 :
				constant;
			from = constants[constant];
		}
		break;
	default:
		from = zero;
		break;
	}
	for (axis = 0; axis < 4; axis++)
		value[axis] = negate ? -from[swizzle[axis]] : from[swizzle[axis]];
}

static void masked_write(float *to, const float *from, unsigned long mask)
{
	/* bit 3 is x */
	if (mask & 8) to[0] = from[0];
	if (mask & 4) to[1] = from[1];
	if (mask & 2) to[2] = from[2];
	if (mask & 1) to[3] = from[3];
}

void nv2a_vertex_program_position(const DWORD *instructions, unsigned long instruction_count,
	const float (*constants)[4], const float (*inputs)[4], float position[4])
{
	float temporaries[12][4];
	unsigned long index;
	int a0 = 0, axis;

	memset(temporaries, 0, sizeof(temporaries));
	/* oPos, which r12 also names */
	position[0] = position[1] = position[2] = 0.0f;
	position[3] = 1.0f;
	for (index = 0; index < instruction_count; index++)
	{
		const DWORD *instruction = instructions + index * 4;
		unsigned long mac = field(instruction, 1, 21, 4);
		unsigned long ilu = field(instruction, 1, 25, 3);
		unsigned long mac_mask = field(instruction, 3, 24, 4);
		unsigned long temporary = field(instruction, 3, 20, 4);
		unsigned long ilu_mask = field(instruction, 3, 16, 4);
		unsigned long output_mask = field(instruction, 3, 12, 4);
		unsigned long output_is_register = field(instruction, 3, 11, 1);
		unsigned long output_address = field(instruction, 3, 3, 8);
		unsigned long output_from_ilu = field(instruction, 3, 2, 1);
		int relative = (int)field(instruction, 3, 1, 1);
		float a[4], b[4], c[4], mac_value[4] = { 0 }, ilu_value[4] = { 0 };

		operand_value(instruction, 'A', relative, a0, constants, inputs, (const float (*)[4])temporaries, position, a);
		operand_value(instruction, 'B', relative, a0, constants, inputs, (const float (*)[4])temporaries, position, b);
		operand_value(instruction, 'C', relative, a0, constants, inputs, (const float (*)[4])temporaries, position, c);
		for (axis = 0; axis < 4; axis++)
		{
			switch (mac)
			{
			case _mac_mov: case _mac_arl: mac_value[axis] = a[axis]; break;
			case _mac_mul: mac_value[axis] = a[axis] * b[axis]; break;
			case _mac_add: mac_value[axis] = a[axis] + c[axis]; break;
			case _mac_mad: mac_value[axis] = a[axis] * b[axis] + c[axis]; break;
			case _mac_dp3: mac_value[axis] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; break;
			case _mac_dph: mac_value[axis] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + b[3]; break;
			case _mac_dp4: mac_value[axis] = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]; break;
			case _mac_min: mac_value[axis] = a[axis] < b[axis] ? a[axis] : b[axis]; break;
			case _mac_max: mac_value[axis] = a[axis] > b[axis] ? a[axis] : b[axis]; break;
			case _mac_slt: mac_value[axis] = a[axis] < b[axis] ? 1.0f : 0.0f; break;
			case _mac_sge: mac_value[axis] = a[axis] >= b[axis] ? 1.0f : 0.0f; break;
			default: break;
			}
		}
		if (mac == _mac_dst)
		{
			mac_value[0] = 1.0f;
			mac_value[1] = a[1] * b[1];
			mac_value[2] = a[2];
			mac_value[3] = b[3];
		}
		switch (ilu)
		{
		case _ilu_mov:
			memcpy(ilu_value, c, sizeof(ilu_value));
			break;
		case _ilu_rcp:
			ilu_value[0] = 1.0f / c[0];
			break;
		case _ilu_rcc:
			{
				float r = 1.0f / c[0];

				if (r > 0.0f)
					r = r < 5.42101e-20f ? 5.42101e-20f : r > 1.884467e+19f ? 1.884467e+19f : r;
				else
					r = r < -1.884467e+19f ? -1.884467e+19f : r > -5.42101e-20f ? -5.42101e-20f : r;
				ilu_value[0] = r;
			}
			break;
		case _ilu_rsq:
			ilu_value[0] = 1.0f / sqrtf(fabsf(c[0]));
			break;
		case _ilu_exp:
			ilu_value[0] = exp2f(floorf(c[0]));
			ilu_value[1] = c[0] - floorf(c[0]);
			ilu_value[2] = exp2f(c[0]);
			ilu_value[3] = 1.0f;
			break;
		case _ilu_log:
			{
				float x = fabsf(c[0]);

				if (x == 0.0f)
				{
					ilu_value[0] = ilu_value[2] = -1.0e30f;
					ilu_value[1] = ilu_value[3] = 1.0f;
				}
				else
				{
					float e = floorf(log2f(x));

					ilu_value[0] = e;
					ilu_value[1] = x / exp2f(e);
					ilu_value[2] = log2f(x);
					ilu_value[3] = 1.0f;
				}
			}
			break;
		case _ilu_lit:
			ilu_value[0] = 1.0f;
			ilu_value[1] = c[0] > 0.0f ? c[0] : 0.0f;
			ilu_value[2] = c[0] > 0.0f ? powf(c[1] > 0.0f ? c[1] : 0.0f,
				c[3] < -127.9961f ? -127.9961f : c[3] > 127.9961f ? 127.9961f : c[3]) : 0.0f;
			ilu_value[3] = 1.0f;
			break;
		default:
			break;
		}
		/* the scalar results fill every component, as the shaders' vec4() */
		if (ilu == _ilu_rcp || ilu == _ilu_rcc || ilu == _ilu_rsq)
			ilu_value[1] = ilu_value[2] = ilu_value[3] = ilu_value[0];

		/* results are written only after both units have read their inputs */
		if (mac == _mac_arl)
			a0 = (int)floorf(mac_value[0] + 0.001f);
		else if (mac != _mac_nop && mac_mask)
			masked_write(temporary == 12 ? position : temporary < 12 ? temporaries[temporary] : temporaries[0],
				mac_value, mac_mask);
		if (ilu != _ilu_nop && ilu_mask)
		{
			unsigned long ilu_temporary = mac != _mac_nop ? 1 : temporary;

			masked_write(ilu_temporary == 12 ? position : ilu_temporary < 12 ? temporaries[ilu_temporary] :
				temporaries[0], ilu_value, ilu_mask);
		}
		if (output_mask && (output_from_ilu ? ilu : mac) != 0 && output_is_register && output_address == 0)
			masked_write(position, output_from_ilu ? ilu_value : mac_value, output_mask);
		if (field(instruction, 3, 0, 1))
			break;
	}
}
