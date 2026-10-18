"""Pinned native geometry capability, separate from base acquisition/compile identity."""
import hashlib
import json
from pathlib import Path

from ..api import execution_identity as base_execution_identity
from .app.kernel import partition_triangles, validate_triangle_cover
from .app.surfaces import triangulate_points, polygon_union, covered_points


def execution_identity(repo_root: Path, verify_native_tools: bool) -> dict:
    base = base_execution_identity(repo_root, verify_native_tools)
    root = Path(__file__).resolve().parent
    files = [root / "api.py", *sorted((root / "app").glob("*.py"))]
    implementation = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
    payload = json.dumps({"base_execution": base, "geometry_implementation": implementation},
                         sort_keys=True, separators=(",", ":")).encode("utf-8")
    return {**base, "identity_sha256": hashlib.sha256(payload).hexdigest()}


__all__ = ("execution_identity", "partition_triangles", "validate_triangle_cover", "triangulate_points", "polygon_union", "covered_points")
