from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path, PurePosixPath

from .contract_inputs import COMMON_TEST_PROFILE_IDS, declared_input_files, is_shared_contract_path
from .canonical_mode import upstream_replay_required
from .producer_descriptors import (
    ProducerDescriptor,
    ProducerDescriptorError,
    load_producer_descriptors,
    recorded_verify,
)
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


def _module_of(path: str, repo_root: Path) -> str | None:
    """Nearest enclosing UBT module, by the <Module>/<Module>.Build.cs layout."""
    for parent in PurePosixPath(path).parents:
        if parent.name and (repo_root / parent / f"{parent.name}.Build.cs").is_file():
            return parent.name
    return None


def _verify_selection(
    proof_inputs: list[str],
    descriptors: tuple[ProducerDescriptor, ...],
    repo_root: Path,
) -> dict[str, object]:
    """Route each proof input through the owning producer declaration."""
    owners: dict[str, list[ProducerDescriptor]] = {}
    for descriptor in descriptors:
        for module in descriptor.modules:
            owners.setdefault(module, []).append(descriptor)
    pipeline_modules = {
        module for descriptor in descriptors if descriptor.is_pipeline for module in descriptor.modules
    }
    rules = []
    selected: dict[str, ProducerDescriptor] = {}
    durable_paths: list[str] = []
    for path in proof_inputs:
        declared = [descriptor for descriptor in descriptors if descriptor.path == path]
        data_owners = [descriptor for descriptor in descriptors if path in descriptor.data_inputs]
        if declared:
            producers = list(descriptors) if declared[0].is_pipeline else declared
            rule = "pipeline descriptor" if declared[0].is_pipeline else "producer descriptor"
            durable_paths.append(path)
        elif data_owners:
            producers = data_owners
            rule = "declared data input"
            durable_paths.append(path)
        elif path == "scripts/config/ue_path.conf":
            producers = list(descriptors)
            rule = "engine configuration"
            durable_paths.append(path)
        else:
            module = _module_of(path, repo_root)
            named = owners.get(module, []) if module else []
            if module in pipeline_modules:
                rule, producers = f"shared module {module}", list(descriptors)
            elif len(named) == 1 and not named[0].is_pipeline:
                rule, producers = f"producer module {module}", named
            else:
                rule, producers = (f"ambiguous module {module}" if named else "proof input"), list(descriptors)
        selected.update((descriptor.key, descriptor) for descriptor in producers)
        rules.append({
            "path": path,
            "rule": rule,
            "producers": sorted(descriptor.label for descriptor in producers),
        })
    recorded = {descriptor.label: recorded_verify(repo_root, descriptor) for descriptor in selected.values()}
    return {
        "verifies": sorted(verify for verify in recorded.values() if verify),
        "unrecorded_verifies": sorted(label for label, verify in recorded.items() if not verify),
        "verify_rules": rules,
        "durable_identity_paths": durable_paths,
    }


def _matrix_label(profile_id: str) -> str:
    return profile_id


def _matrix_order(profile_id: str) -> tuple[int, str]:
    return (0, profile_id)


def plan_for_paths(paths: list[str], repo_root: Path = REPO_ROOT, base: str | None = None) -> dict[str, object]:
    changed = sorted({path.replace("\\", "/").lstrip("./") for path in paths if path})
    executable_inputs = executable_profile_inputs(repo_root, COMMON_TEST_PROFILE_IDS)
    profile_inputs = {
        _matrix_label(profile_id): inputs
        for profile_id, inputs in executable_inputs.items()
    }
    all_profile_inputs = set().union(*profile_inputs.values()) if profile_inputs else set()
    declared_inputs = declared_input_files(repo_root, executable_inputs)
    proof_inputs = [
        path for path in changed
        if is_shared_contract_path(path) or path in declared_inputs or path == "scripts/config/ue_path.conf"
    ]
    shared_changed = any(path not in all_profile_inputs for path in proof_inputs)
    selection = _verify_selection(proof_inputs, load_producer_descriptors(repo_root), repo_root)
    l1_required = any(
        path in proof_inputs
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
        if any(f"/Data/Canonical/{profile_id}/" in f"/{path}" for path in changed):
            matrices.add(profile_id)

    candidates: set[str] = set()
    if selection["durable_identity_paths"]:
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
    matrix_mode = "full" if upstream_replay_required(changed, base, repo_root) else "canonical_authority"
    return {
        "changed_path_count": len(changed),
        "inner_loop": "L0",
        "l1_required": l1_required,
        "l1_reason": "cross-component world seam changed" if l1_required else "none",
        "l2_matrices": sorted(matrices, key=_matrix_order),
        "l2_matrix_modes": {profile_id: matrix_mode for profile_id in matrices},
        "l2_reason": (
            "authenticated shared inputs changed" if shared_changed
            else "profile-specific inputs changed" if matrices
            else "none"
        ),
        "l3_candidate_owners": sorted(candidates),
        "l4_required": l4_required,
        "l4_reason": "cook, IoStore, shipping, or packaged-runtime boundary changed" if l4_required else "none",
        **selection,
    }


def _release_proof_only(path: str) -> bool:
    if PurePosixPath(path).suffix.lower() in {".md", ".dsl"}:
        return True
    if any(_under(path, root) for root in (
        "docs", "todo", "scripts/ue/world/test", "scripts/ue/package/tests", "scripts/ue/test",
    )):
        return True
    parts = PurePosixPath(path).parts
    if len(parts) >= 4 and parts[:2] == ("tools", "World") and parts[3] == "tests":
        return True
    return path == "scripts/ue/package/run_release_preflight.ps1"


def plan_release_impact(
    paths: list[str], repo_root: Path = REPO_ROOT, base: str | None = None,
) -> dict[str, object] | None:
    """Release projection of the planner; proof checks do not imply generation.

    This changes neither the common Check input identity nor the ordinary planner.
    Unknown/non-documentation paths remain visible to its authoritative decisions.
    """
    changed = sorted({path.replace("\\", "/").lstrip("./") for path in paths if path})
    production = [path for path in changed if not _release_proof_only(path)]
    return plan_for_paths(production, repo_root, base) if production else None


def _git_output(repo_root: Path, arguments: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", *arguments], cwd=repo_root, capture_output=True, check=False
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise PlanningError(detail or "Git could not inspect changed paths")
    return [value for value in result.stdout.decode("utf-8").split("\0") if value]


def changed_path_snapshot(base: str, repo_root: Path = REPO_ROOT) -> dict[str, object]:
    revision = _git_output(repo_root, ["rev-parse", "--verify", "--end-of-options", f"{base}^{{commit}}"])
    if len(revision) != 1:
        raise PlanningError(f"Git base did not resolve to one commit: {base}")
    tracked = _git_output(repo_root, ["diff", "--no-renames", "--name-only", "-z", revision[0].strip(), "--"])
    untracked = _git_output(repo_root, ["ls-files", "--others", "--exclude-standard", "-z", "--"])
    return {"requested_base": base, "base": revision[0].strip(),
            "changed_paths": sorted(set(tracked + untracked))}


def changed_paths(base: str, repo_root: Path = REPO_ROOT) -> list[str]:
    return changed_path_snapshot(base, repo_root)["changed_paths"]


def format_plan(plan: dict[str, object], base: str) -> str:
    matrices = ", ".join(
        f"{name} ({plan['l2_matrix_modes'][name]})" for name in plan["l2_matrices"]
    ) or "none"
    owners = ", ".join(plan["l3_candidate_owners"]) or "none"
    verifies = ", ".join(plan["verifies"]) or "none"
    unrecorded = ", ".join(plan["unrecorded_verifies"]) or "none"
    return "\n".join((
        f"World pipeline plan from {base} ({plan['changed_path_count']} changed paths)",
        f"Inner loop: {plan['inner_loop']}",
        f"L1 required: {'yes' if plan['l1_required'] else 'no'} - {plan['l1_reason']}",
        f"L2 matrices: {matrices} - {plan['l2_reason']}",
        f"L3 candidate owners: {owners}",
        f"L4 required: {'yes' if plan['l4_required'] else 'no'} - {plan['l4_reason']}",
        f"Verifies: {verifies}",
        f"Selected producers without a recorded verify baseline: {unrecorded}",
        "Verify selection:",
        *(
            f"  {rule['path']}: {rule['rule']} -> "
            + (rule["producers"][0] if len(rule["producers"]) == 1 else "every verify")
            for rule in plan["verify_rules"]
        ),
    ))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Plan world pipeline gates without running them")
    parser.add_argument("--base", default="HEAD")
    args = parser.parse_args(argv)
    try:
        print(format_plan(plan_for_paths(changed_paths(args.base), base=args.base), args.base))
        return 0
    except (PlanningError, ProfileInputError, ProducerDescriptorError) as error:
        print(f"World pipeline plan failed: {error}", file=sys.stderr)
        return 2
