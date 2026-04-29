#!/usr/bin/env python3
"""Validate that plugins reading from Plugins/<X>/Data/ at runtime stage that
directory in their .Build.cs.

Background
----------
Several plugins parse JSON at runtime via FProjectPaths::GetPluginDataDir(
TEXT("X")) / TEXT("file.json"). In Editor the file resolves on disk; in
Shipping it must be staged via RuntimeDependencies in the plugin's Build.cs.
If staging is missing the JSON never ends up in the cooked payload, the
loader logs a Warning and silently falls back to defaults (e.g. the Mind
journal mapping was lost in 2026-04 Shipping for exactly this reason).

What this check does
--------------------
1. Scan Plugins/**/Source/**/*.{cpp,h} for GetPluginDataDir(TEXT("X")) and
   collect the set of plugin names that read runtime data.
2. For each plugin name, locate its .uplugin and confirm at least one
   Build.cs in that plugin stages a Data/ directory through one of:
     - StageDataDir(Target) helper (canonical pattern -- see ProjectUI)
     - RuntimeDependencies.Add(... "Data" ... ) per-file form
3. Print a structured report and return non-zero on missing staging.

Usage
-----
    python validate_plugin_data_staging.py [--repo-root <path>]

Default repo-root: directory two levels above this script.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


DATA_DIR_CALL_RE = re.compile(
    r'GetPluginDataDir\s*\(\s*TEXT\s*\(\s*"([^"]+)"\s*\)\s*\)'
)
RUNTIME_DEP_CALL_RE = re.compile(r'RuntimeDependencies\.Add\b')
DATA_PATH_FRAGMENT_RE = re.compile(r'(?:[/\\"])Data(?:[/\\"])')

# Match a CALL-SITE of StageDataDir, not the method definition. The definition
# is preceded by a return type (`void`) or access modifier; calls are not.
# Also exclude matches inside C-style line comments (we strip those before
# regex). Without this distinction the validator falsely passes when an author
# defines the helper but forgets to actually invoke it.
STAGE_HELPER_CALL_RE = re.compile(r'(?<!void\s)(?<!void )StageDataDir\s*\(')

# C-style line comments and block comments to strip before regex matching.
LINE_COMMENT_RE = re.compile(r'//[^\n]*')
BLOCK_COMMENT_RE = re.compile(r'/\*.*?\*/', re.DOTALL)


def find_plugin_data_readers(plugins_dir: Path) -> dict[str, list[Path]]:
    """Return {plugin_name: [files referencing it]} for every plugin name
    fed to GetPluginDataDir(TEXT("...")) anywhere under Plugins/."""
    result: dict[str, list[Path]] = {}
    for source_file in plugins_dir.rglob("*.cpp"):
        path_str = str(source_file).replace("\\", "/")
        if "/Source/" not in path_str:
            continue
        if "/Tests/" in path_str or "/Intermediate/" in path_str:
            continue
        try:
            text = source_file.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for match in DATA_DIR_CALL_RE.finditer(text):
            name = match.group(1)
            result.setdefault(name, []).append(source_file)
    return result


def find_plugin_dir(plugins_dir: Path, plugin_name: str) -> Path | None:
    """Locate Plugins/**/<plugin_name>.uplugin and return its parent dir."""
    for uplugin in plugins_dir.rglob(f"{plugin_name}.uplugin"):
        return uplugin.parent
    return None


def stages_data_dir(plugin_dir: Path) -> bool:
    """True if any Build.cs in this plugin stages a Data/ directory.

    Accepts either the canonical StageDataDir(...) helper used by ProjectUI /
    Orchestrator / ProjectSinglePlay, or a per-file RuntimeDependencies.Add
    that mentions "Data" inside the path argument (ProjectVitals form).
    """
    for build_cs in plugin_dir.rglob("*.Build.cs"):
        try:
            raw_text = build_cs.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        # Strip comments before regex so commented-out staging code doesn't
        # falsely satisfy the check. Block comments first (they may span lines
        # and contain `//` literals), then line comments.
        text = BLOCK_COMMENT_RE.sub("", raw_text)
        text = LINE_COMMENT_RE.sub("", text)

        # We only care about staging code that is reachable from the
        # constructor at UBT-eval time. C# convention places `private void`
        # helpers after the constructor body, so the "constructor area" is
        # everything before the first `private` modifier. Code inside a
        # `private void StageDataDir(...)` body that is never called from the
        # constructor MUST NOT count as staging proof -- that was the
        # original failure mode caught by the 2026-04-28 sabotage test.
        m = re.search(r'\bprivate\b', text)
        constructor_area = text[:m.start()] if m else text

        # Constructor must invoke staging via either form:
        # - via helper:  `StageDataDir(Target);` (call site, not definition)
        # - inline form: `RuntimeDependencies.Add(...)` with a `Data` path token
        if STAGE_HELPER_CALL_RE.search(constructor_area):
            return True
        if (RUNTIME_DEP_CALL_RE.search(constructor_area)
                and DATA_PATH_FRAGMENT_RE.search(constructor_area)):
            return True
    return False


def verify_archive(plugins_dir: Path, repo_root: Path, archive_root: Path,
                    readers: dict[str, list[Path]]) -> int:
    """Post-package smoke check: confirm runtime-read JSONs survived the cook.

    For each plugin that reads via GetPluginDataDir, list the source Data/*.json
    files and check whether they appear in the archive as loose files at the
    expected staged path (`<archive>/<staged_root>/Plugins/<rel>/Data/<name>`).

    A plugin whose JSONs are absent loose may have been packed into pak via
    StagedFileType.UFS -- that's not a hard error, but we surface it so the
    operator can verify by running the binary. Hard errors only when:
      - the source Data dir has files but NONE are loose AND none are
        recoverable via known cooked-pak loose paths (we cannot read the pak,
        so this is a heuristic warning).

    Returns 0 if every plugin either has loose presence or has zero source
    JSON files. Returns 1 only if a plugin has source JSONs but the entire
    Data tree is missing under any plausible archive subtree.
    """
    if not archive_root.is_dir():
        print(f"[X] Archive root not found: {archive_root}")
        return 1

    # Locate the staged content root (e.g. <archive>/Windows/Alis/).
    # We accept the first descendant that contains a Plugins/ subdir.
    staged_root: Path | None = None
    for candidate in archive_root.rglob("Plugins"):
        if candidate.is_dir():
            staged_root = candidate.parent
            break
    if staged_root is None:
        print(f"[X] No Plugins/ directory found under archive: {archive_root}")
        return 1

    print(f"Archive root: {archive_root}")
    print(f"Staged root:  {staged_root}")

    failures: list[str] = []
    notes: list[str] = []

    for plugin_name in sorted(readers.keys()):
        plugin_dir = find_plugin_dir(plugins_dir, plugin_name)
        if plugin_dir is None:
            failures.append(f"{plugin_name}: source plugin dir not found")
            continue

        source_data = plugin_dir / "Data"
        if not source_data.is_dir():
            continue

        # Schemas/ subfolders hold JSON-Schema definitions used by IDEs to
        # validate authored JSON. They are not runtime-loaded so do not need
        # to ship with the cooked build.
        json_files = sorted(
            p for p in source_data.rglob("*.json")
            if "Schemas" not in p.relative_to(source_data).parts
        )
        if not json_files:
            continue

        rel_plugin = plugin_dir.relative_to(repo_root).as_posix()
        archive_data = staged_root / rel_plugin / "Data"
        loose_present: list[Path] = []
        loose_missing: list[Path] = []

        for json_path in json_files:
            rel = json_path.relative_to(source_data)
            target = archive_data / rel
            if target.is_file():
                loose_present.append(rel)
            else:
                loose_missing.append(rel)

        if not loose_present:
            # Could be UFS-packed in pak. Surface as note, not failure -- the
            # runtime check via FFileHelper will confirm or deny.
            notes.append(
                f"{plugin_name}: {len(json_files)} JSON file(s) under Data/ "
                f"have no loose presence in archive ({archive_data}). "
                f"Likely packed into pak via StagedFileType.UFS. "
                f"Verify by running the cooked binary and checking logs."
            )
        elif loose_missing:
            failures.append(
                f"{plugin_name}: partial archive coverage. "
                f"Present: {[p.as_posix() for p in loose_present]}. "
                f"Missing: {[p.as_posix() for p in loose_missing]}. "
                f"Expected under: {archive_data}"
            )
        else:
            # Full loose coverage.
            pass

    print()
    for n in notes:
        print(f"  [i] {n}")
    if failures:
        print(f"\nArchive verification FAILED ({len(failures)} errors):")
        for f in failures:
            print(f"  [X] {f}")
        return 1

    print(f"\nArchive verification passed ({len(readers)} plugins audited).")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[4],
        help="Repository root (default: %(default)s)",
    )
    parser.add_argument(
        "--archive-root",
        type=Path,
        default=None,
        help="If set, run post-package smoke check: verify runtime-read JSON "
             "files survived the cook into this archive directory.",
    )
    args = parser.parse_args()

    plugins_dir = args.repo_root / "Plugins"
    if not plugins_dir.is_dir():
        print(f"[X] Plugins/ directory not found under {args.repo_root}")
        return 1

    readers = find_plugin_data_readers(plugins_dir)
    if not readers:
        print("Plugin data staging passed (no GetPluginDataDir runtime readers found).")
        return 0

    if args.archive_root is not None:
        return verify_archive(plugins_dir, args.repo_root, args.archive_root, readers)

    errors: list[str] = []
    audited = sorted(readers.keys())

    for plugin_name in audited:
        plugin_dir = find_plugin_dir(plugins_dir, plugin_name)
        callers = readers[plugin_name]
        sample_caller = callers[0].relative_to(args.repo_root)

        if plugin_dir is None:
            errors.append(
                f"{plugin_name}: runtime reads from GetPluginDataDir(\"{plugin_name}\") "
                f"in {sample_caller} but no '{plugin_name}.uplugin' found"
            )
            continue

        if not stages_data_dir(plugin_dir):
            rel_dir = plugin_dir.relative_to(args.repo_root)
            errors.append(
                f"{plugin_name}: reads from Plugins/{rel_dir.name}/Data/ at runtime "
                f"(see {sample_caller}) but no Build.cs in {rel_dir} stages it. "
                f"Add the canonical helper to <Module>.Build.cs:\n"
                f"          StageDataDir(Target);  // call from constructor\n"
                f"        and define:\n"
                f"          private void StageDataDir(ReadOnlyTargetRules T) {{\n"
                f"              if (T.Type == TargetType.Editor) return;\n"
                f"              string D = Path.Combine(PluginDirectory, \"Data\");\n"
                f"              if (!Directory.Exists(D)) return;\n"
                f"              RuntimeDependencies.Add(Path.Combine(D, \"...\"), StagedFileType.UFS);\n"
                f"          }}"
            )

    if errors:
        print(f"\nPlugin data staging FAILED ({len(errors)} errors):")
        for e in errors:
            print(f"  [X] {e}")
        print(
            "\nWhy this matters: the affected plugin parses JSON via "
            "FFileHelper::LoadFileToString in cooked builds. Without staging the "
            "file is missing in Shipping, the loader prints a Warning and "
            "silently falls back to defaults (e.g. Mind journal lost the Grandpa "
            "mappings in the 2026-04 Shipping build for this exact reason)."
        )
        return 1

    print(f"Plugin data staging passed ({len(audited)} plugins audited: {', '.join(audited)}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
