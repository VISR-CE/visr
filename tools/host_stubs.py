#!/usr/bin/env python3
"""Generate and check port/ios/tests/host_stubs.c, the shared weak host stubs, and
port/ios/tests/port_stubs.c, the port's seams (SEAM_HEADERS) for probes that run on Linux too.

Native probes compile production .c/.m files (or bodies extracted from them) and
used to keep their own hand-written stubs for the host calls those files make.
When production code gained a call, every such probe stopped linking, one at a
time. This tool owns the single copy instead:

* Every exported (non-static, non-inline) function declared in a header under
  port/ios/host gets a weak default definition in host_stubs.c. The definition
  is copied from the header's own declaration, so the stub file includes the real
  headers and a signature change breaks its compile. The default does nothing
  and returns zero, except for the few entries in NEUTRAL below.
* A probe overrides a default by defining the function itself (strong symbols
  win over weak ones); the probe's own file never needs the default.
* tools/ios_test.py links the compiled stub object into every probe it builds.

Usage:
    tools/host_stubs.py --write   regenerate port/ios/tests/host_stubs.c and port_stubs.c
    tools/host_stubs.py --check   fail if a declared host function has no weak
                                  default, or if either file is stale
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probekit import source as probe_source  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'port/ios/host'
OUTPUT = ROOT / 'port/ios/tests/host_stubs.c'

# Headers that need UIKit (not present for the macOS probes) are not covered, nor gpu_side_imports.h, which
# holds the guest imports' definitions for host_gpu_dispatch.c and a probe to include, not declarations, nor
# host_gamepad_merge.h, which declares no host function (only static inline helpers over SDL's gamepads).
SKIP_HEADERS = {'host_orientation.h', 'host_upscale_tick.h', 'gpu_side_imports.h', 'host_gamepad_merge.h'}

# host_stubs.c in three parts, by what the header needs (measured by compiling each alone): Objective-C headers
# inside #ifdef __OBJC__, Apple C headers inside #ifdef __APPLE__, portable headers unguarded, so the file compiled
# as plain C on Linux holds the portable part, as does any build with -DHOST_STUBS_PORTABLE_ONLY
# (tools/test_host_stubs_portable.py). A header in none of the lists fails
# --check; a portable header that stops compiling as plain C fails that test.
OBJC_HEADERS = {
    'host_benchmark_catalog.h', 'host_benchmark_outbox.h', 'host_benchmark_presentation.h',
    'host_benchmark_snapshot.h', 'host_map_identity.h', 'host_perf_validation.h', 'host_qr.h',
    'host_report_delivery.h',
}
APPLE_C_HEADERS = {
    'host_frame_pacer.h', 'host_main_thread.h', 'host_quit_signals.h', 'host_spatial_input.h',
}
PORTABLE_HEADERS = {
    'frame_waits.h', 'gpu_call_hash.h', 'gpu_capture.h', 'gpu_owner_check.h', 'gpu_queue.h', 'gpu_stream_ring.h',
    'host.h', 'host_astc_store.h', 'host_audio_output.h', 'host_audio_session.h', 'host_benchmark.h',
    'host_cloud_saves.h', 'host_config.h', 'host_controller_age.h', 'host_crash.h', 'host_data_updates.h',
    'host_external.h', 'host_external_mode.h', 'host_frame_counter_stats.h', 'host_frame_timeline.h',
    'host_game_thread.h', 'host_geometry.h', 'host_guest_environment.h', 'host_hinge.h', 'host_join_link.h',
    'host_lan.h', 'host_log_file.h', 'host_log_send.h', 'host_low_power.h', 'host_main_sync_log.h', 'host_natpmp.h',
    'host_open_website.h', 'host_perf_hooks.h', 'host_perf_metal.h', 'host_perf_reports.h', 'host_posture.h',
    'host_posture_layout.h', 'host_profile.h', 'host_recording_pack.h', 'host_save_writer.h', 'host_shader_pack.h',
    'host_shader_upload.h', 'host_socket_revival.h', 'host_spatial_gamepad.h', 'host_spatial_pose.h',
    'host_spatial_record.h', 'host_startup.h', 'host_stereo.h', 'host_stereo_head.h', 'host_stereo_hud.h',
    'host_stereo_posture.h', 'host_texture_repair.h', 'host_texture_settings.h', 'host_texture_upscale.h',
    'host_theater.h', 'host_thread_cpu.h', 'host_window_stats.h', 'ios_host.h', 'menu_background_redraw.h',
    'menu_difficulty.h', 'menu_picture_redraw.h', 'menu_picture_shapes.h', 'menu_picture_trace.h', 'metal_gate.h',
    'texture_refine.h', 'texture_upscale_encode.h', 'texture_upscale_generate.h', 'texture_upscale_model.h',
    'texture_upscale_pixels.h',
}
PART_GUARDS = {'objc': '__OBJC__', 'apple': '__APPLE__', 'portable': None}


def part_of(header: str):
    """'objc', 'apple' or 'portable' for a listed header (a fixture's unlisted header renders as portable)."""
    if header in OBJC_HEADERS:
        return 'objc'
    if header in APPLE_C_HEADERS:
        return 'apple'
    return 'portable'


def unlisted_headers(found) -> list[str]:
    listed = OBJC_HEADERS | APPLE_C_HEADERS | PORTABLE_HEADERS
    return sorted({entry[0] for entry in found.values()} - listed)


# Defaults that must do more than return zero. Keyed by function name; the body
# may use the declared parameter names.
NEUTRAL = {
    'host_config_string': 'if (out && size) { snprintf(out, size, "%s", fallback ? fallback : ""); }',
    'host_config_real': 'return fallback;',
}

# Non-void functions whose zero default is accepted. Any other non-void default prints its name and
# aborts when a probe executes it, so a seam that should have been faked fails loudly instead of letting
# the probe pass on a made-up zero. Add a name here only when zero is a sound answer for every probe.
ZERO_OK = {
    'gpu_queue_eye_span_begin', 'gpu_queue_eye_span_end', 'host_stereo_replay_begin', 'host_stereo_replay_end',
    'host_benchmark_running', 'host_device_profile_pending', 'host_external_active', 'host_perf_metal_dropped',
    'host_stereo_entered', 'host_window_up', 'host_external_size', 'host_display_supports_portrait',
    'host_benchmark_presented', 'host_texture_upscale_first_run_phase', 'host_texture_upscale_enabled',
    'host_texture_upscale_busy', 'texture_refine_cancelled', 'host_config_profile_installed',
    'host_perf_metal_timing_supported', 'host_on_main_thread', 'host_game_thread_enabled',
    'host_development_hooks', 'host_gpu_encode_queue_on', 'host_frame_timeline_frame_id', 'host_frame_timeline_logging', 'host_perf_hooks_dynamic_enabled', 'host_perf_hooks_frame_id',
}

HEADER_NOTE = """\
/* GENERATED by tools/host_stubs.py --write: do not edit by hand.
 *
 * Weak default definitions of every exported host function declared in
 * port/ios/host headers, linked into each native probe by tools/ios_test.py. A
 * probe overrides one by defining it strongly. A new host function appears here
 * the next time tools/host_stubs.py --write runs; --check fails until it has.
 * Defaults do nothing and return zero unless tools/host_stubs.py says otherwise.
 */
"""

MARK = '__attribute__((weak)) '


def strip_comments(text: str) -> str:
    """The text with its comments blanked and its strings kept (probekit.source's one comment splitter)."""
    return probe_source.mask(text, strings=False)


def directive_condition(directive: str, rest: str) -> str:
    rest = rest.strip()
    if directive == 'ifdef':
        return f'defined({rest})'
    if directive == 'ifndef':
        return f'!defined({rest})'
    return f'({rest})'


def statements(text: str):
    """Yield (conditions, statement) for each top-level statement outside bodies."""
    stack: list[dict] = []
    skip_guard = True
    buffer: list[str] = []
    snapshot: list[str] = []
    brace = paren = 0
    body_after_paren = False
    text, extern_open = re.subn(r'extern\s+"C"\s*\{', '', text)
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        match = re.match(r'\s*#\s*(\w+)\s*(.*)$', line)
        if match and brace == 0 and paren == 0 and not buffer:
            directive, rest = match.groups()
            while rest.endswith('\\') and i < len(lines):
                rest = rest[:-1] + lines[i]
                i += 1
            if directive in ('if', 'ifdef', 'ifndef'):
                condition = directive_condition(directive, rest)
                if skip_guard and directive == 'ifndef' and re.fullmatch(r'\w+_H_?', rest.strip()):
                    stack.append({'guard': True})
                else:
                    stack.append({'guard': False, 'prior': [], 'cur': condition})
                skip_guard = False
            elif directive == 'else' and stack and not stack[-1]['guard']:
                entry = stack[-1]
                entry['prior'].append(entry['cur'])
                entry['cur'] = ' && '.join(f'!({c})' for c in entry['prior'])
            elif directive == 'elif' and stack and not stack[-1]['guard']:
                entry = stack[-1]
                entry['prior'].append(entry['cur'])
                entry['cur'] = ' && '.join([f'!({c})' for c in entry['prior']] + [directive_condition('if', rest)])
            elif directive == 'endif' and stack:
                stack.pop()
            elif directive == 'define':
                while rest.endswith('\\') and i < len(lines):
                    rest = rest[:-1] + lines[i]
                    i += 1
            continue
        skip_guard = False if line.strip() else skip_guard
        # (a string or character literal's text is not code: an embedded shader's semicolons, parentheses and
        # braces never end or open a statement)
        quote, escaped = None, False
        for ch in line:
            buffer.append(ch)
            if not snapshot:
                snapshot = [e['cur'] for e in stack if not e['guard']]
            if quote:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == quote:
                    quote = None
                continue
            if ch in '"\'':
                quote = ch
            elif ch == '(':
                paren += 1
            elif ch == ')':
                paren -= 1
            elif ch == '{' and paren == 0:
                if brace == 0:
                    body_after_paren = bool(re.search(r'\)\s*(__attribute__\s*\(\(.*\)\))?\s*\{$', ''.join(buffer)))
                brace += 1
            elif ch == '}' and paren == 0 and brace == 0 and extern_open > 0:
                extern_open -= 1
                buffer.pop()
            elif ch == '}' and paren == 0:
                brace -= 1
                if brace == 0 and body_after_paren:
                    buffer, snapshot, body_after_paren = [], [], False
            elif ch == ';' and brace == 0 and paren == 0:
                yield snapshot, ''.join(buffer)
                buffer, snapshot = [], []
        buffer.append('\n')
        if not ''.join(buffer).strip():
            buffer, snapshot = [], []
    if brace or paren or extern_open:
        raise ValueError(f'unbalanced or unfinished header text (brace {brace}, paren {paren}, '
                         f'extern "C" {extern_open})')


def split_top(text: str) -> list[str]:
    parts, depth, current = [], 0, ''
    for ch in text:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(current)
            current = ''
        else:
            current += ch
    parts.append(current)
    return [p.strip() for p in parts]


BASIC = {'unsigned', 'signed', 'int', 'long', 'short', 'char', 'float', 'double', 'const', 'volatile',
         'void', 'struct', 'enum', 'union', '_Bool'}


def name_parameter(param: str, index: int) -> tuple[str, bool]:
    """Return the parameter text with a name, and whether it already had one."""
    if param in ('', 'void', '...'):
        return param, True
    if '(*' in param or '(^' in param:
        if re.search(r'\(\s*[*^]\s*\)', param):
            return re.sub(r'\(\s*([*^])\s*\)', rf'(\1_p{index})', param, count=1), False
        return param, True
    bare = re.sub(r'\[[^\]]*\]', '', param).strip()
    tokens = re.findall(r'\w+|\*', bare)
    last = tokens[-1]
    named = (last != '*' and last not in BASIC and len(tokens) > 1
             and not (tokens[-2] in ('struct', 'enum', 'union')))
    if len(tokens) == 2 and tokens[0] in ('const', 'volatile') and last not in BASIC:
        named = False
    if named:
        return param, True
    return f'{param}{"" if param.endswith("*") else " "}_p{index}', False


def declaration(statement: str):
    """Return (name, returns_void, noreturn, definition_head) or None."""
    s = ' '.join(statement.split())
    if not s.endswith(';'):
        return None
    s = s[:-1].strip()
    if re.search(r'\b(typedef|static|inline|__inline)\b|=|\{|^#', s):
        return None
    if re.match(r'^(struct|enum|union)\s+\w+$', s):
        return None
    noreturn = bool(re.search(r'noreturn', s))
    s = re.sub(r'^\s*extern\s+', '', s)
    attrs = re.search(r'\s*(__attribute__\s*\(\(.*\)\))\s*$', s)
    tail = ''
    if attrs:
        tail = attrs.group(1)
        s = s[:attrs.start()]
    match = re.match(r'^(?P<ret>.+?[\s\*])(?P<name>\w+)\s*\((?P<params>.*)\)$', s)
    if not match:
        return None
    ret, name, params = match.group('ret').strip(), match.group('name'), match.group('params')
    if '(' in ret or re.search(r'\b(if|while|return|sizeof)\b', ret):
        return None
    # a leading attribute or macro belongs to the return type text as written
    named = []
    for index, param in enumerate(split_top(params)):
        text, _ = name_parameter(param, index)
        named.append(text)
    head = f'{ret} {name}({", ".join(named)})'
    return name, ret == 'void', noreturn, head


def collect(host: Path = HOST):
    found = {}
    for header in sorted(host.glob('*.h')):
        if header.name in SKIP_HEADERS:
            continue
        text = strip_comments(header.read_text())
        for conditions, statement in statements(text):
            parsed = declaration(statement)
            if not parsed:
                continue
            name, is_void, noreturn, head = parsed
            if name in found:
                continue
            found[name] = (header.name, conditions, is_void, noreturn, head)
    return found


def render(found) -> str:
    out = [HEADER_NOTE, '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n',
           '#if defined(__clang__)\n#pragma clang diagnostic ignored "-Wunused-parameter"\n'
           '#pragma clang diagnostic ignored "-Wmissing-prototypes"\n#endif\n']
    for part, guard in PART_GUARDS.items():
        names = [name for name in sorted(found) if part_of(found[name][0]) == part]
        if not names:
            continue
        out.append(f'/* {part} headers */' if guard is None else
                   f'#if defined({guard}) && !defined(HOST_STUBS_PORTABLE_ONLY)  /* {part} headers */')
        out += [f'#include "{header}"' for header in sorted({found[name][0] for name in names})]
        out.append('')
        for name in names:
            header, conditions, is_void, noreturn, head = found[name]
            body = default_body(name, head, is_void, noreturn, NEUTRAL, ZERO_OK, 'host_stubs')
            for condition in conditions:
                out.append(f'#if {condition}')
            out.append(f'{MARK}{head}')
            out.append('{' + (f' {body} ' if body else ' ') + '}')
            for _ in conditions:
                out.append('#endif')
        if guard is not None:
            out.append(f'#endif  /* {guard} */')
        out.append('')
    return '\n'.join(out).rstrip('\n') + '\n'


def missing_defaults(found, stub_text: str) -> list[str]:
    return sorted(set(found) - defined_names(stub_text))


def defined_names(text: str) -> set[str]:
    return set(re.findall(r'__attribute__\(\(weak\)\) [^\n]*?\b(\w+)\(', text))


# The port's own seams for probes that run on Linux as well (port/ios/tests/port_stubs.c, plain C). Each entry is a
# header and whether the stub file can include it: an includable header compiles with only the probes' include paths
# (no SDL, no Xbox SDK), so the stubs include it and a signature change breaks their compile; for a header that
# cannot be included, `functions` names what to cover and the generator copies each declaration from the header's
# text, so --check fails once the header's declaration no longer matches the copy.
SEAM_HEADERS = [
    {'header': 'port/linux/src/port_config.h', 'includable': True},
    {'header': 'port/linux/src/platform.h', 'includable': False, 'functions': ('platform_log',)},
]
PORT_OUTPUT = ROOT / 'port/ios/tests/port_stubs.c'
PORT_INCLUDES = ['-Iport/linux/src']

# As NEUTRAL and ZERO_OK, for the port seams: an unset setting reads as off, zero or empty, as most probes that
# defined their own did; any other non-void default aborts with its name.
PORT_NEUTRAL = {
    'config_string': 'return "";',
}
PORT_ZERO_OK = {'config_boolean', 'config_integer', 'config_real', 'config_reload_boolean'}

PORT_HEADER_NOTE = """\
/* GENERATED by tools/host_stubs.py --write: do not edit by hand.
 *
 * Weak default definitions of the port's seams for probes that also run on
 * Linux (tools/host_stubs.py SEAM_HEADERS): every function port_config.h
 * declares, and the named functions of headers a plain C probe cannot include,
 * whose declarations are copied here. Plain C (-std=gnu11), linked by
 * tools/probekit/cc.py's build(port_stubs=True) and with build(host_stubs=True).
 * A probe overrides one by defining it strongly. --check fails until --write
 * runs after a seam header gains a function or changes a declaration.
 */
"""


def collect_seams(root: Path = ROOT, seams=None):
    """name -> (header, conditions, is_void, noreturn, head, copied declaration or None), and the names a seam entry
    lists that its header no longer declares."""
    found, gone = {}, []
    for seam in SEAM_HEADERS if seams is None else seams:
        text = strip_comments((root / seam['header']).read_text(encoding='latin-1'))
        wanted = seam.get('functions')
        seen = set()
        for conditions, statement in statements(text):
            parsed = declaration(statement)
            if not parsed or parsed[0] in found or (wanted is not None and parsed[0] not in wanted):
                continue
            name, is_void, noreturn, head = parsed
            copy = None if seam['includable'] else ' '.join(statement.split())
            found[name] = (seam['header'], conditions, is_void, noreturn, head, copy)
            seen.add(name)
        gone += [f"{name} ({seam['header']})" for name in wanted or () if name not in seen]
    return found, gone


def default_body(name, head, is_void, noreturn, neutral, zero_ok, label):
    body = neutral.get(name)
    if body is not None:
        return body
    if noreturn:
        return 'abort();'
    if not is_void and name not in zero_ok:
        return f'fprintf(stderr, "{label}: probe called {name} without defining it\\n"); abort();'
    if is_void:
        return ''
    ret = head[: head.index(name + '(')].strip()
    return f'{ret} _stub_v; memset(&_stub_v, 0, sizeof _stub_v); return _stub_v;'


def render_seams(found) -> str:
    out = [PORT_HEADER_NOTE, '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n',
           '#if defined(__clang__)\n#pragma clang diagnostic ignored "-Wunused-parameter"\n'
           '#pragma clang diagnostic ignored "-Wmissing-prototypes"\n#endif\n']
    for header in sorted({entry[0] for entry in found.values() if entry[5] is None}):
        out.append(f'#include "{Path(header).name}"')
    copied = [name for name in sorted(found) if found[name][5] is not None]
    if copied:
        out.append('')
        out.append('/* copied from headers a plain C probe cannot include */')
        out += [found[name][5] + f'  /* {found[name][0]} */' for name in copied]
    out.append('')
    for name in sorted(found):
        header, conditions, is_void, noreturn, head, _ = found[name]
        body = default_body(name, head, is_void, noreturn, PORT_NEUTRAL, PORT_ZERO_OK, 'port_stubs')
        for condition in conditions:
            out.append(f'#if {condition}')
        out.append(f'{MARK}{head}')
        out.append('{' + (f' {body} ' if body else ' ') + '}')
        for _ in conditions:
            out.append('#endif')
    return '\n'.join(out) + '\n'


def port_stub_flags(options) -> list[str]:
    """The options the port stub object has to match: -D, -std=, -m32 and the target, in the probe's order."""
    options, kept, index = [str(option) for option in options], [], 0
    while index < len(options):
        option = options[index]
        if option.startswith(('-D', '-std=', '--target=', '-target=')) or option == '-m32':
            kept.append(option)
        elif option in ('-target', '-arch', '-isysroot') and index + 1 < len(options):
            kept += [option, options[index + 1]]
            index += 1
        index += 1
    return kept


_port_objects: dict = {}


def port_stub_object(build_dir: Path, compiler, compile_flags=()) -> Path:
    """Compile port_stubs.c as plain C with the probe's own compiler, once per compiler and flag set."""
    import hashlib
    import subprocess
    key = (tuple(compiler), tuple(compile_flags))
    if key not in _port_objects:
        output = Path(build_dir) / f'port-stubs-{hashlib.sha1(repr(key).encode()).hexdigest()[:10]}.o'
        subprocess.run([*compiler, '-c', '-x', 'c', '-std=gnu11', '-Wall', '-Wextra', '-Werror', *PORT_INCLUDES,
                        *compile_flags, str(PORT_OUTPUT), '-o', str(output)], cwd=ROOT, check=True)
        _port_objects[key] = output
    return _port_objects[key]


INCLUDES = ['-Iport/ios/host', '-Iport/runtime', '-Iport/linux/src', '-Iport/runtime/include', '-Iport/ios']
_objects: dict = {}


def stub_object(build_dir: Path, compile_flags=()) -> Path:
    """Compile the stubs once per define/std set and return the object to link into a probe.

    Any driver that builds a probe against host code calls this and appends the result to its link
    command (after a '-x none' if it passed -x). compile_flags may carry the probe's -D, -std= and
    target flags so the object matches the probe."""
    import hashlib
    import subprocess
    flags = tuple(compile_flags)
    key = hashlib.sha1(repr(flags).encode()).hexdigest()[:10]
    output = build_dir / f'host-stubs-{key}.o'
    if flags not in _objects:
        subprocess.run(['xcrun', 'clang', '-c', '-x', 'objective-c', '-fno-objc-arc', '-fblocks', '-Wall',
                        '-Wextra', '-Werror', *INCLUDES, *flags, str(OUTPUT), '-o', str(output)],
                       cwd=ROOT, check=True)
        _objects[flags] = output
    return _objects[flags]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    found = collect()
    expected = render(found)
    unlisted = unlisted_headers(found)
    if unlisted:
        print('host headers in none of OBJC_HEADERS, APPLE_C_HEADERS and PORTABLE_HEADERS (tools/host_stubs.py); '
              'add each to the list of what it needs to compile:')
        for header in unlisted:
            print(f'  {header}')
        return 1
    seams, gone = collect_seams()
    port_expected = render_seams(seams)
    if gone:
        print('SEAM_HEADERS in tools/host_stubs.py names functions their headers no longer declare:')
        for name in gone:
            print(f'  {name}')
        return 1
    if args.write:
        OUTPUT.write_text(expected)
        PORT_OUTPUT.write_text(port_expected)
        print(f'wrote {OUTPUT.relative_to(ROOT)}: {len(found)} weak defaults; '
              f'{PORT_OUTPUT.relative_to(ROOT)}: {len(seams)}')
        return 0
    current = OUTPUT.read_text() if OUTPUT.exists() else ''
    missing = missing_defaults(found, current)
    if missing:
        print('host functions declared in port/ios/host/*.h without a weak default in '
              'port/ios/tests/host_stubs.c (run tools/host_stubs.py --write):')
        for name in missing:
            print(f'  {name}  ({found[name][0]})')
        return 1
    if current != expected:
        print('port/ios/tests/host_stubs.c is stale: run tools/host_stubs.py --write')
        return 1
    port_current = PORT_OUTPUT.read_text() if PORT_OUTPUT.exists() else ''
    if port_current != port_expected:
        missing = missing_defaults(seams, port_current)
        detail = f" (no default for {', '.join(missing)})" if missing else ' (a seam declaration changed)'
        print(f'port/ios/tests/port_stubs.c is stale{detail}: run tools/host_stubs.py --write')
        return 1
    print(f'host stubs ok: {len(found)} weak defaults; port stubs ok: {len(seams)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
