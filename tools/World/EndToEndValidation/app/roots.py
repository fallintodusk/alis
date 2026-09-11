from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]


class WorldDataRootError(RuntimeError):
    pass


@dataclass(frozen=True)
class WorldDataRoots:
    plugin_name: str
    plugin_root: Path
    content_root: Path
    presentation_root: Path
    data_root: Path


def resolve_world_data_roots(plugin_name: str) -> WorldDataRoots:
    if re.fullmatch(r"Project[A-Za-z0-9]+", plugin_name) is None:
        raise WorldDataRootError("World-data owner is not a Project* plugin token")
    matches = [
        path
        for path in (REPO_ROOT / "Plugins").rglob(f"{plugin_name}.uplugin")
        if path.stem == plugin_name
    ]
    if len(matches) != 1:
        raise WorldDataRootError("World-data owner must resolve to one project plugin descriptor")
    try:
        descriptor = json.loads(matches[0].read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise WorldDataRootError("World-data plugin descriptor is unreadable") from error
    if descriptor.get("CanContainContent") is not True:
        raise WorldDataRootError("World-data owner must be content-capable")
    plugin_root = matches[0].parent.resolve()
    content_root = plugin_root / "Content"
    return WorldDataRoots(
        plugin_name=plugin_name,
        plugin_root=plugin_root,
        content_root=content_root,
        presentation_root=content_root / "Generated" / "Presentation",
        data_root=plugin_root / "Data",
    )


def resolve_owned_data_path(data_root: Path, value: str) -> Path:
    supplied = Path(value)
    candidate = supplied if supplied.is_absolute() else REPO_ROOT / supplied
    resolved = candidate.resolve()
    if not resolved.is_relative_to(data_root.resolve()) or not resolved.is_file():
        raise WorldDataRootError("Profile path is not an existing file under the declared Data root")
    return resolved
