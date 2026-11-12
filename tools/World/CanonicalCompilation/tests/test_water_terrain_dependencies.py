"""Water consumes pre-conditioning Terrain, including off-footprint fallback support."""
from contextlib import redirect_stdout
from copy import deepcopy
from dataclasses import replace
from io import StringIO
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO/'tools'))
from World.CanonicalCompilation.app.contracts import read_json
from World.CanonicalCompilation.app.pipeline import compilation_root, compile_world, load_profile
from World.CanonicalCompilation.app.source import load_source_bundle
from World.CanonicalCompilation.app.spatial import grid_id
from World.CanonicalCompilation.app.features import compile_features
from World.CanonicalCompilation.tests.test_water_incremental import COMPILER_PROFILE, SOURCE_PROFILE, _water_feature, _water_semantics
from World.SourceIngestion.app.cli import main as source_main
from World.CanonicalCompilation.app.water_terrain_dependencies import changed_water_sources


def polygon(west, south, east, north):
    return {'type': 'Polygon', 'coordinates': [[[west, south], [east, south], [east, north], [west, north], [west, south]]]}


class WaterTerrainDependencyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with redirect_stdout(StringIO()):
            if source_main(['run', '--profile', str(SOURCE_PROFILE)]) != 0:
                raise RuntimeError('Synthetic source ingestion failed')

    @staticmethod
    def features(root):
        return {f['feature_id']: f for path in (root/'canonical/features').glob('*.json')
                for f in read_json(path)['features']}

    def test_raster_only_change_refits_dependent_water_and_matches_cold_compile(self):
        profile = deepcopy(load_profile(COMPILER_PROFILE))
        profile['$schema'] = 'https://alis.world/schemas/world-compiler/compiler-profile-v1.json'
        profile['profile_id'] = 'water_terrain_dependency'
        profile['water_semantics'] = _water_semantics()
        # The dependency fixture has 500 m grid cells, not playable geography.
        profile['terrain_surface']['water_conditioning_limits'] = {
            'maximum_cut_m': 2., 'maximum_support_distance_m': 1000.,
            'accepted_accuracy': ['derived_not_surveyed', 'low_confidence_point_fallback']}
        bundle = load_source_bundle(REPO, compilation_root(), profile)
        raster = deepcopy(bundle.raster)
        raster['provider_payload']['samples'] = [[100. for _ in row] for row in raster['provider_payload']['samples']]
        water = [
            _water_feature('way/20', polygon(-.55, .45, -.45, .55), water='lake'),
            _water_feature('way/21', polygon(-.76, .74, -.74, .76), water='lake'),
            _water_feature('way/22', polygon(.74, .74, .76, .76), water='lake'),
            _water_feature('relation/10', polygon(-.98, .15, -.1, .3), water='river'),
            _water_feature('way/11', {'type': 'LineString', 'coordinates': [[-.95, .225], [-.15, .225]]}, waterway='river', width='12 m'),
        ]
        initial = replace(bundle, raster=raster, features=[*bundle.features, *water])
        changed_raster = deepcopy(raster)
        changed_raster['provider_payload']['samples'][2][2] = 99.
        changed = replace(initial, raster=changed_raster)
        scratch = REPO/'tmp/world/canonical_water_dependencies'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            root = Path(temporary)
            overlay = {'$schema': 'https://alis.world/schemas/world-compiler/authored-overlay-v2.json',
                'schema_version': 2, 'overlay_id': 'water_dependency', 'grid_id': grid_id(profile['grid']),
                'provenance': [], 'terrain_patches': [], 'feature_overrides': [],
                'missing_target_policy': 'reject_and_require_rebase'}
            (root/'overlay.json').write_text(json.dumps(overlay), encoding='ascii')
            profile['authored_overlay'] = (root/'overlay.json').relative_to(REPO).as_posix()
            profile_path = root/'profile.json'
            profile_path.write_text(json.dumps(profile), encoding='ascii')
            with patch('World.CanonicalCompilation.app.pipeline.load_source_bundle', return_value=initial):
                compile_world(str(profile_path), output_root_value=root/'base')
            with patch('World.CanonicalCompilation.app.pipeline.load_source_bundle', return_value=changed):
                with patch('World.CanonicalCompilation.app.pipeline.compile_features', wraps=compile_features) as fit:
                    compile_world(str(profile_path), output_root_value=root/'incremental', base_result=root/'base/compile_result.json')
                compile_world(str(profile_path), output_root_value=root/'cold')
                with patch('World.CanonicalCompilation.app.pipeline.compile_features', wraps=compile_features) as noop_fit:
                    compile_world(str(profile_path), output_root_value=root/'noop', base_result=root/'incremental/compile_result.json')
            equivalent_raster = deepcopy(changed_raster)
            equivalent_raster['provider_payload']['samples'][2][2] = 99.01
            with patch('World.CanonicalCompilation.app.pipeline.load_source_bundle', return_value=replace(changed, raster=equivalent_raster)):
                with patch('World.CanonicalCompilation.app.pipeline.compile_features', wraps=compile_features) as quantized_fit:
                    compile_world(str(profile_path), output_root_value=root/'quantized', base_result=root/'incremental/compile_result.json')
            before, incremental, cold = [self.features(root/name) for name in ('base', 'incremental', 'cold')]
            self.assertEqual(cold, incremental, 'A Terrain-only input change cannot retain stale Water functions')
            terrain_fields = ('core_samples', 'halo_window', 'height_edges', 'surface_semantics', 'water_fit')
            def terrain_surface(name):
                return {path.name: {key: read_json(path)[key] for key in terrain_fields}
                        for path in (root/name/'canonical/terrain').glob('*.json')}
            self.assertEqual(terrain_surface('cold'), terrain_surface('incremental'),
                             'Refitted Water must also reconstruct the final Terrain consumer')
            for suffix in ('way:20', 'way:21', 'relation:10'):
                key = next(k for k in before if k.endswith(suffix))
                self.assertNotEqual(before[key]['attributes']['surface_function'], incremental[key]['attributes']['surface_function'])
            remote = next(k for k in before if k.endswith('way:22'))
            self.assertEqual(before[remote], incremental[remote])
            self.assertEqual(fit.call_count, 1)
            self.assertEqual({f['provider_feature_id'] for f in fit.call_args.args[6]}, {'way/20', 'way/21', 'relation/10', 'way/11'})
            self.assertEqual(noop_fit.call_count, 0)
            self.assertEqual(quantized_fit.call_count, 0, 'Identical quantized Water inputs must remain reusable')
            self.assertEqual(self.features(root/'quantized'), incremental)
            identifier = grid_id(profile['grid'])
            self.assertEqual(read_json(root/'incremental/reports/diff.json')['feature_rebuilt_cells'], [f'{identifier}:x0:y0'])
            self.assertEqual(read_json(root/'noop/reports/diff.json')['feature_rebuilt_cells'], [])

    @staticmethod
    def cell():
        return {'bounds': [0, 0, 2, 2], 'sample_spacing': [1, 1], 'core_samples': [[100.]*3 for _ in range(3)],
                'water_fit': {'core_samples': [[100.]*3 for _ in range(3)],
                              'halo_window': {'samples': [[100.]*5 for _ in range(5)]}}}

    def test_final_surface_only_changes_and_constant_water_do_not_refit(self):
        before = {'cell': self.cell()}
        after = deepcopy(before)
        after['cell']['core_samples'][0][0] = 120.
        derived = {'feature_id': 'water', 'feature_class': 'water',
                   'attributes': {'surface_function': {'vertical_basis': 'canonical_terrain_polygon_quantile'}},
                   'geometry': polygon(.1, .1, .2, .2)}
        profile = {'grid': {'canonical_crs': 'ALIS:FIXTURE_METRE_V1'}}
        with patch('World.CanonicalCompilation.app.water_terrain_dependencies.polygon_sampling_dependencies', side_effect=AssertionError('Unchanged fitting inputs need no geometry work')):
            self.assertEqual(changed_water_sources(REPO, profile, before, after, SimpleNamespace(by_owner={'cell': [derived]}), REPO/'tmp/world/unused'), set())
        constant = deepcopy(derived)
        constant['attributes']['surface_function'] = {'vertical_basis': 'profile_marine_level', 'level_m': 0.}
        after['cell']['water_fit']['halo_window']['samples'][0][0] = 90.
        self.assertEqual(changed_water_sources(REPO, profile, before, after, SimpleNamespace(by_owner={'cell': [constant]}), REPO/'tmp/world/unused'), set())

    def test_flowing_fit_uses_consumed_halo_and_returns_complete_group(self):
        before = {'cell': self.cell()}
        after = deepcopy(before)
        after['cell']['water_fit']['halo_window']['samples'][2][0] = 99.
        flow = {'feature_id': 'flow', 'feature_class': 'water',
                'source_refs': [{'provider_feature_id': 'relation/10'}],
                'attributes': {'surface_group_members': ['relation/10', 'way/11'],
                               'surface_function': {'vertical_basis': 'canonical_terrain_robust_fit',
                                                    'knots': [[-.5, .5, 100.], [.5, .5, 100.]]}}}
        result = changed_water_sources(REPO, {'grid': {}}, before, after,
                                       SimpleNamespace(by_owner={'cell': [flow]}), REPO/'tmp/world/unused')
        self.assertEqual(result, {'relation/10', 'way/11'})


if __name__ == '__main__':
    unittest.main()
