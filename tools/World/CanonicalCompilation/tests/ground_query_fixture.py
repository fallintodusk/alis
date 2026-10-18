"""Test-only typed Ground payload, not an authenticated/loadable authority wrapper."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.refinement.ground import build_ground_surface
from World.CanonicalCompilation.tests.test_ground_surface import fixture


def main(destination):
    if destination.exists():
        raise RuntimeError('Ground fixture requires a fresh disposable directory')
    destination.relative_to(ROOT / 'tmp')
    cells, grid, evidence, selected = fixture()
    result = build_ground_surface(ROOT, cells, grid, evidence, selected,
                                  maximum_triangles=32, canonical_crs='EPSG:32618')
    payload = {'classification': 'Test-only typed Ground construction; no authority admission',
               'cells': [], 'source_queries': []}
    for key, cell in cells.items():
        payload['cells'].append({'cell_id': key, 'bounds': cell['bounds'],
            'base_heights': [z for row in cell['core_samples'] for z in row],
            'base_weights': [[cell['surface_semantics']['core_samples'][role][y][x]
                             for role in ('ground', 'hydro_transition')]
                            for y in range(2) for x in range(2)],
            'quads': result['surfaces'][key]['quads']})
    for y, row in enumerate(grid['samples']):
        for x, height in enumerate(row):
            payload['source_queries'].append([grid['origin'][0] + x * grid['spacing'][0],
                grid['origin'][1] + y * grid['spacing'][1], height])
    destination.mkdir(parents=True)
    (destination / 'ground-typed-fixture.json').write_text(json.dumps(payload) + '\n', encoding='ascii')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve())
