"""Asset-free framing schema and converter regressions (collected by ci_test.py)."""
import copy
import json
from pathlib import Path
import subprocess
import sys

import pytest
import map_framing as framing

CAMERA = '1 2 3\n1 0 0\n0 0 1\n1.12\n'


def good():
    return framing.import_camera('stock:example', CAMERA)


def test_source():
    assert framing.validate(json.loads((Path(__file__).resolve().parents[1] / 'port/assets/map-framings.json').read_text())) == []


@pytest.mark.parametrize('field,value', [
    ('position', [1, 2]), ('position', [float('nan'), 0, 0]),
    ('forward', [2, 0, 0]), ('up', [1, 0, 0]), ('up', [0, 0, float('inf')]),
    ('fov', 0.19), ('fov', 2.61), ('fov', True), ('fov', float('inf')),
    ('bsp', -1), ('bsp', 0.5), ('bsp', True),
    ('focus', [-0.1, 0.5]), ('focus', [0, 1.1]), ('focus', [0]),
    ('safe', [0.8, 0, 0.2, 1]), ('safe', [0, 1, 1, 0]),
    ('safe', [0, 0, 1.1, 1]), ('safe', [0, 0, 1]),
    ('settle_ticks', -1), ('settle_ticks', 601), ('settle_ticks', True),
    ('scene', {'ai': 'hide', 'variant': 'slayer'}),
    ('scene', {'ai': 'keep', 'variant': 2}), ('scene', None),
])
def test_bad_shot(field, value):
    data = good()
    data['maps']['stock:example']['shots'][0][field] = value
    errors = framing.validate(data)
    assert errors and any(f'.{field}' in e for e in errors)


@pytest.mark.parametrize('key', ['example', 'stock:Example', 'stock:../bad', 'blake2b:' + 'a'*63, 'blake2b:' + 'g'*64])
def test_bad_key(key):
    data = good()
    data['maps'][key] = data['maps'].pop('stock:example')
    assert framing.validate(data)


@pytest.mark.parametrize('revision', [0, -1, True, 1.5, None])
def test_bad_revision(revision):
    data = good()
    data['maps']['stock:example']['revision'] = revision
    assert '.revision' in framing.validate(data)[0]


@pytest.mark.parametrize('count', [0, 9])
def test_shot_count(count):
    data = good()
    entry = data['maps']['stock:example']
    entry['shots'] *= count
    assert framing.validate(data)


@pytest.mark.parametrize('value', [None, [], {}, {'schema': True, 'maps': {}}, {'schema': 2, 'maps': {}}])
def test_shape(value):
    assert framing.validate(value)


def test_missing_fields_and_multiple_errors():
    data = good()
    data['maps']['stock:example']['shots'] = [{}]
    errors = framing.validate(data)
    assert len(errors) == 9
    assert all(e.startswith('$.maps["stock:example"].shots[0].') for e in errors)


def test_boundaries():
    for fov in [0.2, 2.6]:
        data = good()
        shot = data['maps']['stock:example']['shots'][0]
        shot.update(fov=fov, bsp=1, settle_ticks=600, focus=[0, 1], safe=[0, 0, 1, 1], scene={'ai': 'keep', 'variant': ''})
        shot['forward'] = [1.0005, 0, 0]
        assert not framing.validate(data)
    assert not framing.validate(framing.import_camera('blake2b:' + 'a'*64, CAMERA))
    assert not framing.validate(framing.import_camera('blake2b:' + 'A'*64, CAMERA))


def test_merge_and_stability():
    first = good()
    newer = copy.deepcopy(first)
    newer['maps']['stock:example']['revision'] = 2
    for fragment in [first, {**newer, 'maps': {'stock:example': {**newer['maps']['stock:example'], 'revision': 1}}}]:
        with pytest.raises(ValueError, match='revision'):
            framing.merge(first, [fragment])
    assert framing.merge(first, [newer]) == newer
    assert framing.merge(newer, [first], force=True) == first
    assert first['maps']['stock:example']['revision'] == 1
    two = framing.import_camera('stock:another', CAMERA)
    a = framing.merge(first, [two])
    b = framing.merge(two, [first])
    assert framing.stable(a) == framing.stable(b)
    assert framing.stable(a).endswith('\n')


@pytest.mark.parametrize('kind', ['visr', 'halopc', 'h1aek'])
def test_camera_round_trip(kind):
    data = framing.import_camera('stock:example', CAMERA, kind)
    shot = data['maps']['stock:example']['shots'][0]
    text = '\n'.join(' '.join(str(v) for v in shot[k]) for k in ['position', 'forward', 'up']) + '\n' + str(shot['fov'])
    assert framing.import_camera('stock:example', text, kind) == data
    assert framing.import_camera('stock:example', text, kind, 0.8)['maps']['stock:example']['shots'][0]['fov'] == 0.8
    with pytest.raises(ValueError):
        framing.import_camera('stock:example', '1 2 3')


def test_cli(tmp_path, capsys):
    source = tmp_path / 'source.json'
    source.write_text(framing.stable(good()))
    fragment = tmp_path / 'fragment.json'
    fragment.write_text(source.read_text())
    assert framing.main(['merge', '--into', str(source), str(fragment)]) == 1
    assert framing.main(['merge', '--force', '--into', str(source), str(fragment)]) == 0
    assert framing.main(['validate', str(source)]) == 0
    source.write_text('{"schema": 0, "maps": null}')
    assert framing.main(['validate', str(source)]) == 1
    assert '$.schema' in capsys.readouterr().err


@pytest.fixture(scope='module')
def native_probe(tmp_path_factory):
    tmp_path = tmp_path_factory.mktemp('native-framing')
    root = Path(__file__).resolve().parents[1]
    executable = tmp_path / 'photo-probe'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'port/linux/game'),
                    str(root / 'port/ios/tests/photo_framing_probe.c'),
                    str(root / 'port/linux/game/photo_framing.c'), '-lm', '-o', str(executable)], check=True)
    return executable


def test_native_probe(tmp_path, native_probe):
    executable = native_probe
    fragment = tmp_path / 'native.json'
    subprocess.run([str(executable), str(fragment)], check=True)
    data = json.loads(fragment.read_text())
    assert not framing.validate(data)
    assert data['maps']['stock:example']['revision'] == 2


@pytest.mark.parametrize('field,value', [
    ('position', [1, 2]), ('position', [float('nan'), 0, 0]),
    ('forward', [2, 0, 0]), ('up', [1, 0, 0]),
    ('fov', 0.19), ('fov', 2.61), ('fov', True),
    ('bsp', -1), ('bsp', 0.5), ('bsp', True),
    ('focus', [-0.1, 0.5]), ('safe', [0.8, 0, 0.2, 1]),
    ('settle_ticks', 601), ('scene', {'ai': 'hide', 'variant': 'slayer'}),
])
def test_native_rejects_bad_shot(tmp_path, native_probe, field, value):
    data = good()
    data['maps']['stock:example']['shots'][0][field] = value
    path = tmp_path / 'invalid.json'
    path.write_text(json.dumps(data))
    assert subprocess.run([str(native_probe), '--read', str(path), 'stock:example']).returncode == 1


@pytest.mark.parametrize('text', ['{}', '{', '[]', '{"schema":1,"maps":{}}',
    '{"schema":1,"schema":1,"maps":{}}', '{"schema":NaN}', '{} trailing'])
def test_native_rejects_malformed(tmp_path, native_probe, text):
    path = tmp_path / 'invalid.json'
    path.write_text(text)
    assert subprocess.run([str(native_probe), '--read', str(path), 'stock:example']).returncode == 1


def test_extra_nonfinite_number():
    data = good()
    data['extra'] = [float('inf')]
    assert '$["extra"][0]' in framing.validate(data)[0]


def test_finite_integer_overflow():
    data = good()
    data['maps']['stock:example']['shots'][0]['position'][0] = 10**400
    assert framing.validate(data)
