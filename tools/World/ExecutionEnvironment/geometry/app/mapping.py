"""Certified triangle-cover mapping with owned canonical float64 restoration proof."""
import math

from ...app.contracts import ExecutionEnvironmentError
from .kernel import _Geometry, _local_polygon, _noncollapsed_indices, _roundtrip_area_bound, _validate_cover


def _triangle(value):
    if (not isinstance(value, list) or len(value) != 3 or any(not isinstance(p, list) or len(p) != 2
            or any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in p)
            for p in value)):
        raise ExecutionEnvironmentError('native_geometry_input', 'Mapping needs finite XY triangles')
    a, b, c = value
    determinant = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    if not math.isfinite(determinant) or abs(determinant) <= 1e-12:
        raise ExecutionEnvironmentError('native_geometry_input', 'Mapping triangle is collapsed or overflowing')


def _polygon(face):
    return {'type': 'Polygon', 'coordinates': [[*face, face[0]]]}


def map_triangle_cover(repo_root, source, target, triangles):
    """Map a complete cover with the same barycentric operation as native Road realization."""
    _triangle(source); _triangle(target)
    if not isinstance(triangles, list) or not 1 <= len(triangles) <= 65536:
        raise ExecutionEnvironmentError('native_geometry_input', 'Mapping needs a bounded nonempty source cover')
    for face in triangles:
        _triangle(face)
    origin = target[0]
    target_local = [[p[i]-origin[i] for i in (0, 1)] for p in target]
    delta = [[source[j][i]-source[0][i] for i in (0, 1)] for j in (1, 2)]
    determinant = delta[0][0]*delta[1][1]-delta[0][1]*delta[1][0]
    local, result = [], []
    for face in triangles:
        before, after = [], []
        for point in face:
            x, y = [point[i]-source[0][i] for i in (0, 1)]
            u = (x*delta[1][1]-y*delta[1][0])/determinant
            v = (delta[0][0]*y-delta[0][1]*x)/determinant
            before.append([target_local[1][i]*u+target_local[2][i]*v for i in (0, 1)])
            # Keep native Road's addition order; measure its difference from the local result.
            after.append([origin[i]+target_local[1][i]*u+target_local[2][i]*v for i in (0, 1)])
        local.append(before); result.append(after)
    if any(not math.isfinite(v) for faces in (local, result) for face in faces for p in face for v in p):
        raise ExecutionEnvironmentError('native_geometry_input', 'Mapped output overflows')
    restored = [[[p[i]-origin[i] for i in (0, 1)] for p in face] for face in result]
    displacement = max(math.dist(a, b) for before, after in zip(local, restored) for a, b in zip(before, after))
    bound = _roundtrip_area_bound(local, restored)
    native = _Geometry(repo_root)
    try:
        source_origin = source[0]
        source_local = _local_polygon(_polygon(source), source_origin)['coordinates'][0][:-1]
        faces_local = [_local_polygon(_polygon(face), source_origin)['coordinates'][0][:-1] for face in triangles]
        _validate_cover(native, native.read(_polygon(source_local)), faces_local, convex_parent=source_local)
        region = native.read(_polygon(target_local))
        _validate_cover(native, region, local, convex_parent=target_local)
        retained = _noncollapsed_indices(native, restored)
        _validate_cover(native, region, [restored[i] for i in retained], bound, target_local)
        return {'triangles': [result[i] for i in retained], 'source_indices': retained,
                'maximum_vertex_displacement_m': displacement, 'roundtrip_area_bound_m2': bound}
    finally:
        native.close()
