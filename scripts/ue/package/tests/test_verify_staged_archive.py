from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "verify_staged_archive.py"
SPEC = importlib.util.spec_from_file_location("verify_staged_archive", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ArchiveIntegrityTests(unittest.TestCase):
    def test_accepts_complete_archive_and_rejects_missing_or_truncated_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            staged = root / "staged"
            archive = root / "archive"
            staged_file = staged / "Alis" / "Alis.uproject"
            archived_file = archive / "Alis" / "Alis.uproject"
            staged_file.parent.mkdir(parents=True)
            archived_file.parent.mkdir(parents=True)
            staged_file.write_bytes(b"project")
            archived_file.write_bytes(b"project")
            self.assertEqual([], MODULE.archive_problems(staged, archive))

            archived_file.write_bytes(b"short")
            self.assertEqual(
                ["size mismatch: Alis/Alis.uproject"],
                MODULE.archive_problems(staged, archive),
            )
            archived_file.unlink()
            self.assertEqual(
                ["missing: Alis/Alis.uproject"],
                MODULE.archive_problems(staged, archive),
            )


if __name__ == "__main__":
    unittest.main()
