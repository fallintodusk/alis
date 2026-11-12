"""Native finite planar arrangements; no height, source-role or admission policy."""
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path

from ..app.contracts import ExecutionEnvironmentError
from .app.kernel import _Geometry, _local_polygon, _roundtrip_area_bound, _validate_cover
from .api import execution_identity as parent_identity


def execution_identity(repo_root, verify_native_tools):
    """Only consumers of this optional operation acquire its implementation identity."""
    parent = parent_identity(repo_root, verify_native_tools)
    payload = {'parent': parent, 'constrained_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    return {**parent, 'identity_sha256': digest}


def _refuse(message):
    raise ExecutionEnvironmentError('native_geometry_constraints', message)


def _point(value):
    return (isinstance(value, list) and len(value) == 2 and all(
        type(v) in (int, float) and math.isfinite(v) for v in value))


def _edge(a, b):
    return tuple(sorted((tuple(a), tuple(b))))


def _coordinates(native, geometry):
    sequence = native.sequence(native.context, geometry)
    size = C.c_uint()
    if not sequence or native.size(native.context, sequence, C.byref(size)) != 1:
        _refuse('Native line coordinate sequence is unavailable')
    result = []
    for i in range(size.value):
        x, y = C.c_double(), C.c_double()
        if native.x(native.context, sequence, i, C.byref(x)) != 1 or native.y(native.context, sequence, i, C.byref(y)) != 1:
            _refuse('Native line coordinate read failed')
        result.append([x.value, y.value])
    return result


def _lines(native, geometry):
    kind = native.bind('GEOSGeomTypeId_r', [C.c_void_p, C.c_void_p], C.c_int)(native.context, geometry)
    if kind in (1, 2):
        return [_coordinates(native, geometry)]
    if kind not in (5, 7):
        _refuse('Native arrangement contains a non-linear component')
    count = native.count(native.context, geometry)
    if count < 0:
        _refuse('Native line collection count failed')
    return [line for i in range(count) for line in _lines(native, native.part(native.context, geometry, i))]


def _closed_cells(native, arrangement, mandatory):
    """Return a complete closed-cell graph, or None for native cut/dangle linework."""
    p = C.c_void_p
    cuts, dangles, invalid = p(), p(), p()
    polygonize = native.bind('GEOSPolygonize_full_r', [p, p, C.POINTER(p), C.POINTER(p), C.POINTER(p)], p)
    regions = native.own(polygonize(native.context, arrangement, C.byref(cuts), C.byref(dangles), C.byref(invalid)))
    diagnostics = [native.own(value) for value in (cuts, dangles, invalid)]
    empty = native.bind('GEOSisEmpty_r', [p, p], C.c_byte)
    states = [empty(native.context, value) for value in diagnostics]
    if any(value not in (0, 1) for value in states):
        _refuse('Native polygonization diagnostics are unknown')
    if states[2] != 1:
        _refuse('Native polygonization contains invalid rings')
    if states[:2] != [1, 1]:
        return None
    required = {_edge(a, b) for line in _lines(native, arrangement) for a, b in zip(line, line[1:])}
    edges, vertices = Counter(), set()
    holes = native.bind('GEOSGetNumInteriorRings_r', [p, p], C.c_int)
    hole = native.bind('GEOSGetInteriorRingN_r', [p, p, C.c_int], p)
    count = native.count(native.context, regions)
    if count < 1:
        _refuse('Closed constraint graph has no native cells')
    for i in range(count):
        piece = native.part(native.context, regions, i)
        n = holes(native.context, piece)
        if n < 0:
            _refuse('Closed constraint graph has an unknown ring census')
        for boundary in [native.ring(native.context, piece), *[hole(native.context, piece, j) for j in range(n)]]:
            ring = _coordinates(native, boundary)
            if len(ring) < 4 or ring[0] != ring[-1]:
                _refuse('Closed constraint graph has an invalid cell ring')
            vertices.update(map(tuple, ring))
            edges.update(_edge(a, b) for a, b in zip(ring, ring[1:]))
    if edges.keys() != required or any(n not in (1, 2) for n in edges.values()) or not mandatory <= vertices:
        _refuse('Closed constraint graph lost a noded edge or mandatory station')
    return regions


def triangulate_constraints(repo_root, polygon, segments, points, maximum_triangles):
    """Cover a polygon with a conforming mesh retaining noded constraints and input XY."""
    if (not isinstance(polygon, dict) or polygon.get('type') not in ('Polygon', 'MultiPolygon') or
            not isinstance(segments, list) or len(segments) > 4096 or
            not isinstance(points, list) or len(points) > 4096 or any(not _point(p) for p in points) or
            any(not isinstance(s, list) or len(s) != 2 or any(not _point(p) for p in s) or s[0] == s[1]
                for s in segments) or type(maximum_triangles) is not int or not 1 <= maximum_triangles <= 65536):
        _refuse('Arrangement needs bounded finite XY constraints, points and triangle capacity')
    polygons = polygon.get('coordinates') if polygon['type'] == 'MultiPolygon' else [polygon.get('coordinates')]
    if not isinstance(polygons, list) or not polygons:
        _refuse('Arrangement needs a nonempty polygon')
    rings = []
    for component in polygons:
        if not isinstance(component, list) or not component:
            _refuse('Polygon component has no rings')
        for ring in component:
            if (not isinstance(ring, list) or len(ring) < 4 or ring[0] != ring[-1] or
                    any(not _point(p) for p in ring)):
                _refuse('Arrangement needs finite closed polygon rings')
            rings.append(ring)
    if sum(map(len, rings)) > 4096:
        _refuse('Polygon exceeds arrangement input capacity')
    mandatory = {tuple(p) for ring in rings for p in ring}
    mandatory.update(tuple(p) for s in segments for p in s)
    mandatory.update(tuple(p) for p in points)
    if len(mandatory) > 4096:
        _refuse('Combined mandatory stations exceed arrangement input capacity')
    origin = min(mandatory)
    local = {tuple(p[i]-origin[i] for i in (0, 1)): list(p) for p in sorted(mandatory)}
    if len(local) != len(mandatory):
        _refuse('Local translation collapsed distinct mandatory points')
    native = _Geometry(repo_root)
    p = C.c_void_p
    try:
        ctx = native.context
        region = native.read(_local_polygon(polygon, origin))
        if native.measure(region) <= 0:
            _refuse('Arrangement domain has no area')
        covers = native.bind('GEOSCovers_r', [p, p, p], C.c_byte)
        def covered(shape):
            result = covers(ctx, region, shape)
            if result not in (0, 1):
                _refuse('Native domain membership is unknown')
            return result == 1
        for point in local:
            if not covered(native.read({'type': 'Point', 'coordinates': list(point)})):
                _refuse('Mandatory point leaves the polygon or enters a hole')
        lines = {}
        def add(a, b):
            if a != b:
                key = _edge(a, b)
                lines[key] = {'type': 'LineString', 'coordinates': [list(v) for v in key]}
        for ring in rings:
            shifted = [[v[i]-origin[i] for i in (0, 1)] for v in ring]
            for a, b in zip(shifted, shifted[1:]):
                add(a, b)
        for segment in segments:
            shifted = [[v[i]-origin[i] for i in (0, 1)] for v in segment]
            if not covered(native.read({'type': 'LineString', 'coordinates': shifted})):
                _refuse('Constraint segment leaves the polygon or crosses a hole')
            add(*shifted)
        unary = native.bind('GEOSUnaryUnion_r', [p, p], p)
        arrangement, regions = None, None
        if not points:
            arrangement = native.own(unary(ctx, native.read({'type': 'GeometryCollection', 'geometries': list(lines.values())})))
            regions = _closed_cells(native, arrangement, set(local))
        if regions is None:
            # Isolated stations and known open/cut edges need native scaffolding.
            delaunay = native.bind('GEOSDelaunayTriangulation_r', [p, p, C.c_double, C.c_int], p)
            mesh = native.own(delaunay(ctx, native.read({'type': 'MultiPoint', 'coordinates': list(local)}), 0., 1))
            for line in _lines(native, mesh):
                for a, b in zip(line, line[1:]):
                    add(a, b)
            arrangement = native.own(unary(ctx, native.read({'type': 'GeometryCollection', 'geometries': list(lines.values())})))
        required_edges = {_edge(a, b) for line in _lines(native, arrangement) for a, b in zip(line, line[1:])}
        if len(required_edges) > 3*maximum_triangles:
            _refuse('Noded arrangement exceeds final triangle capacity')
        polygonize = native.bind('GEOSPolygonize_r', [p, C.POINTER(p), C.c_uint], p)
        if regions is None:
            regions = native.own(polygonize(ctx, (p*1)(arrangement), 1))
        count = native.count(ctx, regions)
        if count < 1:
            _refuse('Native arrangement has no polygon faces')
        faces, expected_edges, all_cell_edges = [], Counter(), Counter()
        interior_point = native.bind('GEOSPointOnSurface_r', [p, p], p)
        contains = native.bind('GEOSContains_r', [p, p, p], C.c_byte)
        hole_count = native.bind('GEOSGetNumInteriorRings_r', [p, p], C.c_int)
        hole_ring = native.bind('GEOSGetInteriorRingN_r', [p, p, C.c_int], p)
        for i in range(count):
            piece = native.part(ctx, regions, i)
            point = native.own(interior_point(ctx, piece))
            if contains(ctx, piece, point) != 1:
                _refuse('Native arrangement interior classification is unknown')
            holes = hole_count(ctx, piece)
            if holes < 0:
                _refuse('Native arrangement ring census failed')
            piece_edges = Counter()
            boundaries = [native.ring(ctx, piece), *[hole_ring(ctx, piece, j) for j in range(holes)]]
            for boundary in boundaries:
                ring = _coordinates(native, boundary)
                if len(ring) < 4 or ring[0] != ring[-1] or any(a == b for a, b in zip(ring, ring[1:])):
                    _refuse('Native arrangement has an invalid boundary ring')
                piece_edges.update(_edge(a, b) for a, b in zip(ring, ring[1:]))
            all_cell_edges.update(piece_edges)
            if covered(point):
                expected_edges.update(piece_edges)
                faces.extend(native.triangles(piece, maximum_triangles-len(faces)))
        if (all_cell_edges.keys() != required_edges or any(n not in (1, 2) for n in all_cell_edges.values())):
            _refuse('Native noding has an unclosed source or scaffold edge')
        _validate_cover(native, region, faces)
        edges = Counter(_edge(a, b) for face in faces for a, b in zip(face, (*face[1:], face[0])))
        orientation_sum = Counter()
        for face in faces:
            direction = native.orientation(ctx, *[v for point in face for v in point])
            if direction not in (-1, 1):
                _refuse('Arrangement contains a collapsed face')
            for a, b in zip(face, (*face[1:], face[0])):
                orientation_sum[_edge(a, b)] += direction*(1 if tuple(a) < tuple(b) else -1)
        vertices = {tuple(v) for face in faces for v in face}
        if not set(local) <= vertices or not expected_edges.keys() <= edges.keys():
            _refuse('Triangulation lost a mandatory station or noded constraint')
        if any(n not in (1, 2) or n == 2 and orientation_sum[e] != 0 for e, n in edges.items()):
            _refuse('Arrangement has nonmanifold mesh edges')
        # Generated intersections need not satisfy an exact predicate on their
        # original lines. Preserve the owned cell rings by exact mesh incidence.
        if (any(n not in (1, 2) or edges.get(edge) != n for edge, n in expected_edges.items()) or
                {edge for edge, n in edges.items() if n == 1} !=
                {edge for edge, n in expected_edges.items() if n == 1}):
            _refuse('Arrangement has a nonconforming cell boundary or interior edge')
        result, restored_local = [], []
        for face in faces:
            restored = [local.get(tuple(v), [v[i]+origin[i] for i in (0, 1)]) for v in face]
            direction = native.orientation(ctx, *[x for v in restored for x in v])
            if direction not in (-1, 1):
                _refuse('Canonical restoration collapsed an arrangement face')
            if direction != native.orientation(ctx, *[x for v in face for x in v]):
                _refuse('Canonical restoration reversed an arrangement face')
            # Preserve actual vertex correspondence before output normalization.
            restored_local.append([[v[i]-origin[i] for i in (0, 1)] for v in restored])
            if direction < 0:
                restored.reverse()
            first = min(range(3), key=lambda i: restored[i])
            result.append(restored[first:]+restored[:first])
        _validate_cover(native, region, restored_local, _roundtrip_area_bound(faces, restored_local))
        if len({tuple(v) for face in faces for v in face}) != len({tuple(v) for face in result for v in face}):
            _refuse('Canonical restoration collapsed distinct arrangement nodes')
        return sorted(result)
    finally:
        native.close()
