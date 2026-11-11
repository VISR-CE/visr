#!/usr/bin/env python3
"""Validate, merge, and import numeric map camera framings."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
import sys

KEY = re.compile(r"(?:stock:[a-z0-9_]+|blake2b:[0-9a-fA-F]{64})\Z")


def validate(data):
    """Return all schema errors with JSON paths; map geometry is checked by the app."""
    errors = []

    def error(path, message):
        errors.append(f"{path}: {message}")

    def integer(value, path, low, high=None):
        if type(value) is not int or value < low or (high is not None and value > high):
            error(path, f"expected integer in {low}..{high if high is not None else 'infinity'}")
            return False
        return True

    def number(value):
        try:
            return type(value) in (int, float) and math.isfinite(value)
        except OverflowError:
            return False

    def vector(value, path, length, bounded=False):
        if not isinstance(value, list) or len(value) != length:
            error(path, f"expected {length} numbers")
            return False
        valid = True
        for index, item in enumerate(value):
            if not number(item) or (bounded and not 0 <= item <= 1):
                error(f"{path}[{index}]", "expected finite number" + (" in [0,1]" if bounded else ""))
                valid = False
        return valid

    def pose(value, path):
        vector(value.get('position'), path + '.position', 3)
        axes = []
        for axis in ('forward', 'up'):
            v = value.get(axis)
            ok = vector(v, path + '.' + axis, 3)
            if ok and abs(math.hypot(*v) - 1) > 1e-3:
                error(path + '.' + axis, 'expected unit vector within 1e-3')
                ok = False
            axes.append(ok)
        if all(axes) and abs(sum(a*b for a, b in zip(value['forward'], value['up']))) > 1e-3:
            error(path + '.up', 'expected orthogonal forward and up within 1e-3')

    def mp_scene(mp, path):
        if not isinstance(mp, dict):
            error(path, 'expected object')
            return
        for key in ('teams', 'no_map_weapons', 'no_shields', 'always_invisible', 'infinite_grenades'):
            if type(mp.get(key)) is not bool:
                error(path + '.' + key, 'expected boolean')
        for key, high in (('weapon_set', 13), ('vehicle_set', 255), ('loadout', 1),
                          ('primary_weapon', 9), ('secondary_weapon', 9), ('vehicle_respawn_time', 32767)):
            integer(mp.get(key), path + '.' + key, 0, high)
        if mp.get('starting_equipment') not in ('generic', 'map'):
            error(path + '.starting_equipment', 'expected generic or map')
        sets = mp.get('vehicle_sets')
        if not isinstance(sets, list) or len(sets) != 2:
            error(path + '.vehicle_sets', 'expected two vehicle sets')
        else:
            for side, value in enumerate(sets):
                integer(value, f'{path}.vehicle_sets[{side}]', 0, 255)
        counts = mp.get('vehicle_counts')
        if not isinstance(counts, list) or len(counts) != 2:
            error(path + '.vehicle_counts', 'expected two rows of six vehicle counts')
        else:
            for side, row in enumerate(counts):
                if not isinstance(row, list) or len(row) != 6:
                    error(f'{path}.vehicle_counts[{side}]', 'expected six vehicle counts')
                else:
                    for index, value in enumerate(row):
                        integer(value, f'{path}.vehicle_counts[{side}][{index}]', 0, 4)

    def finite_tree(value, path):
        if type(value) in (int, float) and not number(value):
            error(path, 'expected finite number')
        elif isinstance(value, dict):
            for key, item in value.items():
                finite_tree(item, f'{path}[{json.dumps(key)}]')
        elif isinstance(value, list):
            for index, item in enumerate(value):
                finite_tree(item, f'{path}[{index}]')

    if not isinstance(data, dict):
        return ["$: expected object"]
    finite_tree(data, '$')
    if type(data.get('schema')) is not int or data['schema'] not in (1, 2):
        error('$.schema', 'expected schema 1 or 2')
    maps = data.get('maps')
    if not isinstance(maps, dict):
        return errors + ['$.maps: expected object']
    for key, entry in maps.items():
        path = f'$.maps[{json.dumps(key)}]'
        if not KEY.fullmatch(key):
            error(path, 'expected stock scenario name or 64 BLAKE2b hex digits')
        if not isinstance(entry, dict):
            error(path, 'expected object')
            continue
        integer(entry.get('revision'), path + '.revision', 1)
        shots = entry.get('shots')
        if not isinstance(shots, list) or not 1 <= len(shots) <= 8:
            error(path + '.shots', 'expected 1 to 8 shots')
            continue
        for index, shot in enumerate(shots):
            p = f'{path}.shots[{index}]'
            if not isinstance(shot, dict):
                error(p, 'expected object')
                continue
            pose(shot, p)
            fov = shot.get('fov')
            if not number(fov) or not 0.2 <= fov <= 2.6:
                error(p + '.fov', 'expected finite radians in [0.2,2.6]')
            integer(shot.get('bsp'), p + '.bsp', 0)
            vector(shot.get('focus'), p + '.focus', 2, True)
            safe = shot.get('safe')
            if vector(safe, p + '.safe', 4, True) and not (safe[0] < safe[2] and safe[1] < safe[3]):
                error(p + '.safe', 'expected left < right and top < bottom')
            integer(shot.get('settle_ticks'), p + '.settle_ticks', 0, 600)
            scene = shot.get('scene')
            if not isinstance(scene, dict):
                error(p + '.scene', 'expected object')
            else:
                if scene.get('ai') not in ('erase', 'keep'):
                    error(p + '.scene.ai', 'expected erase or keep')
                if not isinstance(scene.get('variant'), str):
                    error(p + '.scene.variant', 'expected string')
                if 'mp' in scene:
                    mp_scene(scene['mp'], p + '.scene.mp')
                if 'moved_vehicles' in scene:
                    moved = scene['moved_vehicles']
                    if not isinstance(moved, list) or len(moved) > 16:
                        error(p + '.scene.moved_vehicles', 'expected at most 16 moved vehicles')
                    else:
                        for index, vehicle in enumerate(moved):
                            vp = f'{p}.scene.moved_vehicles[{index}]'
                            if not isinstance(vehicle, dict):
                                error(vp, 'expected object')
                                continue
                            tag = vehicle.get('tag')
                            if not isinstance(tag, str) or not 1 <= len(tag) < 256 or any(not 32 <= ord(c) <= 126 for c in tag):
                                error(vp + '.tag', 'expected 1 to 255 printable ASCII characters')
                            pose(vehicle, vp)

    return errors


def checked(data):
    errors = validate(data)
    if errors:
        raise ValueError('\n'.join(errors))
    return data


def stable(data):
    return json.dumps(data, indent=2, sort_keys=True, allow_nan=False) + '\n'


def merge(source, fragments, force=False):
    result = json.loads(json.dumps(checked(source)))
    result['schema'] = 2
    for fragment in fragments:
        checked(fragment)
        for key, entry in fragment['maps'].items():
            old = result['maps'].get(key)
            if old and entry['revision'] <= old['revision'] and not force:
                raise ValueError(f'$.maps[{json.dumps(key)}].revision: must rise above {old["revision"]}')
            result['maps'][key] = entry
    return checked(result)


def import_camera(key, text, camera_format='visr', fov=None):
    # TODO: Confirm H1A-EK's layout against a sample before supporting other layouts.
    values = [float(value) for value in text.split()]
    if len(values) != 10:
        raise ValueError('$: camera file must contain position, forward, up, and FOV (10 numbers)')
    shot = dict(position=values[:3], forward=values[3:6], up=values[6:9],
                fov=values[9] if fov is None else fov, bsp=0,
                focus=[0.5, 0.5], safe=[0.19, 0, 0.81, 1], settle_ticks=30,
                scene=dict(ai='erase', variant='slayer'))
    return checked(dict(schema=2, maps={key: dict(revision=1, shots=[shot])}))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, add_help=False)
    parser.add_argument('--help', action='help')
    commands = parser.add_subparsers(dest='command', required=True)
    def command(name):
        sub = commands.add_parser(name, add_help=False)
        sub.add_argument('--help', action='help')
        return sub
    sub = command('validate')
    sub.add_argument('path', type=Path)
    sub = command('merge')
    sub.add_argument('--into', required=True, type=Path)
    sub.add_argument('--force', action='store_true')
    sub.add_argument('fragments', nargs='+', type=Path)
    sub = command('import-camera')
    sub.add_argument('--map', required=True)
    sub.add_argument('--format', required=True, choices=('visr', 'halopc', 'h1aek'))
    sub.add_argument('--fov', type=float)
    sub.add_argument('camera_file', type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == 'validate':
            checked(json.loads(args.path.read_text()))
        elif args.command == 'merge':
            data = merge(json.loads(args.into.read_text()),
                         [json.loads(p.read_text()) for p in args.fragments], args.force)
            temporary = args.into.with_suffix(args.into.suffix + '.tmp')
            temporary.write_text(stable(data))
            temporary.replace(args.into)
        else:
            print(stable(import_camera(args.map, args.camera_file.read_text(), args.format, args.fov)), end='')
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
