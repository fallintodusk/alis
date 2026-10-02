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


def sanitize_tree(root: pathlib.Path) -> None:
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        # The mirror tooling carries its identity patterns itself.
        if rel.startswith("scripts/git/mirror/"):
            continue
        suffix = path.suffix.lower()
        if suffix not in TEXT_SUFFIXES and not path.name.lower().startswith("readme"):
            continue
        try:
            original = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        text = sanitize_text(original, suffix)
        if text == original:
            continue
        newline = "\r\n" if "\r\n" in original else "\n"
        with open(path, "w", encoding="utf-8", newline=newline) as handle:
            handle.write(text)


def residual_machine_paths(root: pathlib.Path) -> list[tuple[str, list[int]]]:
    """(path, line numbers) of every file in the tree that carries a machine path."""
    found = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        raw = path.read_bytes()
        # Binary content fails the mirror's binary guard instead.
        if b"\0" in raw[:8192]:
            continue
        lines = machine_local_paths.machine_path_lines(raw.decode("utf-8", errors="ignore"))
        if lines:
            found.append((path.relative_to(root).as_posix(), lines))
    return found


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
