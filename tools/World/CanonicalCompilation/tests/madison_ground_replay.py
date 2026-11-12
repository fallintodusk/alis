"""Replay an authored dry-ground window and retain the separate cell refusal."""
from copy import deepcopy
import csv
import json
import math
from pathlib import Path
import sys
from time import perf_counter
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError, read_json, validate_document
from World.CanonicalCompilation.app.lineage import derive_overlay_changes
from World.CanonicalCompilation.app.incremental import terrain_impact_cells
from World.CanonicalCompilation.app.pipeline import load_profile
from World.CanonicalCompilation.app.terrain_surface import finalize_terrain_cells
from World.CanonicalCompilation.app.terrain_water_coverage import _intersects

FIXTURE = ROOT / 'Plugins/World/ProjectWorldTestData/Data/Fixtures/madison_dry_ground'
EDITED = {(7, 22), (8, 22), (7, 23), (8, 23)}
INCIDENT = {(x, y) for x in range(6, 9) for y in range(21, 24)}


def inputs():
    documents = {}
    for name in ('terrain', 'water', 'profile', 'overlay'):
        path = FIXTURE / f'{name}.json'
        documents[name] = read_json(path)
        validate_document(documents[name], path)
    documents['profile'] = load_profile(FIXTURE / 'profile.json')
    return documents


def window(cell):
    """A finalizer sample window, not a publishable canonical cell."""
    result = deepcopy(cell)
    samples = [row[7:9] for row in cell['water_fit']['core_samples'][22:24]]
    result.update(bounds=[585373.41, 4510632.45, 585403.41, 4510662.45],
                  core_samples=deepcopy(samples), halo_window={'samples': deepcopy(samples)},
                  water_fit={'core_samples': deepcopy(samples),
                             'halo_window': {'samples': deepcopy(samples)}})
    return result


def point(cell, x, y):
    return [cell['bounds'][0] + 30 * x, cell['bounds'][3] - 30 * y,
            cell['core_samples'][y][x],
            *[cell['surface_semantics']['core_samples'][role][y][x]
              for role in ('ground', 'hydro_transition')]]


def faces(cell, x, y):
    a, b, c, d = (point(cell, i, j) for i, j in ((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)))
    return [(a, c, b), (b, c, d)]


def maximum_gradient(cell):
    gradients = []
    for x, y in INCIDENT:
        for a, b, c in faces(cell, x, y):
            dx, dy = b[0] - a[0], b[1] - a[1]
            ex, ey = c[0] - a[0], c[1] - a[1]
            dz, ez = b[2] - a[2], c[2] - a[2]
            determinant = dx * ey - dy * ex
            gradients.append(math.hypot((dz * ey - dy * ez) / determinant,
                                        (dx * ez - dz * ex) / determinant))
    return max(gradients)


def face_quality(before, after):
    """Report every changed-face gradient and incident crease, including neighbors."""
    fields = []
    for cell in (before, after):
        field = {}
        for x in range(5, 10):
            for y in range(20, 25):
                for index, (a, b, c) in enumerate(faces(cell, x, y)):
                    dx, dy, ex, ey = b[0]-a[0], b[1]-a[1], c[0]-a[0], c[1]-a[1]
                    dz, ez = b[2]-a[2], c[2]-a[2]
                    determinant = dx*ey-dy*ex
                    field[x, y, index] = ((dz*ey-dy*ez)/determinant,
                                          (dx*ez-dz*ex)/determinant)
        fields.append(field)
    rows = []
    for face in sorted(fields[0]):
        if face[:2] not in INCIDENT:
            continue
        prior, final = fields[0][face], fields[1][face]
        rows.append({'face': list(face), 'before_gradient': list(prior),
                     'after_gradient': list(final), 'before_slope_percent': 100*math.hypot(*prior),
                     'after_slope_percent': 100*math.hypot(*final)})
    edges = {}
    for face in fields[0]:
        vertices = faces(before, *face[:2])[face[2]]
        for a, b in zip(vertices, (*vertices[1:], vertices[0])):
            edges.setdefault(tuple(sorted((tuple(a[:2]), tuple(b[:2])))), []).append(face)
    creases = []
    for edge, pair in sorted(edges.items()):
        if len(pair) != 2 or not any(face[:2] in INCIDENT for face in pair):
            continue
        angles = []
        for field in fields:
            a, b = (field[face] for face in pair)
            cosine = (1+a[0]*b[0]+a[1]*b[1])/math.sqrt((1+sum(v*v for v in a))*(1+sum(v*v for v in b)))
            angles.append(math.degrees(math.acos(max(-1., min(1., cosine)))))
        creases.append({'edge': [list(p) for p in edge], 'faces': [list(face) for face in pair],
                        'before_normal_angle_degrees': angles[0], 'after_normal_angle_degrees': angles[1]})
    return {'faces': rows, 'incident_edges': creases,
            'steepened_face_count': sum(row['after_slope_percent'] > row['before_slope_percent']+1e-10 for row in rows),
            'interpretation': 'Source agreement and per-face shape are separate; no geographic or gameplay acceptance follows.'}


def reference_error(cell):
    errors = []
    with (FIXTURE / 'reference.csv').open(newline='', encoding='ascii') as stream:
        for sample in csv.DictReader(stream):
            x, y, z = (float(sample[key]) for key in ('x_m', 'y_m', 'ground_m'))
            u = (x - cell['bounds'][0]) / 30
            v = (cell['bounds'][3] - y) / 30
            i, j = min(8, math.floor(u)), min(23, math.floor(v))
            fx, fy = u - i, v - j
            a, b = cell['core_samples'][j][i:i + 2]
            c, d = cell['core_samples'][j + 1][i:i + 2]
            height = (a + fx * (b - a) + fy * (c - a) if fx + fy <= 1
                      else d + (1 - fx) * (c - d) + (1 - fy) * (b - d))
            errors.append(abs(height - z))
    return {'sample_count': len(errors), 'mean_absolute_m': sum(errors) / len(errors),
            'maximum_m': max(errors)}


def replay(overlay=None):
    data = inputs()
    overlay = deepcopy(data['overlay'] if overlay is None else overlay)
    base = data['terrain']
    compiled = SimpleNamespace(by_owner={base['cell_id']: data['water']['features']})
    local = window(base)
    profile = deepcopy(data['profile'])
    profile['grid']['halo_samples'] = 0
    started = perf_counter()
    finalize_terrain_cells({base['cell_id']: local}, compiled, profile, base['grid_id'], overlay)
    elapsed = perf_counter() - started

    # Assemble a typed consumer model only. This does not bypass cell admission.
    for role in ('ground', 'hydro_transition'):
        expected = [row[7:9] for row in base['surface_semantics']['core_samples'][role][22:24]]
        if local['surface_semantics']['core_samples'][role] != expected:
            raise ValueError('Typed authored model cannot discard changed finalizer semantic weights')
    candidate = deepcopy(base)
    for x, y in EDITED:
        height = local['core_samples'][y - 22][x - 7]
        candidate['core_samples'][y][x] = height
        candidate['halo_window']['samples'][y + 1][x + 1] = height
    empty = deepcopy(overlay)
    empty['terrain_patches'] = []
    bounds, changed_features = derive_overlay_changes(empty, overlay)
    dirty = terrain_impact_cells(data['profile'], bounds)
    intersections = []
    for x, y in sorted(INCIDENT):
        a, _, _, d = (point(base, i, j) for i, j in ((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)))
        for water in data['water']['features']:
            if _intersects(water, (a[0], d[1], d[0], a[1])):
                intersections.append({'quad': [x, y], 'feature_id': water['feature_id']})
    whole = deepcopy(base)
    whole_started = perf_counter()
    try:
        finalize_terrain_cells({base['cell_id']: whole}, compiled, data['profile'], base['grid_id'], overlay)
    except CompilerError as error:
        refusal = {'code': error.code, 'message': str(error), 'details': error.details}
    else:
        refusal = None
    changed = {(x, y) for y, row in enumerate(base['core_samples'])
               for x, height in enumerate(row) if candidate['core_samples'][y][x] != height}
    affected = {(i, j) for x, y in changed for i in (x - 1, x) for j in (y - 1, y)}
    report = {'scope': 'Authored sample-window and typed consumer demonstration; cell admission remains separate',
              'edited_nodes': sorted(map(list, changed)), 'incident_quads': sorted(map(list, affected)),
              'dirty_cells': sorted(map(list, dirty)), 'changed_feature_ids': sorted(changed_features),
              'water_intersections': intersections, 'window_seconds': elapsed,
              'full_cell_seconds': perf_counter() - whole_started,
              'reference_before': reference_error(base), 'reference_after': reference_error(candidate),
              'maximum_gradient_before': maximum_gradient(base),
              'maximum_gradient_after': maximum_gradient(candidate),
              'face_quality': face_quality(base, candidate),
              'full_cell_refusal': refusal, 'full_cell_after_refusal_equals_input': whole == base}
    return base, candidate, local, report


def native_payload(cell, base):
    quads = []
    for x, y in sorted(INCIDENT):
        a, b, c, d = (point(cell, i, j) for i, j in ((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)))
        quads.append({'coordinate': [x - 6, y - 21], 'vertices': [a, b, c, d],
                      'triangles': [[0, 2, 1], [1, 2, 3]], 'water_triangles': []})
    return {'classification': 'Typed authored Ground model only; full-cell publication refused',
            'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
            'engine_georeference_origin_m': [585628., 4511322.], 'height_origin_m': 0.,
            'source_frame_bundle_sha256': '84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067',
            'original_cell_id': cell['cell_id'], 'original_quad_coordinates': sorted(map(list, INCIDENT)),
            'reduction': 'Nine complete original quads; original XYZ, weights and engine frame retained; typed indices shifted by6,21.',
            'cell_id': 'captured_madison_dry_ground',
            'bounds': [585343.41, 4510602.45, 585433.41, 4510692.45],
            'sample_size': [4, 4], 'sample_spacing_m': [30., 30.],
            'base_heights': [base['core_samples'][y][x] for y in range(21, 25) for x in range(6, 10)],
            'base_weights': [point(cell, x, y)[3:] for y in range(21, 25) for x in range(6, 10)],
            'quads': quads}


def main(destination):
    destination = destination.resolve()
    if not destination.is_relative_to(ROOT / 'tmp') or destination.exists():
        raise ValueError('Replay requires a fresh directory under repository tmp')
    base, candidate, _, report = replay()
    destination.mkdir(parents=True)
    for name, payload in (('before', native_payload(base, base)), ('after', native_payload(candidate, base)), ('report', report)):
        (destination / f'{name}.json').write_text(json.dumps(payload, indent=2) + '\n', encoding='ascii')
    print(json.dumps(report))


if __name__ == '__main__':
    main(Path(sys.argv[1]))
