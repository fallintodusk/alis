"""Captured pure composite for a typed native consumer test; no authority admission."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.refinement.ground_composite import build_ground_composite
from World.CanonicalCompilation.tests.ground_composite_fixture import fixture


def main(destination):
    destination.relative_to(ROOT / 'tmp')
    if destination.exists():
        raise RuntimeError('Composite fixture requires a fresh disposable directory')
    cells, grid, evidence, selected, inner = fixture()
    result = build_ground_composite(ROOT, cells, grid, evidence, selected, inner,
                                    canonical_crs='EPSG:32618', maximum_triangles=4096)
    cell = cells['captured_quad']
    payload = {
        'classification': 'Reduced captured typed Terrain consumer only; no canonical loader, historical outer seam or production admission',
        'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
        'engine_georeference_origin_m': [585628., 4511322.], 'height_origin_m': 0.,
        'source_frame_bundle_sha256': '84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067',
        'original_cell_id': 'grid_36784852dea14f43:x-1:y-3',
        'original_quad_coordinates': [[4, 11]],
        'reduction': 'One complete original30m quad, preserving every captured XYZ and semantic weight; typed coordinate rebased to0,0 only.',
        'cell_id': 'captured_composite_quad', 'bounds': cell['bounds'],
        'sample_size': [2, 2], 'sample_spacing_m': [30., 30.],
        'base_heights': [z for row in cell['core_samples'] for z in row],
        'base_weights': [[cell['surface_semantics']['core_samples'][role][y][x]
                          for role in ('ground', 'hydro_transition')]
                         for y in range(2) for x in range(2)],
        'quads': result['surfaces']['captured_quad']['quads'],
        'construction_consumed': result['consumed'],
    }
    destination.mkdir(parents=True)
    (destination / 'composite-terrain-fixture.json').write_text(
        json.dumps(payload, separators=(',', ':')) + '\n', encoding='ascii', newline='\n')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve())
