"""Every job of the release workflow that builds the guest installs the same pinned toolchain.

The guest's hash depends on the compiler, and Homebrew serves different llvm builds on different runners, so the probe,
the unsigned build and the TestFlight build must install from one fixed homebrew-core commit and stop unless the compiler
is the pinned one. Otherwise the hash the reviewed policy names can differ from what a later job ships."""

import re
from pathlib import Path

import pytest
import yaml

WORKFLOW = Path(__file__).resolve().parent.parent / ".github/workflows/release.yml"
BUILDING_JOBS = ("probe", "unsigned", "testflight")
STEP_NAME = "Install the pinned build toolchain"


@pytest.fixture(scope="module")
def workflow() -> dict:
    return yaml.safe_load(WORKFLOW.read_text())


def install_steps(job: dict) -> list[dict]:
    return [step for step in job["steps"] if step.get("name") == STEP_NAME]


def test_the_workflow_defines_one_pin_and_one_compiler(workflow: dict) -> None:
    env = workflow["env"]
    assert re.fullmatch(r"[0-9a-f]{40}", env["RELEASE_HOMEBREW_CORE_PIN"])
    assert re.fullmatch(r"Homebrew clang version \d+\.\d+\.\d+", env["RELEASE_COMPILER_VERSION"])


@pytest.mark.parametrize("name", BUILDING_JOBS)
def test_each_building_job_installs_the_pinned_toolchain_once(workflow: dict, name: str) -> None:
    assert len(install_steps(workflow["jobs"][name])) == 1


def test_the_three_jobs_run_the_same_install_script(workflow: dict) -> None:
    scripts = {install_steps(workflow["jobs"][name])[0]["run"] for name in BUILDING_JOBS}
    assert len(scripts) == 1


def test_the_install_script_uses_the_pin_and_checks_the_compiler(workflow: dict) -> None:
    script = install_steps(workflow["jobs"]["probe"])[0]["run"]
    assert "$RELEASE_HOMEBREW_CORE_PIN" in script and "$RELEASE_COMPILER_VERSION" in script
    # no literal commit or version in the script, so the workflow's env is the only place to change
    assert not re.search(r"[0-9a-f]{40}|clang version \d", script)
    assert script.index('checkout --quiet --force "$RELEASE_HOMEBREW_CORE_PIN"') < script.index("brew install")
    assert "HOMEBREW_NO_AUTO_UPDATE=1" in script and "HOMEBREW_NO_INSTALL_FROM_API=1" in script
    # a toolchain already on the runner must not survive into the build
    assert script.index("brew uninstall --force --ignore-dependencies llvm lld") < script.index("brew install")


def test_the_install_comes_before_anything_that_builds(workflow: dict) -> None:
    builders = {"probe": "Build unsigned optimized tester provenance without publishing",
                "unsigned": "Build and check the unsigned apps from the tag"}
    for job, builder in builders.items():
        names = [step.get("name") for step in workflow["jobs"][job]["steps"]]
        assert names.index(STEP_NAME) < names.index(builder)


def test_no_other_step_installs_homebrew_packages(workflow: dict) -> None:
    for job in workflow["jobs"].values():
        for step in job["steps"]:
            if step.get("name") != STEP_NAME:
                assert "brew install" not in step.get("run", "")


def test_formulas_already_on_the_runner_are_skipped_not_reinstalled(workflow: dict) -> None:
    script = install_steps(workflow["jobs"]["probe"])[0]["run"]
    assert 'brew list --formula --versions "$formula"' in script and 'brew install "$formula"' in script
    assert "brew uninstall --force --ignore-dependencies llvm lld\n" in script
    assert script.index("brew uninstall") < script.index("for formula in")


def test_probe_requires_the_policy_to_name_the_pinned_compiler_before_installing_anything(workflow: dict) -> None:
    steps = workflow["jobs"]["probe"]["steps"]
    names = [step.get("name") for step in steps]
    guard = steps[names.index("Require the policy to name the pinned compiler")]
    assert names.index("Require the policy to name the pinned compiler") < names.index(STEP_NAME)
    assert "RELEASE_COMPILER_VERSION" in guard["run"] and "guest_compiler_version" in guard["run"]
