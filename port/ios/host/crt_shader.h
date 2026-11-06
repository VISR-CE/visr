/* display.crt_effect (Hardcore graphics only): the 640x480 picture as a composite CRT television showed it, drawn
in place of the bilinear stretch that puts the picture on the screen (gpu_metal.m's present pass) and by
tools/crt_stills.m for look checks. Written from published descriptions of how a CRT forms its picture:

- composite video carries color in a narrow band beside the brightness signal, so color smears sideways across a
  few pixels while brightness stays sharp: the picture is blended along the line in YIQ, luma with a narrow kernel
  and chroma with a wide one;
- the beam draws each of the 480 lines as a bright band with dark gaps between, wider where the picture is
  bright (the spot grows with beam current): two neighboring lines' Gaussian beam profiles summed, their width
  following the line's brightness, normalized so the average brightness holds;
- an aperture grille splits each display pixel column into red, green and blue phosphor stripes: a three-column
  mask at the display's own pixels, only where a line covers at least two display pixels (finer would alias);
- bright phosphors glow onto their neighbors: a little of the picture's brightest light, blurred, is added back.

No screen curvature. The picture is assumed sRGB-like and worked in gamma 2.0 for speed. The present pass passes
the picture's size and the drawn area's size in pixels (CrtSizes). */
#pragma once

#define CRT_SHADER_SOURCE \
	"struct CrtSizes { float2 source; float2 output; };\n" \
	"static float3 crt_yiq(float3 c)\n" \
	"{\n" \
	"\treturn float3(dot(c, float3(0.299, 0.587, 0.114)), dot(c, float3(0.596, -0.274, -0.322)),\n" \
	"\t\tdot(c, float3(0.211, -0.523, 0.312)));\n" \
	"}\n" \
	"static float3 crt_rgb(float3 y)\n" \
	"{\n" \
	"\treturn float3(dot(y, float3(1.0, 0.956, 0.621)), dot(y, float3(1.0, -0.272, -0.647)),\n" \
	"\t\tdot(y, float3(1.0, -1.106, 1.703)));\n" \
	"}\n" \
	/* one line of the picture at u, as the composite signal leaves it: linear light */ \
	"static float3 crt_line(texture2d<float> picture, sampler linear, float u, float v, float texel)\n" \
	"{\n" \
	"\tconst float luma[5] = { 0.0, 0.25, 0.5, 0.25, 0.0 };\n" \
	"\tconst float chroma[5] = { 0.12, 0.2, 0.36, 0.2, 0.12 };\n" \
	"\tfloat3 sum = float3(0.0);\n" \
	"\tfor (int tap = 0; tap < 5; tap++)\n" \
	"\t{\n" \
	"\t\tfloat3 yiq = crt_yiq(picture.sample(linear, float2(u + float(tap - 2) * texel, v)).rgb);\n" \
	"\t\tsum += float3(yiq.x * luma[tap], yiq.yz * chroma[tap]);\n" \
	"\t}\n" \
	"\tfloat3 c = saturate(crt_rgb(sum));\n" \
	"\treturn c * c;\n" \
	"}\n" \
	/* the beam's profile across a line, d lines from its center, wider for brighter light; the integral of the
	   profile over the line pitch is one, so the average brightness of a flat field holds */ \
	"static float3 crt_beam(float d, float3 c)\n" \
	"{\n" \
	"\tfloat3 width = mix(float3(0.26), float3(0.46), sqrt(c));\n" \
	"\tfloat3 x = d / width;\n" \
	"\treturn exp(-0.5 * x * x) / (width * 2.5066283);\n" \
	"}\n" \
	"fragment float4 crt_fragment(PresentOut in [[stage_in]], texture2d<float> picture [[texture(0)]],\n" \
	"\tsampler linear [[sampler(0)]], constant CrtSizes &sizes [[buffer(0)]])\n" \
	"{\n" \
	"\tfloat2 uv = in.coordinates;\n" \
	"\tfloat texel = 1.0 / sizes.source.x;\n" \
	"\tfloat y = uv.y * sizes.source.y - 0.5;\n" \
	"\tfloat row = floor(y), f = y - row;\n" \
	"\tfloat3 above = crt_line(picture, linear, uv.x, (row + 0.5) / sizes.source.y, texel);\n" \
	"\tfloat3 below = crt_line(picture, linear, uv.x, (row + 1.5) / sizes.source.y, texel);\n" \
	"\tfloat3 color = above * crt_beam(f, above) + below * crt_beam(1.0 - f, below);\n" \
	/* phosphor glow: the brightest light of the neighborhood, a little of it */ \
	"\tfloat2 spread = 1.5 / sizes.source;\n" \
	"\tfloat3 glow = float3(0.0);\n" \
	"\tfor (int tap = 0; tap < 4; tap++)\n" \
	"\t{\n" \
	"\t\tfloat2 offset = float2(tap & 1 ? 1.0 : -1.0, tap & 2 ? 1.0 : -1.0) * spread;\n" \
	"\t\tfloat3 c = picture.sample(linear, uv + offset).rgb;\n" \
	"\t\tglow += max(c * c - 0.35, 0.0);\n" \
	"\t}\n" \
	"\tcolor += glow * 0.06;\n" \
	/* the aperture grille, only with two or more display pixels a line */ \
	"\tif (sizes.output.y >= 2.0 * sizes.source.y)\n" \
	"\t{\n" \
	"\t\tuint column = uint(in.position.x) % 3u;\n" \
	"\t\tfloat3 mask = float3(0.7);\n" \
	"\t\tmask[column] = 1.0;\n" \
	"\t\tcolor *= mask * 1.25;\n" \
	"\t}\n" \
	"\treturn float4(sqrt(saturate(color)), 1.0);\n" \
	"}\n"
