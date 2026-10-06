# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.
"""World-owned release impact adapter; planning never executes World gates."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.planning import changed_path_snapshot, plan_release_impact


def is_toolkit_schema(path: str) -> bool:
    return re.match(r"^tools/World/[^/]+/contracts/.*\.schema\.json$", path) is not None


def world_owners(paths: list[str]) -> list[str]:
    owners = set()
    for path in paths:
        path = path.replace("\\", "/")
        if path.endswith((".md", ".dsl", ".txt")):
            continue
        if (re.match(r"^Plugins/World/[^/]+/(Source/|[^/]+\.uplugin$|Content/)", path)
                or re.match(r"^scripts/ue/world/test/(integration/.*uncooked|performance/(run_.*|project_world_product_route_arguments\.ps1))", path)):
            owners.update(("WorldRuntime", "PerformanceEvidence"))
        if re.match(r"^scripts/ue/world/test/(.*performance.*|.*manhattan.*|.*uncooked.*)", path):
            owners.add("PerformanceEvidence")
        if path.startswith("scripts/ue/world/test/") and Path(path).name in {
            "release_preflight_plan.py", "validate_release_contracts.py", "test_release_preflight_plan.py"
        }:
            owners.add("WorldContracts")
        if path in {"tools/World/EndToEndValidation/app/planning.py",
                    "tools/World/EndToEndValidation/tests/test_planning.py"}:
            owners.add("WorldContracts")
        if not path.endswith(".json"):
            continue
        if is_toolkit_schema(path):
            owners.add("WorldContracts")
        if re.match(r"^Plugins/World/[^/]+/Data/(Profiles|Runtime|Presentation|Authored|Schemas)/", path):
            owners.add("WorldContracts")
            if re.match(r"^Plugins/World/[^/]+/Data/(Runtime|Presentation|Authored|Profiles/Realization)/", path):
                owners.add("Projection")
            if re.match(r"^Plugins/World/[^/]+/Data/Runtime/", path):
                owners.update(("WorldRuntime", "PerformanceEvidence"))
            if "/Schemas/" in path and any(word in Path(path).name for word in ("realization", "runtime", "presentation", "authored")):
                owners.add("Projection")
    return sorted(owners)


def release_world_plan(base: str, repo_root: Path = REPO_ROOT) -> dict[str, object]:
    snapshot = changed_path_snapshot(base, repo_root)
    paths = snapshot["changed_paths"]
    world_plan = plan_release_impact(paths, repo_root, snapshot["base"])
    owners = set(world_owners(paths))
    if world_plan and world_plan["l1_required"]:
        owners.add("WorldContracts")
    return {**snapshot, "owners": sorted(owners), "world_plan": world_plan,
            "world_gates_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="HEAD")
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    args = parser.parse_args()
    try:
        print(json.dumps(release_world_plan(args.base, args.repo_root)))
        return 0
    except (RuntimeError, OSError, ValueError) as error:
        print(f"World release impact planning failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
