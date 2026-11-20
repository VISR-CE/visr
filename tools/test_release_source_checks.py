"""A release check that reads a constant the source defines runs with the source's tools.

The release controller is main's code, but the source being released can be a release branch on an older profile feed
generation. A check that compares the policy with the controller's copy of such a constant rejects a policy that is right
for the source (release run 44807380482: main had opened generation 5, release/0.6.20 reads generation 4)."""

import ast
import re
from pathlib import Path

import pytest
import yaml

TOOLS = Path(__file__).resolve().parent
WORKFLOW = TOOLS.parent / ".github/workflows/release.yml"

try:
    from tools import release, release_validation as validation
except ImportError:  # run as a script directory
    import release
    import release_validation as validation

# what a function reads when its answer depends on the source tree
SOURCE_CONSTANTS = ("packs.data_pack", "profile_revision", "PROFILE_FEED", "DEVICE_PROFILE_DOMAINS")


def functions(path: Path) -> dict[str, tuple[ast.FunctionDef, str]]:
    text = path.read_text()
    return {n.name: (n, ast.get_source_segment(text, n)) for n in ast.parse(text).body if isinstance(n, ast.FunctionDef)}


def test_every_function_reading_a_source_constant_is_source_bound_or_a_helper_of_one() -> None:
    reading = {name for name, (_, text) in functions(TOOLS / "release_validation.py").items()
               if any(constant in text for constant in SOURCE_CONSTANTS)}
    # helpers and the feed verifiers are reached only through the source-bound checks, load_policy only parses
    # the URL grammar and the fixed public key (the same in every generation), and probe/check already run in the
    # source checkout (the workflow's preflight job checks out the source before calling them)
    allowed = set(validation.SOURCE_BOUND_CHECKS) | {"profile_feed_domain", "verify_feed", "_verify_feed", "load_policy"}
    assert reading <= allowed, f"{sorted(reading - allowed)} read a source constant; add them to SOURCE_BOUND_CHECKS or justify"


def test_the_controller_never_calls_a_source_bound_check_directly() -> None:
    for module in ("release.py", "release_unsigned.py", "release_macos.py", "release_from_tag.py"):
        path = TOOLS / module
        if not path.exists():
            continue
        for node in ast.walk(ast.parse(path.read_text())):
            if isinstance(node, ast.Attribute) and node.attr in validation.SOURCE_BOUND_CHECKS:
                assert isinstance(node.value, ast.Name) and node.value.id != "validation", (
                    f"{module}:{node.lineno} calls validation.{node.attr}; use run_in_source")


def test_run_in_source_refuses_a_check_that_is_not_source_bound() -> None:
    with pytest.raises(ValueError, match="not a source-bound"):
        release.run_in_source(TOOLS, "load_policy")


def test_run_in_source_runs_the_check_in_the_given_checkout(monkeypatch, tmp_path) -> None:
    calls = []
    monkeypatch.setattr(release, "run", lambda *args, **kw: calls.append((args, kw)))
    release.run_in_source(tmp_path, "require_asset_policy")
    (args, kw), = calls
    assert kw["root"] == tmp_path and args[-1] == "require_asset_policy" and "from tools import release_validation" in args[2]


def test_prepare_checks_the_asset_policy_after_checking_out_the_source() -> None:
    text = (TOOLS / "release.py").read_text()
    prepare = text[text.index("def prepare("):]
    assert prepare.index('run("git", "checkout", "--detach", source)') < prepare.index('run_in_source(ROOT, "require_asset_policy")')


def test_publish_checks_feeds_in_the_application_checkout() -> None:
    text = (TOOLS / "release.py").read_text()
    assert 'run_in_source(root, "check_feeds")' in text


def test_workflow_steps_on_the_controller_checkout_call_no_source_bound_check() -> None:
    workflow = yaml.safe_load(WORKFLOW.read_text())
    for name in ("prepare", "sync-main", "identity-check"):
        for step in workflow["jobs"][name]["steps"]:
            for check in validation.SOURCE_BOUND_CHECKS:
                assert check not in step.get("run", ""), f"{name} runs {check} against the controller checkout"
    # preflight checks out the source, so its direct call is the source's own
    preflight = workflow["jobs"]["preflight"]["steps"]
    assert preflight[0]["with"]["ref"] == "${{ inputs.source_sha }}"
