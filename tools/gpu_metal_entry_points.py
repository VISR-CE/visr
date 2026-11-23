#!/usr/bin/env python3
"""Every symbol a built gpu_metal.o exports is one the host may call by name: a row of
port/ios/host/gpu_operation.h's GPU_METAL_DIRECT_ENTRY_POINTS, with its class. The backend's operations are static and
reached through its table (gpu_backend_metal), so they reach the backend in order with the draws (through the encode
queue when it is on); one exported by name is a way around it.

The list is read by compiling a small C program that expands the header's macros, not by reading its text.
Run after every build by tools/ios_build.py (iOS, tvOS, visionOS: each sees its own conditional exports) and
tools/macos_build.py; by hand: tools/gpu_metal_entry_points.py PATH/gpu_metal.o"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# the table itself, and what the Objective-C runtime and blocks export
ALLOWED = {'gpu_backend_metal'}
RUNTIME_PREFIXES = ('OBJC_', '_OBJC_', '__block_descriptor', '__copy_helper_block', '__destroy_helper_block')

PROGRAM = r'''
#include "gpu_operation.h"
#include <stdio.h>
int main(void)
{
#define DIRECT(name, class) printf("direct %s %s\n", #name, #class);
	GPU_METAL_DIRECT_ENTRY_POINTS(DIRECT)
#define OPERATION_F(type, name, parameters, arguments) printf("operation gpu_metal_%s\n", #name);
#define OPERATION_P(name, parameters, arguments) printf("operation gpu_metal_%s\n", #name);
	GPU_BACKEND_FUNCTIONS(OPERATION_F, OPERATION_P)
	return 0;
}
'''


def tables():
    """(direct entry points by name with their class, the backend operations' gpu_metal_ names)"""
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        raise SystemExit('gpu_metal_entry_points: a C compiler is required')
    with tempfile.TemporaryDirectory(prefix='gpu-metal-entry-points-') as temp:
        source = Path(temp) / 'tables.c'
        executable = Path(temp) / 'tables'
        source.write_text(PROGRAM)
        subprocess.run([compiler, '-std=c11', '-I', str(ROOT / 'port/ios/host'), '-I', str(ROOT / 'port/linux/src'),
                        str(source), '-o', str(executable)], check=True)
        lines = subprocess.run([str(executable)], check=True, capture_output=True, text=True).stdout.splitlines()
    direct, operations = {}, set()
    for line in lines:
        words = line.split()
        if words[0] == 'direct':
            direct[words[1]] = words[2]
        else:
            operations.add(words[1])
    return direct, operations


def exported(nm_output):
    """the defined, exported symbols in `nm --extern-only --defined-only` output, without the leading underscore;
    data symbols count too (an exported global is a way around the table as well)"""
    names = set()
    for line in nm_output.splitlines():
        words = line.split()
        if not words:
            continue
        name = words[-1]
        names.add(name[1:] if name.startswith('_') else name)
    return names


def problems(names, direct, operations):
    found = []
    for name in sorted(names):
        if name in ALLOWED or name in direct or name.startswith(RUNTIME_PREFIXES):
            continue
        if name in operations:
            found.append(f'{name} is a backend operation exported by name: make it static, so the host reaches it '
                         'through the table')
        else:
            found.append(f'{name} is an exported symbol without a class: add it to GPU_METAL_DIRECT_ENTRY_POINTS '
                         '(gpu_operation.h) with why the host may call it by name, or make it an operation')
    return found


def check(object_path):
    output = subprocess.run(['nm', '--extern-only', '--defined-only', str(object_path)], check=True, capture_output=True, text=True).stdout
    direct, operations = tables()
    found = problems(exported(output), direct, operations)
    if found:
        raise SystemExit('gpu_metal_entry_points: ' + str(object_path) + '\n  ' + '\n  '.join(found))
    print(f'gpu_metal_entry_points: {object_path}: every exported symbol is classed')


def find_object(build, configuration):
    """the newest gpu_metal.o under a build folder built in configuration (Xcode's folder for it: Release, or
    Release-iphoneos and the like), or None; another configuration's object, left from an earlier build, is not it"""
    objects = sorted((path for path in Path(build).rglob('gpu_metal.o') if configuration in path.parts),
                     key=lambda path: path.stat().st_mtime)
    return objects[-1] if objects else None


def check_build(build, configuration):
    """check the configuration's gpu_metal.o just built under build (tools/ios_build.py, tools/macos_build.py): each
    platform's build sees its own conditional exports"""
    found = find_object(build, configuration)
    if not found:
        raise SystemExit(f'no {configuration} gpu_metal.o under {build}: the entry point check has nothing to read')
    check(found)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: gpu_metal_entry_points.py PATH/gpu_metal.o')
    check(Path(sys.argv[1]))
