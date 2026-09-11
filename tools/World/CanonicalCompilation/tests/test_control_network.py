from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.contracts import CompilerError, read_json, validate_document
from World.CanonicalCompilation.app.control_network import (
    geodesic_destination,
    geodesic_distance,
    qualify_control_network,
)


class ControlNetworkTests(unittest.TestCase):
    PROFILE_PATH = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Controls" / "kazan_territory_v1.control.json"
    )
    MANHATTAN_PROFILE_PATH = (
        REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
        / "Controls" / "manhattan_showcase_v1.control.json"
    )

    def _qualify_mutation(self, mutation) -> None:
        profile = read_json(self.PROFILE_PATH)
        profile["$schema"] = "https://alis.world/schemas/world-compiler/control-network-v1.json"
        mutation(profile)
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            path = Path(directory) / "control.json"
            path.write_text(json.dumps(profile), encoding="utf-8")
            qualify_control_network(REPO_ROOT, path)

    def test_wgs84_destination_preserves_the_requested_distance(self) -> None:
        origin = [49.108322, 55.796504]
        destination = geodesic_destination(origin, 5580.0, 45.0)
        self.assertAlmostEqual(5580.0, geodesic_distance(origin, destination), places=5)

    def test_production_control_network_is_accepted(self) -> None:
        receipt, path = qualify_control_network(REPO_ROOT, self.PROFILE_PATH)
        self.assertEqual("accepted", receipt["status"])
        self.assertLessEqual(receipt["metrics"]["max_projection_scale_delta_m"], 1.5)
        self.assertRegex(receipt["implementation_sha256"], "^[0-9a-f]{64}$")
        self.assertEqual(receipt["inputs_hash"], path.parent.name)
        self.assertTrue(receipt["metrics"]["non_collinear"])
        validate_document(read_json(path), path)

    def test_manhattan_control_network_is_accepted_with_measured_utm_scale(self) -> None:
        receipt, path = qualify_control_network(REPO_ROOT, self.MANHATTAN_PROFILE_PATH)
        self.assertEqual("accepted", receipt["status"])
        self.assertGreater(receipt["metrics"]["max_projection_scale_delta_m"], 1.5)
        self.assertLessEqual(receipt["metrics"]["max_projection_scale_delta_m"], 2.0)
        validate_document(read_json(path), path)

    def test_unknown_accuracy_fails_closed(self) -> None:
        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(
                lambda profile: profile["controls"][0]["horizontal_provenance"].update(accuracy_m=None)
            )
        self.assertEqual("control_accuracy_unknown", raised.exception.code)

    def test_transform_and_pairwise_sabotage_fail_separately(self) -> None:
        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(
                lambda profile: profile["controls"][1]["expected_projected"].__setitem__(0, 374181.0)
            )
        self.assertEqual("control_numeric_error", raised.exception.code)

        def pairwise(profile: dict) -> None:
            profile["controls"][1]["expected_projected"][0] += 0.02
            profile["controls"][1]["horizontal_provenance"]["accuracy_m"] = 1.0
            profile["controls"][1]["horizontal_class"] = "standard"
            profile["controls"][1]["horizontal_tolerance_m"] = 2.0
            profile["gates"]["numeric_forward_inverse_m"] = 1.0
            profile["gates"]["boundary_corner_error_m"] = 1.0
            profile["gates"]["pairwise_distance_error_m"] = 0.001

        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(pairwise)
        self.assertEqual("control_pairwise_error", raised.exception.code)

    def test_horizontal_class_and_absolute_z_fail_closed(self) -> None:
        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(
                lambda profile: profile["controls"][0]["horizontal_provenance"].update(accuracy_m=6.0)
            )
        self.assertEqual("control_horizontal_class_error", raised.exception.code)

        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(
                lambda profile: profile["controls"][1].update(horizontal_tolerance_m=2.01)
            )
        self.assertEqual("control_horizontal_class_error", raised.exception.code)

        def absolute_without_provenance(profile: dict) -> None:
            profile["controls"][0]["vertical"]["mode"] = "absolute"

        with self.assertRaises(CompilerError) as raised:
            self._qualify_mutation(absolute_without_provenance)
        self.assertEqual("control_vertical_error", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
