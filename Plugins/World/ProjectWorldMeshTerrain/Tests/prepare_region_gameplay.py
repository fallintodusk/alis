"""Bind source-axis diagnostic routes to an authenticated saved regional scene."""
import argparse
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO/'tools'))
from World.ExecutionEnvironment.geometry.affine import _cross, _height


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def sample(field, point):
    heights = []
    for face in field:
        if any(point[i] < min(p[i] for p in face) or point[i] > max(p[i] for p in face) for i in (0, 1)):
            continue
        exact = [[F(v) for v in p] for p in face]
        xy = list(map(F, point))
        sign = 1 if _cross(*exact) > 0 else -1
        if all(sign*_cross(a, b, xy) >= 0 for a, b in zip(exact, [*exact[1:], exact[0]])):
            heights.append(float(_height(exact, xy)))
    if heights and max(heights)-min(heights) > 1e-7:
        raise ValueError('Contradictory saved support remains unknown')
    return max(heights) if heights else None


def top_support(ground, structures, water, point):
    base = sample(ground, point)
    if base is None:
        raise ValueError('Source route leaves complete-cell Ground support')
    body, wet = sample(structures, point), sample(water, point)
    height = max(base, body) if body is not None else base
    if wet is not None and height <= wet:
        raise ValueError('Source route has submerged actual top support')
    return height, wet, body is not None and body >= base


def slice_axis(points, start, end):
    if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in (start, end)):
        raise ValueError('Source route stations must be finite numbers')
    lengths = [0.]
    for a, b in zip(points, points[1:]):
        lengths.append(lengths[-1]+math.dist(a, b))
    if end > lengths[-1] and end-lengths[-1] <= math.ulp(lengths[-1])*len(points):
        end = lengths[-1]
    if not 0 <= start < end <= lengths[-1] or end-start > 2000:
        raise ValueError('Route interval escapes its actual source axis')
    def at(station):
        index = next((i for i in range(len(points)-1) if lengths[i] <= station <= lengths[i+1]), len(points)-2)
        t = (station-lengths[index])/(lengths[index+1]-lengths[index])
        return [points[index][j]+t*(points[index+1][j]-points[index][j]) for j in (0, 1)]
    stations = [start, *[v for v in lengths if start < v < end], end]
    selected = []
    for station in stations:
        point = at(station)
        if not selected or point != selected[-1]:
            selected.append(point)
    return selected


def compose_axis(request, features):
    points, consumed, widths = [], [], []
    for leg in request.get('legs', [request]):
        feature = features[leg['source_feature_id']]
        geometry = feature['geometry']
        if feature['feature_class'] != 'road' or geometry['type'] not in ('LineString', 'MultiLineString'):
            raise ValueError('A source-axis diagnostic route requires an actual canonical road axis')
        axis = geometry['coordinates'] if geometry['type'] == 'LineString' else geometry['coordinates'][0]
        if geometry['type'] == 'MultiLineString' and len(geometry['coordinates']) != 1:
            raise ValueError('Disconnected source parts require an explicit route decision')
        selected = slice_axis(axis, leg['start_m'], leg['end_m'])
        if points and points[-1] != selected[0]:
            raise ValueError('Source route legs must share their exact original endpoint')
        points.extend(selected[1:] if points else selected)
        consumed.append({'feature_id': feature['feature_id'], 'interval_m': [leg['start_m'], leg['end_m']]})
        widths.append(feature['attributes']['width_m'])
    return points, consumed, min(widths)


def fields(document, origin, height_origin):
    def actors(name):
        return [[actor['vertices_canonical'][i] for i in face]
                for actor in document.get(name, {}).get('actors', []) for face in actor['triangles']]
    ground = []
    variant = next(v for v in document['variants'] if v['variant'] == 'fallback')
    for component in variant['components']:
        if component['kind'] != 'gpu_fallback':
            continue
        vertices = [[p[0]*.01+origin[0], -p[1]*.01+origin[1], p[2]*.01+height_origin]
                    for p in component['world_vertices_cm']]
        ground.extend([[vertices[i] for i in face] for face in component['triangles']])
    return ground, actors('regional_structures_before'), actors('regional_water_before')


def prepare(repo, view_path, route_path, output):
    repo, view_path, route_path, output = [Path(p).resolve() for p in (repo, view_path, route_path, output)]
    if not output.is_relative_to(repo/'tmp/world') or output.exists():
        raise ValueError('Gameplay output must be fresh below repository tmp/world')
    view = read(view_path)
    sections = (repo/view['sections']).resolve()
    scene = sections.parent
    if output.is_relative_to(scene) or scene.is_relative_to(output) or output.is_relative_to(view_path.parent):
        raise ValueError('Gameplay output must be separate from saved scene and observer inputs')
    for relative, pin in view['saved_files'].items():
        if digest(repo/relative) != pin:
            raise ValueError('Saved scene changed before route preparation')
    original = read(repo/view['original_fixture'])
    result = repo/original['canonical_result']['path']
    if digest(result) != original['canonical_result']['sha256']:
        raise ValueError('Original canonical input identity changed')
    entries = {e['path']: e for e in read(result)['outputs']}
    def artifact(relative):
        path = result.parent/relative
        if digest(path) != entries[relative]['sha256']:
            raise ValueError('Source route artifact no longer matches its canonical identity')
        return read(path)
    coverage = artifact('canonical/coverage.json')
    origin = coverage['engine_georeference_origin']; height_origin = coverage['grid']['vertical_origin_m']
    ground, structures, water = fields(read(sections), origin, height_origin)
    requested = read(route_path)['routes']
    wanted = {leg['source_feature_id'] for request in requested for leg in request.get('legs', [request])}; features = {}
    for name in sorted(entries):
        if not name.startswith('canonical/features/'):
            continue
        for feature in artifact(name)['features']:
            if feature['feature_id'] in wanted:
                features[feature['feature_id']] = feature
        if len(features) == len(wanted):
            break
    routes, proof = [], []
    for request in requested:
        points, consumed, width = compose_axis(request, features)
        positions, witnesses, clearance = [], 0, []
        for point in points:
            z, _, _ = top_support(ground, structures, water, point)
            positions.append([(point[0]-origin[0])*100, -(point[1]-origin[1])*100, (z-height_origin)*100])
        for a, b in zip(points, points[1:]):
            length = math.dist(a, b); steps = math.ceil(length/.25)
            normal = [-(b[1]-a[1])/length, (b[0]-a[0])/length]
            for step in range(steps+1):
                for lateral in (-width/2+.23, 0., width/2-.23):
                    point = [a[i]+(b[i]-a[i])*step/steps+normal[i]*lateral for i in (0, 1)]
                    z, wet, _ = top_support(ground, structures, water, point)
                    if wet is not None:
                        clearance.append(z-wet)
                    witnesses += 1
        routes.append({'id': request['id'], 'width_m': width, 'points_world_cm': positions, 'source_legs': consumed})
        proof.append({'id': request['id'], 'sampled_support_witnesses': witnesses,
            'minimum_sampled_water_clearance_m': min(clearance) if clearance else None,
            'scope': 'Stationed three-lane support preflight; runtime capsule dimensions/contacts and continuous movement remain required.'})
    payload = {'schema_version': 1, 'scene_input': {'path': view_path.relative_to(repo).as_posix(), 'sha256': digest(view_path)},
        'route_source': {'path': route_path.relative_to(repo).as_posix(), 'sha256': digest(route_path)}, 'routes': routes,
        'scope': 'Configured actual Hero on unchanged saved region. Diagnostic source-axis subsets; no full-way, navigation or streaming admission.'}
    output.mkdir(parents=True)
    path = output/'input.json'; path.write_text(json.dumps(payload, indent=2)+'\n', encoding='ascii')
    (output/'support.json').write_text(json.dumps(proof, indent=2)+'\n', encoding='ascii')
    return {'input': path.relative_to(repo).as_posix(), 'sha256': digest(path), 'map': view['map'],
            'plugin': next(k for k in view['saved_files'] if k.endswith('.uplugin')), 'routes': proof}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--scene-input', type=Path, required=True)
    parser.add_argument('--routes', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(REPO, args.scene_input, args.routes, args.output), indent=2))


if __name__ == '__main__':
    main()
