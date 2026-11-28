"""The present pass's dither (display.scene_precision, gpu_metal.m's present_dither_fragment) and its noise.

Bug class: a dither that shifts brightness or color, lifts black or dims white, or tiles a pattern that is not
uniform. The shader's arithmetic is evaluated here over every texel of the committed noise for a range of source
values and output depths; the expression it evaluates is the one in the shader's source text.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from probekit import source as probe_source  # noqa: E402
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


def test_the_pass_and_its_log_share_one_dither_decision():
    """Bug class: a second copy of the present pass's dither decision that drifts from what the frame does (the
    first layer-format line said "on" for a CRT frame and "not needed" for a dithered one, Nov 25 review). The
    comparison of the picture's levels with the drawable's lives only in present_dither_state; the pass picks its
    pipeline from it and present_state_log names its result."""
    source = METAL.read_text()
    helper = probe_source.function_in(source, 'present_dither_state')
    outside = source.replace(helper, '')
    assert re.search(r'format_levels\(picture\) > format_levels\(drawable\)', helper)
    assert not re.search(r'format_levels\([^)]*\)\s*>\s*format_levels\(', outside)
    frame = probe_source.function_in(source, 'present_frame')
    assert 'present_dither_state(picture.pixelFormat, target.pixelFormat' in frame
    assert 'present_state_log(' in frame
    assert 'dither_state == PRESENT_DITHER_CRT' in frame
    assert 'dither_state == PRESENT_DITHER_ON || dither_state == PRESENT_DITHER_RAMP' in frame


def test_the_state_line_follows_every_change_of_the_tuple():
    """Bug class: judged state logged only on change from a starting value, or only once, so its value in force is
    silent (the grain rehearsal could not show an 8-bit layer). present_state_log compares the whole tuple
    with the last one stated, with no one-shot flag, and names each dither state."""
    source = METAL.read_text()
    log = probe_source.function_in(source, 'present_state_log')
    for name in ('output_layer', 'perf_output', 'drawable', 'picture', 'dither'):
        assert re.search(rf'{name} == last_\w+', log), name
    assert 'stated' not in log
    names = probe_source.function_in(source, 'present_dither_state_name')
    for word in ('"on"', '"not needed"', '"off"', '"replaced by the precision ramp"', '"replaced by the CRT effect"'):
        assert word in names
