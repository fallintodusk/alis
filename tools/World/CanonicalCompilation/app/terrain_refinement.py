"""Bounded final-surface construction from an immutable canonical grid."""

import math
from pathlib import Path

from World.ExecutionEnvironment.api import ExecutionEnvironmentError, execution_identity, partition_triangles
from .contracts import CompilerError, canonical_hash, file_hash
from .terrain_surface import _footprint_distance

SCHEMA = "https://alis.world/schemas/world-compiler/terrain-refinement-v1.json"
EPSILON = 1e-8


def _refuse(code, message, **details):
    raise CompilerError(code, message, **details)


def _policy(policy, water):
    names = ("maximum_cut_m", "maximum_fill_m", "maximum_dry_distance_m", "maximum_bank_gradient", "water_clearance_m")
    if any(isinstance(policy.get(n), bool) or not isinstance(policy.get(n), (int, float))
           or not math.isfinite(policy[n]) or policy[n] < 0 for n in names):
        _refuse("refinement_policy_invalid", "Refinement needs explicit finite quality limits")
    if not isinstance(policy.get("maximum_triangles"), int) or isinstance(policy["maximum_triangles"], bool) or not 2 <= policy["maximum_triangles"] <= 4096:
        _refuse("refinement_policy_invalid", "Refinement triangle limit must be between 2 and 4096")
    accuracy = water.get("attributes", {}).get("surface_function", {}).get("accuracy")
    if not isinstance(accuracy, str) or accuracy.startswith("low_confidence") or accuracy not in policy.get("accepted_accuracy", []):
        _refuse("refinement_source_excluded", "Water source accuracy is not admitted", accuracy=accuracy)
    function = water["attributes"]["surface_function"]
    level = function.get("level_m")
    if (water.get("feature_class") != "water" or water.get("geometry", {}).get("type") not in {"Polygon", "MultiPolygon"}
            or "knots" in function
            or isinstance(level, bool) or not isinstance(level, (int, float)) or not math.isfinite(level)):
        _refuse("refinement_surface_unsupported", "This bounded implementation requires constant polygon Water")
    return level - policy["water_clearance_m"]


def _point(cell, x, y):
    return [cell["bounds"][0] + x * cell["sample_spacing"][0],
            cell["bounds"][3] - y * cell["sample_spacing"][1], cell["core_samples"][y][x],
            *[cell["surface_semantics"]["core_samples"][role][y][x] for role in ("ground", "hydro_transition")]]


def _interpolate(triangle, xy):
    a, b, c = triangle
    dx, dy = xy[0] - a[0], xy[1] - a[1]
    ux, uy, vx, vy = b[0] - a[0], b[1] - a[1], c[0] - a[0], c[1] - a[1]
    determinant = ux * vy - uy * vx
    if not math.isfinite(determinant) or abs(determinant) <= 1e-12:
        _refuse("refinement_grid_invalid", "Base triangle is degenerate")
    u, v = (dx * vy - dy * vx) / determinant, (ux * dy - uy * dx) / determinant
    return [*xy, *[a[i] + u * (b[i] - a[i]) + v * (c[i] - a[i]) for i in range(2, 5)]]


def _domain(cells, selected):
    result = {}
    for identifier, quads in sorted(selected.items()):
        if identifier not in cells or not quads:
            _refuse("refinement_selection_invalid", "Selected cell is missing or empty")
        cell = cells[identifier]
        if cell.get("authored_patch_ids"):
            _refuse("refinement_authored_conflict", "Selected authored terrain needs an explicit conflict policy")
        rows = cell["core_samples"]
        if len(rows) < 2 or len(rows[0]) < 2 or any(len(r) != len(rows[0]) for r in rows):
            _refuse("refinement_grid_invalid", "Base grid is not rectangular")
        if cell["surface_semantics"]["roles"] != ["ground", "hydro_transition"]:
            _refuse("refinement_grid_invalid", "Base surface roles are unsupported")
        for x, y in sorted(quads):
            if not 0 <= x < len(rows[0]) - 1 or not 0 <= y < len(rows) - 1:
                _refuse("refinement_selection_invalid", "Selected quad is outside its grid")
            a, b, c, d = _point(cell, x, y), _point(cell, x + 1, y), _point(cell, x, y + 1), _point(cell, x + 1, y + 1)
            key = (a[0], c[1], b[0], a[1])
            if key in result or any(not math.isfinite(float(v)) for p in (a, b, c, d) for v in p):
                _refuse("refinement_grid_invalid", "Duplicate or nonfinite selected quad")
            result[key] = (identifier, x, y, (a, b, c, d))
    return result


def _external_edges(domain):
    edges = {}
    for _, _, _, (a, b, c, d) in domain.values():
        for left, right in ((a, b), (b, d), (d, c), (c, a)):
            key = tuple(sorted((tuple(left[:2]), tuple(right[:2]))))
            edges.setdefault(key, []).append((left, right))
    for entries in edges.values():
        if len(entries) == 2:
            first, second = (sorted(pair) for pair in entries)
            if any(abs(a - b) > EPSILON for l, r in zip(first, second) for a, b in zip(l, r)):
                _refuse("refinement_seam_conflict", "Selected base grids disagree on their shared edge")
    return [values[0] for values in edges.values() if len(values) == 1]


def _check_outer(point, edges):
    for a, b in edges:
        dx, dy = b[0] - a[0], b[1] - a[1]
        cross = (point[0] - a[0]) * dy - (point[1] - a[1]) * dx
        fraction = ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / (dx * dx + dy * dy)
        if abs(cross) <= EPSILON * math.hypot(dx, dy) and -EPSILON <= fraction <= 1 + EPSILON:
            expected = a[2] + fraction * (b[2] - a[2])
            if abs(point[2] - expected) > EPSILON:
                _refuse("refinement_domain_open", "Height change reaches an unselected boundary")
            if EPSILON < fraction < 1 - EPSILON:
                _refuse("refinement_neighbor_required", "Boundary subdivision needs selected neighbor context")


def _check_seams(outputs, domain):
    edges = {}
    for _, (identifier, x, y, (a, b, c, d)) in sorted(domain.items()):
        quad = next(q for q in outputs[identifier]["quads"] if q["coordinate"] == [x, y])
        for left, right in ((a, b), (b, d), (d, c), (c, a)):
            key = tuple(sorted((tuple(left[:2]), tuple(right[:2]))))
            dx, dy = right[0] - left[0], right[1] - left[1]
            points = sorted(p for p in quad["vertices"] if abs((p[0]-left[0])*dy-(p[1]-left[1])*dx) <= EPSILON * math.hypot(dx, dy))
            edges.setdefault(key, []).append(points)
    for pair in edges.values():
        if len(pair) > 2 or (len(pair) == 2 and (len(pair[0]) != len(pair[1]) or
            any(abs(a - b) > EPSILON for l, r in zip(pair[0], pair[1]) for a, b in zip(l, r)))):
            _refuse("refinement_seam_conflict", "Adjacent replacements disagree on boundary geometry or weights")


def _quad(repo_root, cell, x, y, points, water, level, policy, edges):
    a, b, c, d = points
    vertices, triangles, wet_indices = [], [], []
    indices = {}
    metrics = {"maximum_cut_m": 0.0, "maximum_fill_m": 0.0, "cut_volume_m3": 0.0,
               "maximum_dry_distance_bound_m": 0.0, "maximum_bank_gradient": 0.0}
    for original in ((a, c, b), (b, c, d)):
        try:
            partitions = partition_triangles(repo_root, [p[:2] for p in original], water["geometry"], policy["maximum_triangles"])
        except ExecutionEnvironmentError as error:
            raise CompilerError(error.code, str(error), **error.details) from error
        for role, faces in sorted(partitions.items()):
            for face in faces:
                points_out, cuts, distances = [], [], []
                for xy in face:
                    point = _interpolate(original, xy)
                    prior = point[2]
                    distance = _footprint_distance(water, xy)
                    if role == "wet" or distance <= EPSILON:
                        point[2] = min(prior, level)
                    _check_outer(point, edges)
                    cuts.append(prior - point[2])
                    distances.append(distance)
                    points_out.append(point)
                p, q, r = points_out
                determinant = (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
                if abs(determinant) <= 1e-12:
                    _refuse("refinement_degenerate", "Native output collapsed")
                if determinant < 0:
                    points_out.reverse()
                triangle = []
                for point in points_out:
                    key = tuple(point[:2])
                    if key in indices:
                        if any(abs(vertices[indices[key]][i] - point[i]) > EPSILON for i in range(2, 5)):
                            _refuse("refinement_seam_conflict", "Shared vertex has conflicting heights or weights")
                    else:
                        indices[key] = len(vertices)
                        vertices.append(point)
                    triangle.append(indices[key])
                if role == "wet":
                    wet_indices.append(len(triangles))
                triangles.append(triangle)
                metrics["maximum_cut_m"] = max(metrics["maximum_cut_m"], *cuts)
                metrics["cut_volume_m3"] += abs(determinant) * sum(cuts) / 6
                if role == "dry" and max(cuts) > EPSILON:
                    gx = ((q[2]-p[2])*(r[1]-p[1])-(r[2]-p[2])*(q[1]-p[1])) / determinant
                    gy = ((q[0]-p[0])*(r[2]-p[2])-(r[0]-p[0])*(q[2]-p[2])) / determinant
                    metrics["maximum_bank_gradient"] = max(metrics["maximum_bank_gradient"], math.hypot(gx, gy))
                    # Distance-to-footprint is 1-Lipschitz: this bounds the complete face.
                    bound = min(distances[i] + max(math.dist(face[i], v) for v in face) for i in range(3))
                    metrics["maximum_dry_distance_bound_m"] = max(metrics["maximum_dry_distance_bound_m"], bound)
    if len(triangles) > policy["maximum_triangles"]:
        _refuse("refinement_quality_exception", "Quad triangle budget exceeded")
    for metric, limit in (("maximum_cut_m", "maximum_cut_m"), ("maximum_bank_gradient", "maximum_bank_gradient"),
                          ("maximum_dry_distance_bound_m", "maximum_dry_distance_m")):
        if metrics[metric] > policy[limit] + EPSILON:
            _refuse("refinement_quality_exception", "Candidate exceeds explicit quality policy", metric=metric, actual=metrics[metric])
    order = sorted(range(len(vertices)), key=lambda i: vertices[i])
    remap = {old: new for new, old in enumerate(order)}
    tagged = sorted((tuple(remap[i] for i in face), index in wet_indices) for index, face in enumerate(triangles))
    return {"coordinate": [x, y], "vertices": [vertices[i] for i in order],
            "triangles": [list(face) for face, _ in tagged],
            "water_triangles": [i for i, (_, wet) in enumerate(tagged) if wet]}, metrics


def refine_terrain_cells(repo_root: Path, cells: dict, water: dict, policy: dict, selected: dict) -> dict:
    """Return final-surface payloads without mutating base cells or publishing authority."""
    level = _policy(policy, water)
    domain = _domain(cells, selected)
    edges = _external_edges(domain)
    outputs = {}
    execution = execution_identity(repo_root, True)["identity_sha256"]
    owner = Path(__file__).parents[1]
    implementation = canonical_hash({p.relative_to(owner).as_posix(): file_hash(p) for p in sorted([
        *owner.glob("app/*refinement*.py"), *owner.glob("contracts/*refinement*.schema.json")])})
    for key, (identifier, x, y, points) in sorted(domain.items()):
        quad, metrics = _quad(repo_root, cells[identifier], x, y, points, water, level, policy, edges)
        surface = outputs.setdefault(identifier, {
            "$schema": SCHEMA, "schema_version": 1, "grid_id": cells[identifier]["grid_id"], "cell_id": identifier,
            "coordinate_rule": "canonical_boundary_intersections_float64",
            "consumed": {"water_feature_id": water["feature_id"], "water_sha256": canonical_hash(water),
                         "policy_sha256": canonical_hash(policy), "implementation_sha256": implementation,
                         "execution_identity": execution, "domain_sha256": canonical_hash({
                             str(bounds): {"cell_id": info[0], "quad": [info[1], info[2]], "base_sha256": canonical_hash(cells[info[0]])}
                             for bounds, info in sorted(domain.items()) if
                             bounds[0] <= cells[identifier]["bounds"][2] and bounds[2] >= cells[identifier]["bounds"][0] and
                             bounds[1] <= cells[identifier]["bounds"][3] and bounds[3] >= cells[identifier]["bounds"][1]})},
            "policy": policy, "quads": [], "metrics": {}})
        surface["quads"].append(quad)
        for name, value in metrics.items():
            surface["metrics"][name] = (surface["metrics"].get(name, 0) + value if name == "cut_volume_m3"
                                        else max(surface["metrics"].get(name, 0), value))
    _check_seams(outputs, domain)
    return outputs
