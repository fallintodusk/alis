#!/usr/bin/env python3
# License terms: see repository root LICENSE.
"""Generate a reproducible component-license manifest from one tagged commit."""

from __future__ import annotations

import argparse
import ast
import json
import subprocess
import sys
import tomllib
from collections import Counter
from pathlib import Path, PurePosixPath

from validate_licensing import (
    COMPONENT_CLASS_PATTERN,
    LOCAL_METADATA_NAMES,
    PUBLIC_SOURCE_PATTERN,
    UNEXPECTED_OWNER_MARKERS,
    cargo_component_class,
    nearest_root,
    software_assignments,
    unreal_relative_class,
    validate_repository,
)


class ManifestError(RuntimeError):
    pass


def git(repo_root: Path, *args: str, text: bool = True) -> str | bytes:
    result = subprocess.run(
        ["git", *args],
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=text,
    )
    return result.stdout


def resolve_tagged_head(repo_root: Path, tag: str) -> str:
    if git(repo_root, "status", "--porcelain").strip():
        raise ManifestError("Source repository must be clean")
    head = git(repo_root, "rev-parse", "HEAD").strip()
    try:
        tagged = git(repo_root, "rev-parse", f"refs/tags/{tag}^{{commit}}").strip()
    except subprocess.CalledProcessError as error:
        raise ManifestError(f"Release tag does not exist: {tag}") from error
    if head != tagged:
        raise ManifestError(f"Release tag {tag} does not identify HEAD")
    return head


def tree_entries(repo_root: Path, commit: str) -> dict[str, str]:
    raw = git(
        repo_root,
        "ls-tree",
        "-r",
        "--full-tree",
        "-z",
        commit,
        text=False,
    )
    entries: dict[str, str] = {}
    for record in raw.split(b"\0"):
        if not record:
            continue
        metadata, encoded_path = record.split(b"\t", 1)
        _mode, object_type, object_id = metadata.decode("ascii").split()
        if object_type != "blob":
            continue
        path = encoded_path.decode("utf-8", errors="surrogateescape")
        entries[path.replace("\\", "/")] = object_id
    return entries


def read_blob(repo_root: Path, commit: str, path: str) -> bytes:
    return git(repo_root, "show", f"{commit}:{path}", text=False)


def imports_unreal(source: bytes) -> bool:
    try:
        tree = ast.parse(source.decode("utf-8"))
    except (SyntaxError, UnicodeDecodeError):
        return False
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name == "unreal" for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module == "unreal":
            return True
    return False


def local_declarations(
    repo_root: Path,
    commit: str,
    paths: set[str],
    assignments: dict[str, str],
) -> dict[str, dict[str, str]]:
    declarations: dict[str, dict[str, str]] = {}
    allowed = set(assignments) | {"original-terms"}
    for rel in sorted(paths):
        path = PurePosixPath(rel)
        if path.name.upper() not in LOCAL_METADATA_NAMES:
            continue
        if path.parent.as_posix() in {".", "LICENSES"}:
            continue
        text = read_blob(repo_root, commit, rel).decode("utf-8", errors="replace")
        marker = COMPONENT_CLASS_PATTERN.search(text)
        if marker is None:
            continue
        component_class = marker.group(1)
        if component_class not in allowed:
            raise ManifestError(f"Unknown component class in {rel}")
        public_marker = PUBLIC_SOURCE_PATTERN.search(text)
        public_source = public_marker.group(1) if public_marker else "included"
        if public_source == "excluded":
            raise ManifestError(f"Excluded component survived public tree: {rel}")
        parent = path.parent.as_posix()
        declaration = {"class": component_class, "evidence": rel}
        existing = declarations.get(parent)
        if existing is not None and existing["class"] != component_class:
            raise ManifestError(f"Conflicting component declarations in {parent}")
        if existing is None:
            declarations[parent] = declaration
    return declarations


def is_documentation(path: PurePosixPath) -> bool:
    lowered_parts = {part.lower() for part in path.parts}
    return (
        path.name.lower().startswith("readme")
        or path.parts[0] in {"docs", "todo"}
        or "docs" in lowered_parts
        or path.suffix.lower() in {".md", ".markdown", ".dsl"}
    )


def component_entry(
    path: str,
    blob: str,
    boundary: str,
    component_class: str,
    assignments: dict[str, str],
    evidence: str | None = None,
) -> dict[str, object]:
    license_id = assignments.get(component_class)
    if component_class == "original-terms":
        license_id = "original-terms"
    if license_id is None:
        raise ManifestError(f"No root license assignment for {component_class}: {path}")
    entry: dict[str, object] = {
        "path": path,
        "blob": blob,
        "kind": "component",
        "boundary": boundary,
        "component_class": component_class,
        "license": license_id,
    }
    if evidence:
        entry["evidence"] = evidence
    return entry


def classify_path(
    path_text: str,
    blob: str,
    assignments: dict[str, str],
    plugin_roots: set[str],
    cargo_roots: dict[str, str],
    root_cargo_class: str | None,
    declarations: dict[str, dict[str, str]],
) -> dict[str, object]:
    path = PurePosixPath(path_text)
    if (
        path_text == "LICENSE"
        or path.parts[0] == "LICENSES"
        or path.name.upper() in LOCAL_METADATA_NAMES
    ):
        return {
            "path": path_text,
            "blob": blob,
            "kind": "legal-evidence",
        }

    declaration_root = nearest_root(path_text, set(declarations))
    declaration = (
        declarations[declaration_root]
        if declaration_root is not None
        else None
    )
    if declaration is not None and declaration["class"] == "original-terms":
        return component_entry(
            path_text,
            blob,
            declaration_root,
            declaration["class"],
            assignments,
            declaration["evidence"],
        )

    if is_documentation(path):
        boundary = path.parts[0] if len(path.parts) > 1 else "."
        return component_entry(
            path_text,
            blob,
            boundary,
            "documentation",
            assignments,
        )

    if declaration is not None:
        return component_entry(
            path_text,
            blob,
            declaration_root,
            declaration["class"],
            assignments,
            declaration["evidence"],
        )

    cargo_root = nearest_root(path_text, set(cargo_roots))
    if cargo_root is not None:
        return component_entry(
            path_text,
            blob,
            cargo_root,
            cargo_roots[cargo_root],
            assignments,
        )

    plugin_root = nearest_root(path_text, plugin_roots)
    if plugin_root is not None:
        relative = path.relative_to(plugin_root).as_posix()
        component_class = unreal_relative_class(relative)
        if component_class == "provenance-required":
            raise ManifestError(
                "Tagged public asset provenance is not yet supported: "
                f"{path_text}"
            )
        if component_class is None:
            raise ManifestError(f"Unknown Unreal component path: {path_text}")
        return component_entry(
            path_text,
            blob,
            plugin_root,
            component_class,
            assignments,
        )

    project_class = unreal_relative_class(path_text)
    if project_class == "provenance-required":
        raise ManifestError(
            "Tagged public asset provenance is not yet supported: "
            f"{path_text}"
        )
    if project_class is not None:
        return component_entry(
            path_text,
            blob,
            ".",
            project_class,
            assignments,
        )

    if root_cargo_class is not None and path_text in {"Cargo.toml", "Cargo.lock"}:
        return component_entry(
            path_text,
            blob,
            ".",
            root_cargo_class,
            assignments,
        )

    if len(path.parts) == 1:
        return component_entry(
            path_text,
            blob,
            ".",
            "separate-process",
            assignments,
        )

    raise ManifestError(f"Unknown component boundary: {path_text}")


def cargo_roots(
    repo_root: Path,
    commit: str,
    paths: set[str],
) -> dict[str, str]:
    result: dict[str, str] = {}
    for rel in sorted(paths):
        path = PurePosixPath(rel)
        if path.name != "Cargo.toml":
            continue
        data = tomllib.loads(
            read_blob(repo_root, commit, rel).decode("utf-8")
        )
        root = path.parent.as_posix()
        if root != ".":
            result[root] = cargo_component_class(data)
    return result


def root_cargo_class(
    repo_root: Path,
    commit: str,
    paths: set[str],
) -> str | None:
    if "Cargo.toml" not in paths:
        return None
    data = tomllib.loads(
        read_blob(repo_root, commit, "Cargo.toml").decode("utf-8")
    )
    return cargo_component_class(data)


def generate_manifest(repo_root: Path, tag: str) -> dict[str, object]:
    commit = resolve_tagged_head(repo_root, tag)
    entries_by_path = tree_entries(repo_root, commit)
    paths = set(entries_by_path)
    if "LICENSE" not in paths:
        raise ManifestError("Tagged source tree has no root LICENSE")
    validation_errors = validate_repository(repo_root, sorted(paths))
    if validation_errors:
        details = "; ".join(validation_errors)
        raise ManifestError(f"Tagged tree failed licensing validation: {details}")
    policy = read_blob(repo_root, commit, "LICENSE").decode("utf-8")
    assignments = software_assignments(policy)
    declarations = local_declarations(
        repo_root,
        commit,
        paths,
        assignments,
    )
    plugin_roots = {
        PurePosixPath(rel).parent.as_posix()
        for rel in paths
        if PurePosixPath(rel).suffix == ".uplugin"
    }
    cargo = cargo_roots(repo_root, commit, paths)
    root_cargo = root_cargo_class(repo_root, commit, paths)

    entries = [
        classify_path(
            rel,
            blob,
            assignments,
            plugin_roots,
            cargo,
            root_cargo,
            declarations,
        )
        for rel, blob in sorted(entries_by_path.items())
    ]

    for entry in entries:
        rel = str(entry["path"])
        if (
            rel.endswith(".py")
            and entry.get("component_class") == "separate-process"
            and imports_unreal(read_blob(repo_root, commit, rel))
        ):
            raise ManifestError(
                "Unreal-importing script requires a local ue-in-process "
                f"declaration: {rel}"
            )
        if entry.get("component_class") != "ue-in-process":
            continue
        head = read_blob(repo_root, commit, rel)[:512].decode(
            "utf-8",
            errors="replace",
        )
        if any(marker in head for marker in UNEXPECTED_OWNER_MARKERS):
            raise ManifestError(f"Unresolved ownership notice: {rel}")

    tree = git(repo_root, "rev-parse", f"{commit}^{{tree}}").strip()
    counts = Counter(
        str(entry.get("component_class", entry["kind"]))
        for entry in entries
    )
    return {
        "schema": "alis-effective-component-manifest-v1",
        "source_tag": tag,
        "source_commit": commit,
        "source_tree": tree,
        "policy_blob": entries_by_path["LICENSE"],
        "entry_count": len(entries),
        "counts": dict(sorted(counts.items())),
        "entries": entries,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[4],
    )
    args = parser.parse_args()
    try:
        manifest = generate_manifest(args.repo_root.resolve(), args.tag)
    except (ManifestError, subprocess.CalledProcessError) as error:
        print(f"[X] {error}", file=sys.stderr)
        return 1

    rendered = json.dumps(
        manifest,
        indent=2,
        sort_keys=True,
        ensure_ascii=True,
    ) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
        print(f"[OK] Wrote {args.output}")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
