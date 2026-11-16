"""Effective-input admission and receipt binding, isolated from geographic construction."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.EndToEndValidation.app import immutable_canonical as operation
from World.EndToEndValidation.app import validation
from World.EndToEndValidation.app import refinement_selection, enrollment
from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash


class EffectiveCanonicalTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'tmp/world/effective_canonical_tests'
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.profile = {'profile_id': 'effective_fixture', 'thresholds': {}, 'profiles': {
            name: {'canonical_authority': True, 'compiler_profile': name, 'world_data_plugin': 'Fixture',
                   'source_profile_path': f'{name}.source.json'} for name in ('kazan', 'manhattan')}}
        self.path = self.root / 'validation.json'
        self.path.write_text(json.dumps(self.profile))
        self.child = {'effective_identity': 'a'*64, 'result_sha256': 'b'*64, 'bundle_sha256': 'c'*64}
        self.authorities = {}
        self.inputs = {}
        # The public authority validator is the mocked boundary. Its archive
        # authentication has independent CanonicalCompilation regression coverage.
        for name in self.profile['profiles']:
            contract = {'run_inputs_hash': '1'*64, 'implementation_sha256': '2'*64,
                        'source_inputs_hash': '3'*64, 'execution_environment': {'identity_sha256': '4'*64}}
            result = {'inputs_hash': '1'*64, 'operation': 'compile'}
            archive = self.root / f'{name}.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                bundle.writestr('run_contract.json', json.dumps(contract))
                bundle.writestr('compile_result.json', json.dumps(result))
            active = self.root / f'{name}.json'
            authority = {'authority_id': name, 'profile_id': name, 'world_data_plugin': 'Fixture',
                'external_inputs': {'source_profile_path': f'{name}.source.json'},
                'inputs_hash': '1'*64, 'compile_result_sha256': '5'*64,
                'bundle': {'path': archive.name, 'sha256': file_hash(archive)}}
            if name == 'kazan':
                child_archive = self.root / 'carried-child.zip'
                child_archive.write_bytes(b'frozen child archive boundary fixture')
                self.child['bundle_sha256'] = file_hash(child_archive)
                authority['selected_refinement'] = {'base_result_sha256': '5'*64,
                    'effective_identity': self.child['effective_identity'], 'result_sha256': self.child['result_sha256'],
                    'bundle': {'path': child_archive.name, 'sha256': self.child['bundle_sha256']}}
            active.write_text(json.dumps(authority))
            self.authorities[active] = authority
            self.inputs[name] = (self.path, active, active.name, file_hash(active), {'validation.json': file_hash(self.path)})
        mocks = (
            patch.object(operation, '_git', side_effect=lambda repo, args:
                ('0'*40).encode() if args[0] == 'rev-parse' else self.path.read_bytes()),
            patch.object(operation, '_selected_inputs', side_effect=lambda repo, settings, base:
                self.inputs[settings['compiler_profile']]),
            patch.object(operation, 'validate_canonical_authority', side_effect=lambda repo, active, compiler:
                deepcopy(self.authorities[active])),
        )
        for mock in mocks:
            mock.start()
            self.addCleanup(mock.stop)

    def snapshot(self):
        return operation.snapshot_selection(self.root, self.profile, '0'*40, self.path)

    def test_mixed_effective_selection_pins_child_without_current_producer_replay(self):
        before = {p.name: p.read_bytes() for p in self.authorities}
        selected = self.snapshot()
        self.assertEqual(self.child, selected['profiles']['kazan']['selected_refinement'])
        self.assertNotIn('selected_refinement', selected['profiles']['manhattan'])
        self.assertEqual('5'*64, selected['profiles']['kazan']['compile_result_sha256'])
        operation.validate_selection(self.root, self.profile, selected)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.authorities})

    def test_execution_cannot_omit_child_or_substitute_base_result(self):
        selected = self.snapshot()
        execution = {'matrix_mode': operation.MODE, 'immutable_canonical_selection': selected, 'profiles': {}}
        for name, record in selected['profiles'].items():
            execution['profiles'][name] = {'canonical_authority': deepcopy(record),
                'realization_compile_result_sha256': record.get('selected_refinement', {}).get(
                    'result_sha256', record['compile_result_sha256'])}
        operation.validate_execution(self.root, self.profile, execution)
        for mutation in ('omit', 'base', 'swap'):
            bad = deepcopy(execution)
            record = bad['profiles']['kazan']
            if mutation == 'omit': record['canonical_authority'].pop('selected_refinement')
            elif mutation == 'base': record['realization_compile_result_sha256'] = '5'*64
            else: record['canonical_authority']['selected_refinement']['effective_identity'] = 'd'*64
            with self.subTest(mutation=mutation), self.assertRaises(ValidationFailure):
                operation.validate_execution(self.root, self.profile, bad)

    def test_changed_frozen_child_cannot_reuse_selection(self):
        selected = self.snapshot()
        active = self.inputs['kazan'][1]
        self.authorities[active]['selected_refinement']['bundle']['sha256'] = 'd'*64
        with self.assertRaises(ValidationFailure):
            operation.validate_selection(self.root, self.profile, selected)

    def test_carried_child_archive_race_refuses_and_selected_target_cannot_be_replaced(self):
        selected = self.snapshot()
        refinement_selection._check_base_pins(self.root, selected)
        archive = self.root / 'carried-child.zip'
        archive.write_bytes(archive.read_bytes() + b'corrupt')
        with self.assertRaises(ValidationFailure):
            refinement_selection._check_base_pins(self.root, selected)
        with self.assertRaises(ValidationFailure):
            refinement_selection._candidate(self.root, {}, selected['profiles']['kazan'], self.path)

    def test_enrollment_compares_the_same_effective_authority_without_mutation(self):
        active = self.inputs['kazan'][1]
        authority = deepcopy(self.authorities[active])
        result = self.root / 'effective.json'
        result.write_bytes(b'authenticated effective result boundary')
        authority['selected_refinement']['result_sha256'] = file_hash(result)
        expected = {'active_path': active.relative_to(ROOT).as_posix(), 'active_sha256': file_hash(active),
            'authority_id': authority['authority_id'], 'inputs_hash': authority['inputs_hash'],
            'bundle_sha256': authority['bundle']['sha256'], 'compile_result_sha256': authority['compile_result_sha256'],
            **operation.selected_refinement_record(authority)}
        record = {'canonical_authority': expected, 'realization_compile_result_sha256': file_hash(result)}
        enrollment._validate_matrix_authority(record, active, authority, result)
        bad = deepcopy(record)
        bad['canonical_authority'].pop('selected_refinement')
        with self.assertRaises(ValidationFailure):
            enrollment._validate_matrix_authority(bad, active, authority, result)
        result.write_bytes(b'wrong materialized base')
        with self.assertRaises(ValidationFailure):
            enrollment._validate_matrix_authority(record, active, authority, result)

    def test_consumer_identity_includes_refinement_reader(self):
        original = operation.file_hash
        for owner, name in (('refinement', 'authority.py'), ('supplemental', 'ground-grid.schema.json'),
                            ('supplemental', 'derived-ground.schema.json')):
            with self.subTest(owner=owner, name=name), patch.object(operation, 'file_hash', side_effect=lambda path:
                    '0'*64 if path.name == name and owner in path.parts else original(path)):
                altered = operation.consumer_identity()
            self.assertNotEqual(operation.consumer_identity(), altered)

    def test_refinement_inventory_uses_effective_outputs_and_rejects_wrong_operation(self):
        path = self.root / 'refinement_result.json'
        document = {'$schema': 'https://alis.world/schemas/world-compiler/refinement-result-v1.json',
            'schema_version': 1, 'operation': 'refine', 'status': 'accepted',
            'outputs': [{'path': 'canonical/terrain/a.json', 'byte_size': 123}]}
        path.write_text(json.dumps(document))
        self.assertEqual(123, validation._canonical_bytes_from_result(path))
        for field, value in (('operation', 'compile'), ('status', 'refused'), ('$schema', 'unknown')):
            path.write_text(json.dumps({**document, field: value}))
            with self.subTest(field=field), self.assertRaises(ValidationFailure):
                validation._canonical_bytes_from_result(path)


if __name__ == '__main__':
    unittest.main()
