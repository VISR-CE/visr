"""Stage a portal SDK archive without trusting an unpinned download.

VISR_DISCORD_SDK is a local archive or HTTPS URL. The lead must independently
record its digest in VISR_DISCORD_SDK_SHA256 before the build accepts it.
SDK extraction and linking wait for inspection of the actual portal package.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import tempfile
import urllib.parse
import urllib.request


def prepare(source, expected_sha256, destination):
    if len(expected_sha256) != 64 or any(c not in '0123456789abcdef' for c in expected_sha256):
        raise ValueError('VISR_DISCORD_SDK_SHA256 must be a recorded lowercase SHA-256 digest')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        staged = Path(temporary) / 'sdk.archive'
        parsed = urllib.parse.urlparse(source)
        if parsed.scheme == 'https':
            with urllib.request.urlopen(source, timeout=60) as response:
                if urllib.parse.urlparse(response.url).scheme != 'https':
                    raise ValueError('SDK download must stay on HTTPS')
                with staged.open('wb') as output:
                    shutil.copyfileobj(response, output)
        elif '://' in source:
            raise ValueError('VISR_DISCORD_SDK must be a local archive or HTTPS URL')
        else:
            shutil.copyfile(Path(source).expanduser(), staged)
        with staged.open('rb') as archive_file:
            digest = hashlib.file_digest(archive_file, 'sha256').hexdigest()
        if digest != expected_sha256:
            raise ValueError('Discord SDK SHA-256 mismatch; archive rejected')
        archive = destination / 'sdk.archive'
        staged.replace(archive)
        (destination / 'receipt.json').write_text(json.dumps({'sha256': digest}, indent=2) + '\n')
    return archive


def prepare_from_environment(destination):
    source = os.environ.get('VISR_DISCORD_SDK')
    if not source:
        print('Discord preflight: SDK absent; linking and browser authorization unavailable', flush=True)
        return None
    archive = prepare(source, os.environ.get('VISR_DISCORD_SDK_SHA256', ''), destination)
    print('Discord preflight: archive verified; SDK inspection and integration still required', flush=True)
    return archive
