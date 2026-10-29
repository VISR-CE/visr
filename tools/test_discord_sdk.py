"""Archive provenance checks use synthetic bytes, never a proprietary SDK."""
import hashlib
import json
import pytest
from discord_sdk import prepare


def test_archive_is_staged_only_with_matching_recorded_digest(tmp_path):
    source = tmp_path / 'portal.zip'
    source.write_bytes(b'synthetic SDK archive')
    expected = hashlib.sha256(source.read_bytes()).hexdigest()
    archive = prepare(str(source), expected, tmp_path / 'build')
    assert archive.read_bytes() == source.read_bytes()
    assert json.loads((archive.parent / 'receipt.json').read_text()) == {'sha256': expected}


def test_mismatch_preserves_previous_verified_archive(tmp_path):
    destination = tmp_path / 'build'
    destination.mkdir()
    (destination / 'sdk.archive').write_bytes(b'previous verified archive')
    source = tmp_path / 'portal.zip'
    source.write_bytes(b'changed archive')
    with pytest.raises(ValueError, match='mismatch'):
        prepare(str(source), '0' * 64, destination)
    assert (destination / 'sdk.archive').read_bytes() == b'previous verified archive'
    assert not (destination / 'receipt.json').exists()


@pytest.mark.parametrize('digest', ['', '0' * 63, 'g' * 64])
def test_missing_or_invalid_digest_is_rejected_before_reading_source(tmp_path, digest):
    with pytest.raises(ValueError, match='recorded lowercase'):
        prepare('missing.zip', digest, tmp_path / 'build')
    assert not (tmp_path / 'build').exists()


def test_insecure_url_is_rejected(tmp_path):
    with pytest.raises(ValueError, match='HTTPS'):
        prepare('http://example.com/sdk.zip', '0' * 64, tmp_path / 'build')
