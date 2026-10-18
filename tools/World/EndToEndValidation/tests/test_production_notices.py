from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import validation
from World.EndToEndValidation.app.contracts import ValidationFailure


class ProductionNoticesTests(unittest.TestCase):
    def setUp(self) -> None:
        scratch = REPO_ROOT / "tmp/world/end_to_end_validation/tests/production_notices"
        scratch.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.profile = {"profiles": {
            "synthetic": {}, "kazan": {"canonical_authority": True},
            "manhattan": {"canonical_authority": True}}}
        self.execution = {"profiles": {}}
        for city in ("kazan", "manhattan"):
            for route in ("compiled", "promoted"):
                self._write(city, route)
            self.execution["profiles"][city] = {
                "compile_first_root": str(self.root / city / "compiled"),
                "realization_compile_result": str(self.root / city / "promoted/compile_result.json")}

    def _write(self, city: str, route: str, modified_notice: bool = True) -> None:
        path = self.root / city / route / "reports/attribution.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"entries": [
            {"license": {"id": "ODbL-1.0", "attribution": "Contributors",
                         "source_or_alteration_offer": "Pinned source"}},
            {"license": {"id": "LicenseRef-COP-DEM-GLO-30", "modified_notice": modified_notice}},
        ]}), encoding="utf-8")

    def _validate(self, mode: str) -> list[dict]:
        return validation._validate_production_notices(self.profile, self.execution, mode)

    def test_both_city_notices_pass_from_each_mode_owned_root(self) -> None:
        for mode in ("full", "canonical_authority"):
            with self.subTest(mode=mode):
                checks = self._validate(mode)
                self.assertEqual(["kazan", "manhattan"], [check["profile"] for check in checks])
                self.assertTrue(all(check["check"] == "attribution_notices_and_offer" for check in checks))

    def test_manhattan_corruption_refuses_in_both_modes(self) -> None:
        for mode, route in (("full", "compiled"), ("canonical_authority", "promoted")):
            with self.subTest(mode=mode):
                self._write("manhattan", route, modified_notice=False)
                with self.assertRaises(ValidationFailure) as raised:
                    self._validate(mode)
                self.assertEqual("copernicus_notice_missing", raised.exception.code)
                self._write("manhattan", route)

    def test_missing_manhattan_notice_refuses(self) -> None:
        (self.root / "manhattan/promoted/reports/attribution.json").unlink()
        with self.assertRaises(ValidationFailure) as raised:
            self._validate("canonical_authority")
        self.assertEqual("json_read_failed", raised.exception.code)

    def test_lineage_flag_cannot_disable_real_source_notices_in_full_mode(self) -> None:
        for city in ("kazan", "manhattan"):
            for flag in (False, None):
                with self.subTest(city=city, canonical_authority=flag):
                    self.profile["profiles"][city] = ({"canonical_authority": flag} if flag is not None else {})
                    self._write(city, "compiled", modified_notice=False)
                    with self.assertRaises(ValidationFailure) as raised:
                        self._validate("full")
                    self.assertEqual("copernicus_notice_missing", raised.exception.code)
                    self._write(city, "compiled")


if __name__ == "__main__":
    unittest.main()
