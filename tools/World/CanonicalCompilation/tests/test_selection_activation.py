"""Canonical selection publication stays CAS-protected and independently recoverable."""
from contextlib import redirect_stderr, redirect_stdout
import base64
import io
import json
from pathlib import Path
import sys
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.tests import test_refinement_authority as fixtures
from World.CanonicalCompilation.refinement.app import activation as operation
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash
from World.CanonicalCompilation.app import cli


class SelectionActivationTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.RefinementAuthorityTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.authority, self.candidate = self.fixture.prepare()
        self.old_bytes = self.fixture.active.read_bytes()
        self.old_sha = file_hash(self.fixture.active)
        derive = patch.object(operation.promotion, 'canonical_active_path', return_value=self.fixture.active)
        derive.start()
        self.addCleanup(derive.stop)

    def prepare(self):
        return operation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate, self.old_sha)

    def test_prepare_publish_repeat_and_restore_preserve_exact_pointer_and_base_bytes(self):
        base_zip = self.fixture.active.parent / self.authority['bundle']['path']
        before = file_hash(base_zip)
        transition = self.prepare()
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())
        self.assertEqual('prior', operation.validate_selection_transition(ROOT, transition)['state'])
        self.assertTrue((self.fixture.active.parent / transition['bundle']['path']).is_file())
        self.assertTrue(operation.publish_selection_transition(ROOT, transition)['changed'])
        self.assertEqual(self.candidate.read_bytes(), self.fixture.active.read_bytes())
        self.assertFalse(operation.publish_selection_transition(ROOT, transition)['changed'])
        self.assertEqual('refinement_result.json', self.fixture.materialize().name)
        self.assertTrue(operation.publish_selection_transition(ROOT, transition, restore=True)['changed'])
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())
        self.assertFalse(operation.publish_selection_transition(ROOT, transition, restore=True)['changed'])
        self.assertEqual('compile_result.json', self.fixture.materialize().name)
        self.assertEqual(before, file_hash(base_zip))

    def test_wrong_prior_or_stale_producer_refuses_before_installation(self):
        target = self.fixture.active.parent / self.authority['selected_refinement']['bundle']['path']
        for expected in ('0'*64, 'missing'):
            with self.subTest(expected=expected), self.assertRaises(CompilerError):
                operation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate, expected)
        with patch.object(operation, 'implementation_identity', return_value='0'*64), self.assertRaises(CompilerError):
            self.prepare()
        self.assertFalse(target.exists())
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())

    def test_existing_conflicting_immutable_payload_is_never_overwritten(self):
        target = self.fixture.active.parent / self.authority['selected_refinement']['bundle']['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b'foreign immutable payload')
        with self.assertRaises(CompilerError):
            self.prepare()
        self.assertEqual(b'foreign immutable payload', target.read_bytes())
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())

    def test_unknown_pointer_refuses_publish_and_recovery_without_overwriting_it(self):
        transition = self.prepare()
        self.fixture.active.write_bytes(self.old_bytes + b' ')
        unknown = self.fixture.active.read_bytes()
        for restore in (False, True):
            with self.subTest(restore=restore), self.assertRaises(CompilerError):
                operation.publish_selection_transition(ROOT, transition, restore=restore)
            self.assertEqual(unknown, self.fixture.active.read_bytes())
        with self.assertRaises(CompilerError):
            operation.validate_selection_transition(ROOT, transition)

    def test_forged_recovery_target_bytes_or_lineage_refuse_unchanged(self):
        transition = self.prepare()
        changes = (
            ('active_path', self.candidate.relative_to(ROOT).as_posix()),
            ('profile_path', '../outside.json'), ('profile_sha256', '0'*64),
            ('old_active_bytes_base64', base64.b64encode(b'{}').decode()),
            ('new_active_bytes_base64', 'not base64'), ('effective_result_sha256', '0'*64),
        )
        for field, value in changes:
            forged = {**transition, field: value}
            with self.subTest(field=field), self.assertRaises(CompilerError):
                operation.publish_selection_transition(ROOT, forged)
            self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())

    def test_replace_failure_preserves_the_last_known_pointer_and_supports_retry(self):
        transition = self.prepare()
        with patch.object(operation.os, 'replace', side_effect=OSError('injected')), self.assertRaises(CompilerError):
            operation.publish_selection_transition(ROOT, transition)
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())
        operation.publish_selection_transition(ROOT, transition)
        selected = self.fixture.active.read_bytes()
        with patch.object(operation.os, 'replace', side_effect=OSError('injected')), self.assertRaises(CompilerError):
            operation.publish_selection_transition(ROOT, transition, restore=True)
        self.assertEqual(selected, self.fixture.active.read_bytes())
        operation.publish_selection_transition(ROOT, transition, restore=True)
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())

    def test_historical_recovery_and_noop_do_not_rerun_unused_current_producers(self):
        transition = self.prepare()
        operation.publish_selection_transition(ROOT, transition)
        with patch.object(operation, 'implementation_identity', side_effect=AssertionError('Historical state is frozen')):
            self.assertEqual('selected', operation.validate_selection_transition(ROOT, transition)['state'])
            self.assertFalse(operation.publish_selection_transition(ROOT, transition)['changed'])
            operation.publish_selection_transition(ROOT, transition, restore=True)
        with patch.object(operation, 'implementation_identity', return_value='0'*64), self.assertRaises(CompilerError):
            operation.publish_selection_transition(ROOT, transition)
        self.assertEqual(self.old_bytes, self.fixture.active.read_bytes())

    def test_competing_selection_tickets_cannot_overwrite_the_first_commit(self):
        first = self.prepare()
        self.candidate.write_bytes(self.candidate.read_bytes() + b' ')
        second = self.prepare()
        self.assertNotEqual(first['new_active_sha256'], second['new_active_sha256'])
        results, failures = [], []

        def publish(transition):
            try:
                results.append(operation.publish_selection_transition(ROOT, transition))
            except CompilerError as error:
                failures.append(error.code)

        workers = [threading.Thread(target=publish, args=(ticket,)) for ticket in (first, second)]
        for worker in workers: worker.start()
        for worker in workers: worker.join()
        self.assertEqual(1, len(results))
        self.assertEqual(1, len(failures))
        self.assertIn(file_hash(self.fixture.active), (first['new_active_sha256'], second['new_active_sha256']))

    def test_public_cli_prepares_validates_publishes_and_restores_exact_owned_selection(self):
        def invoke(arguments):
            output, errors = io.StringIO(), io.StringIO()
            with redirect_stdout(output), redirect_stderr(errors):
                code = cli.main(arguments)
            return code, json.loads(output.getvalue() or errors.getvalue())

        code, transition = invoke(['prepare-selection-transition', '--profile', str(self.fixture.profile_path),
            '--candidate', str(self.candidate), '--expected-active-sha256', self.old_sha])
        self.assertEqual(0, code)
        path = self.fixture.root / 'transition.json'
        path.write_text(json.dumps(transition), encoding='utf-8')
        for action, state in (('validate', 'prior'), ('publish', 'selected'), ('restore', 'prior')):
            code, result = invoke(['selection-transition', '--transition', str(path), '--action', action])
            self.assertEqual(0, code)
            self.assertEqual(state, result['state'])
        self.fixture.active.write_bytes(self.old_bytes + b' ')
        code, rejected = invoke(['selection-transition', '--transition', str(path), '--action', 'publish'])
        self.assertEqual(6, code)
        self.assertEqual('rejected', rejected['status'])
        self.assertEqual('selection-transition', rejected['operation'])
        self.assertEqual(self.old_bytes + b' ', self.fixture.active.read_bytes())


if __name__ == '__main__':
    unittest.main()
