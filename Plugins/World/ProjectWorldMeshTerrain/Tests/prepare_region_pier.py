"""Prepare the captured Pier25 two-cell combined consumer without changing authority."""
import argparse
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO/'tools'))
from World.CanonicalCompilation.tests.pier25_connection_fixture import fixture, shore_contact_xy
from World.CanonicalCompilation.tests.test_pier25_connection import contact_error
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover, _height
from World.ExecutionEnvironment.geometry.api import partition_triangles


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def authenticated(result, expected):
    if digest(result) != expected:
        raise ValueError('Canonical result differs from its explicitly selected identity')
    document = read(result)
    entries = {entry['path']: entry for entry in document['outputs']}
    if len(entries) != len(document['outputs']) or document['status'] != 'accepted':
        raise ValueError('Canonical result must be accepted with unique output identities')
    for relative, entry in entries.items():
        path = (result.parent/relative).resolve()
        if not path.is_relative_to(result.parent) or path.stat().st_size != entry['byte_size'] or digest(path) != entry['sha256']:
            raise ValueError('Canonical output differs from its declared complete snapshot')
    return document, entries


def grid_values(terrain, x, y):
    west, south, east, north = terrain['bounds']
    if not west <= x <= east or not south <= y <= north:
        raise ValueError('Witness escapes its complete selected original cell')
    dx, dy = terrain['sample_spacing']; samples = terrain['core_samples']
    u, v = (x-west)/dx, (north-y)/dy
    i = max(0, min(len(samples[0])-2, math.floor(u)))
    j = max(0, min(len(samples)-2, math.floor(v)))
    u, v = u-i, v-j
    a, b = samples[j][i:i+2]; c, d = samples[j+1][i:i+2]
    query = a*(1-u)*(1-v)+b*u*(1-v)+c*(1-u)*v+d*u*v
    physical = a+(b-a)*u+(c-a)*v if u+v <= 1 else b*(1-v)+c*(1-u)+d*(u+v-1)
    return query, physical


def ground_faces(terrains, bounds):
    faces = []
    for terrain in terrains:
        west, _, _, north = terrain['bounds']; dx, dy = terrain['sample_spacing']; samples = terrain['core_samples']
        for j in range(len(samples)-1):
            for i in range(len(samples[0])-1):
                x, y = west+i*dx, north-j*dy
                if x+dx < bounds[0] or x > bounds[2] or y < bounds[1] or y-dy > bounds[3]:
                    continue
                a, b = [x, y, samples[j][i]], [x+dx, y, samples[j][i+1]]
                c, d = [x, y-dy, samples[j+1][i]], [x+dx, y-dy, samples[j+1][i+1]]
                faces.extend([[a, b, c], [b, d, c]])
    return faces


def prove(repo, deck, ground, water_features, offset):
    captured, _, _ = fixture()
    if deck != captured['triangles']:
        raise ValueError('This captured-case adapter requires the admitted exact v2 Pier25 model')
    gap = 1000-measure_affine_cover(ground, deck, offset_m=1000)['maximum_height_error_upper_m']
    if gap < -1e-8:
        raise ValueError('The admitted deck penetrates actual complete-cell Ground')
    vertices = {tuple(p[:2]): p for face in deck for p in face}
    cap_errors = [contact_error(ground, [vertices[tuple(p)] for p in edge]) for edge in shore_contact_xy()]
    if max(cap_errors) > 1e-8:
        raise ValueError('An entire shore cap loses actual Ground contact')
    minimum = math.inf; fragments = 0; water_ids = set()
    for feature in water_features:
        geometry = feature['geometry']
        for triangle in deck:
            pieces = partition_triangles(repo, [p[:2] for p in triangle], geometry, 4096)['wet']
            if not pieces:
                continue
            function = feature['attributes']['surface_function']
            if feature['attributes']['surface_behavior'] != 'standing' or 'level_m' not in function:
                raise ValueError('Overlapping Water requires a separately supported surface function')
            level = function['level_m']+offset
            exact = [[F(v) for v in p] for p in triangle]
            minimum = min(minimum, *(float(_height(exact, [F(v) for v in p]))-level for face in pieces for p in face))
            fragments += len(pieces); water_ids.add(feature['feature_id'])
    if not fragments or minimum <= 1e-8:
        raise ValueError('The full-width deck has no positive clearance above actual overlapping ordinary Water')
    return {'minimum_deck_above_ground_m': gap, 'full_cap_ground_error_m': cap_errors,
            'minimum_deck_above_overlapping_water_m': minimum, 'clipped_water_fragments': fragments,
            'overlapping_water_feature_ids': sorted(water_ids), 'deck_faces': len(deck), 'ground_faces': len(ground)}


def prepare(repo, result, result_hash, base, base_hash, water_profile, output):
    repo, result, base, water_profile, output = [Path(p).resolve() for p in (repo, result, base, water_profile, output)]
    if not output.is_relative_to(repo/'tmp/world') or output.exists():
        raise ValueError('Combined observation must use a fresh repository tmp/world directory')
    for source in (result.parent, base.parent):
        if output.is_relative_to(source) or source.is_relative_to(output):
            raise ValueError('Combined observation output must be disjoint from both canonical input roots')
    document, entries = authenticated(result, result_hash)
    _, originals = authenticated(base, base_hash)
    if document['base_result_sha256'] != base_hash:
        raise ValueError('The selected candidate must bind this exact immutable base')
    unchanged = [name for name in originals if name.startswith('canonical/terrain/')]
    if not unchanged or any(entries[name] != originals[name] for name in unchanged):
        raise ValueError('The combined scene must retain every original Terrain artifact')
    features = {}
    for name in sorted(entries):
        if name.startswith('canonical/features/'):
            for feature in read(result.parent/name)['features']:
                features[feature['feature_id']] = feature
    target = next(f for f in features.values() if f['feature_class'] == 'constructed_surface')
    if sum(f['feature_class'] == 'constructed_surface' for f in features.values()) != 1:
        raise ValueError('The bounded scene selects exactly one admitted separate structure')
    ids = sorted(r['cell_id'] for r in target['representations'])
    if len(ids) != 2 or len(set(ids)) != 2:
        raise ValueError('The admitted structure requires exactly two complete original cells')
    cells = [read(result.parent/name) for name in sorted(entries) if name.startswith('canonical/cells/')]
    selected = [c for c in cells if c['cell_id'] in ids]
    if len(selected) != 2:
        raise ValueError('An admitted structural membership cell is missing')
    terrains = [read(result.parent/c['terrain_artifact']['path']) for c in selected]
    waters = [f for f in features.values() if f['feature_class'] == 'water'
              and any(r['cell_id'] in ids for r in f['representations'])]
    for f in waters:
        owner = next(c for c in cells if c['cell_id'] == f['owner_cell_id'])
        old = read(base.parent/owner['feature_artifact']['path'])['features']
        if f != next(v for v in old if v['feature_id'] == f['feature_id']):
            raise ValueError('The candidate changed selected ordinary Water authority')
    deck = target['attributes']['constructed_surface']['triangles']
    points = [p for face in deck for p in face]
    bounds = [min(p[i] for p in points) for i in (0, 1)]+[max(p[i] for p in points) for i in (0, 1)]
    ground = ground_faces(terrains, bounds)
    profile = read(water_profile); layer = next(l for l in profile['layers'] if l['layer_id'] == 'water')
    proof = prove(repo, deck, ground, waters, layer['settings']['surface_offset_m'])
    queries, physics = [], []
    for cell, terrain in zip(selected, terrains):
        inside = [face for face in deck if cell['bounds'][0] <= sum(p[0] for p in face)/3 < cell['bounds'][2]
                  and cell['bounds'][1] <= sum(p[1] for p in face)/3 < cell['bounds'][3]]
        for face in inside[::max(1, len(inside)//8)][:8]:
            x, y = [sum(p[i] for p in face)/3 for i in (0, 1)]
            query, physical = grid_values(terrain, x, y)
            queries.append({'cell_id': cell['cell_id'], 'xy': [x, y], 'base_m': query, 'final_m': query})
            physics.append([x, y, physical])
    a, b = deck[0][0], deck[0][1]
    payload = {'regional_observation': True, 'water_preparation_scope': 'selected_complete_cells',
        'canonical_result': {'path': result.relative_to(repo).as_posix(), 'sha256': result_hash},
        'cell_id': ids[0], 'cell_ids': ids, 'constructed_feature_id': target['feature_id'],
        'terrain_queries': queries, 'required_physics_points': physics,
        'water_profile': {'world_data_plugin': profile['world_data_plugin'],
            'path': water_profile.relative_to(water_profile.parents[2]).as_posix(), 'sha256': digest(water_profile)},
        'player_eye': {'position': [a[0], a[1], a[2]+1.7], 'target': b},
        'classification': 'Inferred separate physical access fixture; exact Terrain/Water retained. No survey, Hero or geography acceptance.'}
    output.mkdir(parents=True)
    (output/'input.json').write_text(json.dumps(payload, indent=2)+'\n', encoding='ascii')
    proof.update({'canonical_result_sha256': result_hash, 'immutable_base_sha256': base_hash,
        'selected_complete_cells': ids, 'authenticated_parent_outputs': len(entries),
        'all_original_terrain_outputs_unchanged': len(unchanged), 'selected_water_features_unchanged': len(waters),
        'material_scope': 'Separate deck uses explicit existing BasicShapeMaterial fixture binding; Terrain/Water keep accepted materials.'})
    (output/'l0.json').write_text(json.dumps(proof, indent=2)+'\n', encoding='ascii')
    return {'input': (output/'input.json').relative_to(repo).as_posix(), 'sha256': digest(output/'input.json'), **proof}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    for name in ('canonical-result', 'base-result', 'water-profile'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--result-sha256', required=True)
    parser.add_argument('--base-sha256', required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(REPO, args.canonical_result, args.result_sha256,
        args.base_result, args.base_sha256, args.water_profile, args.output), indent=2))


if __name__ == '__main__':
    main()
