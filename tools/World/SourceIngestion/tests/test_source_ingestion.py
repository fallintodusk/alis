from __future__ import annotations

import copy
import hashlib
import http.server
import json
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from argparse import Namespace
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
WORLD_ROOT = REPO_ROOT / "tools" / "World"
SOURCE_ROOT = WORLD_ROOT / "SourceIngestion"
ENVIRONMENT_ROOT = WORLD_ROOT / "ExecutionEnvironment"
TEST_PROFILE_ROOT = (
    REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
    / "Profiles" / "SourceIngestion"
)
sys.path.insert(0, str(TOOLS_ROOT))

from World.SourceIngestion.app.adapters import (
    _convert_demonstration_boundary,
    _convert_osmium_geojson,
    _data_artifact,
    _portable_metadata,
    _provider_feature_fingerprint,
    decode_sources,
)
from World.SourceIngestion.app.cli import _execute_profile, main
from World.SourceIngestion.app.acquisition import (
    _download,
    build_ledger,
    locate_sources,
    materialize_sources,
    source_cache_path,
    verify_source_file,
)
from World.SourceIngestion.app.contracts import (
    IngestionError,
    canonical_hash,
    file_hash,
    read_json,
    validate_document,
)
from World.SourceIngestion.app.profiles import area_fingerprint, build_plan, validate_profile
from World.SourceIngestion.app.run_identity import default_output_root, run_contract
from World.SourceIngestion.app.osm_building_relations import parse_building_relation_memberships
import World.ExecutionEnvironment.app.python_environment as python_environment
import World.SourceIngestion.bootstrap as bootstrap


class SourceIngestionTests(unittest.TestCase):
    def _profile(self, name: str) -> tuple[dict, Path]:
        path = (
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "Data"
            / "Profiles" / "SourceIngestion" / f"{name}.source.json"
            if name.startswith("kazan_")
            else TEST_PROFILE_ROOT / f"{name}.source.json"
        )
        return read_json(path), path

    def test_kazan_plan_is_anonymous_and_below_budget(self) -> None:
        profile, path = self._profile("kazan_p0")
        validate_profile(profile, path, REPO_ROOT)
        plan = build_plan(profile)
        self.assertEqual(793803271, plan["planned_network_bytes"])
        self.assertLess(plan["planned_network_bytes"], plan["network_budget_bytes"])
        self.assertEqual({"anonymous_public_download"}, {item["auth_mode"] for item in plan["inputs"]})
        self.assertEqual(area_fingerprint(profile["area"]), plan["area"]["area_fingerprint"])

    def test_area_identity_uses_coordinates_not_human_names(self) -> None:
        profile, path = self._profile("kazan_p0")
        validate_profile(profile, path, REPO_ROOT)
        original = area_fingerprint(profile["area"])
        renamed = copy.deepcopy(profile["area"])
        renamed["area_id"] = "different_human_alias"
        renamed["label"] = "Different human label"
        resized = copy.deepcopy(profile["area"])
        resized["bbox"][2] += 0.001
        self.assertEqual(original, area_fingerprint(renamed))
        self.assertNotEqual(original, area_fingerprint(resized))
        resized_profile = copy.deepcopy(profile)
        resized_profile["area"] = resized
        self.assertNotEqual(
            default_output_root(REPO_ROOT, profile, canonical_hash(profile)),
            default_output_root(REPO_ROOT, resized_profile, canonical_hash(resized_profile)),
        )

    def test_run_contract_changes_with_every_pinned_execution_input(self) -> None:
        profile, _ = self._profile("kazan_p0")
        with tempfile.TemporaryDirectory() as directory:
            fake_repo = Path(directory) / "repo"
            fake_source = fake_repo / "tools" / "World" / "SourceIngestion"
            fake_environment = fake_repo / "tools" / "World" / "ExecutionEnvironment"
            shutil.copytree(SOURCE_ROOT, fake_source, ignore=shutil.ignore_patterns("__pycache__"))
            shutil.copytree(ENVIRONMENT_ROOT, fake_environment, ignore=shutil.ignore_patterns("__pycache__"))
            initial = run_contract(fake_repo, profile)
            tool_lock = fake_environment / "toolchain.lock.json"
            tool_lock.write_text(tool_lock.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            tool_changed = run_contract(fake_repo, profile)
            requirements = fake_environment / "requirements.lock.txt"
            requirements.write_text(requirements.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            dependencies_changed = run_contract(fake_repo, profile)
            implementation = fake_source / "app" / "adapters.py"
            implementation.write_text(implementation.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            implementation_changed = run_contract(fake_repo, profile)
            source_api = fake_source / "api.py"
            source_api.write_text(source_api.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            source_api_changed = run_contract(fake_repo, profile)
            environment_api = fake_environment / "api.py"
            environment_api.write_text(environment_api.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            environment_api_changed = run_contract(fake_repo, profile)
        hashes = {
            item["run_inputs_hash"]
            for item in (
                initial,
                tool_changed,
                dependencies_changed,
                implementation_changed,
                source_api_changed,
                environment_api_changed,
            )
        }
        self.assertEqual(6, len(hashes))
        self.assertIn(initial["run_inputs_hash"], default_output_root(
            REPO_ROOT, profile, initial["run_inputs_hash"], "decode"
        ).parts)

    def test_provider_boundary_identity_is_independent_of_acquisition_area(self) -> None:
        feature = {
            "provider_feature_id": "relation/79374",
            "geometry": {"type": "Polygon", "coordinates": [[[49.0, 55.0], [50.0, 55.0], [49.0, 55.0]]]},
        }
        fingerprint = _provider_feature_fingerprint({"snapshot_id": "snapshot-a"}, feature)
        self.assertNotEqual(area_fingerprint({
            "bbox": [49.08, 55.78, 49.13, 55.81],
            "axis_order": "longitude_latitude",
            "crs": "EPSG:4326",
        }), fingerprint)
        self.assertNotEqual(
            fingerprint,
            _provider_feature_fingerprint({"snapshot_id": "snapshot-b"}, feature),
        )

    def test_synthetic_decode_is_deterministic_and_preserves_identity(self) -> None:
        profile, path = self._profile("synthetic_two_cell")
        validate_profile(profile, path, REPO_ROOT)
        paths = locate_sources(profile, path, REPO_ROOT / "tmp" / "world" / "source_ingestion" / "cache")
        ledger = build_ledger(profile, paths)
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as first, tempfile.TemporaryDirectory(dir=temp_parent) as second:
            first_outputs = decode_sources(profile, paths, ledger, Path(first), REPO_ROOT)
            second_outputs = decode_sources(profile, paths, ledger, Path(second), REPO_ROOT)
            self.assertEqual([item["sha256"] for item in first_outputs], [item["sha256"] for item in second_outputs])
            manifest_path = next(Path(item["path"]) for item in first_outputs if item["kind"] == "provider_features_manifest")
            manifest = read_json(manifest_path)
            self.assertEqual(1, len(manifest["shards"]))
            feature_collection = read_json(manifest_path.parent / manifest["shards"][0]["path"])
            features = feature_collection["features"]
            self.assertEqual(5, len(features))
            self.assertIn("way/100", {item["provider_feature_id"] for item in features})
            self.assertEqual(4, sum(item["provider_class"] == "building" for item in features))
            self.assertEqual(ledger["area"]["area_fingerprint"], feature_collection["area"]["area_fingerprint"])
            raster_path = next(Path(REPO_ROOT / item["path"]) for item in first_outputs if item["kind"] == "raster_layer")
            raster = read_json(raster_path)
            self.assertEqual(ledger["area"]["area_fingerprint"], raster["area"]["area_fingerprint"])
            self.assertEqual(ledger["snapshots"][0]["snapshot_id"], raster["vertical_provenance"]["source_ref"])
            self.assertEqual(0.0, raster["vertical_provenance"]["source_accuracy_m"])

    def test_representative_fixture_adds_provider_neutral_environment_classes(self) -> None:
        profile, path = self._profile("synthetic_representative_v1")
        validate_profile(profile, path, REPO_ROOT)
        paths = locate_sources(profile, path, REPO_ROOT / "tmp" / "world" / "source_ingestion" / "cache")
        ledger = build_ledger(profile, paths)
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            outputs = decode_sources(profile, paths, ledger, Path(directory), REPO_ROOT)
            manifest_path = next(Path(item["path"]) for item in outputs if item["kind"] == "provider_features_manifest")
            manifest = read_json(manifest_path)
            features = read_json(manifest_path.parent / manifest["shards"][0]["path"])["features"]
            self.assertEqual(9, len(features))
            self.assertEqual(
                {"water", "land_cover", "vegetation_area", "foliage_point"},
                {
                    item["provider_class"]
                    for item in features
                    if item["provider_feature_id"] in {"way/300", "way/301", "way/302", "node/303"}
                },
            )
            self.assertEqual(ledger["area"]["area_fingerprint"], read_json(manifest_path.parent / manifest["shards"][0]["path"])["area"]["area_fingerprint"])

    def test_corrupt_payload_is_rejected(self) -> None:
        profile, path = self._profile("synthetic_two_cell")
        source = profile["sources"][0]
        original = (path.parent / source["local_path"]).resolve().read_bytes()
        corrupt = bytearray(original)
        corrupt[-2] = ord("x") if corrupt[-2] != ord("x") else ord("y")
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            candidate = Path(directory) / source["file_name"]
            candidate.write_bytes(corrupt)
            with self.assertRaisesRegex(IngestionError, "SHA-256"):
                verify_source_file(source, candidate)

    def test_profile_rejects_missing_license_and_out_of_area(self) -> None:
        profile, path = self._profile("synthetic_two_cell")
        missing_license = copy.deepcopy(profile)
        missing_license["sources"][0]["license"] = {}
        with self.assertRaises(IngestionError) as context:
            validate_profile(missing_license, path, REPO_ROOT)
        self.assertEqual("contract_violation", context.exception.code)
        out_of_area = copy.deepcopy(profile)
        out_of_area["sources"][0]["coverage_bbox"] = [10.0, 10.0, 11.0, 11.0]
        with self.assertRaisesRegex(IngestionError, "intersect"):
            validate_profile(out_of_area, path, REPO_ROOT)

    def test_profile_rejects_unsupported_adapter_and_release(self) -> None:
        profile, path = self._profile("synthetic_two_cell")
        unsupported = copy.deepcopy(profile)
        unsupported["sources"][0]["adapter"] = "future_provider"
        with self.assertRaises(IngestionError) as context:
            validate_profile(unsupported, path, REPO_ROOT)
        self.assertEqual("contract_violation", context.exception.code)
        missing_release = copy.deepcopy(profile)
        missing_release["sources"][0]["release"] = ""
        with self.assertRaises(IngestionError) as context:
            validate_profile(missing_release, path, REPO_ROOT)
        self.assertEqual("contract_violation", context.exception.code)
        unsafe_name = copy.deepcopy(profile)
        unsafe_name["sources"][0]["file_name"] = "../source.json"
        with self.assertRaises(IngestionError) as context:
            validate_profile(unsafe_name, path, REPO_ROOT)
        self.assertEqual("contract_violation", context.exception.code)

    def test_profile_schema_rejects_missing_adapter_contract_and_unknown_fields(self) -> None:
        profile, path = self._profile("kazan_p0")
        missing_decode = copy.deepcopy(profile)
        del missing_decode["sources"][0]["decode"]
        with self.assertRaisesRegex(IngestionError, "declared schema"):
            validate_profile(missing_decode, path, REPO_ROOT)
        misspelled = copy.deepcopy(profile)
        misspelled["sources"][0]["deocde"] = misspelled["sources"][0].pop("decode")
        with self.assertRaisesRegex(IngestionError, "declared schema"):
            validate_profile(misspelled, path, REPO_ROOT)
        missing_boundary = copy.deepcopy(profile)
        del missing_boundary["area"]["demonstration_boundary"]
        with self.assertRaisesRegex(IngestionError, "declared schema"):
            validate_profile(missing_boundary, path, REPO_ROOT)

    def test_multiple_dem_sources_require_explicit_mosaic_contract(self) -> None:
        profile, path = self._profile("kazan_territory_v1")
        without_contract = copy.deepcopy(profile)
        without_contract.pop("raster_mosaic")
        with self.assertRaises(IngestionError) as context:
            validate_profile(without_contract, path, REPO_ROOT)
        self.assertEqual("raster_mosaic_contract_missing", context.exception.code)

    def test_invalid_profile_cli_emits_structured_failure(self) -> None:
        profile, _ = self._profile("kazan_p0")
        profile["$schema"] = "https://alis.world/schemas/world-source/source-profile-v1.json"
        del profile["sources"][0]["decode"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile_path = root / "invalid.source.json"
            profile_path.write_text(json.dumps(profile), encoding="utf-8")
            with redirect_stderr(StringIO()):
                exit_code = main(["plan", "--profile", str(profile_path), "--output-root", str(root / "output")])
            result = read_json(root / "output" / "plan_result.json")
        self.assertNotEqual(0, exit_code)
        self.assertEqual("rejected", result["status"])
        self.assertEqual("contract_violation", result["errors"][0]["code"])

    def test_external_output_root_uses_declared_relative_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result, output_root = _execute_profile(
                Namespace(operation="run", profile=str(self._profile("synthetic_two_cell")[1]), cache_root=str(root / "cache"), output_root=str(root / "output")),
                REPO_ROOT,
            )
            self.assertEqual(root / "output", output_root)
            self.assertEqual("output_root", result["path_base"])
            self.assertTrue(all(not Path(item["path"]).is_absolute() for item in result["outputs"] if "path" in item))
            self.assertTrue(all(
                "sha256" in item and "byte_size" in item
                for item in result["outputs"] if "path" in item
            ))
            self.assertTrue((output_root / "run_result.json").is_file())

    def test_accepted_source_run_is_adopted_and_changed_bytes_conflict(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments = Namespace(
                operation="run",
                profile=str(self._profile("synthetic_two_cell")[1]),
                cache_root=str(root / "cache"),
                output_root=str(root / "output"),
            )
            first, output_root = _execute_profile(arguments, REPO_ROOT)
            with patch(
                "World.SourceIngestion.app.cli._run_profile_operation",
                side_effect=AssertionError("accepted run must be adopted"),
            ):
                adopted, _ = _execute_profile(arguments, REPO_ROOT)
            self.assertEqual(first, adopted)
            plan_path = output_root / "plan.json"
            changed = plan_path.read_bytes() + b"\n"
            plan_path.write_bytes(changed)
            with self.assertRaises(IngestionError) as context:
                _execute_profile(arguments, REPO_ROOT)
            self.assertEqual("output_identity_conflict", context.exception.code)
            self.assertEqual(changed, plan_path.read_bytes())

    def test_failure_receipt_replaces_stale_commit_marker_beside_partial_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output_root = Path(directory) / "output"
            output_root.mkdir()
            marker = output_root / "run_result.json"
            marker.write_text("stale", encoding="utf-8")

            def fail_after_partial(*args: object, **kwargs: object) -> dict:
                self.assertFalse(marker.exists())
                (output_root / "partial.bin").write_bytes(b"partial")
                raise IngestionError("forced_failure", "Forced operation failure")

            with patch("World.SourceIngestion.app.cli._run_profile_operation", side_effect=fail_after_partial):
                with redirect_stderr(StringIO()):
                    exit_code = main([
                        "run", "--profile", str(self._profile("synthetic_two_cell")[1]), "--output-root", str(output_root)
                    ])
            result = read_json(marker)
        self.assertNotEqual(0, exit_code)
        self.assertEqual("rejected", result["status"])
        self.assertIsNotNone(result["inputs_hash"])

    def test_raster_artifact_reference_is_relative_and_escape_checked(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = root / "normalized" / "raster.json"
            document.parent.mkdir()
            payload = document.parent / "terrain.tif"
            payload.write_bytes(b"cog")
            artifact = _data_artifact(document, payload, "COG")
            self.assertEqual("terrain.tif", artifact["path"])
            self.assertEqual("document_parent", artifact["path_base"])
            with self.assertRaises(IngestionError):
                _data_artifact(document, root / "outside.tif", "COG")

    def test_python_bootstrap_failure_is_structured_without_traceback(self) -> None:
        failure = python_environment.BootstrapFailure("synthetic_bootstrap_failure", "Safe bootstrap failure", stage="test")
        stderr = StringIO()
        with patch.object(bootstrap, "launch_cli", side_effect=failure), redirect_stderr(stderr):
            exit_code = bootstrap.main(["run", "--profile", "kazan_p0"])
        payload = json.loads(stderr.getvalue())
        self.assertEqual(python_environment.BOOTSTRAP_FAILURE_EXIT, exit_code)
        self.assertEqual("failed", payload["status"])
        self.assertEqual("bootstrap-python", payload["operation"])
        self.assertEqual("synthetic_bootstrap_failure", payload["error"]["code"])
        self.assertNotIn("Traceback", stderr.getvalue())

    def test_provider_metadata_removes_machine_paths_but_preserves_urls(self) -> None:
        value = _portable_metadata({"file": {"name": r"E:\\cache\\source.pbf"}, "url": "https://example.test/a"})
        self.assertEqual("source.pbf", value["file"]["name"])
        self.assertEqual("https://example.test/a", value["url"])

    def test_range_ignored_by_server_restarts_staging_file(self) -> None:
        payload = b"provider-payload"

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self) -> None:
                self.send_response(200)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, format: str, *args: object) -> None:
                return

        with socketserver.TCPServer(("127.0.0.1", 0), Handler) as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
            temp_parent.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
                target = Path(directory) / "source.bin"
                target.with_suffix(".bin.partial").write_bytes(b"stale")
                source = {
                    "source_id": "range_test",
                    "url": f"http://127.0.0.1:{server.server_address[1]}/source.bin",
                    "expected_bytes": len(payload),
                    "hashes": {"sha256": hashlib.sha256(payload).hexdigest()},
                }
                _download(source, target)
                self.assertEqual(payload, target.read_bytes())
                self.assertEqual(source["hashes"]["sha256"], file_hash(target))
            server.shutdown()

    def test_content_addressed_cache_serializes_concurrent_materialization(self) -> None:
        payload = b"shared-provider-payload"
        requests = 0
        request_guard = threading.Lock()

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self) -> None:
                nonlocal requests
                with request_guard:
                    requests += 1
                time.sleep(0.1)
                self.send_response(200)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, format: str, *args: object) -> None:
                return

        with socketserver.TCPServer(("127.0.0.1", 0), Handler) as server, tempfile.TemporaryDirectory() as directory:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            source = {
                "source_id": "shared_source", "adapter": "geofabrik_osm", "file_name": "source.bin",
                "url": f"http://127.0.0.1:{server.server_address[1]}/source.bin", "expected_bytes": len(payload),
                "hashes": {"sha256": hashlib.sha256(payload).hexdigest()},
            }
            profile = {"sources": [source]}
            cache_root = Path(directory) / "cache"
            paths: list[Path] = []
            errors: list[Exception] = []

            def materialize() -> None:
                try:
                    paths.append(materialize_sources(profile, SOURCE_ROOT / "profiles" / "unused.json", REPO_ROOT, cache_root)["shared_source"])
                except Exception as error:
                    errors.append(error)

            workers = [threading.Thread(target=materialize) for _ in range(2)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join()
            server.shutdown()
        self.assertFalse(errors)
        self.assertEqual(1, requests)
        self.assertEqual([source_cache_path(cache_root, source)] * 2, paths)

    def test_osmium_area_id_does_not_replace_provider_relation_identity(self) -> None:
        raw = {
            "type": "FeatureCollection",
            "features": [{
                "type": "Feature",
                "id": "a158749",
                "geometry": {"type": "Polygon", "coordinates": []},
                "properties": {"@type": "relation", "@id": 79374, "boundary": "administrative"},
            }],
        }
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            path = Path(directory) / "boundary.geojson"
            path.write_text(json.dumps(raw), encoding="utf-8")
            value = _convert_osmium_geojson(
                path,
                {"snapshot_id": "snapshot"},
                {"provider": "OSM", "release": "1", "license": {"id": "ODbL-1.0"}},
                {"area_id": "boundary"},
            )
        self.assertEqual("relation/79374", value["features"][0]["provider_feature_id"])

    def test_demonstration_boundary_is_selected_by_profile_not_one_region_literal(self) -> None:
        raw = {
            "type": "FeatureCollection",
            "features": [{
                "type": "Feature",
                "geometry": {"type": "Polygon", "coordinates": []},
                "properties": {
                    "@type": "relation",
                    "@id": 61320,
                    "boundary": "administrative",
                    "ISO3166-2": "US-NY",
                },
            }],
        }
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            path = Path(directory) / "boundary.geojson"
            path.write_text(json.dumps(raw), encoding="utf-8")
            value = _convert_demonstration_boundary(
                path,
                {"snapshot_id": "snapshot"},
                {"provider": "OSM", "release": "1", "license": {"id": "ODbL-1.0"}},
                {"area_id": "new_york_state", "selection": "r/ISO3166-2=US-NY"},
            )
        self.assertEqual("US-NY", value["features"][0]["properties"]["ISO3166-2"])

    def test_osmium_tags_map_to_provider_neutral_world_classes(self) -> None:
        tags = [
            ("way/1", {"natural": "water"}, "water"),
            ("way/2", {"landuse": "meadow"}, "land_cover"),
            ("way/3", {"natural": "wood"}, "vegetation_area"),
            ("node/4", {"natural": "tree"}, "foliage_point"),
            ("way/5", {"building:part": "yes", "height": "24"}, "building"),
        ]
        raw = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "id": provider_id,
                    "geometry": {"type": "Point", "coordinates": [49.1, 55.8]},
                    "properties": {
                        "@type": provider_id.split("/")[0],
                        "@id": int(provider_id.split("/")[1]),
                        **properties,
                    },
                }
                for provider_id, properties, _ in tags
            ],
        }
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            path = Path(directory) / "classes.geojson"
            path.write_text(json.dumps(raw), encoding="utf-8")
            value = _convert_osmium_geojson(
                path,
                {"snapshot_id": "snapshot"},
                {"provider": "OSM", "release": "1", "license": {"id": "ODbL-1.0"}},
                {"area_id": "classes"},
            )
        self.assertEqual(
            {provider_id: provider_class for provider_id, _, provider_class in tags},
            {item["provider_feature_id"]: item["provider_class"] for item in value["features"]},
        )

    def test_osmium_duplicate_closed_way_selects_semantic_area_representation(self) -> None:
        properties = {"@type": "way", "@id": 7, "building": "yes"}
        raw = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "geometry": {"type": "LineString", "coordinates": [[0, 0], [1, 0], [0, 0]]},
                    "properties": properties,
                },
                {
                    "type": "Feature",
                    "geometry": {
                        "type": "MultiPolygon",
                        "coordinates": [[[[0, 0], [1, 0], [0, 1], [0, 0]]]],
                    },
                    "properties": properties,
                },
            ],
        }
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            path = Path(directory) / "duplicate.geojson"
            path.write_text(json.dumps(raw), encoding="utf-8")
            value = _convert_osmium_geojson(
                path,
                {"snapshot_id": "snapshot"},
                {"provider": "OSM", "release": "1", "license": {"id": "ODbL-1.0"}},
                {"area_id": "duplicate"},
            )
        self.assertEqual(1, len(value["features"]))
        self.assertEqual("MultiPolygon", value["features"][0]["geometry"]["type"])

    def test_building_relation_memberships_preserve_only_selected_outline_and_parts(self) -> None:
        document = """<osm version=\"0.6\">
          <relation id=\"90\">
            <member type=\"way\" ref=\"10\" role=\"outline\"/>
            <member type=\"way\" ref=\"11\" role=\"part\"/>
            <member type=\"node\" ref=\"12\" role=\"address\"/>
            <tag k=\"type\" v=\"building\"/>
          </relation>
        </osm>"""
        temp_parent = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "tests"
        temp_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temp_parent) as directory:
            path = Path(directory) / "relations.osm"
            path.write_text(document, encoding="ascii")
            memberships = parse_building_relation_memberships(path, {"way/10", "way/11"})
        self.assertEqual(
            {
                "way/10": [{"provider_relation_id": "relation/90", "relation_type": "building", "role": "outline"}],
                "way/11": [{"provider_relation_id": "relation/90", "relation_type": "building", "role": "part"}],
            },
            memberships,
        )

    def test_tracked_tree_contains_no_provider_payload_or_tool_binary(self) -> None:
        forbidden_suffixes = {".pbf", ".tif", ".exe", ".dll", ".zip", ".bz2"}
        tracked = shutil.which("git")
        self.assertIsNotNone(tracked)
        result = subprocess.run(
            [tracked, "ls-files", "--", "tools/World"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertFalse([path for path in result.stdout.splitlines() if Path(path).suffix.lower() in forbidden_suffixes])

    def test_every_committed_json_declares_a_schema(self) -> None:
        for path in SOURCE_ROOT.rglob("*.json"):
            with self.subTest(path=path):
                value = json.loads(path.read_text(encoding="utf-8"))
                self.assertIn("$schema", value)
                if path.parent.name != "contracts":
                    validate_document(value, path)


if __name__ == "__main__":
    unittest.main()
