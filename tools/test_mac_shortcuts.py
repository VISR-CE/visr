#!/usr/bin/env python3
"""The Mac app's development-only keys are compiled out of release builds, and the menu bar owns the chords."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def text(path):
    return (ROOT / path).read_text()


def main():
    host = text('port/ios/host/host_sdl.c')
    # F8 (the frame pacer's late start) exists only with the development hooks
    handler = re.search(r'(#if[^\n]*\n)(?:[^\n]*\n){0,4}?[^\n]*SDL_SCANCODE_F8', host)
    assert handler and 'defined(HALO_DEVELOPMENT_HOOKS)' in handler.group(1), 'F8 must sit under HALO_DEVELOPMENT_HOOKS'
    # F9 (display.late_look): the Apple guest asks the host whether it is a development build
    guest = text('port/linux/src/sdl_platform.c')
    f9 = guest[guest.index('SDL_SCANCODE_F9') - 200:guest.index('halo_late_look_toggle();')]
    assert 'HALO_ILP32' in f9 and 'host_development_hooks()' in f9, 'F9 must ask host_development_hooks() in the Apple guest'
    # the host answers 1 only when it is built with the hooks
    benchmark = text('port/ios/host/host_benchmark.m')
    body = benchmark[benchmark.index('int host_development_hooks(void)'):]
    assert re.match(r'int host_development_hooks\(void\)\s*\{\s*#ifdef HALO_DEVELOPMENT_HOOKS\s*return 1;\s*#else\s*return 0;', body)
    # app store packages are built without the hooks (tools/ios_build.py development_hooks)
    assert 'HALO_DEVELOPMENT_HOOKS={"OFF" if getattr(args, "app_store_package", None) else "ON"}' in text('tools/ios_build.py')
    # the menu bar carries each chord the host holds back from the game
    menu = text('port/macos/host_macos_menu.m')
    for title, key, mask in (('Release Mouse', 'm', 'control_command'), ('Back', '[', 'command'),
                             ('Enter Full Screen', 'f', 'control_command')):
        assert re.search(r'@"%s", @selector\(\w+:\), @"%s", %s' % (re.escape(title), re.escape(key), mask), menu), title
    print('Mac shortcut gating and menu items passed')


if __name__ == '__main__':
    main()
