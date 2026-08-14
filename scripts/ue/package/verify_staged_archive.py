#!/usr/bin/env python3
"""Fail when a UAT archive is missing or truncates a staged file."""

from __future__ import annotations

import argparse
from pathlib import Path


def archive_problems(staged_root: Path, archive_root: Path) -> list[str]:
    if not staged_root.is_dir():
        return [f"staged root is missing: {staged_root}"]
    if not archive_root.is_dir():
        return [f"archive root is missing: {archive_root}"]

    problems: list[str] = []
    for staged in sorted(path for path in staged_root.rglob("*") if path.is_file()):
        relative = staged.relative_to(staged_root)
        archived = archive_root / relative
        if not archived.is_file():
            problems.append(f"missing: {relative.as_posix()}")
        elif archived.stat().st_size != staged.stat().st_size:
            problems.append(f"size mismatch: {relative.as_posix()}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--staged-root", required=True, type=Path)
    parser.add_argument("--archive-root", required=True, type=Path)
    args = parser.parse_args()
    problems = archive_problems(args.staged_root, args.archive_root)
    if problems:
        print("Archive integrity rejected:")
        for problem in problems[:20]:
            print(f"  [X] {problem}")
        if len(problems) > 20:
            print(f"  [X] {len(problems) - 20} additional problem(s)")
        return 1
    print("Archive integrity accepted: every staged file is present at the staged size.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
