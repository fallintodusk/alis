"""Candidate admission must not inherit the base producer's acceptance claim."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.tests import test_refinement_authority as fixture
from World.EndToEndValidation.app import refinement_selection as operation
from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash


class RefinementSelectionTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixture.RefinementAuthorityTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.authority, self.candidate = self.fixture.prepare()
        self.profile = {'profiles': {'fixture': {'compiler_profile_path': self.fixture.profile_path.relative_to(ROOT).as_posix()}}}
        self.baseline = {
            '$schema': 'https://alis.world/schemas/world-validation/immutable-canonical-selection-v1.json',
            'schema_version': 1, 'mode': 'immutable_canonical', 'changed_base': '0'*40,
            'validation_profile_path': self.fixture.profile_path.relative_to(ROOT).as_posix(),
            'validation_profile_sha256': file_hash(self.fixture.profile_path), 'base_validation_profile_sha256': '0'*64,
            'current_consumer_implementation_sha256': operation.consumer_identity(),
            'executed_upstream_operations': {'source_acquisition': False, 'base_compilation': False, 'canonical_promotion': False},
            'profiles': {'fixture': {
                'authority_id': self.authority['authority_id'], 'active_path': self.fixture.active.relative_to(ROOT).as_posix(),
                'active_sha256': file_hash(self.fixture.active), 'bundle_sha256': self.authority['bundle']['sha256'],
                'compile_result_sha256': self.authority['compile_result_sha256'], 'run_contract_sha256': '0'*64,
                'historical_producer_implementation_sha256': '1'*64, 'historical_source_inputs_hash': '2'*64,
                'historical_execution_identity': '3'*64, 'inputs_hash': self.authority['inputs_hash'],
                'geographic_input_files': {self.fixture.profile_path.relative_to(ROOT).as_posix(): file_hash(self.fixture.profile_path)},
            }}}
        baseline = patch.object(operation, 'snapshot_selection', return_value=self.baseline)
        baseline.start()
        self.addCleanup(baseline.stop)

    def snapshot(self, candidates=None):
        return operation.snapshot_candidates(ROOT, self.profile, '0'*40, self.fixture.profile_path,
            {'fixture': self.candidate} if candidates is None else candidates)

    def test_real_child_archive_and_effective_result_have_separate_pinned_identity(self):
        selected = self.snapshot()
        record = selected['candidates']['fixture']
        self.assertEqual('refinement_candidate', selected['mode'])
        self.assertEqual(self.authority['selected_refinement']['result_sha256'], record['result_sha256'])
        self.assertNotEqual(record['result_sha256'], record['base_result_sha256'])
        self.assertEqual(self.baseline, selected['base_selection'])
        operation.validate_candidates(ROOT, self.profile, selected)

    def test_base_drift_refuses_before_candidate_authentication(self):
        with patch.object(operation, 'snapshot_selection', side_effect=ValidationFailure('base_changed', 'fixture')), \
                patch.object(operation, 'validate_canonical_authority', side_effect=AssertionError('Must refuse upstream first')):
            with self.assertRaises(ValidationFailure):
                self.snapshot()

    def test_wrong_scope_empty_and_base_only_candidates_refuse(self):
        for candidates in ({}, {'unknown': self.candidate}, {'fixture': self.fixture.active}):
            with self.subTest(candidates=candidates), self.assertRaises(ValidationFailure):
                self.snapshot(candidates)

    def test_stale_child_source_native_producers_refuse_without_activation(self):
        before = self.fixture.active.read_bytes()
        with patch.object(operation, 'implementation_identity', return_value='0'*64):
            with self.assertRaises(ValidationFailure): self.snapshot()
        with patch.object(operation, 'osm_context_identity', return_value='0'*64):
            with self.assertRaises(ValidationFailure): self.snapshot()
        with patch.object(operation, 'execution_identity', return_value={'identity_sha256':'0'*64}):
            with self.assertRaises(ValidationFailure): self.snapshot()
        self.assertEqual(before, self.fixture.active.read_bytes())

    def test_changed_candidate_or_selected_record_cannot_reuse_admission(self):
        selected = self.snapshot()
        tampered = deepcopy(selected)
        tampered['candidates']['fixture']['result_sha256'] = '0'*64
        with self.assertRaises(ValidationFailure):
            operation.validate_candidates(ROOT, self.profile, tampered)
        self.candidate.write_bytes(self.candidate.read_bytes() + b' ')
        with self.assertRaises(ValidationFailure):
            operation.validate_candidates(ROOT, self.profile, selected)

    def test_base_pin_race_during_materialization_refuses(self):
        materialize = operation.materialize_canonical
        original = self.fixture.active.read_bytes()

        def changed_base(*arguments):
            result = materialize(*arguments)
            self.fixture.active.write_bytes(original + b' ')
            return result

        try:
            with patch.object(operation, 'materialize_canonical', side_effect=changed_base), self.assertRaises(ValidationFailure):
                self.snapshot()
        finally:
            self.fixture.active.write_bytes(original)

    def test_candidate_changed_after_its_own_materialization_refuses(self):
        check_base = operation._check_base_pins

        def changed_candidate(*arguments):
            check_base(*arguments)
            self.candidate.write_bytes(self.candidate.read_bytes() + b' ')

        with patch.object(operation, '_check_base_pins', side_effect=changed_candidate), self.assertRaises(ValidationFailure):
            self.snapshot()

    def test_producer_contract_changed_during_materialization_refuses(self):
        cases = (
            ('implementation_identity', operation.implementation_identity(), '0'*64),
            ('osm_context_identity', operation.osm_context_identity(), '0'*64),
            ('execution_identity', operation.execution_identity(ROOT, True), {'identity_sha256':'0'*64}),
        )
        before = self.fixture.active.read_bytes()
        for name, original, changed in cases:
            with self.subTest(contract=name), patch.object(operation, name, side_effect=[original, changed]), \
                    self.assertRaises(ValidationFailure):
                self.snapshot()
        self.assertEqual(before, self.fixture.active.read_bytes())


if __name__ == '__main__':
    unittest.main()
