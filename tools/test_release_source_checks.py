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
SOURCE_CONSTANTS = (
    "packs.data_pack", "profile_revision", "PROFILE_FEED", "DEVICE_PROFILE_DOMAINS", "benchmark_catalog",
    "ENVELOPE_LIMITS", "XCODE_BUILDS", "SDK_VERSION", "DATA_PACK_PUBLIC_KEY", "data_pack_definitions",
)
CONTROLLER_MODULES = ("release_validation.py", "release.py", "release_unsigned.py", "release_macos.py")
# Every controller function that reads one of them, with why the controller's copy is the right one. A new reader fails
# the test until it is judged: run it through `run_in_source` (add it to SOURCE_BOUND_CHECKS) or give its reason here.
JUDGED = {
    "release_validation.py": {
        "require_asset_policy": "source bound: run through run_in_source",
        "check_feeds": "source bound: run through run_in_source",
        "profile_feed_domain": "helper of require_asset_policy and verify_feed",
        "verify_feed": "helper of check_feeds",
        "_verify_feed": "helper of verify_feed",
        "load_policy": "checks the policy's field set and URL grammar; preflight repeats it with the source's copy",
        "toolchain": "the controller owns the accepted Xcode builds the runner may use",
        "inspect_build": "the controller judges the built artifacts against the accepted Xcode builds",
        "validate_recording_summary_context": "catalog facts the preflight emitted; the controller accepts a superset",
    },
    "release.py": {"archive_identity": "the controller owns the accepted Xcode builds"},
    "release_unsigned.py": {},
    "release_macos.py": {},
}
HELPERS = ("profile_feed_domain", "verify_feed", "_verify_feed")
# Functions that call a source-bound check or helper without reading a constant themselves. The controller must not
# name them either, or the check runs against the controller's constants by another route.
CALLERS = {
    "check_release": "runs in the preflight job, inside the source checkout",
    "verify_pages_recording_feed": "judges main's own published feeds against main's constants",
}


def functions(path: Path) -> dict[str, str]:
    text = path.read_text()
    return {n.name: ast.get_source_segment(text, n) for n in ast.parse(text).body if isinstance(n, ast.FunctionDef)}


@pytest.mark.parametrize("module", CONTROLLER_MODULES)
def test_every_controller_function_reading_a_source_constant_has_been_judged(module: str) -> None:
    path = TOOLS / module
    if not path.exists():
        pytest.skip(f"{module} does not exist")
    reading = {name for name, text in functions(path).items() if any(c in text for c in SOURCE_CONSTANTS)}
    judged = set(JUDGED[module])
    assert reading == judged, (
        f"unjudged readers {sorted(reading - judged)}; stale entries {sorted(judged - reading)}; "
        "route a new reader through run_in_source or give its reason in JUDGED")


def test_every_function_calling_a_source_bound_check_is_a_judged_caller() -> None:
    bound = set(validation.SOURCE_BOUND_CHECKS) | set(HELPERS)
    text = (TOOLS / "release_validation.py").read_text()
    callers = set()
    for node in ast.parse(text).body:
        if isinstance(node, ast.FunctionDef) and node.name not in bound:
            used = {n.attr if isinstance(n, ast.Attribute) else n.id
                    for n in ast.walk(node) if isinstance(n, (ast.Name, ast.Attribute))}
            if used & bound:
                callers.add(node.name)
    assert callers == set(CALLERS), f"unjudged callers {sorted(callers - set(CALLERS))}; stale {sorted(set(CALLERS) - callers)}"


def test_source_bound_checks_are_judged_as_such() -> None:
    for check in validation.SOURCE_BOUND_CHECKS:
        assert JUDGED["release_validation.py"][check].startswith("source bound")


def test_the_controller_never_names_a_source_bound_check_or_its_helpers_outside_validation() -> None:
    names = set(validation.SOURCE_BOUND_CHECKS) | set(HELPERS) | set(CALLERS)
    for module in CONTROLLER_MODULES:
        if module == "release_validation.py" or not (TOOLS / module).exists():
            continue
        for node in ast.walk(ast.parse((TOOLS / module).read_text())):
            name = node.attr if isinstance(node, ast.Attribute) else node.id if isinstance(node, ast.Name) else None
            assert name not in names, f"{module}:{node.lineno} names {name}; use run_in_source"
            if isinstance(node, ast.ImportFrom):
                assert not {a.name for a in node.names} & names, f"{module}:{node.lineno} imports a source-bound check"


def test_run_in_source_refuses_a_check_that_is_not_source_bound() -> None:
    with pytest.raises(ValueError, match="not a source-bound"):
        release.run_in_source(TOOLS, "load_policy")


def test_run_in_source_runs_the_check_in_the_given_checkout(monkeypatch, tmp_path) -> None:
    calls = []
    monkeypatch.setenv("RELEASE_BUILD_POLICY", "{}")
    monkeypatch.setenv("ANTHROPIC_WORKSPACE_ID", "secret")
    monkeypatch.setattr(release, "run", lambda *args, **kw: calls.append((args, kw)))
    release.run_in_source(tmp_path, "require_asset_policy")
    (args, kw), = calls
    assert kw["root"] == tmp_path and args[-1] == "require_asset_policy" and "-I" in args
    assert "RELEASE_BUILD_POLICY" in kw["env"] and "ANTHROPIC_WORKSPACE_ID" not in kw["env"]


def test_run_in_source_loads_the_given_checkouts_tools_not_the_controllers(tmp_path, monkeypatch) -> None:
    (tmp_path / "tools").mkdir()
    (tmp_path / "tools/release_validation.py").write_text(
        "from pathlib import Path\n"
        "def load_policy(value):\n    return {'seen': value}\n"
        "def require_asset_policy(policy):\n    Path('ran').write_text(policy['seen'])\n"
    )
    monkeypatch.setenv("RELEASE_BUILD_POLICY", "the-policy")
    monkeypatch.setenv("PYTHONPATH", str(TOOLS.parent))
    release.run_in_source(tmp_path, "require_asset_policy")
    assert (tmp_path / "ran").read_text() == "the-policy"


def test_prepare_checks_the_asset_policy_after_checking_out_the_source() -> None:
    text = (TOOLS / "release.py").read_text()
    prepare = text[text.index("def prepare("):]
    assert prepare.index('run("git", "checkout", "--detach", source)') < prepare.index('run_in_source(ROOT, "require_asset_policy")')


def test_publish_checks_feeds_in_the_application_checkout() -> None:
    text = (TOOLS / "release.py").read_text()
    assert 'run_in_source(root, "check_feeds")' in text


def checkout_ref(job: dict) -> str | None:
    for step in job["steps"]:
        if str(step.get("uses", "")).startswith("actions/checkout"):
            return step.get("with", {}).get("ref", "")
    return None


def test_only_jobs_that_check_out_the_source_call_a_source_bound_check_in_a_step() -> None:
    workflow = yaml.safe_load(WORKFLOW.read_text())
    for name, job in workflow["jobs"].items():
        ref = checkout_ref(job)
        for step in job["steps"]:
            called = [c for c in validation.SOURCE_BOUND_CHECKS if c in step.get("run", "")]
            if called:
                assert ref == "${{ inputs.source_sha }}", f"{name} runs {called} against a checkout that is not the source"


def test_the_probe_guard_script_accepts_only_the_pinned_compiler() -> None:
    import json
    import os
    import subprocess

    workflow = yaml.safe_load(WORKFLOW.read_text())
    step = next(s for s in workflow["jobs"]["probe"]["steps"] if s.get("name") == "Require the policy to name the pinned compiler")
    pinned = workflow["env"]["RELEASE_COMPILER_VERSION"]

    def passes(policy: str) -> bool:
        env = {**os.environ, "RELEASE_BUILD_POLICY": policy, "RELEASE_COMPILER_VERSION": pinned}
        return subprocess.run(["bash", "-e", "-c", step["run"]], env=env, capture_output=True).returncode == 0

    assert passes(json.dumps({"guest_compiler_version": pinned}))
    assert not passes(json.dumps({"guest_compiler_version": pinned + "1"}))
    assert not passes("{}") and not passes("") and not passes("[]")
