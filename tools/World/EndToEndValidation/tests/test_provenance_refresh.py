"""A provenance-only canonical change passes enrollment and acceptance without regeneration.

A compile run with identical canonical content yields a new compile-result digest.
The map and layer manifests are republished with that digest over unchanged
artifacts. Generated layers never consume the runtime profile, so they record its
sentinel while the map and the gated Matrix leg carry the real runtime digest.
"""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app import enrollment
from World.EndToEndValidation.app.acceptance import gated_leg_records, verify_manifest_provenance
from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash


MAP = "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory"
MAP_SCOPE = "map_territory_l_projectworldkazanterritory"
LAYERS = ("terrain", "water", "roads")
PRIOR_COMPILE = "1" * 64
CURRENT_COMPILE = "a" * 64
PRESENTATION = "b" * 64
AUTHORED = "c" * 64
RUNTIME = "f" * 64


def _layer_scope_id(layer: str) -> str:
    return f"layer_kazan_territory_v1_{layer}"


def _digest(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def _scope(scope_id: str, owning_layer: str, generation: int, identity: dict[str, str]) -> dict:
    """One audit scope summary; its artifact set never changes across generations."""
    return {
        "scope_id": scope_id,
        "manifest_path": f"scopes/{scope_id}.{generation}.json",
        "manifest_sha256": _digest(f"{scope_id}.{generation}"),
        "input_identity": identity,
        "consumer_references": [] if owning_layer == "map" else [MAP_SCOPE],
        "generation": generation,
        "owning_layer": owning_layer,
        "artifact_set_sha256": _digest(f"{scope_id}:artifacts"),
    }


def _territory(generation: int, compile_result: str, layer_compile_result: str | None = None) -> list[dict]:
    def identity(compile_sha: str, runtime: str) -> dict[str, str]:
        return {
            "compile_result_sha256": compile_sha,
            "presentation_profile_sha256": PRESENTATION,
            "runtime_profile_sha256": runtime,
            "authored_overlay_profile_sha256": AUTHORED,
            "map_package": MAP,
        }

    layer_compile = layer_compile_result or compile_result
    return [
        _scope(MAP_SCOPE, "map", generation, identity(compile_result, RUNTIME)),
        *(_scope(_layer_scope_id(layer), layer, generation, identity(layer_compile, "none")) for layer in LAYERS),
    ]


PRESENTATION_SCOPE = _scope("presentation_kazan_representative_v1", "presentation", 36, {
    "compile_result_sha256": "none",
    "presentation_profile_sha256": PRESENTATION,
    "runtime_profile_sha256": "none",
    "authored_overlay_profile_sha256": "none",
    "map_package": "shared",
})


class ProvenanceRefreshTests(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp")
        self.addCleanup(directory.cleanup)
        receipt = Path(directory.name) / "unreal_first.json"
        receipt.write_text(json.dumps({
            "map_package": MAP,
            "input_sha256": CURRENT_COMPILE,
            "presentation_profile_sha256": PRESENTATION,
            "runtime_profile_sha256": RUNTIME,
            "authored_overlay_set_sha256": AUTHORED,
        }), encoding="utf-8")
        run = {"run_id": "run-provenance-refresh", "document": {"evidence": {"profiles": {"kazan_territory": {
            "unreal_first": str(receipt),
            "unreal_first_sha256": file_hash(receipt),
        }}}}}
        self.leg_records = gated_leg_records([run])
        self.assertEqual(RUNTIME, self.leg_records[MAP]["runtime_profile_sha256"])
        self.assertEqual(CURRENT_COMPILE, self.leg_records[MAP]["compile_result_sha256"])
        self.prior = {"scopes": [*_territory(41, PRIOR_COMPILE), PRESENTATION_SCOPE]}
        self.refreshed = {"scopes": [*_territory(42, CURRENT_COMPILE), PRESENTATION_SCOPE]}

    def _enroll(self, post_audit: dict) -> list[str]:
        return enrollment._validate_post_enrollment(
            self.prior, post_audit, MAP, set(LAYERS), PRESENTATION, self.leg_records
        )

    def test_enrollment_accepts_refreshed_provenance_with_runtime_neutral_layers(self) -> None:
        try:
            enrolled = self._enroll(self.refreshed)
        except ValidationFailure as error:
            self.fail(f"{error.code}: {error.details.get('problems')}")
        self.assertEqual(sorted([MAP_SCOPE, *map(_layer_scope_id, LAYERS)]), enrolled)

    def test_acceptance_proves_refreshed_provenance_with_runtime_neutral_layers(self) -> None:
        self.assertEqual([], verify_manifest_provenance(self.refreshed["scopes"], self.leg_records))

    def test_layers_that_kept_the_prior_compile_result_stay_unproven(self) -> None:
        carried = {"scopes": [*_territory(42, CURRENT_COMPILE, layer_compile_result=PRIOR_COMPILE), PRESENTATION_SCOPE]}

        problems = verify_manifest_provenance(carried["scopes"], self.leg_records)
        for layer in LAYERS:
            self.assertTrue(
                [problem for problem in problems if _layer_scope_id(layer) in problem and "compile_result_sha256" in problem],
                f"{_layer_scope_id(layer)} kept the prior compile result unnoticed: {problems}",
            )
        with self.assertRaises(ValidationFailure) as raised:
            self._enroll(carried)
        self.assertEqual("enrollment_provenance_mismatch", raised.exception.code)

    def test_layer_claiming_runtime_profile_is_rejected(self) -> None:
        scopes = _territory(42, CURRENT_COMPILE)
        scopes[1]["input_identity"]["runtime_profile_sha256"] = RUNTIME
        problems = verify_manifest_provenance(scopes, self.leg_records)
        self.assertEqual(1, len(problems))
        self.assertIn("runtime_profile_sha256", problems[0])


if __name__ == "__main__":
    unittest.main()
