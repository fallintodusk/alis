from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from copy import deepcopy
from contextlib import redirect_stderr, redirect_stdout
from dataclasses import replace
from io import StringIO
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
WORLD_ROOT = REPO_ROOT / "tools" / "World"
CANONICAL_ROOT = WORLD_ROOT / "CanonicalCompilation"
TEST_DATA_ROOT = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
TEST_SOURCE_PROFILES = TEST_DATA_ROOT / "Profiles" / "SourceIngestion"
TEST_COMPILER_PROFILES = TEST_DATA_ROOT / "Profiles" / "CanonicalCompilation"
TEST_PROVIDER_FIXTURES = TEST_DATA_ROOT / "Fixtures" / "Provider"
sys.path.insert(0, str(TOOLS_ROOT))

from World.CanonicalCompilation.app.cli import main as compiler_main
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash, file_hash, read_json, validate_document
from World.CanonicalCompilation.app.features import compile_features
from World.CanonicalCompilation.app.geometry import geometry_rejection_reason
from World.CanonicalCompilation.app.lineage import base_cells, build_lineages, reusable_layers
from World.CanonicalCompilation.app.pipeline import (
    compilation_root,
    compile_world as run_compile_world,
    load_profile,
    profile_path as resolve_profile_path,
)
from World.CanonicalCompilation.app.source import load_source_bundle
from World.CanonicalCompilation.app.spatial import grid_id
from World.CanonicalCompilation.app.terrain import build_terrain_cells as build_terrain_cells_real
from World.SourceIngestion.app.cli import main as source_main


def _compiler_profile_value(value: str) -> str:
    if value in {"synthetic_two_cell", "synthetic_representative_v1"}:
        return str(TEST_COMPILER_PROFILES / f"{value}.compile.json")
    return value


def compile_world(profile_value: str, *args, **kwargs):
    return run_compile_world(_compiler_profile_value(profile_value), *args, **kwargs)


def profile_path(value: str) -> Path:
    return resolve_profile_path(_compiler_profile_value(value))


class CanonicalCompilationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        with redirect_stdout(StringIO()):
            result = source_main([
                "run", "--profile", str(TEST_SOURCE_PROFILES / "synthetic_two_cell.source.json")
            ])
        if result != 0:
            raise RuntimeError("Synthetic source ingestion failed")

    def _compile(self, root: Path) -> tuple[dict, Path]:
        return compile_world(
            str(TEST_COMPILER_PROFILES / "synthetic_two_cell.compile.json"),
            output_root_value=root,
        )

    @staticmethod
    def _features(root: Path) -> list[dict]:
        return [
            feature
            for path in sorted((root / "canonical" / "features").glob("*.json"))
            for feature in read_json(path)["features"]
        ]

    def _accepted_source(
        self,
        root: Path,
        profile_id: str,
        bbox: list[float],
    ) -> tuple[str, Path]:
        profile = read_json(TEST_SOURCE_PROFILES / "synthetic_two_cell.source.json")
        profile["$schema"] = "https://alis.world/schemas/world-source/source-profile-v1.json"
        profile["profile_id"] = profile_id
        profile["area"]["area_id"] = profile_id
        profile["area"]["bbox"] = bbox
        provider = TEST_PROVIDER_FIXTURES / "synthetic_two_cell" / "synthetic_provider.json"
        profile["sources"][0]["local_path"] = provider.as_posix()
        profile_path_value = root / f"{profile_id}.source.json"
        profile_path_value.write_text(json.dumps(profile), encoding="utf-8")
        self.addCleanup(profile_path_value.unlink, missing_ok=True)
        output_root = root / f"{profile_id}.source"
        with redirect_stdout(StringIO()):
            result = source_main([
                "run", "--profile", str(profile_path_value),
                "--output-root", str(output_root),
            ])
        self.assertEqual(0, result)
        return profile_id, output_root / "run_result.json"

    def test_production_polygon_structure_avoids_fixture_quadratic_topology_check(self) -> None:
        ring = [[float(value), 0.0] for value in range(50_000)]
        ring.extend([[49_999.0, 1.0], [0.0, 1.0], [0.0, 0.0]])
        geometry = {"type": "Polygon", "coordinates": [ring]}
        self.assertIsNone(
            geometry_rejection_reason(
                geometry,
                "water",
                check_self_intersections=False,
            )
        )

    def test_synthetic_contract_proves_identity_boundary_rejection_and_overlays(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            result, root = self._compile(Path(directory) / "full")
            self.assertEqual("accepted", result["status"])
            coverage = read_json(root / "canonical" / "coverage.json")
            run_contract = read_json(root / "run_contract.json")
            metrics = read_json(root / "reports" / "metrics.json")
            provenance = read_json(root / "reports" / "provenance.json")
            rejections = read_json(root / "reports" / "rejections.json")["rejections"]
            self.assertEqual(2, len(coverage["cells"]))
            self.assertEqual(5, coverage["feature_count"])
            self.assertEqual(0.0, coverage["grid"]["vertical_origin_m"])
            self.assertEqual(1, len(rejections))
            self.assertEqual("way/999", rejections[0]["source_identity"])
            self.assertEqual(64, len(run_contract["execution_environment"]["identity_sha256"]))
            self.assertEqual(64, len(run_contract["execution_environment"]["toolchain_lock_sha256"]))
            self.assertEqual(2, len(provenance["cell_lineage"]))
            terrain_documents = [
                read_json(path)
                for path in sorted((root / "canonical" / "terrain").glob("*.json"))
            ]
            self.assertEqual(
                {"synthetic_two_cell_v1:63bc34749a6fe624"},
                {item["vertical_provenance"]["source_ref"] for item in terrain_documents},
            )
            self.assertTrue(all(
                item["vertical_provenance"]["sampling_quantization_residual_m"] == 0.05
                for item in terrain_documents
            ))
            lineage_fingerprints = {
                item[lineage_name]["source_area"]["area_fingerprint"]
                for item in coverage["cells"]
                for lineage_name in ("terrain_lineage", "feature_lineage")
            }
            self.assertEqual(lineage_fingerprints, set(coverage["area_fingerprints"]))
            self.assertTrue(all(
                any(value.startswith("source-snapshot:") for value in artifact["dependencies"])
                for artifact in coverage["artifact_descriptors"]
            ))
            self.assertLessEqual(metrics["duration_ms"], 60_000)
            self.assertLessEqual(metrics["canonical_bytes"], 64 * 1024 * 1024)
            features = self._features(root)
            road = next(item for item in features if item["feature_id"] == "alis:synthetic:way:100")
            self.assertEqual(2, len(road["representations"]))
            self.assertEqual(
                road["representations"][0]["geometry"]["coordinates"][-1],
                road["representations"][1]["geometry"]["coordinates"][0],
            )
            crossing = [item for item in features if item["feature_id"] == "alis:synthetic:way:202"]
            self.assertEqual(1, len(crossing))
            self.assertEqual(2, len(crossing[0]["intersecting_cell_ids"]))
            authored = next(item for item in features if item["feature_id"] == "alis:synthetic:way:203")
            self.assertEqual(12.0, authored["attributes"]["height_m"])
            self.assertEqual("authored", authored["attributes"]["height_basis"])
            terrain_paths = sorted((root / "canonical" / "terrain").glob("*.json"))
            west, east = (read_json(path) for path in terrain_paths)
            self.assertFalse(any(
                value is None
                for terrain in (west, east)
                for row in terrain["halo_window"]["samples"]
                for value in row
            ))
            self.assertEqual(west["height_edges"]["east"], east["height_edges"]["west"])
            self.assertEqual(west["height_border_bands"]["east"], east["height_border_bands"]["west"])
            self.assertIn("boundary_height_correction", west["authored_patch_ids"])
            self.assertIn("boundary_height_correction", east["authored_patch_ids"])

    def test_representative_profile_compiles_environment_semantics(self) -> None:
        with redirect_stdout(StringIO()):
            self.assertEqual(0, source_main([
                "run", "--profile",
                str(TEST_SOURCE_PROFILES / "synthetic_representative_v1.source.json"),
            ]))
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            result, root = compile_world(
                "synthetic_representative_v1",
                output_root_value=Path(directory) / "compiled",
            )
            self.assertEqual("accepted", result["status"])
            profile = load_profile(profile_path("synthetic_representative_v1"))
            self.assertEqual("grid_0daecb7b1297da7d", grid_id(profile["grid"]))
            features = self._features(root)
            by_id = {item["feature_id"]: item for item in features}
            pond = by_id["alis:synthetic:way:300"]
            self.assertEqual("water", pond["feature_class"])
            self.assertEqual("pond", pond["attributes"]["water_class"])
            self.assertEqual("polygon", pond["attributes"]["surface_geometry"])
            self.assertEqual("standing", pond["attributes"]["surface_behavior"])
            self.assertEqual(
                {"function_id": "standing_polygon_quantile", "function_version": 1},
                {
                    key: pond["attributes"]["surface_function"][key]
                    for key in ("function_id", "function_version")
                },
            )
            self.assertEqual("meadow", by_id["alis:synthetic:way:301"]["attributes"]["land_cover_class"])
            self.assertEqual("mixed", by_id["alis:synthetic:way:302"]["attributes"]["leaf_type"])
            self.assertEqual("foliage_point", by_id["alis:synthetic:node:303"]["representations"][0]["representation"])

    def test_full_compiles_are_d0_d1_and_d2_deterministic(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            first_root = Path(directory) / "first"
            second_root = Path(directory) / "second"
            self._compile(first_root)
            self._compile(second_root)
            excluded = {"metrics.json", "compile_result.json"}
            first_paths = sorted(
                path.relative_to(first_root)
                for path in first_root.rglob("*.json")
                if path.name not in excluded
            )
            second_paths = sorted(
                path.relative_to(second_root)
                for path in second_root.rglob("*.json")
                if path.name not in excluded
            )
            self.assertEqual(first_paths, second_paths)
            self.assertEqual(
                {path.as_posix(): file_hash(first_root / path) for path in first_paths},
                {path.as_posix(): file_hash(second_root / path) for path in second_paths},
            )
            first_coverage = read_json(first_root / "canonical" / "coverage.json")
            second_coverage = read_json(second_root / "canonical" / "coverage.json")
            self.assertEqual(first_coverage["semantic_hash"], second_coverage["semantic_hash"])
            self.assertEqual(
                [item["content_hash"] for item in first_coverage["artifact_descriptors"]],
                [item["content_hash"] for item in second_coverage["artifact_descriptors"]],
            )

    def test_incremental_run_reuses_west_and_rebuilds_east(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            base_root = Path(directory) / "base"
            partial_root = Path(directory) / "partial"
            self._compile(base_root)
            base_result = base_root / "compile_result.json"
            _, output = compile_world(
                "synthetic_two_cell",
                output_root_value=partial_root,
                base_result=base_result,
                terrain_change_bounds=[(600.0, 600.0, 800.0, 800.0)],
            )
            diff = read_json(output / "reports" / "diff.json")
            profile = load_profile(profile_path("synthetic_two_cell"))
            identifier = grid_id(profile["grid"])
            self.assertEqual([f"{identifier}:x1:y0"], diff["rebuilt_cells"])
            self.assertEqual([f"{identifier}:x0:y0"], diff["reused_cells"])
            self.assertEqual(
                read_json(base_root / "canonical" / "coverage.json")["semantic_hash"],
                read_json(partial_root / "canonical" / "coverage.json")["semantic_hash"],
            )
            self.assertEqual([f"{identifier}:x1:y0"], diff["terrain_rebuilt_cells"])
            self.assertEqual([], diff["feature_rebuilt_cells"])
            metrics = read_json(output / "reports" / "metrics.json")
            self.assertEqual(1, metrics["terrain_processed_cells"])
            self.assertEqual(0, metrics["feature_processed_count"])
            for folder in ("terrain", "features"):
                relative = Path("canonical") / folder / "cell_x0_y0.json"
                self.assertEqual(file_hash(base_root / relative), file_hash(output / relative))
            self.assertLessEqual(read_json(output / "reports" / "metrics.json")["duration_ms"], 30_000)

    def test_profile_growth_builds_new_cell_without_rebuilding_existing_cores(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base_root = root / "base"
            extended_root = root / "extended"
            self._compile(base_root)
            profile = deepcopy(load_profile(profile_path("synthetic_two_cell")))
            profile["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
            profile["target_cells"].append({"x": 2, "y": 0})
            extended_profile = root / "extended.compile.json"
            extended_profile.write_text(json.dumps(profile), encoding="utf-8")
            _, output = compile_world(
                str(extended_profile),
                output_root_value=extended_root,
                base_result=base_root / "compile_result.json",
            )
            metrics = read_json(output / "reports" / "metrics.json")
            diff = read_json(output / "reports" / "diff.json")
            self.assertEqual(1, metrics["terrain_processed_cells"])
            self.assertEqual(0, metrics["feature_processed_count"])
            self.assertEqual(1, len(diff["rebuilt_cells"]))
            self.assertEqual(2, len(diff["reused_cells"]))
            self.assertEqual(2, len(diff["manifest_updated_cells"]))
            for x_value in (0, 1):
                for folder in ("terrain", "features"):
                    relative = Path("canonical") / folder / f"cell_x{x_value}_y0.json"
                    self.assertEqual(file_hash(base_root / relative), file_hash(output / relative))
            old_east = read_json(base_root / "canonical" / "terrain" / "cell_x1_y0.json")
            new_cell = read_json(output / "canonical" / "terrain" / "cell_x2_y0.json")
            self.assertEqual(old_east["height_edges"]["east"], new_cell["height_edges"]["west"])
            self.assertNotEqual(
                file_hash(base_root / "canonical" / "cells" / "cell_x1_y0.json"),
                file_hash(output / "canonical" / "cells" / "cell_x1_y0.json"),
            )

    def test_missing_authored_target_fails_closed(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
        overlay = {
            "overlay_id": "missing-target",
            "terrain_patches": [],
            "feature_overrides": [{"feature_id": "alis:synthetic:way:404", "set": {"height_m": 1.0}}],
        }
        with self.assertRaisesRegex(CompilerError, "Authored overlay target is missing") as raised:
            compile_features(bundle, profile, grid_id(profile["grid"]), overlay)
        self.assertEqual("overlay_rebase_required", raised.exception.code)

    def test_authored_override_survives_provider_attribute_change(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
        changed_features = deepcopy(bundle.features)
        target = next(item for item in changed_features if item["provider_feature_id"] == "way/203")
        target["properties"]["height"] = "99"
        changed_bundle = replace(bundle, features=changed_features)
        overlay = read_json(REPO_ROOT / profile["authored_overlay"])
        compiled = compile_features(changed_bundle, profile, grid_id(profile["grid"]), overlay)
        authored = next(
            item
            for values in compiled.by_owner.values()
            for item in values
            if item["feature_id"] == "alis:synthetic:way:203"
        )
        self.assertEqual(12.0, authored["attributes"]["height_m"])
        self.assertEqual("authored", authored["attributes"]["height_basis"])

    def test_building_owner_is_independent_of_ring_vertex_density(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
        overlay = read_json(REPO_ROOT / profile["authored_overlay"])
        baseline = compile_features(bundle, profile, grid_id(profile["grid"]), overlay)
        changed_features = deepcopy(bundle.features)
        crossing = next(item for item in changed_features if item["provider_feature_id"] == "way/202")
        crossing["geometry"]["coordinates"][0].insert(1, [-0.1, 0.6])
        changed = compile_features(
            replace(bundle, features=changed_features), profile, grid_id(profile["grid"]), overlay
        )
        baseline_feature = next(
            item for values in baseline.by_owner.values() for item in values if item["feature_id"].endswith("way:202")
        )
        changed_feature = next(
            item for values in changed.by_owner.values() for item in values if item["feature_id"].endswith("way:202")
        )
        self.assertEqual(baseline_feature["owner_cell_id"], changed_feature["owner_cell_id"])

    def test_concave_building_uses_stable_cell_order_not_vertex_average(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
        changed_features = deepcopy(bundle.features)
        crossing = next(item for item in changed_features if item["provider_feature_id"] == "way/202")
        crossing["geometry"]["coordinates"] = [[
            [-0.8, 0.1], [0.8, 0.1], [0.8, 0.9], [0.6, 0.9],
            [0.6, 0.3], [-0.6, 0.3], [-0.6, 0.9], [-0.8, 0.9], [-0.8, 0.1],
        ]]
        overlay = read_json(REPO_ROOT / profile["authored_overlay"])
        compiled = compile_features(
            replace(bundle, features=changed_features), profile, grid_id(profile["grid"]), overlay
        )
        feature = next(
            item for values in compiled.by_owner.values() for item in values if item["feature_id"].endswith("way:202")
        )
        self.assertTrue(feature["owner_cell_id"].endswith(":x0:y0"))
        self.assertEqual(2, len(feature["intersecting_cell_ids"]))

    def test_incremental_run_never_processes_unselected_cell_or_vectors(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            base_root = Path(directory) / "base"
            partial_root = Path(directory) / "partial"
            self._compile(base_root)
            with (
                patch(
                    "World.CanonicalCompilation.app.pipeline.build_terrain_cells",
                    wraps=build_terrain_cells_real,
                ) as terrain_call,
                patch(
                    "World.CanonicalCompilation.app.pipeline.compile_features",
                    side_effect=AssertionError("terrain-only rebuild processed vectors"),
                ),
            ):
                compile_world(
                    "synthetic_two_cell",
                    output_root_value=partial_root,
                    base_result=base_root / "compile_result.json",
                    terrain_change_bounds=[(600.0, 600.0, 800.0, 800.0)],
                )
            self.assertEqual([(1, 0)], terrain_call.call_args.args[4])

    def test_feature_change_rebuilds_old_and_new_owner_cells_only(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            base_root = Path(directory) / "base"
            partial_root = Path(directory) / "partial"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            changed_features = deepcopy(bundle.features)
            moved = next(item for item in changed_features if item["provider_feature_id"] == "way/203")
            moved["geometry"]["coordinates"] = [[
                [-0.75, 0.15], [-0.55, 0.15], [-0.55, 0.35], [-0.75, 0.35], [-0.75, 0.15]
            ]]
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=replace(bundle, features=changed_features),
            ):
                _, output = compile_world(
                    "synthetic_two_cell",
                    output_root_value=partial_root,
                    base_result=base_root / "compile_result.json",
                    feature_change_ids=["way/203"],
                )
            diff = read_json(output / "reports" / "diff.json")
            metrics = read_json(output / "reports" / "metrics.json")
            self.assertEqual(2, len(diff["feature_rebuilt_cells"]))
            self.assertEqual([], diff["terrain_rebuilt_cells"])
            self.assertEqual(0, metrics["terrain_processed_cells"])
            self.assertEqual(1, metrics["feature_processed_count"])
            moved_output = next(item for item in self._features(output) if item["feature_id"].endswith("way:203"))
            self.assertTrue(moved_output["owner_cell_id"].endswith(":x0:y0"))

    def test_chained_local_feature_updates_keep_cell_lineage_reusable(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base_root = root / "base"
            first_root = root / "first"
            noop_root = root / "noop"
            second_root = root / "second"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            first_features = deepcopy(bundle.features)
            next(item for item in first_features if item["provider_feature_id"] == "way/204")["properties"][
                "building:levels"
            ] = "3"
            first_bundle = replace(bundle, features=first_features)
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=first_bundle,
            ):
                compile_world(
                    "synthetic_two_cell",
                    output_root_value=first_root,
                    base_result=base_root / "compile_result.json",
                    feature_change_ids=["way/204"],
                )
                compile_world(
                    "synthetic_two_cell",
                    output_root_value=noop_root,
                    base_result=first_root / "compile_result.json",
                )
            first_diff = read_json(first_root / "reports" / "diff.json")
            noop_metrics = read_json(noop_root / "reports" / "metrics.json")
            self.assertEqual(1, len(first_diff["feature_rebuilt_cells"]))
            self.assertEqual(0, noop_metrics["terrain_processed_cells"])
            self.assertEqual(0, noop_metrics["feature_processed_count"])
            self.assertEqual(
                read_json(first_root / "canonical" / "coverage.json")["semantic_hash"],
                read_json(noop_root / "canonical" / "coverage.json")["semantic_hash"],
            )
            feature_lineages = {
                read_json(path)["feature_lineage"]["source_semantic_contract_sha256"]
                for path in (noop_root / "canonical" / "cells").glob("*.json")
            }
            self.assertEqual(1, len(feature_lineages))
            second_features = deepcopy(first_features)
            next(item for item in second_features if item["provider_feature_id"] == "way/201")["properties"][
                "building:levels"
            ] = "4"
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=replace(bundle, features=second_features),
            ):
                compile_world(
                    "synthetic_two_cell",
                    output_root_value=second_root,
                    base_result=noop_root / "compile_result.json",
                    feature_change_ids=["way/201"],
                )
            self.assertEqual(
                1,
                len(read_json(second_root / "reports" / "diff.json")["feature_rebuilt_cells"]),
            )

    def test_overlay_delta_is_derived_without_operator_change_flags(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory, tempfile.TemporaryDirectory(
            dir=CANONICAL_ROOT / "fixtures"
        ) as overlay_directory:
            root = Path(directory)
            base_root = root / "base"
            self._compile(base_root)
            profile = deepcopy(load_profile(profile_path("synthetic_two_cell")))
            profile["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
            overlay = read_json(REPO_ROOT / profile["authored_overlay"])
            overlay["$schema"] = "https://alis.world/schemas/world-compiler/authored-overlay-v1.json"
            overlay["terrain_patches"][0]["delta_m"] = 2.0
            overlay["feature_overrides"][0]["set"]["height_m"] = 13.0
            overlay_path = Path(overlay_directory) / "changed_overlay.json"
            overlay_path.write_text(json.dumps(overlay), encoding="utf-8")
            profile["authored_overlay"] = overlay_path.relative_to(REPO_ROOT).as_posix()
            changed_profile = root / "changed.compile.json"
            changed_profile.write_text(json.dumps(profile), encoding="utf-8")
            _, output = compile_world(
                str(changed_profile),
                output_root_value=root / "changed",
                base_result=base_root / "compile_result.json",
            )
            metrics = read_json(output / "reports" / "metrics.json")
            contract = read_json(output / "run_contract.json")
            self.assertEqual(2, metrics["terrain_processed_cells"])
            self.assertEqual(1, metrics["feature_processed_count"])
            self.assertEqual(["way/203"], contract["authoritative_change_scope"]["provider_feature_ids"])
            authored = next(item for item in self._features(output) if item["feature_id"].endswith("way:203"))
            self.assertEqual(13.0, authored["attributes"]["height_m"])

    def test_changed_provider_snapshot_rebuilds_features_but_reuses_terrain(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base_root = root / "base"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            changed_features = deepcopy(bundle.features)
            next(item for item in changed_features if item["provider_feature_id"] == "way/201")["properties"][
                "building:levels"
            ] = "7"
            changed_ledger = deepcopy(bundle.ledger)
            changed_ledger["snapshots"][0]["hashes"]["sha256"] = "f" * 64
            changed_result = deepcopy(bundle.result)
            changed_result["inputs_hash"] = "e" * 64
            changed_result_path = root / "changed_source_result.json"
            changed_result_path.write_text(json.dumps(changed_result), encoding="utf-8")
            changed_bundle = replace(
                bundle,
                result_path=changed_result_path,
                result=changed_result,
                ledger=changed_ledger,
                features=changed_features,
            )
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=changed_bundle,
            ):
                _, output = compile_world(
                    "synthetic_two_cell",
                    output_root_value=root / "changed",
                    base_result=base_root / "compile_result.json",
                    feature_change_ids=["way/203"],
                )
            metrics = read_json(output / "reports" / "metrics.json")
            contract = read_json(output / "run_contract.json")
            self.assertEqual("reused", contract["base_reuse"]["decision"])
            self.assertEqual("reused", contract["base_reuse"]["layers"]["terrain"]["decision"])
            self.assertEqual("full_rebuild", contract["base_reuse"]["layers"]["feature"]["decision"])
            self.assertIn("source_snapshots", contract["base_reuse"]["reasons"])
            self.assertEqual(0, metrics["terrain_processed_cells"])
            self.assertEqual(6, metrics["feature_processed_count"])

    def test_changed_single_raster_rebuilds_terrain_but_reuses_features(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base_root = root / "base"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            changed_raster = deepcopy(bundle.raster)
            changed_raster["provider_payload"]["samples"][0][0] += 1.0
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=replace(bundle, raster=changed_raster),
            ):
                _, output = compile_world(
                    "synthetic_two_cell",
                    output_root_value=root / "changed",
                    base_result=base_root / "compile_result.json",
                )
            metrics = read_json(output / "reports" / "metrics.json")
            contract = read_json(output / "run_contract.json")
            self.assertEqual("full_rebuild", contract["base_reuse"]["layers"]["terrain"]["decision"])
            self.assertEqual("reused", contract["base_reuse"]["layers"]["feature"]["decision"])
            self.assertEqual(2, metrics["terrain_processed_cells"])
            self.assertEqual(0, metrics["feature_processed_count"])

    def test_same_area_source_result_change_uses_declared_layer_scopes(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            base_root = root / "base"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            changed_result = deepcopy(bundle.result)
            changed_result["inputs_hash"] = "c" * 64
            changed_result_path = root / "changed_source_result.json"
            changed_result_path.write_text(json.dumps(changed_result), encoding="utf-8")
            changed_bundle = replace(
                bundle,
                result_path=changed_result_path,
                result=changed_result,
            )
            with patch(
                "World.CanonicalCompilation.app.pipeline.load_source_bundle",
                return_value=changed_bundle,
            ):
                _, output = compile_world(
                    "synthetic_two_cell",
                    output_root_value=root / "changed",
                    base_result=base_root / "compile_result.json",
                    terrain_change_bounds=[(600.0, 600.0, 800.0, 800.0)],
                    feature_change_ids=["way/203"],
                )
            metrics = read_json(output / "reports" / "metrics.json")
            contract = read_json(output / "run_contract.json")
            self.assertEqual("reused", contract["base_reuse"]["decision"])
            self.assertTrue(contract["authoritative_change_scope"]["source_result_changed"])
            self.assertEqual(1, metrics["terrain_processed_cells"])
            self.assertEqual(1, metrics["feature_processed_count"])

    def test_accepted_source_growth_verifies_existing_feature_cells(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            suffix = str(os.getpid())
            base_source, base_source_result = self._accepted_source(
                root, f"synthetic_growth_base_{suffix}", [-1.5, -0.5, 0.6, 1.5]
            )
            current_source, current_source_result = self._accepted_source(
                root, f"synthetic_growth_current_{suffix}", [-1.5, -0.5, 2.5, 1.5]
            )
            base_profile = deepcopy(load_profile(profile_path("synthetic_two_cell")))
            base_profile["$schema"] = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"
            base_profile["profile_id"] = "synthetic_overlap_growth"
            base_profile["source_profile_id"] = base_source
            base_profile["source_profile"] = (
                root / f"{base_source}.source.json"
            ).relative_to(REPO_ROOT).as_posix()
            base_profile_path = root / "base.compile.json"
            base_profile_path.write_text(json.dumps(base_profile), encoding="utf-8")
            current_profile = deepcopy(base_profile)
            current_profile["source_profile_id"] = current_source
            current_profile["source_profile"] = (
                root / f"{current_source}.source.json"
            ).relative_to(REPO_ROOT).as_posix()
            current_profile["target_cells"].append({"x": 2, "y": 0})
            current_profile_path = root / "current.compile.json"
            current_profile_path.write_text(json.dumps(current_profile), encoding="utf-8")
            _, base_root = compile_world(
                str(base_profile_path),
                source_result=base_source_result,
                output_root_value=root / "base",
            )
            self.assertFalse(any(item["feature_id"].endswith("way:204") for item in self._features(base_root)))
            _, output = compile_world(
                str(current_profile_path),
                source_result=current_source_result,
                output_root_value=root / "current",
                base_result=base_root / "compile_result.json",
            )
            contract = read_json(output / "run_contract.json")
            metrics = read_json(output / "reports" / "metrics.json")
            diff = read_json(output / "reports" / "diff.json")
            identifier = grid_id(current_profile["grid"])
            self.assertEqual("reused", contract["base_reuse"]["decision"])
            self.assertTrue(contract["authoritative_change_scope"]["source_overlap_verified"])
            self.assertEqual(1, metrics["terrain_processed_cells"])
            self.assertEqual(6, metrics["feature_processed_count"])
            self.assertEqual(
                [f"{identifier}:x1:y0", f"{identifier}:x2:y0"],
                diff["feature_rebuilt_cells"],
            )
            for x_value in (0, 1):
                relative = Path("canonical") / "terrain" / f"cell_x{x_value}_y0.json"
                self.assertEqual(file_hash(base_root / relative), file_hash(output / relative))
            west_features = Path("canonical/features/cell_x0_y0.json")
            self.assertEqual(file_hash(base_root / west_features), file_hash(output / west_features))
            self.assertTrue(any(item["feature_id"].endswith("way:204") for item in self._features(output)))

    def test_reuse_rejects_stable_contract_changes_per_layer(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            base_root = Path(directory) / "base"
            self._compile(base_root)
            documents = base_cells(base_root)
            first = next(iter(documents.values()))
            current = {
                "terrain": deepcopy(first["terrain_lineage"]),
                "feature": deepcopy(first["feature_lineage"]),
            }
            cases = (
                ("terrain", "profile_contract_sha256"),
                ("terrain", "compiler_contract_sha256"),
                ("terrain", "source_ingestion_contract_sha256"),
                ("terrain", "source_semantic_contract_sha256"),
                ("feature", "profile_contract_sha256"),
                ("feature", "compiler_contract_sha256"),
                ("feature", "source_ingestion_contract_sha256"),
                ("feature", "source_snapshots"),
            )
            for layer, field in cases:
                changed = deepcopy(documents)
                lineage = next(iter(changed.values()))[f"{layer}_lineage"]
                lineage[field] = [] if field == "source_snapshots" else "changed"
                decisions = reusable_layers(changed, current, True)
                self.assertFalse(decisions[layer][0], f"{layer}:{field}")
                self.assertIn(field, decisions[layer][1])

    def test_profile_contract_changes_invalidate_only_the_owning_layer(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            base_root = Path(directory) / "base"
            self._compile(base_root)
            profile = load_profile(profile_path("synthetic_two_cell"))
            bundle = load_source_bundle(REPO_ROOT, compilation_root(), profile)
            contract = read_json(base_root / "run_contract.json")
            overlay = read_json(REPO_ROOT / profile["authored_overlay"])
            documents = base_cells(base_root)

            feature_profile = deepcopy(profile)
            feature_profile["water_semantics"] = {"policy_id": "changed"}
            feature_decisions = reusable_layers(
                documents, build_lineages(feature_profile, bundle, contract, overlay), False
            )
            self.assertTrue(feature_decisions["terrain"][0])
            self.assertFalse(feature_decisions["feature"][0])

            terrain_profile = deepcopy(profile)
            terrain_profile["raster_sampling"] = {"bilinear_warp_scale": [0.5, 0.5]}
            terrain_decisions = reusable_layers(
                documents, build_lineages(terrain_profile, bundle, contract, overlay), False
            )
            self.assertFalse(terrain_decisions["terrain"][0])
            self.assertTrue(terrain_decisions["feature"][0])

    def test_vertical_origin_is_part_of_grid_identity(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        changed_grid = deepcopy(profile["grid"])
        changed_grid["vertical_origin_m"] = 1.0
        self.assertNotEqual(grid_id(profile["grid"]), grid_id(changed_grid))

    def test_engine_origin_is_not_part_of_grid_identity(self) -> None:
        profile = load_profile(profile_path("synthetic_two_cell"))
        original_grid_id = grid_id(profile["grid"])
        profile["engine_georeference_origin"] = [381409, 6185051]
        self.assertEqual(original_grid_id, grid_id(profile["grid"]))

    def test_explicit_source_profile_path_must_match_declared_identity(self) -> None:
        profile = deepcopy(load_profile(profile_path("synthetic_two_cell")))
        profile["source_profile"] = (
            "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_p0.source.json"
        )
        with self.assertRaisesRegex(CompilerError, "another profile") as raised:
            load_source_bundle(REPO_ROOT, compilation_root(), profile)
        self.assertEqual("source_profile_mismatch", raised.exception.code)

    def test_failed_compile_never_promotes_partial_output(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            output = Path(directory) / "failed"
            with patch(
                "World.CanonicalCompilation.app.pipeline.build_terrain_cells",
                side_effect=CompilerError("injected_failure", "Injected failure"),
            ):
                with self.assertRaises(CompilerError):
                    self._compile(output)
            self.assertFalse(output.exists())
            self.assertFalse(list(output.parent.glob(output.name + ".staging-*")))

    def test_cli_failure_is_structured_without_traceback(self) -> None:
        stderr = StringIO()
        with redirect_stderr(stderr):
            result = compiler_main(["run", "--profile", "missing_profile"])
        value = json.loads(stderr.getvalue())
        self.assertEqual(6, result)
        self.assertEqual("failed", value["status"])
        self.assertNotIn("Traceback", stderr.getvalue())

    def test_dry_run_validates_inputs_without_creating_compiler_output(self) -> None:
        stdout = StringIO()
        runs_root = REPO_ROOT / "tmp" / "world" / "canonical_compilation" / "runs"
        before = {path for path in runs_root.rglob("compile_result.json")}
        with redirect_stdout(stdout):
            result = compiler_main([
                "run", "--profile", str(TEST_COMPILER_PROFILES / "synthetic_two_cell.compile.json"),
                "--dry-run",
            ])
        after = {path for path in runs_root.rglob("compile_result.json")}
        self.assertEqual(0, result)
        self.assertEqual("plan", json.loads(stdout.getvalue())["operation"])
        self.assertEqual(before, after)

    def test_generated_tree_contains_no_raw_provider_payload(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            _, root = self._compile(Path(directory) / "full")
            forbidden = {".pbf", ".tif", ".zip", ".bz2", ".exe", ".dll"}
            self.assertFalse([path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in forbidden])
            for path in root.rglob("*.json"):
                if path.name != "compile_result.json":
                    validate_document(read_json(path), path)

    def test_every_canonical_json_declares_a_schema(self) -> None:
        for path in CANONICAL_ROOT.rglob("*.json"):
            with self.subTest(path=path):
                value = json.loads(path.read_text(encoding="utf-8"))
                self.assertIn("$schema", value)
                if path.parent.name != "contracts":
                    validate_document(value, path)


if __name__ == "__main__":
    unittest.main()
