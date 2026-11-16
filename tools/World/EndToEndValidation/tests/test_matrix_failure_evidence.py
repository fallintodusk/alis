"""Final judgment must retain costly native evidence without accepting a failed run."""
from contextlib import ExitStack, redirect_stderr
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.EndToEndValidation.app import acceptance, cli
from World.EndToEndValidation.app.contracts import ValidationFailure


class MatrixFailureEvidenceTests(unittest.TestCase):
    def test_final_refusal_retains_execution_but_cannot_be_admitted(self):
        scratch = ROOT / 'tmp/world/end_to_end_validation/failure-evidence-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        for failure, expected in ((ValidationFailure('projection_stale', 'Revision differs'), 3),
                                  (RuntimeError('unhandled final judgment'), 6)):
            with self.subTest(failure=type(failure).__name__), tempfile.TemporaryDirectory(dir=scratch) as directory:
                root = Path(directory)
                profile = root / 'validation.json'
                profile.write_text('{}', encoding='ascii')
                execution = {'profiles': {'city': {'unreal_first': 'native.json', 'unreal_first_sha256': 'a' * 64}},
                    'generated_tree_restoration': {'status': 'accepted', 'owners': {'fixture': {'file_count': 17}}}}
                with ExitStack() as stack:
                    stack.enter_context(patch.object(cli, 'REPO_ROOT', root))
                    for name, value in (('profile_path', profile), ('load_profile', {'profile_id': 'fixture'}),
                                        ('validation_profile_contract', {'profile': 'pinned'}),
                                        ('load_common_checks', {}), ('execute', execution)):
                        stack.enter_context(patch.object(cli, name, return_value=value))
                    stack.enter_context(patch.object(cli, 'validate', side_effect=failure))
                    errors = stack.enter_context(redirect_stderr(io.StringIO()))
                    code = cli.main(['run', '--profile', str(profile), '--check-result', 'unused',
                                     '--bootstrap-preflight', 'unused'])
                self.assertEqual(expected, code)
                result_path = Path(json.loads(errors.getvalue())['result'])
                result = json.loads(result_path.read_text(encoding='utf-8'))
                self.assertNotEqual('accepted', result['status'])
                self.assertEqual(execution, result['evidence'])
                self.assertEqual([], result['checks'])
                with patch.object(acceptance, 'PIPELINE_ROOT', result_path.parent.parent):
                    with self.assertRaises(ValidationFailure) as raised:
                        acceptance.accepted_run_evidence(result_path.parent.name, 'fixture')
                self.assertEqual('e2e_run_not_accepted', raised.exception.code)
