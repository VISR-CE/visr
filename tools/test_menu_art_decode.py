"""The decode the menus' picture cache uses (xgpu_texture_decode_rgba, port/linux/src/xbox_textures.c).

Compiles the file's texture-format section as it is (formats, geometry, swizzling, texel conversion, DXT) with a small
driver, and reads back synthetic Xbox textures: a swizzled A8R8G8B8, a linear A8R8G8B8 with a pitch wider than its
rows, and DXT1 blocks. The swizzled texels are laid out here by an independent reference of the Xbox's Morton order.
Palettized textures are refused (their palette is not in the cache's reach).
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys

try:
    import pytest
except ImportError:
    pytest = None
else:
    pytestmark = pytest.mark.skipif(sys.platform != "darwin" or not shutil.which("xcrun"),
                                    reason="needs the Apple toolchain (xcrun)")

ROOT = Path(__file__).resolve().parents[1]

DRIVER = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t DWORD, D3DCOLOR;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define VALIDATION_NOTE(...) ((void)0)
@DEFINES@
@DESCRIPTION@
@SLICE@
int main(int argc, char **argv)
{
	DWORD format_word = (DWORD)strtoul(argv[1], NULL, 0), size_word = (DWORD)strtoul(argv[2], NULL, 0);
	unsigned char texels[4096], *rgba;
	unsigned long width, height, index;
	size_t count = fread(texels, 1, sizeof(texels), stdin);

	(void)count;
	if (!xgpu_texture_decode_rgba(format_word, size_word, texels, &rgba, &width, &height))
	{
		puts("refused");
		return 0;
	}
	printf("%lux%lu", width, height);
	for (index = 0; index < width * height * 4; index++)
		printf(" %02x", rgba[index]);
	putchar('\n');
	free(rgba);
	return 0;
}
"""


def build(directory: Path) -> Path:
    source = (ROOT / "port/linux/src/xbox_textures.c").read_text()
    header = (ROOT / "port/linux/src/xgpu.h").read_text()
    d3d = (ROOT / "port/include/xdk/xdk_d3d8.h").read_text()
    defines = "\n".join(f"#define {m}" for m in re.findall(
        r"^#define ((?:D3DFORMAT|D3DSIZE|D3DTEXTURE)_\w+ \S+)$", d3d, re.M))
    description = header[header.index("struct xgpu_texture_description\n"):header.index("\nvoid xgpu_texture_describe")]
    slice_ = source[source.index("/* ---------- formats */"):source.index("/* ---------- upload */")]
    program = (DRIVER.replace("@DEFINES@", defines).replace("@DESCRIPTION@", description)
               .replace("@SLICE@", slice_))
    (directory / "decode.c").write_text(program)
    binary = directory / "decode"
    subprocess.run(["xcrun", "clang", "-std=gnu11", "-g", "-Wall", "-fsanitize=address,undefined",
                    "-Wno-unused-function", "-Wno-unused-variable", str(directory / "decode.c"),
                    f"--output={binary}"], check=True)
    return binary


def run(binary: Path, format_word: int, size_word: int, texels: bytes) -> str:
    return subprocess.run([str(binary), hex(format_word), hex(size_word)], input=texels, capture_output=True,
                          check=True).stdout.decode().strip()


def morton(width: int, height: int):
    """Texel (x, y) -> index, the Xbox's order: the bits of x and y alternate, x first, until a side runs out."""
    def spread(mask, value):
        result, bit = 0, 1
        while value and bit < 1 << 32:
            if mask & bit:
                if value & 1:
                    result |= bit
                value >>= 1
            bit <<= 1
        return result

    x_mask = y_mask = 0
    mask_bit, bit = 1, 1
    while bit < width or bit < height:
        if bit < width:
            x_mask |= mask_bit
            mask_bit <<= 1
        if bit < height:
            y_mask |= mask_bit
            mask_bit <<= 1
        bit <<= 1
    return lambda x, y: spread(x_mask, x) | spread(y_mask, y)


def format_word(fmt: int, width_log: int, height_log: int) -> int:
    # (formats, mipmap levels, and the sizes: xdk_d3d8.h) one level, a 2D texture
    return (fmt << 8) | (1 << 16) | (width_log << 20) | (height_log << 24) | (2 << 4)


def rgba_text(width, height, pixels):
    flat = " ".join(f"{r:02x} {g:02x} {b:02x} {a:02x}" for r, g, b, a in pixels)
    return f"{width}x{height} {flat}"


def test_swizzled_a8r8g8b8(tmp_path):
    binary = build(tmp_path)
    width, height = 8, 4
    pixels = [(x * 30, y * 60, (x ^ y) * 10, 255 - x) for y in range(height) for x in range(width)]
    texels = bytearray(width * height * 4)
    order = morton(width, height)
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            texels[order(x, y) * 4:order(x, y) * 4 + 4] = bytes((b, g, r, a))
    assert run(binary, format_word(0x06, 3, 2), 0, bytes(texels)) == rgba_text(width, height, pixels)


def test_linear_a8r8g8b8_with_a_wider_pitch(tmp_path):
    binary = build(tmp_path)
    width, height, pitch = 5, 2, 64
    pixels = [(10 * x, 20 * y, 7 * x + y, 200) for y in range(height) for x in range(width)]
    texels = bytearray(pitch * height)
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            texels[y * pitch + x * 4:y * pitch + x * 4 + 4] = bytes((b, g, r, a))
    size_word = (width - 1) | ((height - 1) << 12) | ((pitch // 64 - 1) << 24)
    assert run(binary, (0x12 << 8) | (1 << 16), size_word, bytes(texels)) == rgba_text(width, height, pixels)


def test_dxt1_blocks(tmp_path):
    binary = build(tmp_path)
    red, green = 0xF800, 0x07E0
    blocks = (red.to_bytes(2, "little") + (0x001F).to_bytes(2, "little") + bytes(4) +
              green.to_bytes(2, "little") + (0).to_bytes(2, "little") + bytes(4))
    pixels = [(255, 0, 0, 255) if x < 4 else (0, 255, 0, 255) for y in range(4) for x in range(8)]
    assert run(binary, format_word(0x0C, 3, 2), 0, blocks) == rgba_text(8, 4, pixels)


def test_palettized_textures_are_refused(tmp_path):
    binary = build(tmp_path)
    assert run(binary, format_word(0x0B, 2, 2), 0, bytes(16)) == "refused"


if __name__ == "__main__":
    import tempfile
    for test in (test_swizzled_a8r8g8b8, test_linear_a8r8g8b8_with_a_wider_pitch, test_dxt1_blocks,
                 test_palettized_textures_are_refused):
        with tempfile.TemporaryDirectory() as directory:
            test(Path(directory))
    print("menu art decode passed")
