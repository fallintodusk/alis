"""Small captured Pier shore input; replay is diagnostic, never geographic acceptance."""
from copy import deepcopy
import json
from time import perf_counter

from ..app.spatial import quantize
from ..app.terrain_water_coverage import support_bounds
from ..refinement.app.road_elevation import _height

CAPTURE = {'basis': 'captured_model_not_surveyed_shore',
 'frame': {'horizontal': 'EPSG:32618', 'vertical': 'EPSG:3855'},
 'source_result_sha256': '20cbb8d9fb1d993cf07876f52625c9b2fb9efa8067029919ffee1fab5ee33f66',
 'comparison_source_sha256': 'bce92cc03efd82ebaf814670ee14b4177bb39d4c650d1eb5768a25c8300c5f40',
 'cell': {'bounds': [583333.41, 4508052.45, 583453.41, 4508232.45],
          'sample_spacing': [30.0, 30.0],
          'water_fit': {'halo_window': {'samples': [[2.1, 2.1, 2.3, 2.1, 2.3, 2.1, 3.3],
                                                    [-1.0, -1.0, -0.2, 2.0, 2.2, 2.1, 2.7],
                                                    [-0.8, -1.0, 0.9, 2.1, 2.1, 2.0, 2.5],
                                                    [2.3, 2.3, 2.0, 2.3, 2.1, 1.8, 2.0],
                                                    [-0.9, 2.3, 2.1, 2.1, 2.4, 1.9, 2.5],
                                                    [-1.0, -0.9, 0.2, 2.0, 2.3, 2.1, 2.2],
                                                    [-1.0, -1.0, 0.8, 2.0, 2.3, 2.4, 2.4],
                                                    [-1.0, -1.0, 1.3, 1.7, 2.1, 2.5, 2.9],
                                                    [-1.0, -1.0, 1.5, 1.9, 2.1, 2.4, 3.0]]}}},
 'expected_original_core': [[-1.0, -1.0, -1.0, 2.2, 2.1],
                            [-1.0, -1.0, -1.0, 2.1, 2.0],
                            [-1.0, -1.0, -1.0, 2.1, 1.8],
                            [-1.0, -1.0, -1.0, 2.4, 1.9],
                            [-1.0, -1.0, -1.0, 2.3, 2.1],
                            [-1.0, -1.0, -1.0, 2.3, 2.4],
                            [-1.0, -1.0, 1.7, 2.1, 2.5]],
 'original_water': {'feature_id': 'alis:osm:relation:2389611',
                    'geometry': {'type': 'Polygon',
                                 'coordinates': [[[583273.41, 4508292.45],
                                                  [583392.6752383664, 4508292.45],
                                                  [583384.78, 4508239.41],
                                                  [583379.39, 4508205.27],
                                                  [583375.54, 4508178.23],
                                                  [583366.48, 4508116.95],
                                                  [583365.1266901811, 4508107.095862727],
                                                  [583355.61, 4508037.8],
                                                  [583348.3413980895, 4507992.45],
                                                  [583273.41, 4507992.45],
                                                  [583273.41, 4508292.45]]]},
                    'attributes': {'surface_function': {'function_id': 'standing_polygon_quantile',
                                                        'function_version': 1,
                                                        'vertical_basis': 'canonical_terrain_polygon_quantile',
                                                        'accuracy': 'derived_not_surveyed',
                                                        'sample_count': 15143,
                                                        'level_m': -1.0}}},
 'authored_water': {'feature_id': 'alis:osm:relation:2389611',
                    'geometry': {'type': 'Polygon',
                                 'coordinates': [[[583273.41, 4508292.45],
                                                  [583392.6752383664, 4508292.45],
                                                  [583384.78, 4508239.41],
                                                  [583384.593702402, 4508238.23],
                                                  [583369.5019999997, 4508205.27],
                                                  [583357.54, 4508178.23],
                                                  [583348.48, 4508116.95],
                                                  [583352.6249406129, 4508091.468675766],
                                                  [583358.239949463, 4508056.95],
                                                  [583355.61, 4508037.8],
                                                  [583348.3413980895, 4507992.45],
                                                  [583273.41, 4507992.45],
                                                  [583273.41, 4508292.45]]]},
                    'attributes': {'surface_function': {'function_id': 'standing_polygon_quantile',
                                                        'function_version': 1,
                                                        'vertical_basis': 'canonical_terrain_polygon_quantile',
                                                        'accuracy': 'derived_not_surveyed',
                                                        'sample_count': 15143,
                                                        'level_m': -1.0}}},
 'contacts': [{'xy': [583368.4544677734, 4508118.615664063], 'deck_height_m': 0.42037277221679686},
              {'xy': [583368.749194336, 4508120.593789062], 'deck_height_m': 0.44493320465087893},
              {'xy': [583375.0082958984, 4508173.761796875], 'deck_height_m': 0.8286785125732422},
              {'xy': [583375.2273291016, 4508175.749726563], 'deck_height_m': 0.8116090393066406}],
 'contract': {'water_clearance_m': 0.0,
              'water_conditioning_limits': {'maximum_cut_m': 16.4,
                                            'maximum_support_distance_m': 42.43,
                                            'accepted_accuracy': ['derived_not_surveyed',
                                                                  'low_confidence_point_fallback']}}}


def fixture():
    """Detached model inputs require no source cache, network or Unreal."""
    return deepcopy(CAPTURE)


def compare():
    value = fixture()
    cell = value['cell']; west, _, _, north = cell['bounds']
    outputs = {}
    for name in ('original_water', 'authored_water'):
        bounds = support_bounds(cell, [value[name]], value['contract'], .1, 1)
        raw = cell['water_fit']['halo_window']['samples']
        final = [[quantize(min(z, bound), .1) for z, bound in zip(row, limits)]
                 for row, limits in zip(raw, bounds)]
        core = [row[1:-1] for row in final[1:-1]]
        if name == 'original_water':
            assert core == value['expected_original_core'], 'Reduced fixture no longer reproduces captured Ground'
        contacts = []
        for contact in value['contacts']:
            p = contact['xy']; x = int((p[0]-west)/30); y = int((north-p[1])/30)
            a,b,c,d = [[west+i*30,north-j*30,core[j][i]] for i,j in
                       ((x,y),(x+1,y),(x,y+1),(x+1,y+1))]
            z = _height([(a,c,b),(b,c,d)], p)
            contacts.append({'ground_m':z,'deck_contact_error_m':abs(z-contact['deck_height_m'])})
        outputs[name] = contacts
    return {'classification':'diagnostic_only_not_acceptance','contacts':outputs,
            'authored_model_disposition':'REFUSED' if any(p['deck_contact_error_m']>.01
                for p in outputs['authored_water']) else 'CONTACTS_ONLY'}


def compare_sparse(value=None):
    """Characterize existing exact clipping; never admit an unclosed shoreline field."""
    from collections import Counter
    import math
    from pathlib import Path
    from ..app.contracts import CompilerError
    from ..app.terrain_surface import _footprint_distance, _segments
    from ..refinement.app.geometry import _quad, _check_outer

    value = fixture() if value is None else deepcopy(value)
    cell = value['cell']; west, _, _, north = cell['bounds']
    raw = [row[1:-1] for row in cell['water_fit']['halo_window']['samples'][1:-1]]
    retained = value['expected_original_core']; water = value['original_water']
    level = water['attributes']['surface_function']['level_m']

    def point(i, j, samples):
        # Constant mechanical weights isolate height/topology. These are not
        # authenticated canonical DTOs or a new semantic-weight interpretation.
        return [west+i*30, north-j*30, samples[j][i], 0., 1.]

    def quad_points(x, y, samples):
        return [point(i,j,samples) for i,j in ((x,y),(x+1,y),(x,y+1),(x+1,y+1))]

    selected = set()
    for contact in value['contacts']:
        x = int((contact['xy'][0]-west)/30); y = int((north-contact['xy'][1])/30)
        selected.update((x+dx,y+dy) for dx in (-1,0,1) for dy in (-1,0,1)
                        if 0 <= x+dx < len(raw[0])-1 and 0 <= y+dy < len(raw)-1)
    perimeter = {}; counts = Counter()
    for x,y in selected:
        a,b,c,d = quad_points(x,y,retained)
        for left,right in ((a,b),(b,d),(d,c),(c,a)):
            key = tuple(sorted((tuple(left[:2]),tuple(right[:2]))))
            counts[key] += 1; perimeter[key] = (left,right)
    edges = [perimeter[key] for key,count in counts.items() if count==1]
    # Generous diagnostic limits expose geometry; they authorize no bank policy.
    policy = {'maximum_triangles':4096, 'maximum_cut_m':16.4,
              'maximum_dry_distance_m':100., 'maximum_bank_gradient':100.}
    faces = []; source_faces = []; exterior = set(); failures = Counter()
    dry_cut_volume = 0.; total_cut_volume = 0.; dry_area = 0.; maximum_gradient = 0.
    root = Path(__file__).resolve().parents[4]
    for x,y in sorted(selected):
        a,b,c,d = quad_points(x,y,raw)
        original = [[p[:3] for p in tri] for tri in ((a,c,b),(b,c,d))]
        source_faces.extend(original)
        quad,metrics = _quad(root,cell,x,y,(a,b,c,d),water,level,policy,[])
        total_cut_volume += metrics['cut_volume_m3']
        maximum_gradient = max(maximum_gradient,metrics['maximum_bank_gradient'])
        for vertex in quad['vertices']:
            try: _check_outer(vertex,edges)
            except CompilerError as error:
                key = (error.code,*vertex[:3])
                if key not in exterior: failures[error.code] += 1
                exterior.add(key)
        wet = set(quad['water_triangles'])
        for index,indices in enumerate(quad['triangles']):
            face = [quad['vertices'][i][:3] for i in indices]
            faces.append(face)
            if index in wet: continue
            p,q,r = face
            area = abs((q[0]-p[0])*(r[1]-p[1])-(r[0]-p[0])*(q[1]-p[1]))*.5
            dry_area += area
            dry_cut_volume += area*math.fsum(max(0.,_height(original,v[:2])-v[2]) for v in face)/3
    contacts = []
    for contact in value['contacts']:
        xy = contact['xy']; source = _height(source_faces,xy); modeled = _height(faces,xy)
        distance = _footprint_distance(water,xy)
        nearest = []
        for a,b in _segments(water['geometry']):
            dx,dy = b[0]-a[0],b[1]-a[1]; length2 = dx*dx+dy*dy
            t = max(0.,min(1.,((xy[0]-a[0])*dx+(xy[1]-a[1])*dy)/length2)) if length2 else 0.
            nearest.append([a[0]+t*dx,a[1]+t*dy])
        boundary = min(nearest,key=lambda p:math.dist(p,xy))
        boundary_height = _height(source_faces,boundary)
        contacts.append({**contact, 'source_ground_m':source, 'sparse_ground_m':modeled,
            'dry_ground_departure_m':abs(source-modeled), 'water_distance_m':distance,
            'nearest_water_boundary_xy':boundary, 'source_boundary_height_m':boundary_height,
            'boundary_height_conflict_if_all_dry_preserved_m':max(0.,boundary_height-level),
            'minimum_transition_gradient_if_source_contact_retained':max(0.,source-level)/distance,
            'source_to_old_deck_difference_m':source-contact['deck_height_m']})
    return {'classification':'unpublished_sparse_diagnostic_not_acceptance',
        'selected_quads':len(selected), 'triangles':len(faces), 'dry_area_m2':dry_area,
        'dry_cut_volume_m3':dry_cut_volume, 'total_cut_volume_m3':total_cut_volume,
        'maximum_dry_bank_gradient':maximum_gradient,
        'outer_refusals':dict(failures), 'contacts':contacts,
        'disposition':'NOT_ADMISSIBLE' if exterior or dry_cut_volume>1e-8 else 'DIAGNOSTIC_ONLY'}


if __name__ == '__main__':
    import sys
    start = perf_counter()
    result = compare_sparse() if sys.argv[1:]==['--sparse'] else compare()
    print(json.dumps({**result, 'seconds':perf_counter()-start}, indent=2))
