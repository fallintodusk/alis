from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path, PurePosixPath

from .contract_inputs import is_shared_contract_path
from .profile_inputs import ProfileInputError, executable_profile_inputs


REPO_ROOT = Path(__file__).resolve().parents[4]


class PlanningError(RuntimeError):
    pass


def _under(path: str, root: str) -> bool:
    candidate = PurePosixPath(path)
    try:
        candidate.relative_to(PurePosixPath(root))
        return True
    except ValueError:
        return False


def _generator_input(path: str) -> bool:
    suffix = PurePosixPath(path).suffix.lower()
    fingerprint_suffixes = {".cpp", ".h", ".cs", ".json", ".ps1"}
    if suffix not in fingerprint_suffixes:
        return False
    if _under(path, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor"):
        return not _under(path, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests")
    if _under(path, "Plugins/World/ProjectWorld/Data/Schemas"):
        return True
    if _under(path, "scripts/ue/world"):
        return not _under(path, "scripts/ue/world/test") and path != "scripts/ue/world/audit_generated_authority.ps1"
    return False


def _shipping_boundary(path: str) -> bool:
    if path in {"Alis.uproject", "Config/DefaultGame.ini", "Config/DefaultEngine.ini"}:
        return True
    if path.endswith(".uplugin") or _under(path, "scripts/ue/package"):
        return True
    if path in {
        "tools/World/EndToEndValidation/app/acceptance.py",
        "tools/World/EndToEndValidation/app/package_gate.py",
        "tools/World/EndToEndValidation/app/presentation.py",
    }:
        return True
    if _under(path, "Plugins/World/ProjectWorld/Source/ProjectWorld"):
        return True
    return Path(path).name in {
        "ProjectWorldGeneratedActorLifecycle.cpp",
        "ProjectWorldRuntimeNavigation.cpp",
        "ProjectWorldRuntimeNavigation.h",
        "ProjectWorldRuntimeRealization.cpp",
    }


def _matrix_label(profile_id: str) -> str:
    return "representative" if profile_id == "representative_v1" else profile_id


def _matrix_order(profile_id: str) -> tuple[int, str]:
    return ({"p0": 0, "representative": 1}.get(profile_id, 2), profile_id)


def plan_for_paths(paths: list[str], repo_root: Path = REPO_ROOT) -> dict[str, object]:
    changed = sorted({path.replace("\\", "/").lstrip("./") for path in paths if path})
    profile_inputs = {
        _matrix_label(profile_id): inputs
        for profile_id, inputs in executable_profile_inputs(repo_root).items()
    }
    all_profile_inputs = set().union(*profile_inputs.values()) if profile_inputs else set()
    shared_changed = any(
        is_shared_contract_path(path) and path not in all_profile_inputs
        for path in changed
    )
    generator_changed = any(_generator_input(path) for path in changed)
    l1_required = any(
        generator_changed
        or _under(path, "tools/World/SourceIngestion")
        or _under(path, "tools/World/CanonicalCompilation")
        or _under(path, "tools/World/EndToEndValidation/app")
        or _under(path, "Plugins/World/ProjectWorldTestData/Data")
        for path in changed
    )

    matrices: set[str] = set()
    if shared_changed:
        matrices.update(profile_inputs)
    for profile_id, inputs in profile_inputs.items():
        if any(path in inputs for path in changed):
            matrices.add(profile_id)

    candidates: set[str] = set()
    if generator_changed:
        candidates.add("ProjectWorldData")
    for path in changed:
        if _under(path, "Plugins/World/ProjectWorldData/Content/Generated") or _under(
            path, "Plugins/World/ProjectWorldData/Data/Manifests"
        ) or _under(
            path, "Plugins/World/ProjectWorldData/Content/__ExternalActors__/Generated"
        ) or _under(
            path, "Plugins/World/ProjectWorldData/Content/__ExternalObjects__/Generated"
        ):
            candidates.add("ProjectWorldData")

    l4_required = any(_shipping_boundary(path) for path in changed)
    return {
        "changed_path_count": len(changed),
        "inner_loop": "L0",
        "l1_required": l1_required,
        "l1_reason": "cross-component world seam changed" if l1_required else "none",
        "l2_matrices": sorted(matrices, key=_matrix_order),
        "l2_reason": (
            "authenticated shared inputs changed" if shared_changed
            else "profile-specific inputs changed" if matrices
            else "none"
        ),
        "l3_candidate_owners": sorted(candidates),
        "l4_required": l4_required,
        "l4_reason": "cook, IoStore, shipping, or packaged-runtime boundary changed" if l4_required else "none",
    }


def _git_output(repo_root: Path, arguments: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", *arguments], cwd=repo_root, capture_output=True, check=False
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise PlanningError(detail or "Git could not inspect changed paths")
    return [value for value in result.stdout.decode("utf-8").split("\0") if value]


def changed_paths(base: str, repo_root: Path = REPO_ROOT) -> list[str]:
    revision = _git_output(repo_root, ["rev-parse", "--verify", "--end-of-options", f"{base}^{{commit}}"])
    if len(revision) != 1:
        raise PlanningError(f"Git base did not resolve to one commit: {base}")
    tracked = _git_output(repo_root, ["diff", "--name-only", "-z", revision[0].strip(), "--"])
    untracked = _git_output(repo_root, ["ls-files", "--others", "--exclude-standard", "-z"])
    return sorted(set(tracked + untracked))


def format_plan(plan: dict[str, object], base: str) -> str:
    matrices = ", ".join(plan["l2_matrices"]) or "none"
    owners = ", ".join(plan["l3_candidate_owners"]) or "none"
    return "\n".join((
        f"World pipeline plan from {base} ({plan['changed_path_count']} changed paths)",
        f"Inner loop: {plan['inner_loop']}",
        f"L1 required: {'yes' if plan['l1_required'] else 'no'} - {plan['l1_reason']}",
        f"L2 matrices: {matrices} - {plan['l2_reason']}",
        f"L3 candidate owners: {owners}",
        f"L4 required: {'yes' if plan['l4_required'] else 'no'} - {plan['l4_reason']}",
    ))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Plan world pipeline gates without running them")
    parser.add_argument("--base", default="HEAD")
    args = parser.parse_args(argv)
    try:
        print(format_plan(plan_for_paths(changed_paths(args.base)), args.base))
        return 0
    except (PlanningError, ProfileInputError) as error:
        print(f"World pipeline plan failed: {error}", file=sys.stderr)
        return 2
