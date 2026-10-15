"""Pinned GEOS polygon partition and constrained triangulation, without World policy."""

import ctypes as C
import json
import math
import os
from pathlib import Path

from .contracts import ExecutionEnvironmentError
from .toolchain import require_tools, tool_environment


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
            self.valid = self.bind("GEOSisValid_r", [p, p], C.c_byte)
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


def partition_triangles(repo_root: Path, triangle: list, footprint: dict, maximum_triangles: int) -> dict:
    """Partition one XY triangle by a polygon, preserving its exterior and holes."""
    if len(triangle) != 3 or maximum_triangles < 1 or len(json.dumps(footprint)) > 4_000_000:
        raise ExecutionEnvironmentError("native_geometry_input", "Invalid or oversized partition request")
    native = _Geometry(repo_root)
    try:
        domain = native.read({"type": "Polygon", "coordinates": [[*triangle, triangle[0]]]})
        clip = native.read(footprint)
        wet = native.own(native.intersection(native.context, domain, clip))
        dry = native.own(native.difference(native.context, domain, clip))
        return {"wet": native.triangles(wet, maximum_triangles),
                "dry": native.triangles(dry, maximum_triangles)}
    finally:
        native.close()


def validate_triangle_cover(repo_root: Path, polygon: dict, triangles: list) -> None:
    """Reject missing, overlapping or exterior triangle coverage of a polygon."""
    native = _Geometry(repo_root)
    try:
        region = native.read(polygon)
        remaining, total = region, 0.0
        for points in triangles:
            triangle = native.read({"type": "Polygon", "coordinates": [[*points, points[0]]]})
            total += native.measure(triangle)
            outside = native.own(native.difference(native.context, triangle, region))
            if native.measure(outside) > 1e-9:
                raise ExecutionEnvironmentError("native_geometry_coverage", "Triangle leaves the selected domain")
            remaining = native.own(native.difference(native.context, remaining, triangle))
        tolerance = max(1e-9, native.measure(region) * 1e-12)
        if abs(total - native.measure(region)) > tolerance or native.measure(remaining) > tolerance:
            raise ExecutionEnvironmentError("native_geometry_coverage", "Triangle coverage has holes or overlaps")
    finally:
        native.close()
