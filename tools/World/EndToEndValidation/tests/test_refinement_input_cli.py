"""The public input route authenticates candidates without running or promoting World."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.EndToEndValidation.app import cli, refinement_selection


class RefinementInputCliTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / 'tmp/world/end_to_end_validation/cli-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.profile = self.root / 'validation.json'
        self.profile.write_text('{}', encoding='utf-8')
        self.selection = self.root / 'selection.json'
        self.document = {'base_selection': {'validation_profile_path': self.profile.relative_to(ROOT).as_posix()},
            'candidates': {'fixture': {'result_sha256': 'a'*64}}}
        self.selection.write_text(json.dumps(self.document), encoding='utf-8')
        for name, replacement in (('profile_path', self.profile), ('load_profile', {'profiles': {}})):
            context = patch.object(cli, name, return_value=replacement)
            context.start()
            self.addCleanup(context.stop)
        for name in ('execute', 'execute_checks'):
            context = patch.object(cli, name, side_effect=AssertionError('Input admission must not execute a gate'))
            context.start()
            self.addCleanup(context.stop)

    def run_cli(self, operation, arguments):
        output, errors = io.StringIO(), io.StringIO()
        with redirect_stdout(output), redirect_stderr(errors):
            code = cli.main([operation, '--profile', str(self.profile), '--bootstrap-preflight', 'not-executed', *arguments])
        return code, json.loads(output.getvalue() or errors.getvalue())

    def test_snapshot_dispatches_exact_candidates_and_true_base_without_world_execution(self):
        with patch.object(refinement_selection, 'snapshot_candidates', return_value=self.document) as snapshot:
            code, result = self.run_cli('refinement-inputs', ['--changed-base', 'slice', '--candidate', 'fixture=tmp/child/active.json'])
        self.assertEqual(0, code)
        self.assertEqual(self.document, result)
        self.assertEqual((ROOT, {'profiles': {}}, 'slice', self.profile,
            {'fixture': (ROOT / 'tmp/child/active.json').resolve()}), snapshot.call_args.args)

    def test_malformed_or_duplicate_candidate_entries_refuse_before_admission(self):
        for entries in (['fixture'], ['=tmp/path'], ['fixture='], ['fixture=a', 'fixture=b']):
            arguments = ['--changed-base', 'slice']
            for entry in entries:
                arguments += ['--candidate', entry]
            with self.subTest(entries=entries), patch.object(refinement_selection, 'snapshot_candidates') as snapshot:
                code, result = self.run_cli('refinement-inputs', arguments)
                self.assertEqual(3, code)
                self.assertEqual('rejected', result['status'])
                snapshot.assert_not_called()

    def test_verify_binds_selection_bytes_after_actual_validation(self):
        with patch.object(refinement_selection, 'validate_candidates') as verify:
            code, result = self.run_cli('verify-refinement-inputs', ['--selection', str(self.selection)])
        self.assertEqual(0, code)
        self.assertEqual(cli.file_hash(self.selection), result['selection_sha256'])
        verify.assert_called_once_with(ROOT, {'profiles': {}}, self.document)

    def test_verify_rejects_changed_selection_and_another_profile(self):
        def change(*_):
            self.selection.write_text('{}', encoding='utf-8')

        with patch.object(refinement_selection, 'validate_candidates', side_effect=change):
            code, _ = self.run_cli('verify-refinement-inputs', ['--selection', str(self.selection)])
        self.assertEqual(3, code)
        self.document['base_selection']['validation_profile_path'] = 'other.json'
        self.selection.write_text(json.dumps(self.document), encoding='utf-8')
        with patch.object(refinement_selection, 'validate_candidates') as verify:
            code, _ = self.run_cli('verify-refinement-inputs', ['--selection', str(self.selection)])
        self.assertEqual(3, code)
        verify.assert_not_called()
