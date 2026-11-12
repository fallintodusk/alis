"""Joint inferred basin publication, closed geometry and safe refusal."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from ..app.contracts import CompilerError, canonical_hash, file_hash, read_json
from ..app.artifacts import output_descriptor
from ..app import promotion
from ..refinement.api import prepare_surface_refinement
from ..refinement.app import surface_publication
from ..refinement.app import identity
from ..refinement import basin_admission
from ..refinement.app.authority import _replay_result, _selection_identity, validate_selection_bundle
from ..refinement.app.contracts import write_json
from .identified_basin_fixture import fixture as fine_fixture
from .basin_admission_fixture import add_east_building, add_east_cell, add_feature, make_fixture, reauthenticate
from .test_road_refinement import inventory
from .structural_import_fixture import make_fixture as structural_fixture
from ..refinement import constructed_import

ROOT = Path(__file__).resolve().parents[4]


class BasinAdmissionTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT/'tmp/world/basin_admission_unit'
        scratch.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.fixture = make_fixture(self.root)

    def prepare(self, name='candidate', operation=None):
        return prepare_surface_refinement(ROOT, self.fixture['base'], self.root/name, [operation or self.fixture['operation']])

    def input_operation(self, document, name):
        path = self.root/(name+'.json')
        path.write_text(json.dumps(document, allow_nan=False)+'\n', encoding='ascii')
        return {**self.fixture['operation'], 'input': {'path': path.relative_to(ROOT).as_posix(),
            'sha256': file_hash(path), 'byte_size': path.stat().st_size}}

    def test_building_footprint_from_unselected_owner_refuses_without_publication(self):
        building = add_east_building(self.fixture)
        before = inventory(self.fixture['base'].parent)
        with self.assertRaisesRegex(CompilerError, 'civil support') as failure:
            self.prepare('building-refused')
        self.assertEqual(failure.exception.details['feature_ids'], [building['feature_id']])
        self.assertEqual(before, inventory(self.fixture['base'].parent))
        self.assertFalse((self.root/'building-refused').exists())

    def test_effective_building_volume_is_the_physical_interaction(self):
        building = add_east_building(self.fixture)
        base = self.fixture['base'].parent
        path = base/'canonical/features/east.json'
        shard = read_json(path)
        feature = shard['features'][0]
        feature['attributes']['effective_volumes'] = [{'volume_id': 'way/9002/part',
            'geometry': deepcopy(building['geometry']), 'min_height_m': 0., 'height_m': 10.,
            'height_basis': 'explicit', 'min_height_basis': 'explicit', 'source_feature_id': 'way/9002'}]
        # Outline is outside; the actual selected volume still crosses the basin.
        for point in feature['geometry']['coordinates'][0]:
            point[0] += 100
        write_json(path, shard)
        coverage = read_json(base/'canonical/coverage.json')
        entry = next(e for e in coverage['cells'] if e['cell_id'] == building['owner_cell_id'])
        manifest = read_json(base/entry['path'])
        descriptor = manifest['feature_artifact']
        descriptor.update(content_hash=file_hash(path), byte_size=path.stat().st_size)
        write_json(base/entry['path'], manifest)
        entry.update(sha256=file_hash(base/entry['path']), byte_size=(base/entry['path']).stat().st_size)
        next(d for d in coverage['artifact_descriptors'] if d['artifact_id'] == descriptor['artifact_id']).update(
            {k: v for k, v in descriptor.items() if k != 'path'})
        write_json(base/'canonical/coverage.json', coverage)
        reauthenticate(base)
        with self.assertRaisesRegex(CompilerError, 'civil support') as failure:
            self.prepare('volume-refused')
        self.assertEqual(failure.exception.details['feature_ids'], [building['feature_id']])
        self.assertFalse((self.root/'volume-refused').exists())

    def test_basin_leaf_contract_changes_do_not_invalidate_unused_operations(self):
        common = identity.implementation_identity()
        selected = basin_admission.implementation_identity(ROOT)
        for relative in ('contracts/identified_basin/surface.schema.json',
                'refinement/contracts/identified_basin/input.schema.json'):
            target = Path(__file__).resolve().parents[1]/relative
            def changed(path):
                return canonical_hash({'changed': file_hash(path)}) if Path(path) == target else file_hash(path)
            with self.subTest(contract=relative), patch.object(identity, 'file_hash', changed), patch.object(
                    basin_admission, 'file_hash', changed):
                self.assertEqual(common, identity.implementation_identity())
                self.assertNotEqual(selected, basin_admission.implementation_identity(ROOT))

    def test_earlier_structural_creation_is_in_the_basin_interaction_census(self):
        source = self.root/'captured-structure'
        source.mkdir()
        structural = structural_fixture(source)
        model = deepcopy(structural['model'])
        # Synthetic physical interaction; captured source records stay intact.
        ring = [[586199., 4512908.], [586199., 4512910.], [586202., 4512910.],
            [586202., 4512908.], [586199., 4512908.]]
        item = model['models'][0]
        item['footprint'] = {'type': 'Polygon', 'coordinates': [ring]}
        xyz = [[*p, 20.2] for p in ring[:-1]]
        item['triangles'] = [[xyz[i] for i in row] for row in ((0, 1, 2), (0, 2, 3))]
        item['face_roles'][0]['triangles'] = [0, 1]
        model['provenance']['basis'] = 'mechanical_interaction_control_not_real_pier_geography'
        path = self.root/'mechanical-structure.json'
        path.write_text(json.dumps(model, allow_nan=False)+'\n', encoding='ascii')
        operation = deepcopy(structural['operation'])
        operation['inputs']['model'] = {'path': path.relative_to(ROOT).as_posix(),
            'sha256': file_hash(path), 'byte_size': path.stat().st_size}
        before = inventory(self.fixture['base'].parent)
        prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'structure-alone', [operation])
        with patch.object(constructed_import, 'construct', wraps=constructed_import.construct) as earlier:
            with self.assertRaisesRegex(CompilerError, 'civil support') as failure:
                prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'structure-refused',
                    [self.fixture['operation'], operation])
            self.assertEqual(earlier.call_count, 1)
        self.assertEqual(failure.exception.details['feature_ids'], operation['feature_ids'])
        self.assertEqual(before, inventory(self.fixture['base'].parent))
        self.assertFalse((self.root/'structure-refused').exists())

    def test_closed_source_shaped_basin_stages_water_and_terrain_together(self):
        before = inventory(self.fixture['base'].parent)
        result, child = prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'candidate', [self.fixture['operation']])
        self.assertEqual(result['operation'], 'refine')
        self.assertEqual(before, inventory(self.fixture['base'].parent))
        coverage = read_json(child/'canonical/coverage.json')
        for entry in coverage['cells']:
            manifest = read_json(child/entry['path'])
            self.assertTrue(manifest['terrain_artifact']['path'].startswith('canonical/refined_terrain/'))
        water = next(f for entry in result['outputs'] if entry['path'].startswith('canonical/features/')
            for f in read_json(child/entry['path'])['features'])
        self.assertEqual(water['attributes']['surface_function']['function_id'], 'identified_static_lined_basin')
        self.assertEqual(water['source_refs'], self.fixture['water']['source_refs'])

    def test_disjoint_structure_water_and_terrain_share_one_complete_candidate(self):
        east = add_east_cell(self.fixture)
        base = self.fixture['base'].parent
        original = read_json(base/'canonical/coverage.json')
        manifests = {e['cell_id']: read_json(base/e['path']) for e in original['cells']}
        west, south, right, north = manifests[east]['bounds']
        x, y = (west+right)/2, (south+north)/2
        source = self.root/'captured-disjoint-structure'
        source.mkdir()
        structural = structural_fixture(source)
        model = deepcopy(structural['model'])
        ring = [[x-1, y-1], [x+1, y-1], [x+1, y+1], [x-1, y+1], [x-1, y-1]]
        item = model['models'][0]
        item['footprint'] = {'type': 'Polygon', 'coordinates': [ring]}
        xyz = [[*p, 20.2] for p in ring[:-1]]
        item['triangles'] = [[xyz[i] for i in row] for row in ((0, 1, 2), (0, 2, 3))]
        item['face_roles'][0]['triangles'] = [0, 1]
        model['provenance']['basis'] = 'mechanical_cross_owner_contract_not_real_pier_geography'
        path = self.root/'disjoint-structure.json'
        path.write_text(json.dumps(model, allow_nan=False)+'\n', encoding='ascii')
        operation = deepcopy(structural['operation'])
        operation['inputs']['model'] = {'path': path.relative_to(ROOT).as_posix(),
            'sha256': file_hash(path), 'byte_size': path.stat().st_size}
        before = inventory(base)
        result, child = prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'mixed',
            [self.fixture['operation'], operation])
        coverage = read_json(child/'canonical/coverage.json')
        self.assertEqual((len(coverage['cells']), coverage['feature_count']), (3, 2))
        final = {e['cell_id']: read_json(child/e['path']) for e in coverage['cells']}
        features = [f for manifest in final.values()
            for f in read_json(child/manifest['feature_artifact']['path'])['features']]
        self.assertEqual(len(features), len({f['feature_id'] for f in features}))
        water = next(f for f in features if f['feature_class'] == 'water')
        structure = next(f for f in features if f['feature_class'] == 'constructed_surface')
        self.assertEqual(structure['owner_cell_id'], east)
        self.assertEqual(structure['attributes']['constructed_surface']['triangles'], item['triangles'])
        self.assertEqual(structure['attributes']['constructed_surface']['face_roles'], item['face_roles'])
        self.assertEqual(structure['attributes']['constructed_surface']['policy_sha256'], operation['inputs']['policy']['sha256'])
        self.assertEqual(water['source_refs'], self.fixture['water']['source_refs'])
        for feature in features:
            for identifier, manifest in final.items():
                self.assertEqual(manifest['owned_feature_ids'].count(feature['feature_id']), int(identifier == feature['owner_cell_id']))
                self.assertEqual(manifest['referenced_feature_ids'].count(feature['feature_id']),
                    int(identifier in feature['intersecting_cell_ids'] and identifier != feature['owner_cell_id']))
        support = water['attributes']['surface_function']['terrain_support']
        self.assertEqual(set(support), set(self.fixture['cells']))
        for identifier, manifest in final.items():
            descriptor = manifest['terrain_artifact']
            if identifier == east:
                self.assertEqual(descriptor, manifests[east]['terrain_artifact'])
                self.assertEqual(file_hash(child/descriptor['path']), file_hash(base/descriptor['path']))
            else:
                self.assertTrue(descriptor['path'].startswith('canonical/refined_terrain/'))
                self.assertEqual(support[identifier], {'path': descriptor['path'], 'sha256': descriptor['content_hash'], 'byte_size': descriptor['byte_size']})
        lower = next(m for key, m in final.items() if key != east and m['bounds'][1] == south and m['bounds'][3] == north)
        quad = read_json(child/lower['terrain_artifact']['path'])['quads'][0]
        neighbor = read_json(base/manifests[east]['terrain_artifact']['path'])
        segments = []
        for triangle in quad['triangles']:
            points = [quad['vertices'][i] for i in triangle]
            for a, b in zip(points, points[1:]+points[:1]):
                if a[0] == b[0] == west and a[1] != b[1]:
                    segments.append(sorted((a[1], b[1])))
                    for p in (a, b):
                        fraction = (north-p[1])/(north-south)
                        expected = neighbor['core_samples'][0][0]*(1-fraction)+neighbor['core_samples'][1][0]*fraction
                        self.assertLessEqual(abs(p[2]-expected), 1e-8)
        cursor = south
        for lo, hi in sorted(segments):
            self.assertEqual(lo, cursor)
            cursor = hi
        self.assertTrue(segments)
        self.assertEqual(cursor, north)
        snapshot = inventory(child)
        reverse, cold = prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'mixed-cold',
            [operation, self.fixture['operation']])
        self.assertEqual(result, reverse)
        self.assertEqual(snapshot, inventory(cold))
        _replay_result(ROOT, self.fixture['base'], child/'refinement_result.json', self.root/'mixed-replay')
        self.assertEqual(snapshot, inventory(self.root/'mixed-replay'))
        authority, root = self.authority(child, 'mixed-authority')
        validate_selection_bundle(authority, root)
        self.assertEqual(before, inventory(base))

    def test_retained_source_identity_cold_noop_and_authority_replay(self):
        before = deepcopy(self.fixture['input'])
        result, child = self.prepare()
        snapshot = inventory(child)
        self.assertEqual(result, self.prepare()[0])
        self.assertEqual(snapshot, inventory(child))
        self.assertEqual(result, self.prepare('cold')[0])
        self.assertEqual(snapshot, inventory(self.root/'cold'))
        _replay_result(ROOT, self.fixture['base'], child/'refinement_result.json', self.root/'replay')
        self.assertEqual(snapshot, inventory(self.root/'replay'))
        request = read_json(child/'refinement/request.json')
        retained = read_json(child/request['operations'][0]['input']['path'])
        self.assertEqual(before, retained)
        self.assertEqual(self.fixture['water'], retained['original_feature'])

    def authority(self, child, name):
        root = self.root/name
        root.mkdir()
        base = self.fixture['base'].parent
        base_result = read_json(self.fixture['base'])
        promotion._write_deterministic_bundle(root/'base.zip',
            [(e['path'], base/e['path']) for e in base_result['outputs']], base_result)
        result = read_json(child/'refinement_result.json')
        request = read_json(child/'refinement/request.json')
        selected = {'kind': 'surface_relationships_v1', 'base_result_sha256': result['base_result_sha256'],
            'inputs_hash': result['inputs_hash'], 'result_sha256': file_hash(child/'refinement_result.json'),
            'output_count': len(result['outputs']),
            **{k: request[k] for k in ('implementation_sha256', 'source_context_identity', 'execution_identity')}}
        identity = canonical_hash({k: selected[k] for k in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
        relative = 'refinements/'+identity+'.zip'
        (root/'refinements').mkdir()
        promotion._write_deterministic_bundle(root/relative,
            [(e['path'], child/e['path']) for e in result['outputs']], result, result_name='refinement_result.json')
        selected['bundle'] = {'format': 'deterministic_zip_v1', 'path': relative,
            'sha256': file_hash(root/relative), 'byte_size': (root/relative).stat().st_size}
        authority = {'authority_id': 'closed-basin-fixture', 'profile_id': result['profile_id'],
            'compile_result_sha256': file_hash(self.fixture['base']),
            'bundle': {'path': 'base.zip', 'sha256': file_hash(root/'base.zip'), 'byte_size': (root/'base.zip').stat().st_size}}
        selected['effective_identity'] = _selection_identity(authority, selected)
        authority['selected_refinement'] = selected
        return authority, root

    def test_historical_authority_refuses_extra_or_changed_original_terrain(self):
        _, child = self.prepare()
        authority, root = self.authority(child, 'valid-authority')
        validate_selection_bundle(authority, root)
        for case in ('extra_payload', 'changed_original', 'missing_payload', 'missing_input'):
            bad = self.root/case
            shutil.copytree(child, bad)
            if case == 'extra_payload':
                source = next(bad.glob('canonical/refined_terrain/*.json'))
                shutil.copy2(source, source.parent/'undeclared.json')
            elif case == 'changed_original':
                source = next(bad.glob('canonical/terrain/*.json'))
                terrain = read_json(source)
                terrain['core_samples'][0][0] += 1
                write_json(source, terrain)
            elif case == 'missing_payload':
                next(bad.glob('canonical/refined_terrain/*.json')).unlink()
            else:
                next(bad.glob('refinement/inputs/*.json')).unlink()
            result = read_json(bad/'refinement_result.json')
            result['outputs'] = [output_descriptor('compiler_document', p, bad)
                for p in sorted(bad.rglob('*.json')) if p.name != 'refinement_result.json']
            write_json(bad/'refinement_result.json', result)
            authority, root = self.authority(bad, case+'-authority')
            with self.subTest(case=case), self.assertRaisesRegex(CompilerError, 'unrelated|absent|omits a declared retained input'):
                validate_selection_bundle(authority, root)

    def test_depth_dependency_changes_ground_with_unchanged_modeled_water_geometry(self):
        first, a = self.prepare()
        changed = deepcopy(self.fixture['input'])
        changed['model']['parameters']['depth_m'] = .3
        second, b = self.prepare('deep', self.input_operation(changed, 'deep-input'))
        self.assertNotEqual(first['inputs_hash'], second['inputs_hash'])
        def feature(root):
            return next(f for path in root.glob('canonical/features/*.json') for f in read_json(path)['features'])
        x, y = feature(a), feature(b)
        self.assertEqual(x['geometry'], y['geometry'])
        self.assertEqual(x['source_refs'], y['source_refs'])
        fx, fy = (f['attributes']['surface_function'] for f in (x, y))
        self.assertEqual(fx['level_m'], fy['level_m'])
        self.assertEqual(fx['spill_level_m'], fy['spill_level_m'])
        self.assertNotEqual(fx['terrain_support'], fy['terrain_support'])
        self.assertNotEqual(fx['model_input']['sha256'], fy['model_input']['sha256'])

    def test_unknown_contradictory_stale_inputs_leave_base_and_outputs_unchanged(self):
        before = inventory(self.fixture['base'].parent)
        cases = []
        for kind in ('source', 'frame', 'unknown_field', 'under_route', 'capacity'):
            value = deepcopy(self.fixture['input'])
            if kind == 'source': value['original_feature']['attributes']['surface_function']['level_m'] += 1
            elif kind == 'frame': value['model']['canonical_crs'] = 'EPSG:32639'
            elif kind == 'unknown_field': value['model']['observed_fountain'] = True
            elif kind == 'under_route': value['model']['support']['known_canonical_under_routes'] = ['unresolved_route']
            else: value['maximum_triangles'] = 32
            cases.append(self.input_operation(value, kind))
        cases.append({**self.fixture['operation'], 'input': {**self.fixture['operation']['input'], 'sha256': '0'*64}})
        for i, operation in enumerate(cases):
            with self.subTest(case=i), self.assertRaises(CompilerError):
                self.prepare('refused_'+str(i), operation)
            self.assertFalse((self.root/('refused_'+str(i))).exists())
            self.assertEqual(before, inventory(self.fixture['base'].parent))

    def test_fine_provider_trace_refuses_even_with_matching_original_corner_values(self):
        value = deepcopy(self.fixture['input'])
        fine = fine_fixture()
        value['grid'], value['evidence'] = fine['grid'], fine['evidence']
        value['grid'].update({'$schema': 'https://alis.world/schemas/world-source/ground-grid-v1.json', 'schema_version': 1})
        value['evidence']['grid_content_sha256'] = canonical_hash(value['grid'])
        value['evidence']['processing_sha256'] = canonical_hash({'test': 'retained_fine_reference', 'parent': fine['evidence']})
        before = inventory(self.fixture['base'].parent)
        with self.assertRaisesRegex(CompilerError, 'unselected boundary|neighbor'):
            self.prepare('open_frontier', self.input_operation(value, 'fine-input'))
        self.assertFalse((self.root/'open_frontier').exists())
        self.assertEqual(before, inventory(self.fixture['base'].parent))

    def test_late_basin_civil_refusal_does_not_publish_an_earlier_bridge(self):
        owner = next(key for key, cell in self.fixture['cells'].items() if cell['bounds'][3] == 4512942.45)
        road = {'feature_id': 'alis:osm:way:9001', 'feature_class': 'road',
            'geometry': {'type': 'LineString', 'coordinates': [[586188., 4512930.], [586208., 4512930.]]},
            'attributes': {'road_class': 'primary', 'width_m': 4.},
            'source_refs': [{'provider': 'OpenStreetMap', 'provider_feature_id': 'way/9001', 'release': '2026-08-31'}],
            'owner_cell_id': owner, 'intersecting_cell_ids': [owner], 'representations': [], 'provenance_result': 'accepted'}
        add_feature(self.fixture, road)
        context = {'$schema': 'https://alis.world/schemas/world-source/provider-feature-v1.json', 'schema_version': 1,
            'snapshot_id': 'bounded-basin-bridge-context', 'provider': 'OpenStreetMap', 'release': '2026-08-31',
            'area': {'area_id': 'fixture', 'area_fingerprint': 'sha256:'+'0'*64}, 'shard_id': '0000',
            'license': {'id': 'ODbL-1.0', 'attribution': 'OpenStreetMap contributors'}, 'features': []}
        points = [[-73.97, 40.76], [-73.9698, 40.76]]
        way = {'provider_feature_id': 'way/9001', 'provider_class': 'highway',
            'geometry': {'type': 'LineString', 'coordinates': points},
            'properties': {'@id': 9001, '@type': 'way', '@version': 1, '@timestamp': '2024-01-01T00:00:00Z',
                '@way_nodes': [10, 20], 'highway': 'primary', 'bridge': 'yes', 'layer': '1'},
            'precision': {'source_crs': 'EPSG:4326', 'coordinate_decimals': 7}, 'confidence': None}
        context['features'].append(way)
        for identifier, xy in zip((10, 20), points, strict=True):
            context['features'].append({'provider_feature_id': f'node/{identifier}', 'provider_class': 'provider_feature',
                'geometry': {'type': 'Point', 'coordinates': xy},
                'properties': {'@id': identifier, '@type': 'node', '@version': 1, '@timestamp': '2024-01-01T00:00:00Z'},
                'precision': {'source_crs': 'EPSG:4326', 'coordinate_decimals': 7}, 'confidence': None})
        path = self.root/'bridge-context.json'
        path.write_text(json.dumps(context)+'\n', encoding='ascii')
        bridge = {'kind': 'bridge_elevation', 'feature_ids': [road['feature_id']],
            'context_path': path.relative_to(ROOT).as_posix(), 'sha256': file_hash(path), 'byte_size': path.stat().st_size}
        before = inventory(self.fixture['base'].parent)
        with patch('World.CanonicalCompilation.refinement.app.surface_publication.project_features',
                return_value=({'way/9001': road['geometry']}, [])), patch.object(surface_publication,
                '_bridge', wraps=surface_publication._bridge) as earlier:
            with self.assertRaisesRegex(CompilerError, 'civil support') as failure:
                prepare_surface_refinement(ROOT, self.fixture['base'], self.root/'late-refused', [bridge, self.fixture['operation']])
            self.assertEqual(earlier.call_count, 1)
        self.assertEqual(failure.exception.details['feature_ids'], [road['feature_id']])
        self.assertFalse((self.root/'late-refused').exists())
        self.assertEqual(before, inventory(self.fixture['base'].parent))
