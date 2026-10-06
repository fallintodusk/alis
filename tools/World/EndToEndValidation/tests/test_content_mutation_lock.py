from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import execution
from World.EndToEndValidation.app.contract_inputs import is_shared_contract_path
from World.EndToEndValidation.app.contracts import ValidationFailure


LOCK_SCRIPT = REPO_ROOT / "scripts" / "ue" / "generated_content" / "generated_content_mutation_lock.ps1"
POWERSHELL = shutil.which("powershell.exe") or shutil.which("powershell")


class ContentMutationLockTests(unittest.TestCase):
    """The E2E driver's owner path against the PowerShell lock owner it shares a lock with."""

    def _project(self) -> tempfile.TemporaryDirectory[str]:
        # A private fake project root: asserting on the real lock would fail whenever a
        # legitimate World operation runs.
        parent = REPO_ROOT / "tmp" / "world" / "content_lock_tests"
        parent.mkdir(parents=True, exist_ok=True)
        return tempfile.TemporaryDirectory(dir=parent)

    def _lock_path(self, root: Path) -> Path:
        return root / "tmp" / "world" / "world_realization" / "content_mutation.lock"

    def _powershell(self, root: Path, command: str, token: str | None) -> subprocess.CompletedProcess[str]:
        environment = dict(os.environ)
        environment.pop(execution.CONTENT_LOCK_TOKEN_ENV, None)
        if token is not None:
            environment[execution.CONTENT_LOCK_TOKEN_ENV] = token
        script = f". '{LOCK_SCRIPT}'; {command.replace('<ROOT>', str(root))}"
        return subprocess.run(
            [POWERSHELL, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script],
            env=environment, capture_output=True, text=True, check=False, timeout=120,
        )

    def test_owner_lifecycle_and_second_owner_refused(self) -> None:
        with self._project() as directory:
            lock_path = self._lock_path(Path(directory))
            with patch.object(execution, "CONTENT_LOCK_PATH", lock_path):
                self.assertNotIn(execution.CONTENT_LOCK_TOKEN_ENV, os.environ)
                with execution._content_mutation_lock() as token:
                    self.assertEqual(token, os.environ.get(execution.CONTENT_LOCK_TOKEN_ENV))
                    self.assertEqual(token, lock_path.read_text(encoding="ascii"))
                    with self.assertRaises(ValidationFailure) as raised:
                        with execution._content_mutation_lock():
                            self.fail("A second owner must never acquire the held content lock")
                    self.assertEqual("content_lock_unavailable", raised.exception.code)
                self.assertNotIn(execution.CONTENT_LOCK_TOKEN_ENV, os.environ)

    @unittest.skipUnless(POWERSHELL, "Windows PowerShell is required")
    def test_powershell_child_joins_the_python_owner_through_the_shared_variable(self) -> None:
        # The delegation variable has two spellings, one per language; a child that joins
        # the Python owner proves they agree, and one without it proves the lock is held.
        enter = "$l = Enter-ProjectGeneratedContentMutationLock -ProjectRoot '<ROOT>'; $l.Dispose()"
        with self._project() as directory:
            root = Path(directory)
            with patch.object(execution, "CONTENT_LOCK_PATH", self._lock_path(root)):
                with execution._content_mutation_lock() as token:
                    joined = self._powershell(root, enter, token)
                    self.assertEqual(0, joined.returncode, joined.stderr)
                    refused = self._powershell(root, enter, None)
                    self.assertNotEqual(0, refused.returncode)
                    self.assertIn("Another operation holds", refused.stderr + refused.stdout)

    @unittest.skipUnless(POWERSHELL, "Windows PowerShell is required")
    def test_pending_outer_recovery_refuses_both_owner_implementations(self) -> None:
        pending = "if (Test-ProjectGeneratedContentRecoveryPending -ProjectRoot '<ROOT>') { 'pending' } else { 'clear' }"
        for name in ("outer_recovery.json", "outer_recovery.json.tmp"):
            with self.subTest(marker=name), self._project() as directory:
                root = Path(directory)
                lock_path = self._lock_path(root)
                lock_path.parent.mkdir(parents=True)
                lock_path.write_text("f" * 32, encoding="ascii")
                (lock_path.parent / name).write_text("{}", encoding="utf-8")
                with patch.object(execution, "CONTENT_LOCK_PATH", lock_path):
                    with self.assertRaises(ValidationFailure) as raised:
                        with execution._content_mutation_lock():
                            self.fail("A pending outer recovery must refuse the owner path")
                self.assertEqual("content_recovery_pending", raised.exception.code)
                self.assertNotIn(execution.CONTENT_LOCK_TOKEN_ENV, os.environ)
                # Refusal leaves the lock file as it was.
                self.assertEqual("f" * 32, lock_path.read_text(encoding="ascii"))
                answer = self._powershell(root, pending, None)
                self.assertEqual(0, answer.returncode, answer.stderr)
                self.assertEqual("pending", answer.stdout.strip())

    def test_lock_owner_and_material_host_are_proof_inputs(self) -> None:
        # The common checks run their suites, so an edit to either stales the receipt.
        for relative in (
            "scripts/ue/generated_content/generated_content_mutation_lock.ps1",
            "scripts/ue/generated_content/generated_content_outer_recovery.ps1",
            "scripts/ue/generated_content/recover_generated_content.ps1",
            "scripts/ue/material/run_material_generation.ps1",
            "scripts/ue/material/material_host_recovery.ps1",
        ):
            with self.subTest(path=relative):
                self.assertTrue(is_shared_contract_path(relative))


if __name__ == "__main__":
    unittest.main()
