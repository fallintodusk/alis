from __future__ import annotations

import copy
import unittest
from pathlib import Path
from unittest.mock import patch

from World.EndToEndValidation.app import acceptance
from World.EndToEndValidation.app.acceptance import verify_manifest_provenance


class MaterialAdoptionProvenanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.map = "/ProjectWorldData/Generated/City/L_City"
        self.old = "a" * 64
        self.current = "b" * 64
        self.identity = {
            "map_package": self.map,
            "compile_result_sha256": "c" * 64,
            "presentation_profile_sha256": self.old,
            "runtime_profile_sha256": "none",
            "authored_overlay_profile_sha256": "none",
        }
        self.bridge = {
            "map_scope_id": "map_city",
            "geometry_profile_sha256": self.old,
            "current_profile_sha256": self.current,
            "validated_scope_ids": ["map_city", "layer_city_water", "presentation_shared"],
        }
        self.scopes = [
            {"scope_id": name, "owning_layer": layer, "input_identity": dict(self.identity),
             "presentation_compatibility": copy.deepcopy(self.bridge)}
            for name, layer in (("map_city", "map"), ("layer_city_water", "water"))
        ]
        self.records = {self.map: {
            "compile_result_sha256": "c" * 64,
            "presentation_profile_sha256": self.current,
            "runtime_profile_sha256": None,
            "authored_overlay_profile_sha256": None,
        }}

    def test_owner_authenticated_adoption_preserves_original_geometry_lineage(self) -> None:
        self.assertEqual([], verify_manifest_provenance(self.scopes, self.records))

    def test_bridge_is_scoped_to_exact_map_scope_and_profile_pair(self) -> None:
        for field, value in (
            ("map_scope_id", "missing_map"),
            ("geometry_profile_sha256", "d" * 64),
            ("current_profile_sha256", "e" * 64),
            ("validated_scope_ids", ["other_scope"]),
        ):
            with self.subTest(field=field):
                scopes = copy.deepcopy(self.scopes)
                scopes[1]["presentation_compatibility"][field] = value
                self.assertTrue(verify_manifest_provenance(scopes, self.records))

    def test_bridge_cannot_launder_other_consumed_inputs_or_missing_map_coverage(self) -> None:
        for field in ("compile_result_sha256", "authored_overlay_profile_sha256", "runtime_profile_sha256"):
            with self.subTest(field=field):
                scopes = copy.deepcopy(self.scopes)
                scopes[0]["input_identity"][field] = "d" * 64
                self.assertTrue(verify_manifest_provenance(scopes, self.records))
        self.assertTrue(verify_manifest_provenance(self.scopes, {}))

        scopes = copy.deepcopy(self.scopes)
        other = copy.deepcopy(scopes[0])
        other["scope_id"] = "map_other"
        other["input_identity"]["map_package"] = "/ProjectWorldData/Generated/Other/L_Other"
        scopes.append(other)
        scopes[1]["presentation_compatibility"]["map_scope_id"] = "map_other"
        self.assertTrue(verify_manifest_provenance(scopes, self.records))

    def test_audit_receives_the_explicit_pinned_presentation_input(self) -> None:
        profile = Path("owner/Data/presentation.json")
        with patch.object(acceptance, "_run") as run, patch.object(
            acceptance, "read_json", return_value={"status": "accepted"}
        ):
            acceptance._run_audit("powershell", Path("logs"), "audit", Path("audit.json"),
                                  "ProjectWorldData", presentation_profile=profile)
        command = run.call_args.args[1]
        self.assertEqual(str(profile), command[command.index("-PresentationProfile") + 1])

    def test_shared_presentation_requires_every_consumer_bridge(self) -> None:
        presentation = {
            "scope_id": "presentation_shared", "owning_layer": "presentation",
            "input_identity": {"presentation_profile_sha256": self.old},
            "consumer_references": ["map_city"],
        }
        scopes = copy.deepcopy(self.scopes) + [presentation]
        self.assertEqual([], verify_manifest_provenance(scopes, self.records))
        del scopes[0]["presentation_compatibility"]
        self.assertTrue(verify_manifest_provenance(scopes, self.records))
        scopes = copy.deepcopy(self.scopes) + [presentation]
        scopes[-1]["consumer_references"].append("ungated_map")
        self.assertTrue(verify_manifest_provenance(scopes, self.records))


if __name__ == "__main__":
    unittest.main()
