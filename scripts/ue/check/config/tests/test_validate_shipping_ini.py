from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "validate_shipping_ini.py"


class ShippingCookPolicyTests(unittest.TestCase):
    def run_validator(self, game_ini: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            config_dir = Path(temporary_directory)
            (config_dir / "DefaultGame.ini").write_text(game_ini, encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(SCRIPT), "--config-dir", str(config_dir)],
                text=True,
                capture_output=True,
                check=False,
            )

    @staticmethod
    def accepted_config() -> str:
        return """[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToNeverCook=(Path="/MetaHuman/GenericTracker")
+DirectoriesToNeverCook=(Path="/MetaHuman/Solver")
+DirectoriesToNeverCook=(Path="/MetaHumanCoreTech/GenericTracker")
+DirectoriesToNeverCook=(Path="/MetaHumanCoreTech/RealtimeMono")
"""

    def test_accepts_definition_owned_cook_closure(self) -> None:
        result = self.run_validator(self.accepted_config())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_rejects_broad_project_object_cook_root(self) -> None:
        result = self.run_validator(
            self.accepted_config()
            + '+DirectoriesToAlwaysCook=(Path="/ProjectObject")\n'
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not force the whole /ProjectObject mount", result.stdout)

    def test_rejects_missing_authoring_model_exclusion(self) -> None:
        result = self.run_validator(
            self.accepted_config().replace(
                '+DirectoriesToNeverCook=(Path="/MetaHumanCoreTech/RealtimeMono")\n',
                "",
            )
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("/MetaHumanCoreTech/RealtimeMono", result.stdout)


if __name__ == "__main__":
    unittest.main()
