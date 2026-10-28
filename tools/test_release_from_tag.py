"""Release from any reviewed commit: the tag carries the release, and sync-main merges only its metadata."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
from typing import Any

import pytest

from tools import release
from tools.test_release import policy, recording_summary_context

OLD_ENTRY = "## visr-v0.6.1 (2026-10-27)\n\n### fix\n\n- **loading**: older fix\n"


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def commit_file(root: Path, path: str, content: str, message: str) -> str:
    target = root / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content)
    git(root, "add", path)
    git(root, "commit", "--quiet", "--message", message)
    return git(root, "rev-parse", "HEAD")


def clone(origin: Path, path: Path) -> Path:
    subprocess.check_call(["git", "clone", "--quiet", str(origin), str(path)])
    git(path, "config", "user.name", "Release test")
    git(path, "config", "user.email", "release-test@example.invalid")
    return path


@pytest.fixture
def remote(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> dict[str, Any]:
    """Main holds 0.6.1 (cut the old way), the fix to release, then a later fix that must not ship."""
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", os.devnull)
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
    origin = tmp_path / "origin.git"
    subprocess.check_call(["git", "init", "--quiet", "--bare", "--initial-branch=main", str(origin)])
    seed = clone(origin, tmp_path / "seed")
    git(seed, "checkout", "--quiet", "-B", "main")
    commit_file(seed, "app.txt", "v1\n", "✨ feat(app): first")
    (seed / ".release").mkdir()
    (seed / release.MANIFEST).write_text(
        json.dumps({"tag": "visr-v0.6.1", "version": "0.6.1"}) + "\n"
    )
    (seed / "CHANGELOG.md").write_text(OLD_ENTRY)
    git(seed, "add", "CHANGELOG.md", ".release/current.json")
    git(seed, "commit", "--quiet", "--message", "🔧 chore(release): generate visr-v0.6.1 changelog")
    git(seed, "tag", "--annotate", "visr-v0.6.1", "--message", "visr-v0.6.1")
    wanted = commit_file(seed, "app.txt", "v2\n", "🐛 fix(app): the fix to release")
    later = commit_file(seed, "app.txt", "v3\n", "🐛 fix(app): a later fix that waits")
    git(seed, "push", "--quiet", "origin", "main", "--tags")
    runner = clone(origin, tmp_path / "runner")
    monkeypatch.setattr(release, "ROOT", runner)
    monkeypatch.setattr(release, "require_ci", lambda: None)
    outputs: list[tuple[str, str]] = []
    monkeypatch.setattr(release, "output", lambda *pair: outputs.append(pair))
    return {
        "origin": origin,
        "seed": seed,
        "runner": runner,
        "wanted": wanted,
        "later": later,
        "outputs": outputs,
        "tmp": tmp_path,
    }


def fake_commitizen(monkeypatch: pytest.MonkeyPatch, version: str) -> None:
    """Commitizen is not in the tooling environment; stand in for its two calls and run git for real."""
    real_run = release.run

    def run(*args: Any, **kwargs: Any) -> str:
        if args[:3] == ("cz", "bump", "--get-next"):
            return version
        if args[:2] == ("cz", "bump"):
            changelog = release.ROOT / "CHANGELOG.md"
            entry = f"## visr-v{version} (2026-10-28)\n\n### fix\n\n- **app**: the fix to release\n\n"
            changelog.write_text(entry + changelog.read_text())
            return ""
        return real_run(*args, **kwargs)

    monkeypatch.setattr(release, "run", run)
    monkeypatch.setattr(release, "require_wif_configuration", lambda *_: None)
    monkeypatch.setattr(
        release, "anthropic_release_summary", lambda *_args, **_kwargs: "Try the fix."
    )


def prepare(monkeypatch: pytest.MonkeyPatch, source: str, branch: str = "main", resume: str = "") -> None:
    monkeypatch.setenv("RELEASE_BUILD_POLICY", json.dumps(policy()))
    monkeypatch.setenv("RELEASE_RECORDING_CONTEXT", json.dumps(recording_summary_context()))
    monkeypatch.setenv("RELEASE_SUMMARY_MODEL", "claude-sonnet-5-5")
    from tools.release_validation import preflight_identity

    monkeypatch.setenv("RELEASE_PREFLIGHT_IDENTITY", preflight_identity(source, ["ios"], policy()))
    release.prepare(
        argparse.Namespace(
            source_sha=source,
            source_branch=branch,
            platforms="ios",
            increment="patch",
            resume_tag=resume,
            bootstrap_since="",
            bootstrap_version="0.1.2",
        )
    )


def remote_ref(origin: Path, ref: str) -> str:
    return git(origin, "rev-parse", ref)


def test_release_an_older_main_commit_without_touching_main(remote, monkeypatch: pytest.MonkeyPatch):
    origin, runner = remote["origin"], remote["runner"]
    fake_commitizen(monkeypatch, "0.6.2")

    prepare(monkeypatch, remote["wanted"])

    tag_commit = remote_ref(origin, "visr-v0.6.2^{commit}")
    assert remote_ref(origin, f"{tag_commit}^") == remote["wanted"]
    assert remote_ref(origin, "main") == remote["later"], "prepare must not push main"
    assert git(origin, "show", f"{tag_commit}:app.txt") == "v2"
    manifest = json.loads(git(origin, "show", f"{tag_commit}:{release.MANIFEST}"))
    assert manifest["source_sha"] == remote["wanted"]
    assert manifest["source_branch"] == "main"
    assert ("tag", "visr-v0.6.2") in remote["outputs"]

    # Recovery works while main is still ahead and before the metadata reaches it.
    commit_file(remote["seed"], "other.txt", "x\n", "📝 docs(other): unrelated work")
    git(remote["seed"], "push", "--quiet", "origin", "main")
    prepare(monkeypatch, remote["wanted"], resume="visr-v0.6.2")

    release.sync_main(argparse.Namespace(tag="visr-v0.6.2"))

    main = remote_ref(origin, "main")
    assert git(origin, "merge-base", "--is-ancestor", tag_commit, main) == ""
    assert git(origin, "show", f"{main}:app.txt") == "v3", "the later fix stays on main"
    assert git(origin, "show", f"{main}:other.txt") == "x"
    changelog = git(origin, "show", f"{main}:CHANGELOG.md")
    assert changelog.index("## visr-v0.6.2") < changelog.index("## visr-v0.6.1")
    assert json.loads(git(origin, "show", f"{main}:{release.MANIFEST}"))["version"] == "0.6.2"
    subject = git(origin, "log", "-1", "--format=%s", main)
    assert subject.startswith("🔀 merge(release): ")

    release.sync_main(argparse.Namespace(tag="visr-v0.6.2"))
    assert remote_ref(origin, "main") == main, "a second sync is a no-op"

    # The next release continues from the tag history: main after the merge has it, main before does not.
    release.run("git", "fetch", "--tags", "origin", "+refs/heads/main:refs/remotes/origin/main")
    assert release.require_tag_continuity(main) == ["visr-v0.6.1", "visr-v0.6.2"]
    with pytest.raises(ValueError, match="visr-v0.6.2 is not in the source's history"):
        release.require_tag_continuity(remote["later"])


def test_release_branch_revert_never_reaches_main(remote, monkeypatch: pytest.MonkeyPatch):
    origin, seed = remote["origin"], remote["seed"]
    git(seed, "checkout", "--quiet", "-b", "release/0.6.2", remote["later"])
    git(seed, "revert", "--no-edit", remote["later"])
    held = git(seed, "rev-parse", "HEAD")
    git(seed, "push", "--quiet", "origin", "release/0.6.2")
    # Main moves on and edits the changelog's preamble while the release runs.
    git(seed, "checkout", "--quiet", "main")
    (seed / "CHANGELOG.md").write_text("# Changelog\n\n" + OLD_ENTRY)
    git(seed, "commit", "--quiet", "--all", "--message", "📝 docs(changelog): add a title")
    commit_file(seed, "app.txt", "v4\n", "🐛 fix(app): newer work")
    git(seed, "push", "--quiet", "origin", "main")
    fake_commitizen(monkeypatch, "0.6.2")

    with pytest.raises(ValueError, match="is not on main"):
        prepare(monkeypatch, held)
    prepare(monkeypatch, held, branch="release/0.6.2")
    release.sync_main(argparse.Namespace(tag="visr-v0.6.2"))

    main = remote_ref(origin, "main")
    assert git(origin, "show", f"{main}:app.txt") == "v4"
    changelog = git(origin, "show", f"{main}:CHANGELOG.md")
    assert changelog.startswith("# Changelog\n\n## visr-v0.6.2 (2026-10-28)")
    assert changelog.count("## visr-v0.6.1") == 1
    tag_commit = remote_ref(origin, "visr-v0.6.2^{commit}")
    assert json.loads(git(origin, "show", f"{tag_commit}:{release.MANIFEST}"))["source_branch"] == "release/0.6.2"


def test_sync_retries_when_main_moves_during_the_push(remote, monkeypatch: pytest.MonkeyPatch):
    origin = remote["origin"]
    fake_commitizen(monkeypatch, "0.6.2")
    prepare(monkeypatch, remote["wanted"])
    racer = clone(origin, remote["tmp"] / "racer")
    real_run = release.run
    raced: list[bool] = []

    def run(*args: Any, **kwargs: Any) -> str:
        if args[:3] == ("git", "push", "origin") and args[3] == "HEAD:refs/heads/main" and not raced:
            raced.append(True)
            commit_file(racer, "race.txt", "x\n", "🐛 fix(race): lands mid-sync")
            git(racer, "push", "--quiet", "origin", "main")
        return real_run(*args, **kwargs)

    monkeypatch.setattr(release, "run", run)
    release.sync_main(argparse.Namespace(tag="visr-v0.6.2"))

    main = remote_ref(origin, "main")
    assert git(origin, "show", f"{main}:race.txt") == "x"
    git(origin, "merge-base", "--is-ancestor", "visr-v0.6.2^{commit}", main)


def test_sync_refuses_a_tag_that_changes_code(remote, monkeypatch: pytest.MonkeyPatch):
    seed = remote["seed"]
    git(seed, "checkout", "--quiet", "--detach", remote["wanted"])
    (seed / "app.txt").write_text("sneaky\n")
    (seed / release.MANIFEST).write_text(json.dumps({"tag": "visr-v0.6.2", "version": "0.6.2"}) + "\n")
    git(seed, "commit", "--quiet", "--all", "--message", "🔧 chore(release): generate visr-v0.6.2 changelog")
    git(seed, "tag", "visr-v0.6.2")
    git(seed, "push", "--quiet", "origin", "visr-v0.6.2")
    before = remote_ref(remote["origin"], "main")
    with pytest.raises(ValueError, match="changes more than"):
        release.sync_main(argparse.Namespace(tag="visr-v0.6.2"))
    assert remote_ref(remote["origin"], "main") == before


@pytest.mark.parametrize("branch", ["feature/x", "release/", "release/../main", "release/a.lock", "main2"])
def test_source_branch_names(branch: str):
    with pytest.raises(ValueError, match="main or release/<name>"):
        release.source_branch(branch)


def test_source_branch_defaults_to_main():
    assert release.source_branch("") == "main"
    assert release.source_branch("release/0.6.2") == "release/0.6.2"


def test_changelog_entry_merges_in_version_order():
    released = "## visr-v0.6.2 (2026-10-28)\n\n- new\n\n" + OLD_ENTRY
    assert release.merge_changelog_entry(OLD_ENTRY, released, "0.6.2").startswith(
        "## visr-v0.6.2 (2026-10-28)\n\n- new\n\n## visr-v0.6.1"
    )
    newer = "## visr-v0.6.3 (2026-10-29)\n\n- newest\n\n" + OLD_ENTRY
    merged = release.merge_changelog_entry(newer, released, "0.6.2")
    assert merged.index("0.6.3") < merged.index("0.6.2") < merged.index("0.6.1")
    assert release.merge_changelog_entry(released, released, "0.6.2") == released
    assert release.merge_changelog_entry("", released, "0.6.2").startswith("## visr-v0.6.2")


def test_main_manifest_keeps_the_newest_release():
    old = json.dumps({"version": "0.6.1"}) + "\n"
    new = json.dumps({"version": "0.6.2"}) + "\n"
    assert release.merged_manifest(old, new) == new
    assert release.merged_manifest(new, old) == new
    assert release.merged_manifest(None, old) == old
