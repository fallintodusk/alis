#!/usr/bin/env python3
"""Anonymize a filtered public mirror tree, then verify the final tree.

  sanitize_public_text.py <tree>          rewrite published documentation
  sanitize_public_text.py --check <tree>  fail on any remaining machine path

Machine-local paths are defined by governance
(scripts/ue/check/governance/machine_local_paths.py); this adapter applies that
definition to published files. Documentation gets the portable spelling paired
with each path. Code is never path-rewritten, because a rewritten path would
publish broken code, so the check refuses it and the path is fixed at source.
"""

from __future__ import annotations

import pathlib
import re
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "ue" / "check" / "governance"))
import machine_local_paths  # noqa: E402

TEXT_SUFFIXES = {
    ".md",
    ".txt",
    ".json",
    ".ini",
    ".cs",
    ".cpp",
    ".c",
    ".h",
    ".hpp",
    ".inl",
    ".ps1",
    ".bat",
    ".sh",
    ".py",
    ".yml",
    ".yaml",
    ".dsl",
    ".uplugin",
    ".uproject",
    ".disabled",
}

# Published executable sources: identity rewrites only, never path rewrites.
CODE_SUFFIXES = {".bat", ".cmd", ".ps1", ".psm1", ".sh", ".py", ".cs", ".cpp", ".c", ".h", ".hpp", ".inl"}

IDENTITY_REPLACEMENTS = [
    (re.compile(r"\bAlis Team\b"), "ALIS"),
]

UPLUGIN_REPLACEMENTS = [
    (re.compile(r'("CreatedBy"\s*:\s*)".*?"'), r'\1"ALIS"'),
    (re.compile(r'("CreatedByURL"\s*:\s*)".*?"'), r'\1""'),
    (re.compile(r'("SupportURL"\s*:\s*)".*?"'), r'\1""'),
]


def sanitize_text(text: str, suffix: str) -> str:
    if suffix not in CODE_SUFFIXES:
        for pattern, portable in machine_local_paths.RULES:
            text = pattern.sub(lambda _match: portable, text)
    for pattern, replacement in IDENTITY_REPLACEMENTS:
        text = pattern.sub(replacement, text)
    if suffix == ".uplugin":
        for pattern, replacement in UPLUGIN_REPLACEMENTS:
            text = pattern.sub(replacement, text)
    return text


def sanitize_file(root: pathlib.Path, path: pathlib.Path) -> None:
    if not path.is_file():
        return
    rel = path.relative_to(root).as_posix()
    # The mirror tooling carries its identity patterns itself.
    if rel.startswith("scripts/git/mirror/"):
        return
    suffix = path.suffix.lower()
    if suffix not in TEXT_SUFFIXES and not path.name.lower().startswith("readme"):
        return
    try:
        original = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return
    text = sanitize_text(original, suffix)
    if text == original:
        return
    newline = "\r\n" if "\r\n" in original else "\n"
    with open(path, "w", encoding="utf-8", newline=newline) as handle:
        handle.write(text)


def sanitize_tree(root: pathlib.Path) -> None:
    # Files are independent, so their reads overlap in threads (see
    # residual_machine_paths).
    with ThreadPoolExecutor(max_workers=16) as pool:
        list(pool.map(lambda path: sanitize_file(root, path), list(root.rglob("*"))))


def machine_path_lines_in(path: pathlib.Path) -> list[int]:
    if not path.is_file():
        return []
    raw = path.read_bytes()
    # Binary content fails the mirror's binary guard instead.
    if b"\0" in raw[:8192]:
        return []
    return machine_local_paths.machine_path_lines(raw.decode("utf-8", errors="ignore"))


def residual_machine_paths(root: pathlib.Path) -> list[tuple[str, list[int]]]:
    """(path, line numbers) of every file in the tree that carries a machine path."""
    paths = sorted(root.rglob("*"))
    # Reads overlap in threads: under WSL the tree is on a mounted Windows drive,
    # where each file access is a slow round trip.
    with ThreadPoolExecutor(max_workers=16) as pool:
        per_path = list(pool.map(machine_path_lines_in, paths))
    return [
        (path.relative_to(root).as_posix(), lines)
        for path, lines in zip(paths, per_path)
        if lines
    ]


def main(argv: list[str]) -> int:
    if len(argv) == 3 and argv[1] == "--check":
        found = residual_machine_paths(pathlib.Path(argv[2]))
        for rel, lines in found:
            print(f"[FAIL] machine-local path in published file: {rel} (lines {lines})", file=sys.stderr)
        if found:
            print("[FAIL] Resolve these values at runtime or write a placeholder; see "
                  "docs/architecture/principles.md (Machine-Local Values).", file=sys.stderr)
            return 1
        return 0
    if len(argv) == 2:
        sanitize_tree(pathlib.Path(argv[1]))
        return 0
    print("usage: sanitize_public_text.py [--check] <tree>", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
