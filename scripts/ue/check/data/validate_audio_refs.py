#!/usr/bin/env python3
"""Validate audio cross-references: AudioPresetAsset capability properties.

Check:
  8. AudioPresetAsset capability property -> AUDIO_*.json file must exist
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _lib.discovery import discover_audio_presets, repo_root
from _lib.reporting import ErrorCollector


def _extract_audio_name_from_asset_path(asset_path: str) -> str | None:
    """Extract AUDIO preset filename from UE asset path.

    '/ProjectObject/Dir/AUDIO_Gramophone.AUDIO_Gramophone' -> 'AUDIO_Gramophone'
    """
    path = asset_path.strip()
    segments = path.rsplit("/", 1)
    last = segments[-1] if len(segments) > 1 else segments[0]
    name = last.split(".", 1)[0] if "." in last else last
    return name if name else None


def _validate_audio_preset_refs(
    audio_presets: dict[str, Path],
    errors: ErrorCollector,
) -> None:
    content_root = repo_root() / "Plugins" / "Resources" / "ProjectObject" / "Content"
    if not content_root.exists():
        return

    for path in content_root.rglob("*.json"):
        if path.stem.startswith(("DLG_", "AUDIO_", "Loot_")):
            continue
        try:
            with path.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception:
            continue
        if not isinstance(data, dict):
            continue

        capabilities = data.get("capabilities")
        if not isinstance(capabilities, list):
            continue

        for cap_idx, cap in enumerate(capabilities):
            if not isinstance(cap, dict):
                continue
            props = cap.get("properties")
            if not isinstance(props, dict):
                continue

            asset_path = props.get("AudioPresetAsset")
            if not isinstance(asset_path, str) or not asset_path.strip():
                continue

            audio_name = _extract_audio_name_from_asset_path(asset_path)
            if audio_name and audio_name not in audio_presets:
                errors.add(
                    path.name,
                    f"capabilities[{cap_idx}].properties.AudioPresetAsset",
                    f"references non-existent audio preset '{audio_name}'",
                )


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

def validate_audio_refs(errors: ErrorCollector) -> None:
    audio_presets = discover_audio_presets()
    _validate_audio_preset_refs(audio_presets, errors)


def main() -> int:
    errors = ErrorCollector()
    print("Validating audio references...")
    validate_audio_refs(errors)
    return errors.print_summary("Audio reference validation")


if __name__ == "__main__":
    raise SystemExit(main())
