"""Verify primary asset type directories contain assets.

Parses DefaultGame.ini for PrimaryAssetTypesToScan entries and checks that
each configured scan directory has .uasset files. This is a presence check --
it proves content exists where the Asset Manager expects to find it.

Does NOT require the editor. Uses the project Content/ structure directly.

Types listed in OPTIONAL_EMPTY_TYPES are allowed to have 0 assets without
causing a FAIL (they report as warnings instead).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


# Types that are expected to have 0 assets in some valid project states.
# ProjectAbilitySet: no ability set assets exist yet (directory is empty).
OPTIONAL_EMPTY_TYPES = {"ProjectAbilitySet"}


class ErrorCollector:
    """Matches reporting.py pattern from data/_lib."""

    def __init__(self) -> None:
        self._errors: list[str] = []
        self._warnings: list[str] = []

    def error(self, msg: str) -> None:
        self._errors.append(msg)

    def warn(self, msg: str) -> None:
        self._warnings.append(msg)

    def print_summary(self, label: str) -> int:
        if self._warnings:
            print(f"\n{label} warnings ({len(self._warnings)}):")
            for w in self._warnings:
                print(f"  [!] {w}")

        if not self._errors:
            print(f"{label} passed.")
            return 0

        print(f"\n{label} FAILED ({len(self._errors)} errors):")
        for e in self._errors:
            print(f"  [X] {e}")
        return 1


def find_content_roots(project_root: Path) -> list[Path]:
    """Find all Content/ directories in the project (root + plugins)."""
    roots = []
    # Main project content
    main_content = project_root / "Content"
    if main_content.is_dir():
        roots.append(main_content)
    # Plugin content directories
    plugins_dir = project_root / "Plugins"
    if plugins_dir.is_dir():
        for content_dir in plugins_dir.rglob("Content"):
            if content_dir.is_dir():
                roots.append(content_dir)
    return roots


def resolve_content_path(mount_point: str, content_roots: list[Path]) -> Path | None:
    """Resolve a UE content path like /ProjectGAS/AbilitySets to a filesystem path.

    UE mounts plugin content at /<PluginName>/. So /ProjectGAS/AbilitySets
    maps to Plugins/.../ProjectGAS/Content/AbilitySets.
    """
    # Strip leading /
    clean = mount_point.lstrip("/")
    parts = clean.split("/", 1)
    mount_name = parts[0]  # e.g. "ProjectGAS" or "ProjectObject"
    sub_path = parts[1] if len(parts) > 1 else ""

    for root in content_roots:
        # Main project content mounted as /Game/
        if mount_name == "Game":
            # root must be the project-level Content/ (parent is project root)
            if root.name == "Content" and (root.parent / "Alis.uproject").is_file():
                target = root / sub_path if sub_path else root
                if target.is_dir():
                    return target
            continue

        # Plugin content: .../Plugins/<Category>/<PluginName>/Content/
        plugin_name = root.parent.name
        if plugin_name == mount_name:
            target = root / sub_path if sub_path else root
            return target

    return None


def count_uassets(directory: Path) -> int:
    """Count .uasset files recursively in a directory."""
    if not directory.is_dir():
        return 0
    return len(list(directory.rglob("*.uasset")))


def parse_primary_asset_types(ini_path: Path) -> list[dict]:
    """Parse PrimaryAssetTypesToScan entries from DefaultGame.ini."""
    types = []
    try:
        text = ini_path.read_text(encoding="utf-8-sig")
    except FileNotFoundError:
        return types

    # Match +PrimaryAssetTypesToScan=(...) lines
    pattern = r'\+PrimaryAssetTypesToScan=\((.+)\)'
    for match in re.finditer(pattern, text):
        entry = match.group(1)

        # Extract fields
        type_match = re.search(r'PrimaryAssetType="([^"]+)"', entry)
        # Directories have nested parens: Directories=((Path="..."),(Path="..."))
        # Use a greedy match balanced by the trailing comma or end-of-entry
        dirs_match = re.search(r'Directories=\((\(.+?\))\)', entry)
        editor_match = re.search(r'bIsEditorOnly=(\w+)', entry)

        if not type_match:
            continue

        asset_type = type_match.group(1)
        is_editor = editor_match and editor_match.group(1).lower() == "true"

        # Skip editor-only types (not relevant to shipping)
        if is_editor:
            continue

        # Extract directory paths
        directories = []
        if dirs_match:
            for path_match in re.finditer(r'Path="([^"]+)"', dirs_match.group(1)):
                directories.append(path_match.group(1))

        types.append({
            "type": asset_type,
            "directories": directories,
        })

    return types


def collect_ini_files(project_root: Path) -> list[Path]:
    """Collect all DefaultGame.ini files: root + plugins."""
    ini_files = []
    root_ini = project_root / "Config" / "DefaultGame.ini"
    if root_ini.is_file():
        ini_files.append(root_ini)
    plugins_dir = project_root / "Plugins"
    if plugins_dir.is_dir():
        for plugin_ini in plugins_dir.rglob("Config/DefaultGame.ini"):
            ini_files.append(plugin_ini)
    return ini_files


def main() -> int:
    # Find project root (script is at scripts/ue/check/assets/)
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent.parent.parent.parent

    ini_files = collect_ini_files(project_root)
    if not ini_files:
        print(f"ERROR: No DefaultGame.ini found in {project_root}")
        return 1

    content_roots = find_content_roots(project_root)
    if not content_roots:
        print(f"ERROR: No Content/ directories found in {project_root}")
        return 1

    # Merge scan entries from all ini files (root + plugins)
    all_types: dict[str, dict] = {}
    for ini_path in ini_files:
        for t in parse_primary_asset_types(ini_path):
            key = t["type"]
            if key in all_types:
                # Merge directories, deduplicate
                existing = set(all_types[key]["directories"])
                existing.update(t["directories"])
                all_types[key]["directories"] = sorted(existing)
            else:
                all_types[key] = t

    types = list(all_types.values())
    if not types:
        print("WARNING: No PrimaryAssetTypesToScan entries found in DefaultGame.ini")
        return 0

    errors = ErrorCollector()

    print(f"Checking {len(types)} primary asset types...")
    for t in types:
        asset_type = t["type"]
        total_assets = 0

        resolved_any = False
        for directory in t["directories"]:
            resolved = resolve_content_path(directory, content_roots)
            if resolved is None or not resolved.is_dir():
                continue
            resolved_any = True
            count = count_uassets(resolved)
            total_assets += count

        dirs_str = ", ".join(t["directories"])
        print(f"  {asset_type}: {total_assets} assets ({dirs_str})")

        if not resolved_any:
            if asset_type in OPTIONAL_EMPTY_TYPES:
                errors.warn(f"{asset_type}: no scan directories found on disk ({dirs_str})")
            else:
                errors.error(f"{asset_type}: no scan directories found on disk ({dirs_str})")
        elif total_assets == 0:
            if asset_type in OPTIONAL_EMPTY_TYPES:
                errors.warn(f"{asset_type}: 0 assets (optional-empty, allowed)")
            else:
                errors.error(f"{asset_type}: 0 assets in {dirs_str}")

    return errors.print_summary("Primary asset presence check")


if __name__ == "__main__":
    raise SystemExit(main())
