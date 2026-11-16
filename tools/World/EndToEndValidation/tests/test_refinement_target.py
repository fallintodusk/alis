"""Selected target admission and evidence reject stale gates and false saved/no-op proofs."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.EndToEndValidation.app import refinement_target as target
from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash


class RefinementTargetTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'tmp/world/end_to_end_validation/target-tests'
        parent.mkdir(parents=True, exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.realization = self.write('realization.json', {'profile_id': 'fixture',
            'layers': [{'layer_id': key} for key in ('roads', 'water', 'vegetation')]})
        self.profile = self.write('validation.json', {'profile_id': 'fixture', 'profiles': {'city': {
            'map_package': '/fixture', 'world_data_plugin': 'ProjectWorldTestData',
            'realization_profile': self.relative(self.realization)}}})
        self.selection = {'base_selection': {'validation_profile_path': self.relative(self.profile),
            'validation_profile_sha256': file_hash(self.profile),
            'profiles': {'city': {'active_sha256': 'a' * 64, 'compile_result_sha256': '1' * 64}}},
            'candidates': {'city': {'active_sha256': 'b' * 64, 'result_sha256': 'c' * 64}}}
        self.selected = self.write('selection.json', self.selection)
        self.check = self.write('check.json', {'status': 'accepted'})
        self.request = {'schema_version': 1, 'validation_profile_path': self.relative(self.profile),
            'selection_path': self.relative(self.selected), 'selection_sha256': file_hash(self.selected),
            'check_result_path': self.relative(self.check), 'check_result_sha256': file_hash(self.check),
            'base_run': 'run-20261017T010000Z', 'base_result_sha256': 'd' * 64, 'execute_layers': ['roads', 'water', 'vegetation']}
        self.request_path = self.write('request.json', self.request)
        self.baseline = {'result_sha256': 'd' * 64, 'document': {'evidence': {'matrix_mode': 'immutable_canonical',
            'immutable_canonical_selection': self.selection['base_selection'],
            'common_checks': {'receipt_sha256': file_hash(self.check)}}}}

    def relative(self, path):
        return path.relative_to(ROOT).as_posix()

    def write(self, name, document):
        path = self.root / name
        path.write_text(json.dumps(document), encoding='utf-8')
        return path

    def verify(self):
        with patch.object(target, 'profile_path', return_value=self.profile), \
             patch.object(target, 'load_profile', return_value=json.loads(self.profile.read_text())), \
             patch.object(target, 'validate_candidates') as candidates, \
             patch.object(target, 'load_common_checks', return_value={'receipt_sha256': file_hash(self.check)}), \
             patch.object(target, 'accepted_run_evidence', return_value=self.baseline):
            result = target.verify_request(self.request_path)
            candidates.assert_called_once()
            return result

    def test_exact_child_current_check_and_base_matrix_are_required(self):
        verified = self.verify()
        self.assertEqual('city', verified['candidate_name'])
        self.assertEqual(file_hash(self.request_path), verified['request_sha256'])

    def test_stale_input_gate_and_wrong_matrix_each_refuse(self):
        for field in ('selection_sha256', 'check_result_sha256', 'base_result_sha256'):
            with self.subTest(field=field):
                changed = {**self.request, field: '0' * 64}
                self.write('request.json', changed)
                with self.assertRaises(ValidationFailure):
                    self.verify()
        self.write('request.json', self.request)
        self.baseline['document']['evidence']['matrix_mode'] = 'canonical_authority'
        with self.assertRaises(ValidationFailure):
            self.verify()

    def test_selection_and_base_envelope_must_match_exactly(self):
        self.baseline['document']['evidence']['immutable_canonical_selection'] = {'other': 'input'}
        with self.assertRaises(ValidationFailure):
            self.verify()

    def transport(self):
        native = {'status': 'accepted', 'input_sha256': 'c' * 64, 'execution_layer_ids': self.request['execute_layers'],
            'layer_inventories': [{'final_dirty_units': []}], 'changes': {'updated_actors': 0, 'cross_cell_road_feature_id': ''}}
        ticket = {'old_active_sha256': 'a' * 64, 'new_active_sha256': 'b' * 64, 'effective_result_sha256': 'c' * 64}
        receipts = {}
        for name in ('failure_native', 'saved_native', 'noop_native'):
            path = self.write(name + '.json', native)
            receipts[name] = {'path': self.relative(path), 'sha256': file_hash(path)}
        docs = {'failure_staging': {'staged': 'World'},
            'failure_journal': {'phase': 'publishing', 'prior_active_set_sha256': '2' * 64,
                'canonical_selection_transition': ticket},
            'base_native_city': {'status': 'accepted', 'input_sha256': '1' * 64,
                'layer_inventories': [{'layer_id': key, 'scope_id': 'layer_fixture_' + key}
                    for key in self.request['execute_layers']],
                'realization_profile_sha256': file_hash(self.realization)},
            'base_audit': {'status': 'accepted', 'scopes': [{'owning_layer': key,
                'scope_id': 'map_fixture' if key == 'map' else 'layer_fixture_' + key,
                'input_identity': {'map_package': '/fixture', 'compile_result_sha256': '1' * 64}}
                for key in ('map', 'roads', 'water', 'vegetation')]},
            'base_active': {'prepared': 'base'},
            'saved_audit': {'status': 'accepted'},
            'saved_active': {'canonical_input_compatibility': [{'candidate': {'result_sha256': 'c' * 64},
                'input_selection_sha256': self.request['selection_sha256']}]}}
        staging = self.write('failure_staging.json', docs['failure_staging'])
        docs['failure_journal']['expected_active_set_sha256'] = file_hash(staging)
        for name, doc in docs.items():
            path = self.write(name + '.json', doc)
            receipts[name] = {'path': self.relative(path), 'sha256': file_hash(path)}
        receipts['base_validation_profile'] = {'path': self.relative(self.profile), 'sha256': file_hash(self.profile)}
        receipts['base_profile_city'] = {'path': self.relative(self.realization), 'sha256': file_hash(self.realization)}
        prepared = receipts['base_active']['sha256']
        journal_path = ROOT / receipts['failure_journal']['path']
        journal = json.loads(journal_path.read_text())
        journal['prior_active_set_sha256'] = prepared
        journal_path.write_text(json.dumps(journal))
        receipts['failure_journal']['sha256'] = file_hash(journal_path)
        audit_path = ROOT / receipts['base_audit']['path']
        audit = json.loads(audit_path.read_text()); audit['active_set'] = {'sha256': prepared}
        audit_path.write_text(json.dumps(audit))
        receipts['base_audit']['sha256'] = file_hash(audit_path)
        saved = {'status': 'accepted', 'world_sha256': receipts['saved_active']['sha256'], 'canonical_sha256': 'b' * 64}
        report = {'status': 'accepted', 'request_sha256': file_hash(self.request_path), 'transition': ticket,
            'base_preparation': {'status': 'accepted', 'world_sha256': prepared},
            'failure_recovery': {'status': 'accepted', 'host_exit': 1, 'phase': 'publishing', 'canonical_state': 'selected', 'world_sha256': prepared},
            'saved_publication': saved, 'cold_noop': deepcopy(saved),
            'restoration': {'status': 'accepted', 'before_inventory_sha256': 'f' * 64, 'after_inventory_sha256': 'f' * 64,
                'before_world_sha256': 'e' * 64, 'after_world_sha256': 'e' * 64, 'canonical_state': 'prior'}, 'receipts': receipts}
        return report

    def test_prepared_base_is_distinct_from_the_original_restoration(self):
        report = self.transport()
        self.assertNotEqual(report['restoration']['before_world_sha256'], report['base_preparation']['world_sha256'])
        self.assertEqual(report, target.validate_transport(self.request_path, self.write('transport.json', report)))

    def test_full_base_without_scoped_execution_list_uses_exact_native_inventories(self):
        report = self.transport()
        native = json.loads((ROOT / report['receipts']['base_native_city']['path']).read_text())
        self.assertNotIn('execution_layer_ids', native)
        self.assertEqual(report, target.validate_transport(self.request_path, self.write('transport.json', report)))

    def test_full_base_missing_duplicate_or_foreign_inventory_and_audit_scopes_refuse(self):
        for name, change in (
            ('base_native_city', lambda rows: rows.pop()),
            ('base_native_city', lambda rows: rows.append(deepcopy(rows[0]))),
            ('base_native_city', lambda rows: rows[0].update(scope_id='foreign')),
            ('base_native_city', lambda rows: rows[0].update(layer_id='foreign')),
            ('base_audit', lambda rows: rows[1].update(scope_id='foreign')),
            ('base_audit', lambda rows: rows.__setitem__(1, deepcopy(rows[2])))):
            with self.subTest(name=name, change=change):
                report = self.transport()
                path = ROOT / report['receipts'][name]['path']
                document = json.loads(path.read_text())
                change(document['scopes' if name == 'base_audit' else 'layer_inventories'])
                path.write_text(json.dumps(document))
                report['receipts'][name]['sha256'] = file_hash(path)
                with self.assertRaises(ValidationFailure):
                    target.validate_transport(self.request_path, self.write('transport.json', report))

    def test_missing_or_wrong_prepared_base_proof_refuses(self):
        for key, value in (('input_sha256', '0' * 64), ('execution_layer_ids', ['water']),
                           ('execution_layer_ids', {key: 0 for key in self.request['execute_layers']}),
                           ('realization_profile_sha256', '0' * 64)):
            report = self.transport()
            path = ROOT / report['receipts']['base_native_city']['path']
            native = json.loads(path.read_text()); native[key] = value
            path.write_text(json.dumps(native))
            report['receipts']['base_native_city']['sha256'] = file_hash(path)
            with self.assertRaises(ValidationFailure):
                target.validate_transport(self.request_path, self.write('transport.json', report))
        report = self.transport()
        path = ROOT / report['receipts']['base_audit']['path']
        audit = json.loads(path.read_text())
        audit['scopes'][0]['input_identity']['compile_result_sha256'] = '0' * 64
        path.write_text(json.dumps(audit))
        report['receipts']['base_audit']['sha256'] = file_hash(path)
        with self.assertRaises(ValidationFailure):
            target.validate_transport(self.request_path, self.write('transport.json', report))

    def test_complete_saved_cold_and_recovery_proof_is_accepted(self):
        report = self.transport()
        self.assertEqual(report, target.validate_transport(self.request_path, self.write('transport.json', report)))

    def test_requested_runtime_consumers_cannot_pass_with_publication_only(self):
        self.request['consumer_verification'] = {'vantage_plan_path': self.relative(self.write('views.json', {})),
            'vantage_plan_sha256': 'a' * 64}
        self.write('request.json', self.request)
        report = self.transport()
        with self.assertRaises(ValidationFailure):
            target.validate_transport(self.request_path, self.write('transport.json', report))

    def test_no_status_only_or_wrong_phase_or_changed_pointer_proof(self):
        for field, change in (
            ('failure_recovery', {'phase': 'mutating'}), ('failure_recovery', {'host_exit': 0}),
            ('cold_noop', {'world_sha256': '0' * 64}), ('restoration', {'after_inventory_sha256': '0' * 64}),
            ('saved_publication', {'canonical_sha256': '0' * 64})):
            with self.subTest(field=field, change=change):
                report = self.transport()
                report[field].update(change)
                with self.assertRaises(ValidationFailure):
                    target.validate_transport(self.request_path, self.write('transport.json', report))

    def test_tampered_and_wrong_child_native_and_dirty_noop_refuse(self):
        for change in ({'input_sha256': '0' * 64}, {'changes': {'updated_actors': 1}}, {'changes': {}}, {'layer_inventories': []},
                       {'layer_inventories': [{'final_dirty_units': ['cell']}]}, {'status': 'rejected'}):
            report = self.transport()
            path = ROOT / report['receipts']['noop_native']['path']
            native = json.loads(path.read_text())
            native.update(change)
            path.write_text(json.dumps(native))
            report['receipts']['noop_native']['sha256'] = file_hash(path)
            with self.assertRaises(ValidationFailure):
                target.validate_transport(self.request_path, self.write('transport.json', report))
        report = self.transport()
        (ROOT / report['receipts']['saved_native']['path']).write_text('{}')
        with self.assertRaises(ValidationFailure):
            target.validate_transport(self.request_path, self.write('transport.json', report))


if __name__ == '__main__':
    unittest.main()
