from __future__ import annotations

import json
import shutil
import sys
import tempfile
import unittest
from contextlib import nullcontext
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import execution
from World.EndToEndValidation.app.contracts import ValidationFailure


class MatrixRestorationTests(unittest.TestCase):
    def _execute(
        self, root: Path, content: Path, presentation: Path, data: Path,
        profiles: dict[str, dict[str, str]], realize: object, operation_id: str,
    ) -> dict[str, object]:
        preflight = root / f"{operation_id}-preflight.json"
        preflight.write_text(json.dumps({"status": "accepted"}), encoding="utf-8")
        common = {
            "test_suites": [],
            "receipt": "check.json",
            "receipt_sha256": "1" * 64,
            "operation_id": "check:common:test",
            "common_contract_sha256": "2" * 64,
            "environment": {},
        }
        with (
            patch.object(execution, "REPO_ROOT", root),
            patch.object(execution, "_powershell", return_value="powershell"),
            patch.object(execution, "_content_mutation_lock", return_value=nullcontext()),
            patch.object(execution, "_world_data_roots", return_value=(content, presentation, data)),
            patch.object(execution, "_run_profile", side_effect=realize),
            patch.object(execution, "validate_against"),
        ):
            return execution.execute(
                {"profiles": profiles}, operation_id, root / "evidence", preflight, common,
            )

    def test_accepted_execution_restores_exact_pre_matrix_generated_files(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Plugins" / "World" / "ProjectWorldData" / "Content"
            data = content.parent / "Data"
            presentation = content / "Generated" / "Presentation"
            map_path = content / "Generated" / "P0" / "L_Test.umap"
            map_path.parent.mkdir(parents=True)
            presentation.mkdir(parents=True)
            map_path.write_bytes(b"accepted-map")
            sibling = map_path.parent / "L_Sibling.umap"
            sibling.write_bytes(b"accepted-sibling")
            material = presentation / "MI_Test.uasset"
            material.write_bytes(b"accepted-material")

            settings = {
                "world_data_plugin": "ProjectWorldData",
                "map_package": "/ProjectWorldData/Generated/P0/L_Test",
            }

            def realize(*_args: object) -> dict[str, object]:
                map_path.parent.mkdir(parents=True, exist_ok=True)
                presentation.mkdir(parents=True, exist_ok=True)
                map_path.write_bytes(b"matrix-map")
                (map_path.parent / "L_Test_HLODLayer_Merged.uasset").write_bytes(b"matrix-hlod")
                material.write_bytes(b"matrix-material")
                return {"status": "accepted"}

            result = self._execute(
                root, content, presentation, data,
                {"kazan": settings}, realize, "accepted-matrix",
            )

            self.assertEqual(b"accepted-map", map_path.read_bytes())
            self.assertEqual(b"accepted-sibling", sibling.read_bytes())
            self.assertEqual(b"accepted-material", material.read_bytes())
            self.assertFalse((map_path.parent / "L_Test_HLODLayer_Merged.uasset").exists())
            restoration = result["generated_tree_restoration"]
            self.assertEqual("accepted", restoration["status"])
            self.assertEqual(
                restoration["owners"]["ProjectWorldData"]["before_sha256"],
                restoration["owners"]["ProjectWorldData"]["after_sha256"],
            )
            self.assertEqual(3, restoration["owners"]["ProjectWorldData"]["file_count"])
            self.assertFalse((root / "tmp" / "world" / "end_to_end_validation" / "accepted-matrix" / "map_backup").exists())

    def test_out_of_scope_generated_drift_is_rejected_and_repaired(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Plugins" / "World" / "ProjectWorldData" / "Content"
            data = content.parent / "Data"
            presentation = content / "Generated" / "Presentation"
            map_path = content / "Generated" / "P0" / "L_Test.umap"
            sibling = map_path.parent / "L_Sibling.umap"
            map_path.parent.mkdir(parents=True)
            presentation.mkdir(parents=True)
            map_path.write_bytes(b"accepted-map")
            sibling.write_bytes(b"accepted-sibling")
            (presentation / "MI_Test.uasset").write_bytes(b"accepted-material")
            settings = {
                "world_data_plugin": "ProjectWorldData",
                "map_package": "/ProjectWorldData/Generated/P0/L_Test",
            }

            def realize(*_args: object) -> dict[str, object]:
                map_path.parent.mkdir(parents=True, exist_ok=True)
                map_path.write_bytes(b"matrix-map")
                sibling.write_bytes(b"sabotaged-sibling")
                (map_path.parent / "L_Unexpected.umap").write_bytes(b"unexpected")
                return {"status": "accepted"}

            with self.assertRaises(ValidationFailure) as raised:
                self._execute(
                    root, content, presentation, data,
                    {"kazan": settings}, realize, "out-of-scope-matrix",
                )

            self.assertEqual("matrix_generated_out_of_scope_mutation", raised.exception.code)
            self.assertEqual(b"accepted-map", map_path.read_bytes())
            self.assertEqual(b"accepted-sibling", sibling.read_bytes())
            self.assertFalse((map_path.parent / "L_Unexpected.umap").exists())

    def test_profile_failure_remains_primary_after_exact_restoration(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Plugins" / "World" / "ProjectWorldData" / "Content"
            data = content.parent / "Data"
            presentation = content / "Generated" / "Presentation"
            map_path = content / "Generated" / "P0" / "L_Test.umap"
            sibling = map_path.parent / "L_Sibling.umap"
            map_path.parent.mkdir(parents=True)
            presentation.mkdir(parents=True)
            map_path.write_bytes(b"accepted-map")
            sibling.write_bytes(b"accepted-sibling")
            material = presentation / "MI_Test.uasset"
            material.write_bytes(b"accepted-material")
            settings = {
                "world_data_plugin": "ProjectWorldData",
                "map_package": "/ProjectWorldData/Generated/P0/L_Test",
            }

            def realize(*_args: object) -> dict[str, object]:
                map_path.parent.mkdir(parents=True, exist_ok=True)
                presentation.mkdir(parents=True, exist_ok=True)
                map_path.write_bytes(b"matrix-map")
                sibling.write_bytes(b"sabotaged-sibling")
                material.write_bytes(b"matrix-material")
                raise ValidationFailure("profile_failed", "Injected profile failure")

            with self.assertRaises(ValidationFailure) as raised:
                self._execute(
                    root, content, presentation, data,
                    {"kazan": settings}, realize, "failed-profile-matrix",
                )

            self.assertEqual("profile_failed", raised.exception.code)
            self.assertEqual(b"accepted-map", map_path.read_bytes())
            self.assertEqual(b"accepted-sibling", sibling.read_bytes())
            self.assertEqual(b"accepted-material", material.read_bytes())

    def test_partial_second_backup_failure_restores_every_owner_byte(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            content = root / "Plugins" / "World" / "ProjectWorldData" / "Content"
            data = content.parent / "Data"
            presentation = content / "Generated" / "Presentation"
            first = content / "Generated" / "P0" / "L_First.umap"
            second = content / "Generated" / "Representative" / "L_Second.umap"
            first.parent.mkdir(parents=True)
            second.parent.mkdir(parents=True)
            presentation.mkdir(parents=True)
            first.write_bytes(b"accepted-first")
            second.write_bytes(b"accepted-second")
            material = presentation / "MI_Test.uasset"
            material.write_bytes(b"accepted-material")
            profiles = {
                "first": {
                    "world_data_plugin": "ProjectWorldData",
                    "map_package": "/ProjectWorldData/Generated/P0/L_First",
                },
                "second": {
                    "world_data_plugin": "ProjectWorldData",
                    "map_package": "/ProjectWorldData/Generated/Representative/L_Second",
                },
            }
            original = execution._backup_generated
            call_count = 0

            def fail_second(*args: object) -> list[tuple[Path, Path]]:
                nonlocal call_count
                call_count += 1
                if call_count == 1:
                    return original(*args)
                backup_root = args[1]
                moves = args[6]
                destination = backup_root / second.relative_to(content)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(second), str(destination))
                moves.append((destination, second))
                raise ValidationFailure("backup_failed", "Injected partial backup failure")

            with patch.object(execution, "_backup_generated", side_effect=fail_second):
                with self.assertRaises(ValidationFailure) as raised:
                    self._execute(
                        root, content, presentation, data, profiles,
                        lambda *_args: self.fail("Profile must not run"), "backup-failure-matrix",
                    )

            self.assertEqual("backup_failed", raised.exception.code)
            self.assertEqual(b"accepted-first", first.read_bytes())
            self.assertEqual(b"accepted-second", second.read_bytes())
            self.assertEqual(b"accepted-material", material.read_bytes())


if __name__ == "__main__":
    unittest.main()
