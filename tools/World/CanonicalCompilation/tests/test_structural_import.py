"""Creation-only structural import into the existing immutable-base child publisher."""
from pathlib import Path
from copy import deepcopy
import json
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from ..app.contracts import CompilerError, canonical_hash, file_hash, read_json, validate_document as validate_base
from ..app.artifacts import output_descriptor
from ..app import promotion
from ..refinement.app.contracts import validate_document, write_json
from ..refinement.app import activation, authority, identity
from ..refinement import constructed_import
from ..refinement.api import prepare_surface_refinement
from .structural_import_fixture import FEATURE_ID, ROOT, make_fixture
from .test_road_refinement import inventory


class StructuralImportTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT/'tmp/world/structural_import_unit'; scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=scratch); self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name); self.values = make_fixture(self.root)

    def prepare(self, name='candidate', operation=None):
        return prepare_surface_refinement(ROOT, self.values['base'], self.root/name, [operation or self.values['operation']])

    def change_input(self, name, mutate):
        operation = self.values['operation']; descriptor = operation['inputs'][name]
        path = ROOT/descriptor['path']; value = read_json(path); mutate(value)
        path.write_text(json.dumps(value, allow_nan=False)+'\n', encoding='ascii')
        descriptor.update(sha256=file_hash(path), byte_size=path.stat().st_size)

    def bundle(self, result, output):
        base_zip = self.root/'base.zip'
        if not base_zip.exists():
            base = read_json(self.values['base'])
            promotion._write_deterministic_bundle(base_zip, [(e['path'], self.values['base'].parent/e['path']) for e in base['outputs']], base)
        result_path = output/'refinement_result.json'; request = read_json(output/'refinement/request.json')
        selected = {'base_result_sha256': file_hash(self.values['base']), 'inputs_hash': result['inputs_hash'],
            'result_sha256': file_hash(result_path), 'output_count': len(result['outputs']),
            **{k: request[k] for k in ('implementation_sha256', 'source_context_identity', 'execution_identity')}}
        address = canonical_hash({k: selected[k] for k in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
        relative = f'refinements/{address}.zip'; target = self.root/relative; target.parent.mkdir(exist_ok=True)
        promotion._write_deterministic_bundle(target, [(e['path'], output/e['path']) for e in result['outputs']], result, result_name='refinement_result.json')
        selected['bundle'] = {'path': relative, 'sha256': file_hash(target), 'byte_size': target.stat().st_size}
        document = {'authority_id': 'structural_fixture', 'profile_id': result['profile_id'],
            'compile_result_sha256': file_hash(self.values['base']),
            'bundle': {'path': 'base.zip', 'sha256': file_hash(base_zip)}, 'selected_refinement': selected}
        selected['effective_identity'] = authority._selection_identity(document, selected)
        return document

    def test_complete_new_source_record_is_created_without_changing_terrain_or_base(self):
        before = inventory(self.values['base'].parent)
        result, output = prepare_surface_refinement(ROOT, self.values['base'], self.root/'candidate', [self.values['operation']])
        self.assertEqual(before, inventory(self.values['base'].parent))
        self.assertEqual('refine', result['operation'])
        coverage = read_json(output/'canonical/coverage.json')
        self.assertEqual(1, coverage['feature_count'])
        features = []
        for entry in coverage['cells']:
            manifest = read_json(output/entry['path'])
            features.extend(read_json(output/manifest['feature_artifact']['path'])['features'])
        self.assertEqual([FEATURE_ID], [f['feature_id'] for f in features])
        self.assertEqual('constructed_surface', features[0]['feature_class'])
        self.assertEqual(self.values['footprint'], features[0]['geometry'])
        self.assertEqual(self.values['faces'], features[0]['attributes']['constructed_surface']['triangles'])
        self.assertEqual(2, len(features[0]['intersecting_cell_ids']))
        owner = features[0]['owner_cell_id']
        for entry in coverage['cells']:
            manifest = read_json(output/entry['path']); original = read_json(self.values['base'].parent/entry['path'])
            if entry['cell_id'] == owner:
                self.assertIn(FEATURE_ID, manifest['owned_feature_ids'])
            else:
                self.assertIn(FEATURE_ID, manifest['referenced_feature_ids'])
                self.assertEqual(original['feature_artifact'], manifest['feature_artifact'])
        for path, digest in before.items():
            if path.startswith('canonical/terrain/'):
                self.assertEqual(digest, inventory(output)[path])

    def test_provenance_only_change_moves_acceptance_but_preserves_semantic_model(self):
        first, a = self.prepare()
        self.change_input('model', lambda value: value['provenance'].update(note='Receipt-only attribution update'))
        second, b = self.prepare('attribution')
        def surface(root):
            return next(f['attributes']['constructed_surface'] for path in root.glob('canonical/features/*.json')
                for f in read_json(path)['features'] if f['feature_id'] == FEATURE_ID)
        x, y = surface(a), surface(b)
        self.assertNotEqual(first['inputs_hash'], second['inputs_hash'])
        self.assertNotEqual(x['model_provenance'], y['model_provenance'])
        for key in ('model_sha256', 'triangles', 'face_roles', 'source_bindings', 'policy_sha256'):
            with self.subTest(consumed_field=key):
                self.assertEqual(x[key], y[key])

    def test_shared_model_schema_resolves_locally_without_network(self):
        with patch('urllib.request.urlopen', side_effect=AssertionError('Schema resolution attempted network')) as network:
            validate_document(self.values['model'], self.root/'model.json')
            validate_document(self.values['policy'], self.root/'policy.json')
        network.assert_not_called()

    def test_retained_native_archive_and_fresh_replay_are_exact_without_source_cache(self):
        result, output = self.prepare()
        document = self.bundle(result, output)
        before = inventory(output)
        authority.validate_selection_bundle(document, self.root)
        fresh, replay, _ = authority._replay_result(ROOT, self.values['base'], output/'refinement_result.json', self.root/'replay')
        self.assertEqual(result, fresh); self.assertEqual(before, inventory(replay))
        self.assertEqual(before, inventory(output))
        activation._current_producer(ROOT, document['selected_refinement'], self.root)

    def test_creation_collision_missing_source_unknown_role_and_bad_field_refuse_unchanged(self):
        before = inventory(self.values['base'].parent)
        for kind in ('unknown_role', 'missing_binding', 'role_source', 'coverage', 'height'):
            values = deepcopy(self.values['model'])
            def mutate(model):
                item = model['models'][0]
                if kind == 'unknown_role': item['face_roles'][0]['role'] = 'ground'
                elif kind == 'missing_binding': item['source_bindings'][0]['provider_feature_id'] = 'relation/999999'
                elif kind == 'role_source': item['role'] = 'floating_assembly'
                elif kind == 'coverage': item['triangles'].pop()
                else: item['triangles'][0][0][2] += 1.
            path = ROOT/self.values['operation']['inputs']['model']['path']
            path.write_text(json.dumps(values), encoding='ascii')
            self.change_input('model', mutate)
            with self.subTest(kind=kind), self.assertRaises(CompilerError): self.prepare(kind)
            self.assertFalse((self.root/kind).exists())
        with self.assertRaisesRegex(CompilerError, 'globally absent'):
            constructed_import.admit(ROOT, self.values['operation'], {FEATURE_ID: {}})
        self.assertEqual(before, inventory(self.values['base'].parent))

    def test_late_invalid_second_model_does_not_publish_first(self):
        def mutate(model):
            second = deepcopy(model['models'][0]); second['primary_source_id'] = 'way/1236753478'
            second['source_bindings'][0]['provider_feature_id'] = second['primary_source_id']
            model['models'].append(second)
        self.change_input('model', mutate)
        self.values['operation']['feature_ids'].append('alis:osm:way:1236753478')
        before = inventory(self.values['base'].parent)
        with patch.object(constructed_import, '_prove_field', wraps=constructed_import._prove_field) as proof:
            with self.assertRaisesRegex(CompilerError, 'pier context'): self.prepare()
        self.assertEqual(1, proof.call_count)
        self.assertFalse((self.root/'candidate').exists())
        self.assertEqual(before, inventory(self.values['base'].parent))

    def test_noop_and_selected_constructor_drift_are_distinct_from_frozen_reading(self):
        result, output = self.prepare(); document = self.bundle(result, output)
        before = inventory(output)
        self.assertEqual(result, self.prepare()[0]); self.assertEqual(before, inventory(output))
        with patch.object(constructed_import, 'implementation_identity', return_value='f'*64):
            authority.validate_selection_bundle(document, self.root)
            with self.assertRaises(CompilerError):
                authority._replay_result(ROOT, self.values['base'], output/'refinement_result.json', self.root/'stale')
            with self.assertRaises(CompilerError):
                activation._current_producer(ROOT, document['selected_refinement'], self.root)
        self.assertEqual(before, inventory(output)); self.assertFalse((self.root/'stale').exists())

    def test_unlisted_schema_bearing_input_cannot_enter_frozen_selection(self):
        result, output = self.prepare()
        path = output/'refinement/unlisted.json'; path.write_text(json.dumps(self.values['source']), encoding='ascii')
        result['outputs'].append(output_descriptor('compiler_document', path, output))
        result_path = output/'refinement_result.json'; write_json(result_path, result)
        document = self.bundle(result, output)
        with self.assertRaisesRegex(CompilerError, 'unrelated canonical artifacts'):
            authority.validate_selection_bundle(document, self.root)

    def test_nonfinite_provenance_cannot_publish_nonstandard_json(self):
        before = inventory(self.values['base'].parent)
        descriptor = self.values['operation']['inputs']['model']; path = ROOT/descriptor['path']
        for index, token in enumerate(('NaN', 'Infinity', '-Infinity', '1e400')):
            value = deepcopy(self.values['model']); value['provenance']['unqualified_number'] = 'number_marker'
            payload = json.dumps(value); self.assertEqual(1, payload.count('"number_marker"'))
            path.write_text(payload.replace('"number_marker"', token), encoding='ascii')
            descriptor.update(sha256=file_hash(path), byte_size=path.stat().st_size)
            with self.subTest(token=token), self.assertRaises(CompilerError): self.prepare(f'nonfinite-{index}')
            self.assertFalse((self.root/f'nonfinite-{index}').exists())
        self.assertEqual(before, inventory(self.values['base'].parent))

    def test_isolated_ordinary_and_gangway_grade_cross_branches_refuse(self):
        footprint = {'type': 'Polygon', 'coordinates': [[[0., 0.], [1., 0.], [0., 1.], [0., 0.]]]}
        projected = {'way/1': {'type': 'LineString', 'coordinates': [[0., 0.], [1., 0.]]}}
        cases = [('ordinary', .02, 0., True), ('ordinary', .02001, 0., False),
            ('gangway', .125, .02, True), ('gangway', .12501, 0., False), ('gangway', 0., .02001, False)]
        for kind, gx, gy, valid in cases:
            role = {'role': 'gangway' if kind == 'gangway' else 'fixed_pier', 'source_ids': ['way/1'], 'triangles': [0]}
            if kind == 'gangway': role['axis_source_id'] = 'way/1'
            item = {'footprint': footprint, 'triangles': [[[0., 0., 0.], [1., 0., gx], [0., 1., gy]]],
                'source_bindings': [{'provider_feature_id': 'way/1'}], 'face_roles': [role]}
            with self.subTest(kind=kind, gx=gx, gy=gy):
                if valid: constructed_import._prove_field(ROOT, item, self.values['policy'], projected)
                else:
                    with self.assertRaisesRegex(CompilerError, 'role quality'):
                        constructed_import._prove_field(ROOT, item, self.values['policy'], projected)

    def test_unknown_schema_reference_refuses_offline(self):
        schema = self.root/'unknown-reference.schema.json'
        schema.write_text(json.dumps({'$schema': 'https://json-schema.org/draft/2020-12/schema',
            '$id': 'urn:fixture:unknown-reference', '$ref': 'https://unknown.invalid/missing.json'}), encoding='ascii')
        with patch('urllib.request.urlopen', side_effect=AssertionError('Unknown schema attempted network')) as network:
            with self.assertRaises(CompilerError):
                validate_base({'$schema': schema.name}, self.root/'input.json')
        network.assert_not_called()


if __name__ == '__main__':
    unittest.main()
