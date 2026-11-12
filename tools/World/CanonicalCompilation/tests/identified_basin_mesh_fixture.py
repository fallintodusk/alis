"""Actual joint inferred basin for disposable typed native consumers only."""
from copy import deepcopy
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT/'tools'))
from World.CanonicalCompilation.refinement.identified_basin import build_identified_basin
from World.CanonicalCompilation.tests.identified_basin_fixture import fixture


def main(destination):
    destination.relative_to(ROOT/'tmp')
    if destination.exists():
        raise RuntimeError('Joint basin fixture needs a fresh disposable directory')
    values = fixture()
    result = build_identified_basin(ROOT, **values, canonical_crs='EPSG:32618', maximum_triangles=4096)
    cells = [values['cells'][key] for key in sorted(values['cells'])]
    north, south = cells
    assert north['core_samples'][1] == south['core_samples'][0]
    quads = []
    for y, key in enumerate(sorted(values['cells'])):
        quad = deepcopy(result['surfaces'][key]['quads'][0])
        quad['coordinate'] = [0, y]
        quads.append(quad)
    payload = {'classification': 'Typed inferred joint basin only; no canonical parser/source admission or original-cell exterior closure',
        'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
        'engine_georeference_origin_m': [585628., 4511322.], 'height_origin_m': 0.,
        'source_frame_bundle_sha256': '84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067',
        'original_cell_id': 'grid_36784852dea14f43:x2:y2', 'original_quad_coordinates': [[3, 8], [3, 9]],
        'reduction': 'Two complete original30m quads with exact XYZ/weights; typed indices rebased only; inferred basin stage/support.',
        'cell_id': 'captured_identified_basin',
        'bounds': [north['bounds'][0], south['bounds'][1], north['bounds'][2], north['bounds'][3]],
        'sample_size': [2, 3], 'sample_spacing_m': [30., 30.],
        'base_heights': [z for row in [*north['core_samples'], south['core_samples'][1]] for z in row],
        'base_weights': [[cell['surface_semantics']['core_samples'][role][row][x] for role in ('ground', 'hydro_transition')]
            for cell, row in ((north, 0), (north, 1), (south, 1)) for x in (0, 1)],
        'quads': quads, 'modeled_water': result['modeled_water'], 'construction_consumed': result['consumed'],
        'model_metrics': result['metrics'], 'boundary_encoding_certificates': result['boundary_encoding_certificates']}
    destination.mkdir(parents=True)
    (destination/'composite-terrain-fixture.json').write_text(json.dumps(payload, separators=(',', ':'))+'\n',
        encoding='ascii', newline='\n')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve())
