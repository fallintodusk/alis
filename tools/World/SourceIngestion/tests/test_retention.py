"""Raw retention uses real files; no provider URLs or production storage."""
import hashlib
import importlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from argparse import Namespace
from contextlib import redirect_stderr
from io import StringIO

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.SourceIngestion.app.contracts import IngestionError
from World.SourceIngestion.app.acquisition import materialize_sources, source_cache_path

MODULE = "World.SourceIngestion.retention.api"


class RetentionTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / "tmp/world/source_retention_tests"
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.payload = self.root / "source.pbf"
        self.payload.write_bytes(b"verified immutable provider bytes")
        self.source = {"adapter": "osm_pbf", "source_id": "fixture", "file_name": "snapshot.osm.pbf",
                       "expected_bytes": self.payload.stat().st_size,
                       "hashes": {"sha256": hashlib.sha256(self.payload.read_bytes()).hexdigest()}}
        self.profile = {"sources": [self.source]}
        self.config_path = self.repo / "scripts/config/world_source_storage.local.json"
        self.config_path.parent.mkdir(parents=True)
        self.config = {"$schema": "https://alis.world/schemas/world-source/source-storage-v1.json", "schema_version": 1,
                       "primary_root": str(self.root / "primary"), "replica_root": str(self.root / "replica")}
        self.config_path.write_text(json.dumps(self.config))

    def api(self):
        self.assertIsNotNone(importlib.util.find_spec(MODULE), "Successful ingestion needs durable verified raw retention")
        return importlib.import_module(MODULE)

    def test_retained_bytes_survive_cache_loss_and_recover_without_network(self):
        operation = self.api()
        operation.retain_sources(self.repo, self.profile, {"fixture": self.payload})
        for key in ("primary_root", "replica_root"):
            self.assertEqual(self.payload.read_bytes(), source_cache_path(Path(self.config[key]), self.source).read_bytes())
        source_cache_path(Path(self.config["primary_root"]), self.source).write_bytes(b"corrupt")
        with patch("World.SourceIngestion.app.acquisition._download", side_effect=AssertionError("Network forbidden")):
            paths = materialize_sources(self.profile, self.repo / "profile.json", self.repo, self.repo / "tmp/cache")
        self.assertEqual(self.payload.read_bytes(), paths["fixture"].read_bytes())
        operation.retain_sources(self.repo, self.profile, paths)
        self.assertEqual(self.payload.read_bytes(), source_cache_path(Path(self.config["primary_root"]), self.source).read_bytes())

    def test_missing_config_overlapping_storage_and_corrupt_payload_refuse(self):
        operation = self.api()
        self.config_path.unlink()
        with self.assertRaises(IngestionError):
            operation.retain_sources(self.repo, self.profile, {"fixture": self.payload})
        for invalid in (self.config["primary_root"], str(self.repo / "tmp/replica")):
            self.config["replica_root"] = invalid
            self.config_path.write_text(json.dumps(self.config))
            with self.assertRaises(IngestionError):
                operation.retain_sources(self.repo, self.profile, {"fixture": self.payload})
        self.config["replica_root"] = str(self.root / "replica")
        self.config_path.write_text(json.dumps(self.config))
        self.payload.write_bytes(b"wrong")
        with self.assertRaises(IngestionError):
            operation.retain_sources(self.repo, self.profile, {"fixture": self.payload})
        self.assertFalse((self.root / "primary").exists())

    def test_replica_failure_cannot_publish_accepted_ingestion(self):
        self.api()
        from World.SourceIngestion.app.cli import ProfileContext, _execute_profile, _result
        cache = self.repo / "tmp/cache"
        cached = source_cache_path(cache, self.source)
        cached.parent.mkdir(parents=True)
        cached.write_bytes(self.payload.read_bytes())
        output = self.repo / "tmp/accepted"
        profile = {**self.profile, "profile_id": "fixture"}
        context = ProfileContext(self.repo / "profile.json", profile, {"run_inputs_hash": "0" * 64}, cache, output)
        Path(self.config["replica_root"]).write_text("unavailable storage")
        result = _result("run", "accepted", "fixture", "0" * 64, [])
        with patch("World.SourceIngestion.app.cli._run_profile_operation", return_value=result):
            with self.assertRaises(OSError):
                _execute_profile(Namespace(operation="run"), self.repo, context)
        self.assertFalse(output.exists())
        self.assertEqual([], list(output.parent.glob("accepted.staging-*")))

    def test_missing_storage_refuses_before_acquisition(self):
        self.api()
        from World.SourceIngestion.app.cli import ProfileContext, _execute_profile
        self.config_path.unlink()
        context = ProfileContext(self.repo / "profile.json", self.profile, {"run_inputs_hash": "0" * 64},
                                 self.repo / "tmp/cache", self.repo / "tmp/accepted")
        with patch("World.SourceIngestion.app.cli._run_profile_operation", side_effect=AssertionError("Must refuse first")):
            with self.assertRaises(IngestionError):
                _execute_profile(Namespace(operation="run"), self.repo, context)
        self.assertFalse(context.output_root.parent.exists())

    def test_archive_cache_alias_refuses_before_lock_or_network(self):
        self.api()
        with patch("World.SourceIngestion.app.acquisition._download", side_effect=AssertionError("Must refuse first")):
            with self.assertRaises(IngestionError) as failure:
                materialize_sources(self.profile, self.repo / "profile.json", self.repo, Path(self.config["primary_root"]))
        self.assertEqual("source_cache_storage_overlap", failure.exception.code)
        self.assertFalse(Path(self.config["primary_root"]).exists())

    def test_cached_verify_is_offline_and_preserves_historical_marker_on_refusal(self):
        self.api()
        from World.SourceIngestion.app.cli import ProfileContext, _result, main
        from World.SourceIngestion.app.contracts import write_json
        output = self.repo / "tmp/accepted"
        profile = {**self.profile, "profile_id": "fixture"}
        context = ProfileContext(self.repo / "profile.json", profile, {"run_inputs_hash": "0" * 64}, self.repo / "tmp/cache", output)
        marker = output / "verify_result.json"
        write_json(marker, _result("verify", "accepted", "fixture", "0" * 64, []))
        original = marker.read_bytes()
        with patch("World.SourceIngestion.app.cli._repo_root", return_value=self.repo), \
                patch("World.SourceIngestion.app.cli._prepare_profile", return_value=context), \
                patch("World.SourceIngestion.app.cli.materialize_sources", side_effect=AssertionError("Verify must remain offline")), \
                redirect_stderr(StringIO()):
            self.assertEqual(3, main(["verify", "--profile", "fixture"]))
        self.assertEqual(original, marker.read_bytes())

    def test_unreadable_payload_is_not_absence_or_download_permission(self):
        self.api()
        from World.SourceIngestion.app.acquisition import verify_source_file
        with patch.object(Path, "stat", side_effect=PermissionError("unreadable")):
            with self.assertRaises(PermissionError):
                verify_source_file(self.source, self.payload)

    def test_source_payload_name_cannot_escape_its_hash_directory(self):
        for name in ("../escape.osm.pbf", "/escape.osm.pbf", "C:escape.osm.pbf"):
            with self.subTest(name=name), self.assertRaises(IngestionError):
                source_cache_path(self.root, {**self.source, "file_name": name})

    def test_synthetic_inputs_do_not_consume_provider_storage_configuration(self):
        self.config_path.write_text("invalid unused storage configuration")
        self.payload.write_bytes(b"{}")
        source = {**self.source, "adapter": "synthetic_json", "local_path": self.payload.name,
                  "expected_bytes": 2, "hashes": {"sha256": hashlib.sha256(b"{}").hexdigest()}}
        result = materialize_sources({"sources": [source]}, self.payload.parent / "profile.json", self.repo, self.repo / "tmp/cache")
        self.assertEqual(self.payload, result["fixture"])


if __name__ == "__main__":
    unittest.main()
