#!/usr/bin/env python3
"""Validate that reusable code uses the `Project*` prefix and never `Alis*`.

Background
----------
ALIS is one game shipping on a generic UE framework. Branded code can never
be lifted into another project without a rename pass, so the project rule is:
all reusable code (plugin classes, structs, enums, interfaces, log
categories, macros, INI labels) uses `Project*`. `Alis` is allowed ONLY in
the top-level game module, content folders, user-facing strings,
content-only asset plugins, and a few UE-forced names.

SOT: docs/architecture/principles.md, section "Universal Naming Convention".

What this check does
--------------------
1. Scan first-party C++ source under `Plugins/**/Source/**/*.{cpp,h}` and
   `Source/**/*.{cpp,h}` (skipping the allowed `Source/Alis/` module).
2. Detect declarations matching forbidden patterns: `Alis`-prefixed UE
   reflected types (UCLASS/USTRUCT/UENUM/UINTERFACE), `LogAlis*` log
   categories, and `ALIS_*` non-API macros.
3. Also flag forbidden file names: `Alis*.h` / `Alis*.cpp` outside the
   `Source/Alis/` game module.
4. Print a structured report and exit non-zero on any violation.

Usage
-----
    python validate_no_alis_prefix.py [--repo-root <path>]

Exit codes
----------
    0 - clean
    1 - violations found
    2 - tool error (no source found, etc.)
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# Class / struct / enum / interface declarations with Alis prefix.
# Captures: `class FOO_API UAlisX`, `class UAlisX`, `struct FAlisX`, etc.
# Matches the symbol itself in group(1).
DECL_PATTERNS = [
    # UCLASS-style: class [API_MACRO] U|A Alis<TitleCase>
    re.compile(
        r"\bclass\s+(?:[A-Z][A-Z0-9_]*_API\s+)?([UA]Alis[A-Z][A-Za-z0-9_]*)\b"
    ),
    # Interface: class [API_MACRO] IAlis<TitleCase>
    re.compile(
        r"\bclass\s+(?:[A-Z][A-Z0-9_]*_API\s+)?(IAlis[A-Z][A-Za-z0-9_]*)\b"
    ),
    # USTRUCT-style: struct [API_MACRO] FAlis<TitleCase>
    re.compile(
        r"\bstruct\s+(?:[A-Z][A-Z0-9_]*_API\s+)?(FAlis[A-Z][A-Za-z0-9_]*)\b"
    ),
    # UENUM-style: enum class EAlis<TitleCase> or enum EAlis<TitleCase>
    re.compile(
        r"\benum\s+(?:class\s+)?(EAlis[A-Z][A-Za-z0-9_]*)\b"
    ),
    # Log category: DECLARE_LOG_CATEGORY_EXTERN(LogAlis...)
    re.compile(
        r"DECLARE_LOG_CATEGORY_(?:EXTERN|CLASS)\s*\(\s*(LogAlis[A-Z][A-Za-z0-9_]*)\b"
    ),
    # Custom ALIS_ macros (allow ALIS_API auto-generated module API only).
    re.compile(
        r"#define\s+(ALIS_(?!API\b)[A-Z][A-Z0-9_]*)\b"
    ),
]

LINE_COMMENT_RE = re.compile(r"//[^\n]*")
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)


def strip_comments(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    text = LINE_COMMENT_RE.sub("", text)
    return text


def is_skipped_path(rel: str) -> bool:
    """Paths that are exempt from the rule."""
    parts = rel.replace("\\", "/").split("/")
    # The top-level game module Source/Alis/ is allowed by exception 1.
    if len(parts) >= 2 and parts[0] == "Source" and parts[1] == "Alis":
        return True
    # Intermediate and generated code are not first-party authored.
    if "Intermediate" in parts:
        return True
    if any(p.endswith(".gen.cpp") or p.endswith(".gen.h") for p in parts):
        return True
    # Third-party / vendored plugins are out of scope.
    if "Plugins" in parts:
        i = parts.index("Plugins")
        if i + 1 < len(parts) and parts[i + 1] in ("Local", "InstanceArrayTool"):
            return True
    return False


def find_source_files(repo_root: Path) -> list[Path]:
    """All first-party .cpp/.h to scan."""
    roots = [repo_root / "Source", repo_root / "Plugins"]
    files: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for ext in ("*.cpp", "*.h"):
            for f in root.rglob(ext):
                rel = f.relative_to(repo_root).as_posix()
                if is_skipped_path(rel):
                    continue
                files.append(f)
    return files


def scan_file(path: Path) -> list[tuple[int, str, str]]:
    """Return list of (line_number, symbol, pattern_label) for violations."""
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return []

    stripped = strip_comments(text)
    hits: list[tuple[int, str, str]] = []

    for pattern in DECL_PATTERNS:
        for match in pattern.finditer(stripped):
            symbol = match.group(1)
            # Map back to original line by counting newlines up to match.start
            # in the stripped text - approximation; comments don't change the
            # line count materially because LINE_COMMENT_RE keeps `\n`.
            line_no = stripped.count("\n", 0, match.start()) + 1
            hits.append((line_no, symbol, pattern.pattern[:48] + "..."))
    return hits


def scan_filenames(repo_root: Path, files: list[Path]) -> list[Path]:
    """Flag .cpp/.h files whose basename starts with Alis<Capital>."""
    bad: list[Path] = []
    name_re = re.compile(r"^Alis[A-Z][A-Za-z0-9_]*\.(?:cpp|h)$")
    for f in files:
        if name_re.match(f.name):
            bad.append(f)
    return bad


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[4],
        help="Repository root (default: detected from script location).",
    )
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    if not (repo_root / "Source").exists() and not (repo_root / "Plugins").exists():
        print(f"ERROR: no Source/ or Plugins/ under {repo_root}", file=sys.stderr)
        return 2

    files = find_source_files(repo_root)
    if not files:
        print("ERROR: no first-party C++ source found to scan", file=sys.stderr)
        return 2

    decl_violations: list[tuple[Path, int, str]] = []
    for f in files:
        for line_no, symbol, _label in scan_file(f):
            decl_violations.append((f, line_no, symbol))

    name_violations = scan_filenames(repo_root, files)

    total = len(decl_violations) + len(name_violations)
    if total == 0:
        print(f"[OK] No `Alis*` violations in {len(files)} first-party files.")
        return 0

    print("=" * 72)
    print("[X] FAIL: `Alis*` prefix forbidden in reusable code.")
    print("SOT: docs/architecture/principles.md 'Universal Naming Convention'")
    print("Use `Project*` instead. AGENTS.md 'NO Alis* IN REUSABLE CODE'.")
    print("=" * 72)

    if decl_violations:
        print(f"\nDeclaration violations ({len(decl_violations)}):")
        for path, line_no, symbol in sorted(decl_violations):
            rel = path.relative_to(repo_root).as_posix()
            print(f"  {rel}:{line_no}  ->  {symbol}")

    if name_violations:
        print(f"\nFile name violations ({len(name_violations)}):")
        for path in sorted(name_violations):
            rel = path.relative_to(repo_root).as_posix()
            print(f"  {rel}  ->  rename to Project*.{path.suffix.lstrip('.')}")

    print()
    return 1


if __name__ == "__main__":
    sys.exit(main())
