"""Typed saved-Terrain fixture from the captured optional continuation owner."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT/'tools'))
from World.CanonicalCompilation.refinement.ground_continuation import build_ground_continuation
from World.CanonicalCompilation.tests.ground_continuation_fixture import fixture


def main(destination):
    destination = destination.resolve()
    if destination.exists() or destination == ROOT/'tmp' or not destination.is_relative_to(ROOT/'tmp'):
        raise ValueError('Continuation consumer fixture requires fresh project scratch')
    values = fixture()
    result = build_ground_continuation(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=4096)
    identifier, cell = next(iter(values['cells'].items()))
    payload = {'classification': 'Reduced typed Terrain consumer; identified shore proxy, unknown bed/current coast, no geographic authority',
        'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
        'engine_georeference_origin_m': [585628., 4511322.], 'height_origin_m': 0.,
        'source_frame_bundle_sha256': '84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067',
        'original_cell_id': 'grid_36784852dea14f43:x-1:y-3',
        'original_quad_coordinates': [[1, 13], [2, 13], [3, 13]],
        'reduction': 'Three complete original30m quads; source XY/corners/weights and identified incoming field retained; typed indices rebased0..2 only.',
        'cell_id': identifier, 'bounds': cell['bounds'], 'sample_size': [4, 2], 'sample_spacing_m': [30., 30.],
        'base_heights': [z for row in cell['core_samples'] for z in row],
        'base_weights': [[cell['surface_semantics']['core_samples'][role][y][x]
            for role in ('ground', 'hydro_transition')] for y in range(2) for x in range(4)],
        'quads': result['surfaces'][identifier]['quads'], 'construction_consumed': result['consumed'],
        'model_limits': 'Zero wet comparison is not a bed or Water-role admission; all other outer traces remain OPEN.'}
    destination.mkdir(parents=True)
    path = destination/'composite-terrain-fixture.json'
    path.write_text(json.dumps(payload, separators=(',', ':'), allow_nan=False)+'\n', encoding='ascii', newline='\n')
    print(json.dumps({'path': path.relative_to(ROOT).as_posix(), 'quads': 3,
        'faces': sum(len(q['triangles']) for q in payload['quads'])}))


if __name__ == '__main__':
    main(Path(sys.argv[1]))
