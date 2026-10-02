#!/usr/bin/env python3
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXTENSIONS = {
    ".md",
    ".txt",
    ".json",
    ".uplugin",
    ".uproject",
    ".cs",
    ".cpp",
    ".h",
    ".ini",
    ".yaml",
    ".yml",
}

# Map unwanted code points (NBSP, soft hyphen, zero-width, BOM, etc.) to a space
BAD_INVIS = dict.fromkeys(
    map(ord, "\u00A0\u00AD\u200B\u200C\u200D\u200E\u200F\uFEFF"),
    " ",
)


def normalize(text: str) -> str:
    """Remove invisible junk and normalize punctuation/spacing."""
    text = text.translate(BAD_INVIS)
    text = text.replace("—", " - ").replace("–", "-")
    text = text.replace("“", '"').replace("”", '"')
    text = text.replace("‘", "'").replace("’", "'")
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"[ \t]+\n", "\n", text)
    return text


def main() -> int:
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
            continue
        original = path.read_text("utf-8", errors="ignore")
        cleaned = normalize(original)
        if cleaned != original:
            path.write_text(cleaned, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
