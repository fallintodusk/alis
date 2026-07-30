#!/usr/bin/env python3
# License terms: see repository root LICENSE.
"""Validate explicit ALIS license metadata without inferring legal ownership."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tomllib
from pathlib import Path, PurePosixPath


LEGAL_DOC_PATTERN = re.compile(r"\]\((docs/legal/[^)]+\.md)\)")
SOFTWARE_ASSIGNMENT_PATTERN = re.compile(
    r"^\|\s*`([^`]+)`\s*\|\s*[^|]+\|\s*([^|]+?)\s*\|",
    re.MULTILINE,
)
COMPONENT_CLASS_PATTERN = re.compile(
    r"^ALIS-Component-Class:\s*([a-z0-9-]+)\s*$",
    re.MULTILINE,
)
PUBLIC_SOURCE_PATTERN = re.compile(
    r"^ALIS-Public-Source:\s*(included|excluded)\s*$",
    re.MULTILINE,
)
LOCAL_METADATA_NAMES = {"LICENSE", "LICENSE.MD", "NOTICE", "NOTICE.MD"}
NON_POLICY_CLASSES = {"original-terms"}
UNEXPECTED_OWNER_MARKERS = (
    "Copyright Epic Games, Inc.",
    "SPDX-License-Identifier: LicenseRef-",
)


def repository_paths(repo_root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
        check=True,
        capture_output=True,
    )
    return [
        raw.decode("utf-8", errors="surrogateescape").replace("\\", "/")
        for raw in result.stdout.split(b"\0")
        if raw
    ]


def software_assignments(policy: str) -> dict[str, str]:
    return {
        component_class: license_id.strip()
        for component_class, license_id in SOFTWARE_ASSIGNMENT_PATTERN.findall(policy)
    }


def validate_license_texts(repo_root: Path, errors: list[str]) -> None:
    policy = (repo_root / "LICENSE").read_text(encoding="utf-8")
    referenced = set(re.findall(r"\]\((LICENSES/[^)]+\.txt)\)", policy))
    present = {
        path.relative_to(repo_root).as_posix()
        for path in (repo_root / "LICENSES").glob("*.txt")
    }

    if not referenced:
        errors.append("Root LICENSE does not reference any standard license text")
    for rel in sorted(referenced):
        path = repo_root / rel
        if not path.exists() or path.stat().st_size < 1000:
            errors.append(f"Missing or incomplete standard license text: {rel}")
    for rel in sorted(present - referenced):
        errors.append(f"Unreferenced standard license text adds noise: {rel}")


def validate_policy_references(repo_root: Path, errors: list[str]) -> None:
    policy = (repo_root / "LICENSE").read_text(encoding="utf-8")
    referenced = set(LEGAL_DOC_PATTERN.findall(policy))
    if not referenced:
        errors.append("Root LICENSE does not reference its legal implementation docs")
    for rel in sorted(referenced):
        if not (repo_root / rel).is_file():
            errors.append(f"Missing legal implementation document: {rel}")


def explicit_local_declarations(
    repo_root: Path,
    paths: list[str],
    assignments: dict[str, str],
    errors: list[str],
) -> dict[str, dict[str, str]]:
    declarations: dict[str, dict[str, str]] = {}
    allowed_classes = set(assignments) | NON_POLICY_CLASSES
    for rel in paths:
        path = PurePosixPath(rel)
        parent = path.parent.as_posix()
        if (
            path.name.upper() not in LOCAL_METADATA_NAMES
            or parent in {".", "LICENSES"}
        ):
            continue
        text = (repo_root / rel).read_text(encoding="utf-8", errors="replace")
        marker = COMPONENT_CLASS_PATTERN.search(text)
        if marker is None:
            continue
        component_class = marker.group(1)
        if component_class not in allowed_classes:
            errors.append(f"Unknown explicit component class in: {rel}")
            continue
        public_source = PUBLIC_SOURCE_PATTERN.search(text)
        declaration = {
            "class": component_class,
            "public_source": (
                public_source.group(1) if public_source else "included"
            ),
        }
        if parent in declarations and declarations[parent] != declaration:
            errors.append(f"Conflicting explicit legal metadata in: {parent}")
            continue
        declarations[parent] = declaration
    return declarations


def path_is_within(rel: str, root: str) -> bool:
    if root in {"", "."}:
        return True
    return rel == root or rel.startswith(f"{root}/")


def nearest_root(rel: str, roots: set[str]) -> str | None:
    matches = [root for root in roots if path_is_within(rel, root)]
    return max(matches, key=len, default=None)


def unreal_relative_class(relative_path: str) -> str | None:
    path = PurePosixPath(relative_path)
    if path.suffix in {".uproject", ".uplugin"} or path.name == "BuildUnit.yaml":
        return "ue-in-process"
    if not path.parts:
        return None
    first = path.parts[0]
    if first in {"Source", "Config", "Data"}:
        return "ue-in-process"
    if first in {"Content", "Resources"}:
        return "provenance-required"
    if first.lower() == "docs" or path.name.lower().startswith("readme"):
        return "documentation"
    return None


def validate_unexpected_owner_notices(
    repo_root: Path,
    paths: list[str],
    declarations: dict[str, dict[str, str]],
    errors: list[str],
) -> None:
    plugin_roots = {
        PurePosixPath(rel).parent.as_posix()
        for rel in paths
        if PurePosixPath(rel).suffix == ".uplugin"
    }
    project_exists = any(PurePosixPath(rel).suffix == ".uproject" for rel in paths)
    declaration_roots = set(declarations)

    for rel in paths:
        declaration_root = nearest_root(rel, declaration_roots)
        declaration = (
            declarations[declaration_root]
            if declaration_root is not None
            else None
        )
        if declaration is not None and declaration["class"] == "original-terms":
            continue

        if declaration is None:
            plugin_root = nearest_root(rel, plugin_roots)
            if plugin_root is not None:
                relative = PurePosixPath(rel).relative_to(plugin_root).as_posix()
            elif project_exists:
                relative = rel
            else:
                continue
            if unreal_relative_class(relative) != "ue-in-process":
                continue
        elif declaration["class"] != "ue-in-process":
            continue
        raw = (repo_root / rel).read_bytes()[:512]
        if b"\0" in raw:
            continue
        text = raw.decode("utf-8", errors="replace")
        if any(marker in text for marker in UNEXPECTED_OWNER_MARKERS):
            errors.append(
                "Unexpected ownership notice requires explicit review: "
                f"{rel}"
            )


def validate_local_exclusions(
    repo_root: Path,
    declarations: dict[str, dict[str, str]],
    errors: list[str],
) -> None:
    mirror = repo_root / "scripts" / "git" / "mirror" / "mirror.exclude"
    mirror_lines = {
        line.strip()
        for line in mirror.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }
    for component_root, declaration in declarations.items():
        if declaration["public_source"] != "excluded":
            continue
        expected_rule = f"{component_root}/**"
        if expected_rule not in mirror_lines:
            errors.append(
                "Explicit component requires a public mirror exclusion: "
                f"{expected_rule}"
            )


def cargo_component_class(data: dict[str, object]) -> str:
    package = data.get("package", {})
    workspace = data.get("workspace", {})
    package_alis = package.get("metadata", {}).get("alis", {})
    workspace_alis = workspace.get("metadata", {}).get("alis", {})
    return (
        package_alis.get("component-class")
        or workspace_alis.get("component-class")
        or "separate-process"
    )


def nearest_workspace(
    manifest: PurePosixPath,
    workspaces: dict[str, dict[str, object]],
) -> dict[str, object] | None:
    candidates = {
        root: data
        for root, data in workspaces.items()
        if manifest.parent.as_posix() == root
        or manifest.parent.as_posix().startswith(f"{root}/")
    }
    if not candidates:
        return None
    return candidates[max(candidates, key=len)]


def validate_cargo_packages(
    repo_root: Path,
    paths: list[str],
    assignments: dict[str, str],
    errors: list[str],
) -> None:
    manifests: dict[str, dict[str, object]] = {}
    for rel in paths:
        if PurePosixPath(rel).name == "Cargo.toml":
            manifests[rel] = tomllib.loads(
                (repo_root / rel).read_text(encoding="utf-8")
            )
    workspaces = {
        PurePosixPath(rel).parent.as_posix(): data
        for rel, data in manifests.items()
        if "workspace" in data
    }

    for rel, data in manifests.items():
        component_class = cargo_component_class(data)
        expected_license = assignments.get(component_class)
        if expected_license is None:
            errors.append(f"Cargo manifest has unknown component class: {rel}")
            continue

        workspace_package = data.get("workspace", {}).get("package", {})
        if workspace_package:
            actual_license = workspace_package.get("license")
            if actual_license != expected_license:
                errors.append(
                    f"Cargo workspace license drift: {rel}: "
                    f"{actual_license!r} != {expected_license!r}"
                )

        package = data.get("package")
        if package is None:
            continue
        license_entry = package.get("license")
        if isinstance(license_entry, dict) and license_entry.get("workspace") is True:
            workspace = nearest_workspace(PurePosixPath(rel), workspaces)
            actual_license = (
                workspace.get("workspace", {}).get("package", {}).get("license")
                if workspace
                else None
            )
        else:
            actual_license = license_entry
        if actual_license != expected_license:
            errors.append(
                f"Cargo package license drift: {rel}: "
                f"{actual_license!r} != {expected_license!r}"
            )


def validate_repository(
    repo_root: Path,
    paths: list[str] | None = None,
) -> list[str]:
    errors: list[str] = []
    resolved_paths = paths if paths is not None else repository_paths(repo_root)
    policy = (repo_root / "LICENSE").read_text(encoding="utf-8")
    assignments = software_assignments(policy)
    validate_license_texts(repo_root, errors)
    validate_policy_references(repo_root, errors)
    declarations = explicit_local_declarations(
        repo_root,
        resolved_paths,
        assignments,
        errors,
    )
    validate_local_exclusions(repo_root, declarations, errors)
    validate_cargo_packages(repo_root, resolved_paths, assignments, errors)
    validate_unexpected_owner_notices(
        repo_root,
        resolved_paths,
        declarations,
        errors,
    )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[4],
    )
    args = parser.parse_args()
    errors = validate_repository(args.repo_root.resolve())

    if errors:
        print("[X] Licensing validation failed")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(
        "[OK] License texts, legal docs, explicit local declarations, "
        "Cargo metadata, ownership guards, and mirror exclusions are consistent"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
