"""Project an already qualified finite surface onto one straight native Road ribbon."""
import math

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import map_triangle_cover, partition_triangles, validate_triangle_cover
from ...app.contracts import CompilerError
from .road_elevation import _affine, _overlaps, _polygon


def project_straight_surface(repo, source_faces, points, width, maximum_triangles):
    a, b = points
    length = math.dist(a, b)
    tangent = [(b[i]-a[i])/length for i in (0, 1)]
    normal = [-tangent[1], tangent[0]]
    corners = [[p[i]+side*width/2*normal[i] for i in (0, 1)]
               for p, side in ((a, 1), (a, -1), (b, -1), (b, 1))]
    footprint = _polygon(corners)
    domain = _polygon([[0., -width/2], [length, -width/2], [length, width/2], [0., width/2]])
    ribbon = [[[0., -width/2], [length, -width/2], [0., width/2]],
              [[length, -width/2], [length, width/2], [0., width/2]]]
    uv = []
    groups = [[], []]
    stage = 'surface projection'
    try:
        for face in source_faces:
            if not _overlaps(face, corners):
                continue
            parameter = [[sum((p[i]-a[i])*tangent[i] for i in (0, 1)),
                          sum((p[i]-a[i])*normal[i] for i in (0, 1)), p[2]] for p in face]
            # Preserve native clipping order and arithmetic for captured observed fields.
            for index, half in enumerate(ribbon):
                for clipped in partition_triangles(repo, half, _polygon([p[:2] for p in parameter]), 16)['wet']:
                    groups[index].append(len(uv))
                    uv.append([_affine(parameter, p) for p in clipped])
                    if len(uv) > maximum_triangles:
                        raise CompilerError('road_surface_projection_unsupported', 'Surface exceeds its explicit output budget')
        if any(not math.isfinite(v) for face in uv for p in face for v in p):
            raise CompilerError('road_surface_projection_unsupported', 'Surface interpolation is unknown or overflowing')
        stage = 'parameter coverage'
        validate_triangle_cover(repo, domain, [[p[:2] for p in face] for face in uv])
        # Map each certified parameter cover onto its canonical triangle. Unlike an
        # arbitrary cover check, this operation owns the restoration error proof.
        stage = 'projected mapping'
        targets = [[corners[1], corners[2], corners[0]], [corners[2], corners[3], corners[0]]]
        validate_triangle_cover(repo, footprint, targets)
        world, retained, proofs = {}, set(), []
        for half, target, indices in zip(ribbon, targets, groups):
            mapped = map_triangle_cover(repo, half, target, [[p[:2] for p in uv[i]] for i in indices])
            for source, face in zip(mapped['source_indices'], mapped['triangles']):
                index = indices[source]
                world[index] = [[*xy, p[2]] for xy, p in zip(face, uv[index])]
                retained.add(index)
            proofs.append({key: mapped[key] for key in ('maximum_vertex_displacement_m', 'roundtrip_area_bound_m2')})
        order = [i for i in range(len(uv)) if i in retained]
        if len(order) != len(uv):
            stage = 'retained parameter coverage'
            validate_triangle_cover(repo, domain, [[p[:2] for p in uv[i]] for i in order])
        uv, world = [uv[i] for i in order], [world[i] for i in order]
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code, str(error), projection_stage=stage, **error.details) from error
    return footprint, uv, world, proofs
