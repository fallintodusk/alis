#!/usr/bin/env python3
"""Validate DefaultEngine.ini and DefaultGame.ini for shipping-unsafe settings.

Catches debug-quality overrides, disabled caches, and misconfigured flags
that should not survive to a packaged Shipping build.

Usage:
    python validate_shipping_ini.py [--config-dir <path>]

Default config-dir: <repo_root>/Config
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


PACKAGING_SECTION = "/Script/UnrealEd.ProjectPackagingSettings"
REQUIRED_NEVER_COOK_PATHS = {
    "/MetaHuman/GenericTracker",
    "/MetaHuman/Solver",
    "/MetaHumanCoreTech/GenericTracker",
    "/MetaHumanCoreTech/RealtimeMono",
}


class ErrorCollector:
    """Matches reporting.py pattern from data/_lib."""

    def __init__(self) -> None:
        self._errors: list[str] = []
        self._warnings: list[str] = []

    def error(self, file_label: str, key: str, message: str) -> None:
        self._errors.append(f"{file_label}: {key} {message}")

    def warn(self, file_label: str, key: str, message: str) -> None:
        self._warnings.append(f"{file_label}: {key} {message}")

    def print_summary(self, label: str) -> int:
        """Print results and return exit code (0=pass, 1=fail)."""
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


# --- Rules ---
# Each rule: (ini_file, section, key, check_fn, severity, message)
# check_fn receives the value string; returns True if the value is BAD.

def _is_true(v: str) -> bool:
    return v.strip().lower() in ("true", "1", "yes")


def _is_false(v: str) -> bool:
    return v.strip().lower() in ("false", "0", "no")


def _tsr_history_too_high(v: str) -> bool:
    try:
        return int(v.strip()) > 100
    except ValueError:
        return False


RULES: list[dict] = [
    # PSO disk cache must be enabled
    {
        "file": "DefaultEngine.ini",
        "section": "ConsoleVariables",
        "key": "r.D3D12.PSO.DiskCache",
        "check": _is_false,
        "severity": "error",
        "message": "PSO disk cache disabled - causes shader hitches every session",
    },
    {
        "file": "DefaultEngine.ini",
        "section": "ConsoleVariables",
        "key": "r.D3D12.PSO.DriverOptimizedDiskCache",
        "check": _is_false,
        "severity": "error",
        "message": "driver-optimized PSO cache disabled - loses cross-session PSO reuse",
    },
    # Path tracing should be off for gameplay
    {
        "file": "DefaultEngine.ini",
        "section": "/Script/Engine.RendererSettings",
        "key": "r.PathTracing",
        "check": _is_true,
        "severity": "error",
        "message": "path tracing enabled - adds GPU overhead in all Shipping sessions",
    },
    # TSR history should not be 200%
    {
        "file": "DefaultEngine.ini",
        "section": "ConsoleVariables",
        "key": "r.TSR.History.ScreenPercentage",
        "check": _tsr_history_too_high,
        "severity": "warn",
        "message": "TSR history > 100% - doubles internal resolution, hitch-sensitive",
    },
    # Asset registry dependencies must be serialized for preloading
    {
        "file": "DefaultEngine.ini",
        "section": "AssetRegistry",
        "key": "bSerializeDependencies",
        "check": _is_false,
        "severity": "error",
        "message": "dependency serialization disabled - Phase 3 preloading will find 0 assets",
    },
    # DefaultGame.ini packaging
    {
        "file": "DefaultGame.ini",
        "section": "/Script/UnrealEd.ProjectPackagingSettings",
        "key": "IncludeDebugFiles",
        "check": _is_true,
        "severity": "warn",
        "message": "debug files included in package - bloats distributable size",
    },
]


def parse_ini_loose(path: Path) -> dict[str, dict[str, str]]:
    """Parse UE ini with duplicate-key tolerance.

    UE ini files use + prefix for array appends and may have duplicate keys.
    Standard configparser chokes on these. We do a simple parse that keeps
    the last value for each key (sufficient for flag validation).
    """
    sections: dict[str, dict[str, str]] = {}
    current_section = ""
    try:
        text = path.read_text(encoding="utf-8-sig")
    except FileNotFoundError:
        return sections

    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        # Section header
        m = re.match(r"^\[(.+)\]$", line)
        if m:
            current_section = m.group(1)
            sections.setdefault(current_section, {})
            continue
        # Key=Value (skip + prefixed array entries)
        if line.startswith("+") or line.startswith("-") or line.startswith("!"):
            continue
        m = re.match(r"^([^=]+?)=(.*)$", line)
        if m:
            key = m.group(1).strip()
            val = m.group(2).strip()
            # Strip inline comments
            comment_idx = val.find(";")
            if comment_idx >= 0:
                val = val[:comment_idx].strip()
            sections.setdefault(current_section, {})[key] = val

    return sections


def parse_ue_array_paths(path: Path, section: str, key: str) -> set[str]:
    """Return Path values from UE +Key=(Path="...") array entries."""
    try:
        text = path.read_text(encoding="utf-8-sig")
    except FileNotFoundError:
        return set()

    current_section = ""
    values: set[str] = set()
    pattern = re.compile(
        rf'^\+{re.escape(key)}=\(Path="(?P<path>/[A-Za-z0-9_/-]+)"\)\s*$'
    )
    for raw_line in text.splitlines():
        line = raw_line.strip()
        section_match = re.match(r"^\[(.+)\]$", line)
        if section_match:
            current_section = section_match.group(1)
            continue
        if current_section != section:
            continue
        value_match = pattern.match(line)
        if value_match:
            values.add(value_match.group("path"))
    return values


def validate_cook_policy(config_dir: Path, errors: ErrorCollector) -> None:
    game_ini = config_dir / "DefaultGame.ini"
    always_cook = parse_ue_array_paths(
        game_ini, PACKAGING_SECTION, "DirectoriesToAlwaysCook"
    )
    never_cook = parse_ue_array_paths(
        game_ini, PACKAGING_SECTION, "DirectoriesToNeverCook"
    )

    if "/ProjectObject" in always_cook:
        errors.error(
            "DefaultGame.ini",
            "DirectoriesToAlwaysCook",
            "must not force the whole /ProjectObject mount; definitions own their cook closure",
        )

    missing_exclusions = sorted(REQUIRED_NEVER_COOK_PATHS - never_cook)
    if missing_exclusions:
        errors.error(
            "DefaultGame.ini",
            "DirectoriesToNeverCook",
            "missing Shipping-only MetaHuman authoring exclusions: "
            + ", ".join(missing_exclusions),
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate shipping ini config")
    parser.add_argument(
        "--config-dir",
        default=None,
        help="Path to Config/ directory (default: auto-detect from repo root)",
    )
    args = parser.parse_args()

    if args.config_dir:
        config_dir = Path(args.config_dir)
    else:
        # Walk up from script location to repo root
        script_dir = Path(__file__).resolve().parent
        repo_root = script_dir.parent.parent.parent.parent  # check/config -> check -> ue -> scripts -> root
        config_dir = repo_root / "Config"

    if not config_dir.is_dir():
        print(f"ERROR: Config directory not found: {config_dir}")
        return 1

    errors = ErrorCollector()

    # Cache parsed files
    parsed: dict[str, dict[str, dict[str, str]]] = {}

    for rule in RULES:
        ini_name = rule["file"]
        if ini_name not in parsed:
            parsed[ini_name] = parse_ini_loose(config_dir / ini_name)

        ini_data = parsed[ini_name]
        section = rule["section"]
        key = rule["key"]

        if section not in ini_data or key not in ini_data[section]:
            # Key absent - some rules care about absence
            continue

        value = ini_data[section][key]
        if rule["check"](value):
            msg = f"= {value} -- {rule['message']}"
            if rule["severity"] == "error":
                errors.error(ini_name, key, msg)
            else:
                errors.warn(ini_name, key, msg)

    validate_cook_policy(config_dir, errors)

    return errors.print_summary("Shipping ini validation")


if __name__ == "__main__":
    raise SystemExit(main())
