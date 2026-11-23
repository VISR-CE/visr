"""The present pass's dither (display.scene_precision, gpu_metal.m's present_dither_fragment) and its noise.

Bug class: a dither that shifts brightness or color, lifts black or dims white, or tiles a pattern that is not
uniform. The shader's arithmetic is evaluated here over every texel of the committed noise for a range of source
values and output depths; the expression it evaluates is the one in the shader's source text.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'port/ios/host/blue_noise.h'
METAL = ROOT / 'port/ios/host/gpu_metal.m'


def masks():
    text = HEADER.read_text()
    body = text[text.index('{', text.index('blue_noise_rg')) + 1:text.rindex('}')]
    values = [int(value) for value in re.findall(r'\d+', body)]
    assert len(values) == 64 * 64 * 2
    return values[0::2], values[1::2]


def dithered(color, red, green, step):
    """present_dither_fragment for one channel: noise one output level wide each side, faded within a level of the
    ends, then the drawable's rounding"""
    first = (red * 255.0 + 0.5) / 256.0
    second = (green * 255.0 + 0.5) / 256.0
    room = min(max(min(color, 1.0 - color) / step, 0.0), 1.0)
    out = color + (first + second - 1.0) * step * room
    return min(max(round(out / step), 0), round(1.0 / step)) * step


def test_noise_is_two_uniform_rank_masks():
    for mask in masks():
        counts = [0] * 256
        for value in mask:
            counts[value] += 1
        assert counts == [16] * 256


def test_the_shader_evaluates_this_expression():
    source = METAL.read_text()
    shader = source[source.index('present_dither_fragment'):source.index('copy_fragment')]
    assert '(noise.read(uint2(in.position.xy) % size).rg * 255.0 + 0.5) / 256.0' in shader
    assert 'clamp(min(color, 1.0 - color) / dither.x, 0.0, 1.0)' in shader
    assert '(masks.x + masks.y - 1.0) * dither.x * room' in shader


def test_dither_keeps_the_mean_and_the_ends():
    red, green = masks()
    reds = [value / 255.0 for value in red]
    greens = [value / 255.0 for value in green]
    for levels in (255.0, 510.0, 1023.0):
        step = 1.0 / levels
        # black and white stay exactly black and white at every texel
        for end in (0.0, 1.0):
            assert all(dithered(end, r, g, step) == end for r, g in zip(reds, greens))
        # between levels: the mean over the pattern is the source within a tenth of a level
        for index in range(1, 40):
            color = index * 0.37 * step + 0.002
            mean = sum(dithered(color, r, g, step) for r, g in zip(reds, greens)) / len(reds)
            assert abs(mean - color) < 0.1 * step, (levels, color, mean)
