#!/usr/bin/env python3
"""Validate text files AND tracked paths against the ALIS character-set policy.

Background
----------
ALIS is an English/ASCII codebase (CLAUDE.md "ASCII-ONLY DOCUMENTATION").
Foreign-script characters cause two problems:

  1. In file CONTENT -- a Cyrillic homoglyph (e.g. Cyrillic 'Es' U+0421,
     identical to Latin 'C', spliced into "Cutting") or untranslated Russian
     renders as Russian on the public GitHub mirror.
  2. In file/folder PATHS -- non-ASCII homoglyphs such as a Cyrillic 'C'
     in "Concrete" or "Cutting_Sawing" break references, cooking, and
     cross-platform builds, and are nearly invisible in a file tree.

This check enforces "no foreign-script symbols or comments" for both.

Design
------
Disallowed content character groups are declared as DATA in `BLOCKS` below --
each is a named set of inclusive Unicode codepoint ranges. Add a group (more
scripts, emoji ranges, ...) by adding one entry; no logic changes. Detection is
by integer codepoint range so THIS source stays pure ASCII and never flags
itself. Paths are stricter: any non-ASCII path character is disallowed.

Blocks (extend freely)
----------------------
    cyrillic   : Cyrillic / Russian scripts
    cjk        : CJK ideographs (Chinese hieroglyphs), Japanese kana, Hangul
    emoji      : emoji and pictographs
    typography : non-ASCII punctuation (smart quotes, em/en dashes, arrows,
                 non-breaking space) that should be plain ASCII

Checks
------
    content : characters inside text files (scoped by extension / --all-text)
    paths   : non-ASCII characters in tracked file and folder names (all
              files); on by default, disable with --no-paths

Policy / scope
--------------
    --blocks a,b     : check only these named blocks (default: cyrillic,cjk)
    --all-blocks     : check every declared block
    default          : content scope = documentation (.md, .dsl, .txt)
    --all-text       : content scope = all common text files
    --no-paths       : skip the path check

File source
-----------
    default        : git-tracked files under --repo-root (pre-commit / CI)
    --root <dir>   : every file under <dir> (public-mirror filtered snapshot)

Exit codes
----------
    0 - clean
    1 - disallowed characters found (in content or paths)
    2 - tool error
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

# Disallowed character groups as DATA. Each maps a name to (description, ranges)
# where ranges is a list of inclusive (lo, hi) codepoint pairs. Hex literals
# only -- no regex, no \u escapes, no literal foreign characters -- so this
# file is guaranteed ASCII and never flags itself. Extend by adding entries.
BLOCKS = {
    "cyrillic": (
        "Cyrillic / Russian",
        [
            (0x0400, 0x04FF),  # Cyrillic
            (0x0500, 0x052F),  # Cyrillic Supplement
            (0x2DE0, 0x2DFF),  # Cyrillic Extended-A
            (0xA640, 0xA69F),  # Cyrillic Extended-B
        ],
    ),
    "cjk": (
        "CJK ideographs / kana / hangul",
        [
            (0x3040, 0x309F),  # Hiragana
            (0x30A0, 0x30FF),  # Katakana
            (0x3400, 0x4DBF),  # CJK Extension A
            (0x4E00, 0x9FFF),  # CJK Unified Ideographs
            (0xAC00, 0xD7AF),  # Hangul Syllables
            (0xF900, 0xFAFF),  # CJK Compatibility Ideographs
        ],
    ),
    "emoji": (
        "emoji / pictographs",
        [
            (0x2600, 0x27BF),    # Misc Symbols + Dingbats
            (0xFE00, 0xFE0F),    # Variation Selectors
            (0x1F000, 0x1FAFF),  # Emoticons, pictographs, symbols
        ],
    ),
    "typography": (
        "non-ASCII punctuation (smart quotes, dashes, arrows, NBSP)",
        [
            (0x00A0, 0x00A0),  # non-breaking space
            (0x2010, 0x2015),  # hyphens / en dash / em dash
            (0x2018, 0x201F),  # curly single/double quotes
            (0x2026, 0x2026),  # horizontal ellipsis
            (0x2190, 0x21FF),  # arrows
            (0x2212, 0x2212),  # minus sign
        ],
    ),
}

DEFAULT_BLOCKS = ("cyrillic", "cjk")

DOC_EXT = (".md", ".dsl", ".txt")
TEXT_EXT = (
    ".md", ".dsl", ".txt", ".json", ".ini", ".cs", ".cpp", ".c", ".h",
    ".hpp", ".inl", ".ps1", ".bat", ".sh", ".py", ".yml", ".yaml",
    ".uplugin", ".uproject", ".html", ".htm", ".css", ".js", ".ts",
    ".csv", ".toml", ".xml", ".rst",
)

def classify(ch: str, active) -> str | None:
    """Return the name of the first active block the char falls in, else None."""
    cp = ord(ch)
    for name in active:
        for lo, hi in BLOCKS[name][1]:
            if lo <= cp <= hi:
                return name
    return None


def first_violation(text: str, active):
    for i, ch in enumerate(text):
        block = classify(ch, active)
        if block:
            return i, block
    return -1, None


def first_non_ascii(text: str):
    for i, ch in enumerate(text):
        if not ch.isascii():
            return i, "non_ascii_path"
    return -1, None


def ascii_safe(text: str) -> str:
    """Render a token with non-printable/non-ASCII chars as <U+XXXX>."""
    return "".join(
        c if (c.isascii() and c.isprintable()) else f"<U+{ord(c):04X}>"
        for c in text
    )


def widen_token(line: str, hit: int) -> str:
    """Expand a match index to the whitespace-delimited token around it."""
    start = hit
    while start > 0 and not line[start - 1].isspace():
        start -= 1
    end = hit + 1
    while end < len(line) and not line[end].isspace():
        end += 1
    return line[start:end]


def git_ls_files(repo_root: str, patterns=None):
    cmd = ["git", "-C", repo_root, "ls-files", "-z"]
    if patterns:
        cmd.extend(patterns)
    result = subprocess.run(cmd, capture_output=True)
    if result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(stderr or "git ls-files failed")
    text = result.stdout.decode("utf-8", errors="surrogateescape")
    return [rel for rel in text.split("\0") if rel]


def iter_git_content(repo_root: str, exts):
    patterns = [f"*{e}" for e in exts]
    for rel in git_ls_files(repo_root, patterns):
        yield rel.replace("\\", "/"), os.path.join(repo_root, rel)


def iter_git_paths(repo_root: str):
    for rel in git_ls_files(repo_root):
        yield rel.replace("\\", "/")


def iter_tree_content(root: str, exts):
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            low = name.lower()
            if low.endswith(exts) or low.startswith("readme"):
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace("\\", "/")
                yield rel, full


def iter_tree_paths(root: str):
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            full = os.path.join(dirpath, name)
            yield os.path.relpath(full, root).replace("\\", "/")


def scan_content(files, active):
    violations = []
    for rel, full in files:
        try:
            with open(full, encoding="utf-8") as handle:
                for lineno, line in enumerate(handle, 1):
                    hit, block = first_violation(line, active)
                    if hit >= 0:
                        token = widen_token(line, hit)
                        violations.append((rel, lineno, hit + 1, block, token))
        except UnicodeDecodeError as exc:
            violations.append((rel, 0, 0, "decode", f"not utf-8: {exc}"))
        except OSError:
            continue
    return violations


def scan_paths(paths, root=None):
    violations = []
    for rel in paths:
        hit, block = first_non_ascii(rel)
        if hit >= 0:
            exists = True
            if root:
                exists = os.path.exists(os.path.join(root, rel))
            violations.append((rel, hit + 1, block, exists))
    return violations


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate text content and tracked paths against the ALIS "
                    "character-set policy.",
    )
    parser.add_argument("--repo-root", default=".",
                        help="repo root for the git-tracked scan (default: .)")
    parser.add_argument("--root",
                        help="scan every file under this directory instead of "
                             "git-tracked files (public-mirror snapshot)")
    parser.add_argument("--all-text", action="store_true",
                        help="content scope = all text files, not just docs")
    parser.add_argument("--no-paths", action="store_true",
                        help="skip the file/folder path check")
    parser.add_argument("--blocks", default=",".join(DEFAULT_BLOCKS),
                        help="comma-separated block names to check "
                             f"(default: {','.join(DEFAULT_BLOCKS)}; "
                             f"available: {','.join(BLOCKS)})")
    parser.add_argument("--all-blocks", action="store_true",
                        help="check every declared block")
    args = parser.parse_args(argv)

    if args.all_blocks:
        active = tuple(BLOCKS)
    else:
        active = tuple(b.strip() for b in args.blocks.split(",") if b.strip())
    unknown = [b for b in active if b not in BLOCKS]
    if unknown:
        print(f"[ERROR] unknown block(s): {', '.join(unknown)}; "
              f"available: {', '.join(BLOCKS)}", file=sys.stderr)
        return 2

    exts = TEXT_EXT if args.all_text else DOC_EXT
    content_scope = "all text files" if args.all_text else "docs (.md/.dsl/.txt)"

    try:
        if args.root:
            content_files = list(iter_tree_content(args.root, exts))
            all_paths = [] if args.no_paths else list(iter_tree_paths(args.root))
            where = args.root
        else:
            content_files = list(iter_git_content(args.repo_root, exts))
            all_paths = [] if args.no_paths else list(iter_git_paths(args.repo_root))
            where = args.repo_root
    except RuntimeError as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2

    content_hits = scan_content(content_files, active)
    path_hits = scan_paths(all_paths, where)

    if content_hits or path_hits:
        print(f"[FAIL] Disallowed characters under {where} "
              f"(content blocks: {', '.join(active)}; paths: ASCII):",
              file=sys.stderr)
        if content_hits:
            print(f"  content ({content_scope}):", file=sys.stderr)
            for rel, lineno, col, block, token in content_hits:
                print(f"    {rel}:{lineno}:{col}: [{block}] {ascii_safe(token)}",
                      file=sys.stderr)
        if path_hits:
            print("  paths (file/folder names):", file=sys.stderr)
            for rel, col, block, exists in path_hits:
                state = "" if exists else " [missing on disk]"
                print(f"    {ascii_safe(rel)}: [{block}] (col {col}){state}",
                      file=sys.stderr)
        print(f"\n{len(content_hits)} content + {len(path_hits)} path "
              "occurrence(s). ALIS is English/ASCII -- transliterate/translate "
              "content and rename paths; no foreign-script symbols or comments.",
              file=sys.stderr)
        print("Homoglyph tip: a leading 'C' that is really Cyrillic 'Es' "
              "(U+0421) or 'Er'+'Short-U' looks identical to Latin 'C'.",
              file=sys.stderr)
        return 1

    path_note = "" if args.no_paths else f" + {len(all_paths)} paths"
    print(f"[OK] No disallowed characters "
          f"(content blocks: {', '.join(active)}; paths: ASCII; "
          f"{len(content_files)} {content_scope}{path_note} under {where}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
