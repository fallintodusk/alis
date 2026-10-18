"""Pinned GEOS polygon partition and constrained triangulation, without World policy."""

import ctypes as C
import json
import math
import os
from pathlib import Path

from ...app.contracts import ExecutionEnvironmentError
from ...app.toolchain import require_tools, tool_environment


class _Geometry:
    def __init__(self, repo_root: Path):
        require_tools(repo_root)
        _, tools = tool_environment(repo_root)
        directory = tools["ogr2ogr"].parents[2]
        self.directory = os.add_dll_directory(str(directory))
        self.context = None
        self.owned = []
        try:
            self.library = C.CDLL(str(directory / "geos_c.dll"))
            p = C.c_void_p
            self.init = self.bind("GEOS_init_r", [], p)
            self.finish = self.bind("GEOS_finish_r", [p], None)
            self.destroy = self.bind("GEOSGeom_destroy_r", [p, p], None)
            self.reader_new = self.bind("GEOSGeoJSONReader_create_r", [p], p)
            self.reader_read = self.bind("GEOSGeoJSONReader_readGeometry_r", [p, p, C.c_char_p], p)
            self.reader_free = self.bind("GEOSGeoJSONReader_destroy_r", [p, p], None)
            self.intersection = self.bind("GEOSIntersection_r", [p, p, p], p)
            self.difference = self.bind("GEOSDifference_r", [p, p, p], p)
            self.cdt = self.bind("GEOSConstrainedDelaunayTriangulation_r", [p, p], p)
            self.count = self.bind("GEOSGetNumGeometries_r", [p, p], C.c_int)
            self.part = self.bind("GEOSGetGeometryN_r", [p, p, C.c_int], p)
            self.ring = self.bind("GEOSGetExteriorRing_r", [p, p], p)
            self.sequence = self.bind("GEOSGeom_getCoordSeq_r", [p, p], p)
            self.size = self.bind("GEOSCoordSeq_getSize_r", [p, p, C.POINTER(C.c_uint)], C.c_int)
            self.x = self.bind("GEOSCoordSeq_getX_r", [p, p, C.c_uint, C.POINTER(C.c_double)], C.c_int)
            self.y = self.bind("GEOSCoordSeq_getY_r", [p, p, C.c_uint, C.POINTER(C.c_double)], C.c_int)
            self.area = self.bind("GEOSArea_r", [p, p, C.POINTER(C.c_double)], C.c_int)
            self.distance = self.bind("GEOSDistance_r", [p, p, p, C.POINTER(C.c_double)], C.c_int)
            self.valid = self.bind("GEOSisValid_r", [p, p], C.c_byte)
            self.orientation = self.bind("GEOSOrientationIndex_r", [p, *[C.c_double] * 6], C.c_int)
            self.context = self.init()
            if not self.context:
                self.directory.close()
                raise ExecutionEnvironmentError("native_geometry_failed", "GEOS context creation failed")
        except (OSError, AttributeError):
            self.close()
            raise ExecutionEnvironmentError("native_geometry_unavailable", "Pinned GEOS API cannot be loaded") from None

    def bind(self, name, arguments, result):
        function = getattr(self.library, name)
        function.argtypes, function.restype = arguments, result
        return function

    def own(self, geometry):
        if not geometry:
            raise ExecutionEnvironmentError("native_geometry_failed", "GEOS operation returned no geometry")
        self.owned.append(geometry)
        return geometry

    def read(self, value):
        reader = self.reader_new(self.context)
        if not reader:
            raise ExecutionEnvironmentError("native_geometry_failed", "GEOS reader creation failed")
        try:
            result = self.own(self.reader_read(self.context, reader, json.dumps(value, allow_nan=False).encode()))
        finally:
            self.reader_free(self.context, reader)
        if self.valid(self.context, result) != 1:
            raise ExecutionEnvironmentError("native_geometry_invalid", "Polygon topology is invalid")
        return result

    def measure(self, geometry):
        area = C.c_double()
        if self.area(self.context, geometry, C.byref(area)) != 1 or not math.isfinite(area.value):
            raise ExecutionEnvironmentError("native_geometry_failed", "GEOS area query failed")
        return area.value

    def distance_to(self, point, geometry):
        shape = self.read({"type": "Point", "coordinates": point})
        distance = C.c_double()
        if (self.distance(self.context, shape, geometry, C.byref(distance)) != 1 or
                not math.isfinite(distance.value) or distance.value < 0):
            raise ExecutionEnvironmentError("native_geometry_failed", "GEOS distance query failed")
        return distance.value

    def triangles(self, region, maximum_triangles):
        mesh = self.own(self.cdt(self.context, region))
        count = self.count(self.context, mesh)
        if count < 0 or count > maximum_triangles:
            raise ExecutionEnvironmentError("native_geometry_limit", "Constrained output exceeds triangle limit")
        result = []
        remaining = region
        for index in range(count):
            coordinates = self.sequence(self.context, self.ring(self.context, self.part(self.context, mesh, index)))
            length = C.c_uint()
            if not coordinates or self.size(self.context, coordinates, C.byref(length)) != 1 or length.value != 4:
                raise ExecutionEnvironmentError("native_geometry_failed", "GEOS output is not a triangle")
            points = []
            for corner in range(3):
                x, y = C.c_double(), C.c_double()
                if not self.x(self.context, coordinates, corner, C.byref(x)) or not self.y(self.context, coordinates, corner, C.byref(y)):
                    raise ExecutionEnvironmentError("native_geometry_failed", "GEOS coordinate query failed")
                if not all(math.isfinite(v) for v in (x.value, y.value)):
                    raise ExecutionEnvironmentError("native_geometry_failed", "GEOS returned nonfinite coordinates")
                points.append([x.value, y.value])
            # Check coverage geometrically, not only the sum of triangle areas.
            triangle = self.read({"type": "Polygon", "coordinates": [[*points, points[0]]]})
            outside = self.own(self.difference(self.context, triangle, region))
            if self.measure(outside) > max(1e-10, self.measure(triangle) * 1e-12):
                raise ExecutionEnvironmentError("native_geometry_coverage", "Constrained triangle leaves its region")
            remaining = self.own(self.difference(self.context, remaining, triangle))
            result.append(points)
        total = sum(abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])) / 2 for a, b, c in result)
        if abs(total - self.measure(region)) > max(1e-9, self.measure(region) * 1e-12):
            raise ExecutionEnvironmentError("native_geometry_coverage", "Constrained triangles do not cover the region")
        if self.measure(remaining) > max(1e-9, self.measure(region) * 1e-12):
            raise ExecutionEnvironmentError("native_geometry_coverage", "Constrained triangles leave missing coverage")
        return result

    def close(self):
        if self.context:
            for geometry in reversed(self.owned):
                self.destroy(self.context, geometry)
            self.finish(self.context)
            self.context = None
        self.directory.close()


def _local_polygon(geometry, origin):
    kind = geometry.get("type")
    if kind not in {"Polygon", "MultiPolygon"}:
        raise ExecutionEnvironmentError("native_geometry_input", "Expected polygonal geometry")
    def position(point):
        if len(point) < 2 or any(isinstance(v, bool) or not isinstance(v, (int, float))
                                 or not math.isfinite(v) for v in point[:2]):
            raise ExecutionEnvironmentError("native_geometry_input", "Expected finite polygon coordinates")
        return [point[0] - origin[0], point[1] - origin[1], *point[2:]]
    polygons = [geometry["coordinates"]] if kind == "Polygon" else geometry["coordinates"]
    shifted = [[[position(point) for point in ring] for ring in polygon] for polygon in polygons]
    return {"type": kind, "coordinates": shifted[0] if kind == "Polygon" else shifted}


def _convex_exterior_bound(native, region, points, parent):
    if len(parent) != 3 or len(points) != 3 or native.measure(region) <= 0:
        raise ExecutionEnvironmentError("native_geometry_coverage", "Convex proof requires a nonempty triangle")
    distances = [native.distance_to(point, region) for point in points]
    perimeter = sum(math.dist(parent[i], parent[(i + 1) % 3]) for i in range(3))
    if perimeter <= 0 or any(not math.isfinite(v) or v < 0 for v in [*distances, perimeter]):
        raise ExecutionEnvironmentError("native_geometry_coverage", "Convex containment proof is not finite")
    radius = math.nextafter(max(distances), math.inf)
    # Distance to a convex parent is convex. Its expanded exterior has this Steiner bound.
    bound = math.nextafter(perimeter * radius + math.pi * radius * radius, math.inf)
    if not math.isfinite(bound) or bound < 0:
        raise ExecutionEnvironmentError("native_geometry_coverage", "Convex containment bound overflowed")
    return bound


def _validate_cover(native, region, triangles, roundtrip_area_bound=0.0, convex_parent=None):
    tolerance = max(1e-9, native.measure(region) * 1e-12) + roundtrip_area_bound
    if not math.isfinite(roundtrip_area_bound) or roundtrip_area_bound < 0 or not math.isfinite(tolerance):
        raise ExecutionEnvironmentError("native_geometry_coverage", "Coverage round-trip bound is not finite")
    remaining, total = region, 0.0
    for points in triangles:
        triangle = native.read({"type": "Polygon", "coordinates": [[*points, points[0]]]})
        total += native.measure(triangle)
        outside = native.own(native.difference(native.context, triangle, region))
        if native.measure(outside) > 1e-9 + roundtrip_area_bound:
            # Overlay can misclassify a whole collinear interior face. Certify containment
            # independently at the same area limit, only for a known convex triangle.
            if convex_parent is None or _convex_exterior_bound(native, region, points, convex_parent) > 1e-9 + roundtrip_area_bound:
                raise ExecutionEnvironmentError("native_geometry_coverage", "Triangle leaves the selected domain")
        remaining = native.own(native.difference(native.context, remaining, triangle))
    if abs(total - native.measure(region)) > tolerance or native.measure(remaining) > tolerance:
        raise ExecutionEnvironmentError("native_geometry_coverage", "Triangle coverage has holes or overlaps")


def _roundtrip_area_bound(original, restored):
    bound = 0.0
    for before, after in zip(original, restored, strict=True):
        displacement = max(math.dist(a, b) for a, b in zip(before, after, strict=True))
        perimeter = max(sum(math.dist(points[i], points[(i + 1) % 3]) for i in range(3))
                        for points in (before, after))
        # Each perturbed triangle lies within the other's displacement buffer.
        bound += 2 * perimeter * displacement + 2 * math.pi * displacement * displacement
    if not math.isfinite(bound) or bound < 0:
        raise ExecutionEnvironmentError("native_geometry_coverage", "Coordinate round-trip bound is not finite")
    return bound


def _noncollapsed_indices(native, triangles):
    result = []
    for index, points in enumerate(triangles):
        orientation = native.orientation(native.context, *[value for point in points for value in point])
        if orientation not in (-1, 0, 1):
            raise ExecutionEnvironmentError("native_geometry_failed", "GEOS orientation query failed")
        if orientation:
            result.append(index)
    return result


def partition_triangles(repo_root: Path, triangle: list, footprint: dict, maximum_triangles: int) -> dict:
    """Partition one XY triangle by a polygon, preserving its exterior and holes."""
    if len(triangle) != 3 or maximum_triangles < 1 or len(json.dumps(footprint)) > 4_000_000:
        raise ExecutionEnvironmentError("native_geometry_input", "Invalid or oversized partition request")
    origin = triangle[0][:2]
    # GEOS overlay can lose thin intersections at large projected coordinates.
    # Translation preserves the source geometry without a precision grid or snap.
    domain_input = _local_polygon({"type": "Polygon", "coordinates": [[*triangle, triangle[0]]]}, origin)
    clip_input = _local_polygon(footprint, origin)
    native = _Geometry(repo_root)
    try:
        domain = native.read(domain_input)
        clip = native.read(clip_input)
        wet = native.own(native.intersection(native.context, domain, clip))
        dry = native.own(native.difference(native.context, domain, clip))
        regions = (("wet", wet), ("dry", dry))
        local = {role: native.triangles(region, maximum_triangles) for role, region in regions}
        parent = domain_input["coordinates"][0][:-1]
        _validate_cover(native, domain, local["wet"] + local["dry"], convex_parent=parent)
        result = {role: [[[p[0] + origin[0], p[1] + origin[1]] for p in face]
                         for face in faces] for role, faces in local.items()}
        restored = {role: [_local_polygon({"type": "Polygon", "coordinates": [[*face, face[0]]]}, origin)
                            ["coordinates"][0][:-1] for face in faces] for role, faces in result.items()}
        bounds = {role: _roundtrip_area_bound(local[role], restored[role]) for role, _ in regions}
        # Canonical float64 addition can collapse a native sub-ULP triangle. Keep its
        # measured displacement bound; validate all remaining coverage before returning.
        for role, _ in regions:
            retained = _noncollapsed_indices(native, restored[role])
            result[role] = [result[role][index] for index in retained]
            restored[role] = [restored[role][index] for index in retained]
        for role, region in regions:
            _validate_cover(native, region, restored[role], bounds[role])
        _validate_cover(native, domain, restored["wet"] + restored["dry"], sum(bounds.values()), parent)
        return result
    finally:
        native.close()


def validate_triangle_cover(repo_root: Path, polygon: dict, triangles: list) -> None:
    """Reject missing, overlapping or exterior triangle coverage of a polygon."""
    coordinates = polygon["coordinates"]
    origin = coordinates[0][0] if polygon.get("type") == "Polygon" else coordinates[0][0][0]
    local_polygon = _local_polygon(polygon, origin)
    native = _Geometry(repo_root)
    try:
        local_triangles = [_local_polygon({"type": "Polygon", "coordinates": [[*points, points[0]]]}, origin)
                           ["coordinates"][0][:-1] for points in triangles]
        rings = local_polygon["coordinates"]
        parent = (rings[0][:-1] if local_polygon["type"] == "Polygon" and len(rings) == 1 and
                  len(rings[0]) == 4 and rings[0][0] == rings[0][-1] else None)
        _validate_cover(native, native.read(local_polygon), local_triangles, convex_parent=parent)
    finally:
        native.close()
