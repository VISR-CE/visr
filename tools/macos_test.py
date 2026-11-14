#!/usr/bin/env python3
"""Run asset-free native AppKit probes without launching the application."""
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/macos/probe'


def run(*arguments):
    print('+ ' + shlex.join(str(argument) for argument in arguments), flush=True)
    subprocess.run([str(argument) for argument in arguments], cwd=ROOT, check=True)


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    sdl_flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'sdl3'], text=True))
    common = ['xcrun', '--sdk', 'macosx', 'clang', '-O2', '-mmacosx-version-min=14.0',
              '-DHALO_IOS=1', '-Iport/macos', '-Iport/ios/host', '-Iport/runtime',
              '-Iport/runtime/include', '-Wl,-dead_strip']
    objc = ['-fobjc-arc', '-fblocks', '-framework', 'AppKit']
    # Keep the startup probe asset-free; use the production thread, timer, and exit bodies.
    source = (ROOT / 'port/ios/host/host_main.m').read_text()
    crash = (ROOT / 'port/ios/host/host_crash.c').read_text()
    def definition(text, signature):
        start = text.index(signature)
        opening = text.index('{', start)
        depth = 1
        end = opening + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        return text[start:end]
    def declaration(name):
        return next(line for line in source.splitlines()
                    if line.startswith('static ') and line.endswith(f' {name};'))
    entered = source.index('    static int entered;')
    entered_end = source.index('    #endif\n    @autoreleasepool', entered)
    (BUILD / 'extra_scene.inc').write_text('\n'.join([
        declaration('extra_scene_pending'),
        declaration('extra_scene_watch'),
        # The restore hook arrived with later theater work; older trees lack it.
        *([definition(source, 'void host_extra_scene_restore_watch(void)'),
           '#define HAVE_EXTRA_SCENE_RESTORE_WATCH 1']
          if 'void host_extra_scene_restore_watch(void)' in source else []),
        definition(source, 'void host_extra_scene_poll(void)'),
        'static int scene_start(void) {\n' + source[entered:entered_end] + '\nreturn 0;\n}',
    ]))
    run(*common, '-Wall', '-Wextra', '-Werror', *objc, f'-I{BUILD}',
        'port/ios/tests/extra_scene_probe.m', 'port/ios/host/host_main_thread.c',
        '-framework', 'CoreFoundation', '-o', BUILD / 'extra-scene-probe')
    run(BUILD / 'extra-scene-probe')
    (BUILD / 'game_thread_start.inc').write_text('\n'.join([
        definition(crash, 'int host_crash_thread_install(void)'),
        definition(source, 'void host_exit(int code)'),
        definition(source, 'static void quit_signal(int number)'),
        source[source.index('struct game_thread_start {'):source.index('#endif /* supported game thread startup */')],
    ]))
    for native, tv, name in ((1, 0, 'game-thread-start-probe'),
                            (0, 0, 'ios-game-thread-start-probe'),
                            (0, 1, 'tvos-game-thread-start-probe')):
        run(*common, '-Wall', '-Wextra', '-Werror', f'-DHALO_NATIVE_MAC={native}', f'-DGAME_THREAD_PROBE_TV={tv}',
            *objc, f'-I{BUILD}', 'port/ios/tests/game_thread_start_probe.m',
            'port/ios/host/host_main_thread.c', 'port/ios/host/host_quit_signals.c',
            '-framework', 'CoreFoundation', '-o', BUILD / name)
        run(BUILD / name)
    # These include the real implementation and replace only its SDL/file boundaries.
    run(*common, '-DHALO_NATIVE_MAC=1', *objc, 'tools/macos_import_test.m', 'port/ios/host/host_main_thread.c',
        '-framework', 'UniformTypeIdentifiers', '-o', BUILD / 'import-probe')
    run(BUILD / 'import-probe')
    run(*common, '-DHALO_NATIVE_MAC=1', *objc, 'tools/macos_services_test.m',
        'port/ios/host/host_frame_counter_stats.c', 'port/ios/host/host_main_thread.c',
        'port/ios/host/host_startup.c', 'port/ios/host/host_startup_platform.m', '-framework', 'QuartzCore',
        '-framework', 'CoreGraphics', *sdl_flags, '-o', BUILD / 'services-probe')
    run(BUILD / 'services-probe')
    run('python3', 'tools/test_menu_display_rate.py')
    run(*common, 'tools/macos_shortcuts_test.c', *sdl_flags, '-o', BUILD / 'shortcuts-probe')
    run(BUILD / 'shortcuts-probe')
    run('python3', 'tools/test_mac_shortcuts.py')
    # Verify both the native overrides and the shared host's original mobile flags.
    for native, name in ((1, 'window-native-probe'), (0, 'window-mobile-probe')):
        run(*common, f'-DHALO_NATIVE_MAC={native}', 'tools/macos_window_test.c',
            'port/ios/host/host_config.c', 'port/ios/host/host_main_thread.c', *sdl_flags, '-o', BUILD / name)
        run(BUILD / name)


if __name__ == '__main__':
    main()
