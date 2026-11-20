import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "refresh_developer_asset_authority.py"
SPEC = importlib.util.spec_from_file_location("refresh_developer_asset_authority", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


OWNERS = ("OwnerA", "OwnerB")


class RefreshAssetAuthorityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.contract_path = self.root / "scripts/git/mirror/developer_asset_release.json"
        self.contract_path.parent.mkdir(parents=True)
        self.inventory_path = self.root / "inventory.json"
        self.authorities = []
        self.manifests = {}
        self.sources = {}
        self.artifacts = {}
        for owner in OWNERS:
            authority = self.authority(owner)
            self.authorities.append(authority)
            self.manifests[owner] = self.root / authority["manifest_path"]
            self.manifests[owner].parent.mkdir(parents=True)
            collection = authority["collections"][0]
            self.sources[owner] = self.root / collection["source_root"] / "a.json"
            self.sources[owner].parent.mkdir(parents=True)
            self.artifacts[owner] = self.root / collection["artifact_root"] / "A.uasset"
            self.artifacts[owner].parent.mkdir(parents=True)
            self.write_source(owner)
            self.write_artifact(owner)
        self.write_contract()
        self.write_inventory()

    def tearDown(self):
        self.temp.cleanup()

    def authority(self, owner: str) -> dict:
        return {
            "authority_kind": "generated_definition_manifest",
            "owner": owner,
            "manifest_path": f"Plugins/Resources/{owner}/Data/Manifests/public_generated.json",
            "manifest_schema_ref": "../Schemas/public_generated_manifest.schema.json",
            "manifest_id": f"{owner}_generated_v1",
            "collections": [
                {
                    "asset_class": "Thing",
                    "schema_path": f"Plugins/Resources/{owner}/Data/Schemas/thing.schema.json",
                    "source_root": f"Plugins/Resources/{owner}/Data/Definitions",
                    "package_root": f"/{owner}/Generated",
                    "artifact_root": f"Plugins/Resources/{owner}/Content/Generated",
                }
            ],
        }

    def write_source(self, owner: str) -> None:
        self.sources[owner].write_text(
            json.dumps({"id": "A", "value": "one"}, separators=(",", ":")) + "\n",
            encoding="utf-8",
        )

    def write_artifact(self, owner: str) -> None:
        self.artifacts[owner].write_bytes(f"{owner}-asset-v1".encode("ascii"))

    def write_contract(self) -> None:
        self.contract_path.write_text(
            json.dumps({"schema_version": 2, "asset_authorities": self.authorities}, indent=2) + "\n",
            encoding="utf-8",
        )

    def inventory_record(self, owner: str) -> dict:
        return {
            "asset_class": "Thing",
            "package_name": f"/{owner}/Generated/A",
            "source_json_path": "a.json",
            "source_json_hash": MODULE.normalized_json_md5(self.sources[owner]),
        }

    def write_inventory(self) -> None:
        self.inventory_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "assets": [self.inventory_record(owner) for owner in OWNERS],
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )

    def manifest(self, owner: str) -> dict:
        authority = next(item for item in self.authorities if item["owner"] == owner)
        collection = authority["collections"][0]
        source = self.sources[owner]
        artifact = self.artifacts[owner]
        return {
            "$schema": authority["manifest_schema_ref"],
            "schema_version": 1,
            "manifest_id": authority["manifest_id"],
            "release_contract_sha256": MODULE.digest(self.contract_path),
            "owner": owner,
            "assets": [
                {
                    "asset_class": collection["asset_class"],
                    "package_name": f"/{owner}/Generated/A",
                    "artifact_path": artifact.relative_to(self.root).as_posix(),
                    "artifact_sha256": MODULE.digest(artifact),
                    "source_path": source.relative_to(self.root).as_posix(),
                    "source_sha256": MODULE.normalized_json_sha256(source),
                    "source_json_hash_md5": MODULE.normalized_json_md5(source),
                    "schema_path": collection["schema_path"],
                }
            ],
        }

    def write_manifests(self) -> None:
        for owner in OWNERS:
            self.manifests[owner].write_text(
                json.dumps(self.manifest(owner), indent=2) + "\n", encoding="utf-8"
            )

    def failures(self, owner: str | None = None) -> list[str]:
        return MODULE.check(self.root, self.contract_path, owner)

    def joined(self, failures: list[str]) -> str:
        return "\n".join(failures)

    def test_check_passes_for_current_authority(self):
        self.write_manifests()
        self.assertEqual([], self.failures())

    def test_check_reports_all_stale_sources(self):
        self.write_manifests()
        for owner in OWNERS:
            self.sources[owner].write_text(
                json.dumps({"id": "A", "value": "changed"}, separators=(",", ":")) + "\n",
                encoding="utf-8",
            )
        text = self.joined(self.failures())
        for owner in OWNERS:
            self.assertIn(owner, text)
            self.assertIn("a.json", text)
            self.assertIn("source_json_hash_md5", text)

    def test_check_detects_stale_source_sha256(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["source_sha256"] = "0" * 64
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("source_sha256", self.joined(self.failures()))

    def test_check_detects_stale_artifact_sha256(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["artifact_sha256"] = "0" * 64
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("artifact_sha256", self.joined(self.failures()))

    def test_check_detects_stale_contract_hash(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["release_contract_sha256"] = "0" * 64
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("release_contract_sha256", self.joined(self.failures()))

    def test_check_detects_missing_source(self):
        self.write_manifests()
        self.sources["OwnerA"].unlink()
        self.assertIn("missing", self.joined(self.failures()).lower())

    def test_check_detects_missing_artifact(self):
        self.write_manifests()
        self.artifacts["OwnerA"].unlink()
        self.assertIn("missing", self.joined(self.failures()).lower())

    def test_check_detects_unmanifested_source(self):
        self.write_manifests()
        extra = self.sources["OwnerA"].parent / "b.json"
        extra.write_text('{"id":"B"}\n', encoding="utf-8")
        self.assertIn("b.json", self.joined(self.failures()))

    def test_check_detects_duplicate_package(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"].append(json.loads(json.dumps(manifest["assets"][0])))
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("package", self.joined(self.failures()).lower())

    def test_check_detects_wrong_asset_class(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["asset_class"] = "Other"
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("class", self.joined(self.failures()).lower())

    def test_check_detects_schema_mismatch(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["schema_path"] = "Plugins/Other/schema.json"
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("schema", self.joined(self.failures()).lower())

    def test_check_detects_package_artifact_mismatch(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["artifact_path"] = "Plugins/Resources/OwnerA/Content/Generated/B.uasset"
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("artifact", self.joined(self.failures()).lower())

    def test_check_detects_path_escape(self):
        self.write_manifests()
        manifest = json.loads(self.manifests["OwnerA"].read_text(encoding="utf-8"))
        manifest["assets"][0]["source_path"] = "../outside.json"
        self.manifests["OwnerA"].write_text(json.dumps(manifest), encoding="utf-8")
        self.assertIn("outside.json", self.joined(self.failures()))

    def test_check_does_not_write(self):
        self.write_manifests()
        before = {
            path.relative_to(self.root).as_posix(): (path.read_bytes(), path.stat().st_mtime_ns)
            for path in self.root.rglob("*")
            if path.is_file()
        }
        self.assertEqual([], self.failures())
        after = {
            path.relative_to(self.root).as_posix(): (path.read_bytes(), path.stat().st_mtime_ns)
            for path in self.root.rglob("*")
            if path.is_file()
        }
        self.assertEqual(before, after)

    def test_refresh_all_owners_writes_both(self):
        for manifest in self.manifests.values():
            if manifest.exists():
                manifest.unlink()
        results = MODULE.refresh(self.root, self.inventory_path, self.contract_path)
        self.assertEqual(2, len(results))
        self.assertEqual([], self.failures())
        for owner in OWNERS:
            self.assertTrue(self.manifests[owner].is_file())
            self.assertEqual(owner, json.loads(self.manifests[owner].read_text(encoding="utf-8"))["owner"])

    def test_refresh_writes_lf_manifest(self):
        for manifest in self.manifests.values():
            if manifest.exists():
                manifest.unlink()
        MODULE.refresh(self.root, self.inventory_path, self.contract_path)
        data = self.manifests["OwnerA"].read_bytes()
        self.assertNotIn(bytes([13, 10]), data)
        self.assertIn(bytes([10]), data)

    def test_refresh_single_owner_writes_only_selected(self):
        for manifest in self.manifests.values():
            if manifest.exists():
                manifest.unlink()
        MODULE.refresh(self.root, self.inventory_path, self.contract_path, "OwnerA")
        self.assertTrue(self.manifests["OwnerA"].is_file())
        self.assertFalse(self.manifests["OwnerB"].is_file())
        self.assertEqual([], self.failures("OwnerA"))

    def test_refresh_rejects_unknown_owner(self):
        with self.assertRaises(MODULE.AuthorityError):
            MODULE.refresh(self.root, self.inventory_path, self.contract_path, "OwnerZ")
        with self.assertRaises(MODULE.AuthorityError):
            self.failures("OwnerZ")

    def test_refresh_rejects_stale_inventory(self):
        self.sources["OwnerA"].write_text('{"id":"A","value":"changed"}\n', encoding="utf-8")
        with self.assertRaisesRegex(MODULE.AuthorityError, "stale"):
            MODULE.refresh(self.root, self.inventory_path, self.contract_path, "OwnerA")

    def test_non_definition_authorities_ignored(self):
        self.authorities.append(
            {
                "authority_kind": "producer_input",
                "owner": "OwnerProducer",
                "manifest_path": "Plugins/Resources/OwnerProducer/Data/Producers/missing.json",
            }
        )
        self.write_contract()
        self.write_manifests()
        self.assertEqual([], self.failures())


if __name__ == "__main__":
    unittest.main()
