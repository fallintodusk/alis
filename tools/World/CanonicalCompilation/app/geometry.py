from __future__ import annotations

from typing import Any


def _orientation(a: list[float], b: list[float], c: list[float]) -> float:
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def _on_segment(a: list[float], b: list[float], point: list[float]) -> bool:
    return (
        min(a[0], b[0]) <= point[0] <= max(a[0], b[0])
        and min(a[1], b[1]) <= point[1] <= max(a[1], b[1])
    )


def _segments_intersect(a: list[float], b: list[float], c: list[float], d: list[float]) -> bool:
    o1, o2 = _orientation(a, b, c), _orientation(a, b, d)
    o3, o4 = _orientation(c, d, a), _orientation(c, d, b)
    if (o1 > 0 > o2 or o1 < 0 < o2) and (o3 > 0 > o4 or o3 < 0 < o4):
        return True
    return (
        (o1 == 0 and _on_segment(a, b, c))
        or (o2 == 0 and _on_segment(a, b, d))
        or (o3 == 0 and _on_segment(c, d, a))
        or (o4 == 0 and _on_segment(c, d, b))
    )


def _ring_reason(ring: list[list[float]], check_self_intersections: bool) -> str | None:
    if len(ring) < 4 or ring[0] != ring[-1]:
        return "polygon_not_closed"
    area = sum(
        ring[index][0] * ring[index + 1][1] - ring[index + 1][0] * ring[index][1]
        for index in range(len(ring) - 1)
    )
    if area == 0:
        return "polygon_zero_area"
    if not check_self_intersections:
        return None
    segment_count = len(ring) - 1
    for left in range(segment_count):
        for right in range(left + 1, segment_count):
            if right in {left - 1, left, left + 1} or {left, right} == {0, segment_count - 1}:
                continue
            if _segments_intersect(ring[left], ring[left + 1], ring[right], ring[right + 1]):
                return "polygon_self_intersection"
    return None


def geometry_rejection_reason(
    geometry: dict[str, Any],
    feature_class: str,
    check_self_intersections: bool = True,
) -> str | None:
    geometry_type = geometry.get("type")
    coordinates = geometry.get("coordinates")
    if feature_class == "foliage_point":
        points = coordinates if geometry_type == "MultiPoint" else [coordinates]
        if geometry_type not in {"Point", "MultiPoint"} or not points or not all(
            isinstance(point, list)
            and len(point) >= 2
            and all(isinstance(value, (int, float)) for value in point[:2])
            for point in points
        ):
            return "invalid_foliage_point_geometry"
        return None
    if feature_class == "road" or (feature_class == "water" and geometry_type in {"LineString", "MultiLineString"}):
        lines = coordinates if geometry_type == "MultiLineString" else [coordinates]
        if geometry_type not in {"LineString", "MultiLineString"} or not all(
            isinstance(line, list) and len(line) >= 2 and len({tuple(point) for point in line}) >= 2 for line in lines
        ):
            return f"invalid_{feature_class}_geometry"
        return None
    polygons = coordinates if geometry_type == "MultiPolygon" else [coordinates]
    if geometry_type not in {"Polygon", "MultiPolygon"} or not isinstance(polygons, list) or not polygons:
        return f"invalid_{feature_class}_geometry"
    for polygon in polygons:
        if not isinstance(polygon, list) or not polygon:
            return f"invalid_{feature_class}_geometry"
        for ring in polygon:
            reason = _ring_reason(ring, check_self_intersections)
            if reason:
                return reason
    return None


def _clip_segment(
    start: list[float], end: list[float], bounds: tuple[float, float, float, float]
) -> tuple[list[float], list[float]] | None:
    dx, dy = end[0] - start[0], end[1] - start[1]
    p_values = (-dx, dx, -dy, dy)
    q_values = (
        start[0] - bounds[0],
        bounds[2] - start[0],
        start[1] - bounds[1],
        bounds[3] - start[1],
    )
    lower, upper = 0.0, 1.0
    for p_value, q_value in zip(p_values, q_values, strict=True):
        if p_value == 0:
            if q_value < 0:
                return None
            continue
        ratio = q_value / p_value
        if p_value < 0:
            lower = max(lower, ratio)
        else:
            upper = min(upper, ratio)
        if lower > upper:
            return None
    return (
        [start[0] + lower * dx, start[1] + lower * dy],
        [start[0] + upper * dx, start[1] + upper * dy],
    )


def clip_line(
    coordinates: list[list[float]], bounds: tuple[float, float, float, float], coordinate_step: float
) -> list[list[list[float]]]:
    fragments: list[list[list[float]]] = []
    current: list[list[float]] = []
    places = max(0, len(str(coordinate_step).split(".")[-1]))
    for start, end in zip(coordinates, coordinates[1:], strict=False):
        clipped = _clip_segment(start, end, bounds)
        if clipped is None:
            if len(current) >= 2:
                fragments.append(current)
            current = []
            continue
        first = [round(value, places) for value in clipped[0]]
        second = [round(value, places) for value in clipped[1]]
        if not current or current[-1] != first:
            if len(current) >= 2:
                fragments.append(current)
            current = [first]
        if current[-1] != second:
            current.append(second)
    if len(current) >= 2:
        fragments.append(current)
    return fragments
