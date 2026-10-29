"""Verify and selectively extract the portal SDK into an ignored build directory."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import os
import shutil
import stat
import tempfile
import urllib.parse
import urllib.request
import zipfile

VERSION = '1.10.19337'
SHA256 = 'd784097504685953849cc2842561d8a8013326fe0b8df65a1de63ddefde62045'
FRAMEWORK = 'lib/release/discord_partner_sdk.framework'
XCFRAMEWORK = 'lib/release/discord_partner_sdk.xcframework'
SLICES = {'macos': FRAMEWORK, 'ios': f'{XCFRAMEWORK}/ios-arm64/discord_partner_sdk.framework',
          'ios-simulator': f'{XCFRAMEWORK}/ios-arm64-simulator/discord_partner_sdk.framework'}


def _relative_path(name):
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name or not path.parts:
        raise ValueError('Discord SDK archive contains an unsafe path')
    return path


def _extract(archive, destination, platform):
    """Create files before symlinks so archive order cannot redirect file writes."""
    framework = SLICES[platform]
    selected = {}
    links = []
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            path = _relative_path(entry.filename)
            if path.parts[0] == 'discord_social_sdk':
                path = PurePosixPath(*path.parts[1:])
            name = path.as_posix()
            if not (name.startswith('include/') or name == 'License-Notices.txt' or
                    name == framework or name.startswith(framework + '/')):
                continue
            if name in selected:
                raise ValueError('Discord SDK archive contains duplicate paths')
            selected[name] = entry
        link_paths = {destination / name for name, entry in selected.items()
                      if stat.S_ISLNK(entry.external_attr >> 16)}
        if any(parent in link_paths for name in selected
               for parent in (destination / name).parents):
            raise ValueError('Discord SDK archive writes through a symlink')
        for name, entry in selected.items():
            target = destination / name
            mode = entry.external_attr >> 16
            if stat.S_ISLNK(mode):
                link = package.read(entry).decode('utf-8')
                if PurePosixPath(link).is_absolute() or '\\' in link:
                    raise ValueError('Discord SDK archive contains an unsafe symlink')
                # Relative links may use .. inside their own framework, but cannot escape it.
                resolved = Path(os.path.normpath(str(target.parent / link)))
                if not resolved.is_relative_to(destination / framework):
                    raise ValueError('Discord SDK symlink escapes its framework')
                links.append((target, link))
            elif entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            elif stat.S_IFMT(mode) not in (0, stat.S_IFREG):
                raise ValueError('Discord SDK archive contains an unsupported file type')
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with package.open(entry) as source, target.open('wb') as output:
                    shutil.copyfileobj(source, output)
                target.chmod(mode & 0o777 or 0o644)
        for target, link in links:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to(link)
        for target, _ in links:
            try:
                resolved = target.resolve(strict=True)
            except (OSError, RuntimeError) as error:
                raise ValueError('Discord SDK archive contains a broken or cyclic symlink') from error
            if not resolved.is_relative_to(destination / framework):
                raise ValueError('Discord SDK symlink escapes its framework')
    required = ('include/discordpp.h', 'include/cdiscord.h', 'License-Notices.txt',
                f'{framework}/discord_partner_sdk')
    if not all((destination / name).is_file() for name in required):
        raise ValueError(f'Discord SDK archive is missing required {platform} files')


def prepare(source, expected_sha256, destination, platform='macos'):
    """Return the platform root only after verifying and extracting the entire selection."""
    if len(expected_sha256) != 64 or any(c not in '0123456789abcdef' for c in expected_sha256):
        raise ValueError('VISR_DISCORD_SDK_SHA256 must be a recorded lowercase SHA-256 digest')
    if platform not in SLICES:
        raise ValueError(f'Unsupported Discord SDK platform: {platform}')
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        temporary = Path(temporary)
        parsed = urllib.parse.urlparse(source)
        if parsed.scheme == 'https':
            archive = temporary / 'sdk.zip'
            with urllib.request.urlopen(source, timeout=60) as response:
                if urllib.parse.urlparse(response.url).scheme != 'https':
                    raise ValueError('SDK download must stay on HTTPS')
                with archive.open('wb') as output:
                    shutil.copyfileobj(response, output)
        elif '://' in source:
            raise ValueError('VISR_DISCORD_SDK must be a local archive or HTTPS URL')
        else:
            archive = Path(source).expanduser()
        with archive.open('rb') as archive_file:
            digest = hashlib.file_digest(archive_file, 'sha256').hexdigest()
        if digest != expected_sha256:
            raise ValueError('Discord SDK SHA-256 mismatch; archive rejected')
        receipt = {'version': VERSION, 'sha256': digest, 'platform': platform}
        root = destination / platform
        if (root / 'receipt.json').is_file():
            if json.loads((root / 'receipt.json').read_text()) == receipt:
                required = ('include/discordpp.h', 'include/cdiscord.h', 'License-Notices.txt',
                            f'{SLICES[platform]}/discord_partner_sdk')
                if all((root / name).is_file() for name in required):
                    return root
        staged = temporary / 'extracted'
        staged.mkdir()
        _extract(archive, staged, platform)
        (staged / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if root.exists():
            root.rename(temporary / 'previous')
        staged.rename(root)
    return root


def prepare_from_environment(destination, platform='macos'):
    source = os.environ.get('VISR_DISCORD_SDK')
    if not source:
        raise ValueError('--discord on requires VISR_DISCORD_SDK pointing to '
                         f'DiscordSocialSdk-{VERSION}.zip (local path or HTTPS URL)')
    try:
        root = prepare(source, os.environ.get('VISR_DISCORD_SDK_SHA256', SHA256), destination, platform)
    except zipfile.BadZipFile as error:
        raise ValueError('Discord SDK archive is not a valid ZIP file') from error
    print(f'Discord SDK {VERSION}: verified {platform} framework at {root}', flush=True)
    return root
