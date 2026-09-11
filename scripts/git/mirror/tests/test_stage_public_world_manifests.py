import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "stage_public_world_manifests.py"
SPEC = importlib.util.spec_from_file_location("stage_public_world_manifests", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class PublicWorldManifestStageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.assets = self.root / "assets"
        (self.source / "scopes").mkdir(parents=True)
        entries = []
        for index, scope_id in enumerate(sorted(MODULE.PUBLIC_SCOPE_IDS)):
            artifact = self.assets / f"Content/{index}.uasset"
            artifact.parent.mkdir(parents=True, exist_ok=True)
            artifact.write_bytes(bytes([index]))
            manifest = {
                "scope_id": scope_id,
                "artifacts": [
                    {
                        "path": f"Content/{index}.uasset",
                        "digest_kind": "sha256",
                        "digest": hashlib.sha256(artifact.read_bytes()).hexdigest(),
                    }
                ],
            }
            relative = f"scopes/{scope_id}.1.json"
            path = self.source / relative
            path.write_text(json.dumps(manifest), encoding="utf-8")
            entries.append(
                {
                    "scope_id": scope_id,
                    "manifest_path": relative,
                    "manifest_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                }
            )
        (self.source / "active_set.json").write_text(
            json.dumps({"scopes": entries}), encoding="utf-8"
        )

    def tearDown(self):
        self.temp.cleanup()

    def test_stages_exact_authenticated_authority(self):
        output = self.root / "output"
        MODULE.stage(self.source, self.assets, output)
        self.assertEqual(11, len(list((output / "scopes").glob("*.json"))))

    def test_rejects_artifact_hash_mismatch(self):
        (self.assets / "Content/0.uasset").write_bytes(b"changed")
        with self.assertRaisesRegex(MODULE.StageError, "artifact hash mismatch"):
            MODULE.stage(self.source, self.assets, self.root / "output")

    def test_rejects_incomplete_scope_set(self):
        active_path = self.source / "active_set.json"
        active = json.loads(active_path.read_text(encoding="utf-8"))
        active["scopes"].pop()
        active_path.write_text(json.dumps(active), encoding="utf-8")
        with self.assertRaisesRegex(MODULE.StageError, "authority is incomplete"):
            MODULE.stage(self.source, self.assets, self.root / "output")


if __name__ == "__main__":
    unittest.main()
