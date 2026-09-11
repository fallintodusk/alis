from __future__ import annotations

import json
import math
import tempfile
from pathlib import Path

from .contracts import ExecutionEnvironmentError
from .toolchain import require_tools, run_tool


def transform_points(
    repo_root: Path,
    points: list[list[float]],
    source_crs: str,
    target_crs: str,
) -> list[list[float]]:
    scratch_parent = repo_root / "tmp" / "world" / "execution_environment" / "geospatial"
    scratch_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch_parent) as directory:
        scratch = Path(directory)
        source_path = scratch / "source.geojson"
        output_path = scratch / "projected.geojson"
        source_path.write_text(json.dumps({
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {"point_index": index},
                    "geometry": {"type": "Point", "coordinates": point},
                }
                for index, point in enumerate(points)
            ],
        }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
        try:
            require_tools(repo_root)
            run_tool(repo_root, "ogr2ogr", [
                "-overwrite", "-dim", "XY",
                "-s_srs", source_crs,
                "-t_srs", target_crs,
                "-f", "GeoJSON",
                str(output_path), str(source_path),
            ])
            document = json.loads(output_path.read_text(encoding="utf-8"))
            transformed = sorted(
                document["features"],
                key=lambda feature: int(feature["properties"]["point_index"]),
            )
            coordinates = [feature["geometry"]["coordinates"][:2] for feature in transformed]
        except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
            raise ExecutionEnvironmentError(
                "coordinate_transform_failed", "Pinned OGR returned invalid transformed points"
            ) from error
    if len(coordinates) != len(points) or any(
        len(point) != 2 or not all(math.isfinite(float(value)) for value in point)
        for point in coordinates
    ):
        raise ExecutionEnvironmentError(
            "coordinate_transform_failed", "Pinned OGR returned incomplete transformed points"
        )
    return [[float(value) for value in point] for point in coordinates]
