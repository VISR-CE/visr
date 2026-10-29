"""Archive provenance and extraction checks use synthetic ZIPs, never a portal SDK."""
import hashlib
import json
import stat
import zipfile
import pytest
from discord_sdk import FRAMEWORK, SLICES, SHA256, VERSION, prepare, prepare_from_environment


def sdk_archive(tmp_path, extra=()):
    source = tmp_path / 'portal.zip'
    with zipfile.ZipFile(source, 'w') as archive:
        files = {'include/discordpp.h': b'header', 'include/cdiscord.h': b'header',
                 'License-Notices.txt': b'notices', 'lib/release/unwanted.dll': b'other platform'}
        for platform, framework in SLICES.items():
            files[f'{framework}/Versions/A/discord_partner_sdk'] = platform.encode()
            files[f'{framework}/Versions/A/Resources/PrivacyInfo.xcprivacy'] = b'privacy'
        for name, data in files.items():
            archive.writestr('discord_social_sdk/' + name, data)
        for framework in SLICES.values():
            for name, target in [('Versions/Current', 'A'),
                                 ('discord_partner_sdk', 'Versions/Current/discord_partner_sdk')]:
                entry = zipfile.ZipInfo(f'discord_social_sdk/{framework}/{name}')
                entry.create_system = 3
                entry.external_attr = (stat.S_IFLNK | 0o777) << 16
                archive.writestr(entry, target)
        for name, data, mode in extra:
            entry = zipfile.ZipInfo(name)
            entry.create_system = 3
            entry.external_attr = mode << 16
            archive.writestr(entry, data)
    return source, hashlib.sha256(source.read_bytes()).hexdigest()


@pytest.mark.parametrize('platform', SLICES)
def test_only_target_framework_headers_and_notices_are_extracted(tmp_path, platform):
    source, digest = sdk_archive(tmp_path)
    root = prepare(str(source), digest, tmp_path / 'build', platform)
    framework = root / SLICES[platform]
    assert root == tmp_path / 'build' / platform
    assert (root / 'include/discordpp.h').read_bytes() == b'header'
    assert (root / 'License-Notices.txt').read_bytes() == b'notices'
    assert (framework / 'discord_partner_sdk').is_symlink()
    assert (framework / 'discord_partner_sdk').read_text() == platform
    assert (framework / 'Versions/Current').readlink().as_posix() == 'A'
    assert (framework / 'Versions/A/Resources/PrivacyInfo.xcprivacy').is_file()
    assert not (root / 'lib/release/unwanted.dll').exists()
    for other, path in SLICES.items():
        if other != platform:
            assert not (root / path).exists()
    assert json.loads((root / 'receipt.json').read_text()) == {
        'version': VERSION, 'sha256': digest, 'platform': platform}


def test_mismatch_preserves_previous_verified_extraction(tmp_path):
    source, digest = sdk_archive(tmp_path)
    root = prepare(str(source), digest, tmp_path / 'build')
    with pytest.raises(ValueError, match='mismatch'):
        prepare(str(source), '0' * 64, tmp_path / 'build')
    assert (root / FRAMEWORK / 'discord_partner_sdk').read_bytes() == b'macos'


@pytest.mark.parametrize('digest', ['', '0' * 63, 'g' * 64])
def test_missing_or_invalid_digest_is_rejected_before_reading_source(tmp_path, digest):
    with pytest.raises(ValueError, match='recorded lowercase'):
        prepare('missing.zip', digest, tmp_path / 'build')
    assert not (tmp_path / 'build').exists()


def test_insecure_url_is_rejected(tmp_path):
    with pytest.raises(ValueError, match='HTTPS'):
        prepare('http://example.com/sdk.zip', '0' * 64, tmp_path / 'build')


@pytest.mark.parametrize('name,target,mode', [
    ('../escape', b'bad', stat.S_IFREG | 0o644),
    ('/absolute', b'bad', stat.S_IFREG | 0o644),
    (f'discord_social_sdk/{FRAMEWORK}/escape', b'../../../../escape', stat.S_IFLNK | 0o777),
    (f'discord_social_sdk/{FRAMEWORK}/escape', b'/tmp/escape', stat.S_IFLNK | 0o777),
    (f'discord_social_sdk/{FRAMEWORK}/escape', b'escape', stat.S_IFLNK | 0o777),
    (f'discord_social_sdk/{FRAMEWORK}/discord_partner_sdk/child', b'bad', stat.S_IFREG | 0o644),
])
def test_unsafe_archive_is_rejected_without_publishing(tmp_path, name, target, mode):
    source, digest = sdk_archive(tmp_path, [(name, target, mode)])
    with pytest.raises(ValueError):
        prepare(str(source), digest, tmp_path / 'build')
    assert not (tmp_path / 'build/macos').exists()


def test_incomplete_archive_is_rejected(tmp_path):
    source = tmp_path / 'incomplete.zip'
    with zipfile.ZipFile(source, 'w') as archive:
        archive.writestr('discord_social_sdk/include/discordpp.h', 'header')
    with pytest.raises(ValueError, match='missing required'):
        prepare(str(source), hashlib.sha256(source.read_bytes()).hexdigest(), tmp_path / 'build')


def test_environment_requires_explicit_sdk_source(monkeypatch, tmp_path):
    monkeypatch.delenv('VISR_DISCORD_SDK', raising=False)
    with pytest.raises(ValueError, match='--discord on requires VISR_DISCORD_SDK'):
        prepare_from_environment(tmp_path)


def test_environment_defaults_to_pinned_digest(monkeypatch, tmp_path):
    import discord_sdk
    calls = []
    monkeypatch.setenv('VISR_DISCORD_SDK', 'portal.zip')
    monkeypatch.delenv('VISR_DISCORD_SDK_SHA256', raising=False)
    monkeypatch.setattr(discord_sdk, 'prepare', lambda *args: calls.append(args) or tmp_path)
    assert prepare_from_environment(tmp_path, 'ios') == tmp_path
    assert calls == [('portal.zip', SHA256, tmp_path, 'ios')]


def test_native_passes_prepared_root_and_clears_cached_root():
    from tools import macos_build
    from tools.test_macos_build import options
    configure, _ = macos_build.native_commands(options(discord='on', discord_sdk_root='/sdk/macos'), [])
    assert '-DHALO_DISCORD_SDK_ROOT=/sdk/macos' in configure
    configure, _ = macos_build.native_commands(options(), [])
    assert '-DHALO_DISCORD=OFF' in configure
    assert '-DHALO_DISCORD_SDK_ROOT=' in configure


def test_embedded_code_is_signed_inside_out_without_following_aliases(tmp_path):
    from tools.macos_build import sign_embedded_frameworks
    app = tmp_path / 'VISR.app'
    framework = app / 'Contents/Frameworks/discord_partner_sdk.framework'
    nested = framework / 'Versions/A/Frameworks/libdiscord_krisp.dylib'
    nested.parent.mkdir(parents=True)
    nested.touch()
    (framework / 'Frameworks').symlink_to('Versions/A/Frameworks')
    commands = []
    sign_embedded_frameworks(app, lambda *args: commands.append(args))
    assert commands == [('codesign', '--force', '--sign', '-', nested),
                        ('codesign', '--force', '--sign', '-', framework)]


def test_off_rebuild_removes_stale_framework_and_notices(tmp_path):
    from tools.macos_build import drop_stale_discord
    app = tmp_path / 'VISR.app'
    framework = app / 'Contents/Frameworks/discord_partner_sdk.framework'
    framework.mkdir(parents=True)
    (framework / 'old-binary').touch()
    notices = app / 'Contents/Resources/Licenses/License-Notices.txt'
    notices.parent.mkdir(parents=True)
    notices.write_text('old notices')
    drop_stale_discord(app)
    assert not framework.exists()
    assert not notices.exists()


@pytest.mark.parametrize('arguments,platform', [
    (('--unsigned',), 'ios'), (('--simulator',), 'ios-simulator'),
    (('--native-mac',), 'macos')])
def test_build_prepares_only_selected_platform_and_passes_root(monkeypatch, tmp_path, arguments, platform):
    from tools import discord_sdk
    from tools.test_ios_build import configure_with
    calls = []
    prepared = tmp_path / 'prepared' / platform
    monkeypatch.setattr(discord_sdk, 'prepare_from_environment',
                        lambda *args: calls.append(args) or prepared)
    configure = configure_with(monkeypatch, tmp_path, *arguments, '--discord', 'on')
    assert calls == [(tmp_path / 'build/third_party/discord', platform)]
    assert '-DHALO_DISCORD=ON' in configure
    assert f'-DHALO_DISCORD_SDK_ROOT={prepared}' in configure


def test_ios_off_build_clears_cached_discord_and_removes_stale_embed(monkeypatch, tmp_path):
    from tools.test_ios_build import configure_with
    stale = tmp_path / 'build/ios/app-unsigned/Release-iphoneos/VISR.app/Frameworks/discord_partner_sdk.framework'
    stale.mkdir(parents=True)
    (stale / 'old-binary').touch()
    notices = stale.parent.parent / 'Licenses/License-Notices.txt'
    notices.parent.mkdir(parents=True)
    notices.write_text('old notices')
    configure = configure_with(monkeypatch, tmp_path, '--unsigned', '--discord', 'off')
    assert '-DHALO_DISCORD=OFF' in configure
    assert '-DHALO_DISCORD_SDK_ROOT=' in configure
    assert not stale.exists()
    assert not notices.exists()


@pytest.mark.parametrize('arguments', [('--mac',), ('--unsigned', '--tvos'), ('--unsigned', '--visionos')])
def test_discord_is_rejected_on_unsupported_apple_targets(monkeypatch, tmp_path, arguments):
    from tools.test_ios_build import configure_with
    with pytest.raises(SystemExit):
        configure_with(monkeypatch, tmp_path, *arguments, '--discord', 'on')
