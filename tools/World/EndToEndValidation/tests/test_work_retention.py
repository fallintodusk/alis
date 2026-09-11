from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.cli import _prune_work_runs


class WorkRetentionTests(unittest.TestCase):
    def test_prune_keeps_only_newest_owned_matrix_runs(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            for name in ("run-20260101T000000Z", "run-20260102T000000Z", "run-20260103T000000Z"):
                run = root / name
                run.mkdir()
                (run / "owned.bin").write_bytes(b"owned")
            unrelated = root / "canonical_authority"
            unrelated.mkdir()

            _prune_work_runs(root, retain=1)

            self.assertEqual(["run-20260103T000000Z"], sorted(item.name for item in root.glob("run-*")))
            self.assertTrue(unrelated.is_dir())


if __name__ == "__main__":
    unittest.main()
