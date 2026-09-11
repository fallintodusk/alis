from __future__ import annotations

import json
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Any

from World.ExecutionEnvironment.api import run_tool

from .contracts import IngestionError, file_hash


def run_json(repo_root: Path, tool: str, arguments: list[str]) -> dict[str, Any]:
    output = run_tool(repo_root, tool, arguments)
    try:
        value = json.loads(output)
    except json.JSONDecodeError as exc:
        raise IngestionError(
            "invalid_tool_output", "External source tool returned invalid JSON", tool=tool
        ) from exc
    if not isinstance(value, dict):
        raise IngestionError(
            "invalid_tool_output", "External source tool returned a non-object JSON value", tool=tool
        )
    return value


def portable_metadata(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: portable_metadata(item) for key, item in value.items()}
    if isinstance(value, list):
        return [portable_metadata(item) for item in value]
    if isinstance(value, str) and (
        PureWindowsPath(value).is_absolute() or PurePosixPath(value).is_absolute()
    ):
        return PureWindowsPath(value).name or PurePosixPath(value).name
    return value


def data_artifact(document_path: Path, data_path: Path, artifact_format: str) -> dict[str, Any]:
    try:
        relative = data_path.resolve().relative_to(document_path.parent.resolve())
    except ValueError as exc:
        raise IngestionError("invalid_output_path", "Raster artifact escaped its document directory") from exc
    return {
        "path_base": "document_parent",
        "path": relative.as_posix(),
        "format": artifact_format,
        "sha256": file_hash(data_path),
        "byte_size": data_path.stat().st_size,
    }
