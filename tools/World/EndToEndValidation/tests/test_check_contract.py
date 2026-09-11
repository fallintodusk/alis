from __future__ import annotations

import tempfile
import unittest
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.checks import common_contract_hash
from World.EndToEndValidation.app.profile_inputs import ProfileInputError, profile_input_contract


class CommonCheckContractTests(unittest.TestCase):
    def _write(self, root: Path, relative: str, value: bytes = b"baseline") -> Path:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
        return path

    def test_hash_tracks_immutable_inputs_but_excludes_generated_authority(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game_config = self._write(root, "Config/DefaultGame.ini")
            self._write(root, "Config/DefaultEngine.ini")
            self._write(root, "Alis.uproject")
            self._write(root, "Plugins/World/ProjectWorld/ProjectWorld.uplugin")
            self._write(root, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Builder.cpp")
            self._write(root, "Plugins/World/ProjectWorld/Source/ProjectWorldEditor/ProjectWorldEditor.Build.cs")
            self._write(root, "Plugins/World/ProjectWorld/Data/Schemas/world.schema.json")
            self._write(root, "tools/World/ExecutionEnvironment/requirements.lock.txt")
            self._write(root, "scripts/ue/world/realize.ps1")

            source_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_representative_v1.source.json",
                b'{"sources": []}',
            )
            compiler_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_representative_v1.compile.json",
                b'{}',
            )
            validation_profile = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/representative_v1.validation.json",
                (
                    '{"profile_id":"representative_v1","profiles":{"kazan":{'
                    f'"source_profile_path":"{source_profile.relative_to(root).as_posix()}",'
                    f'"compiler_profile_path":"{compiler_profile.relative_to(root).as_posix()}"'
                    '}}}'
                ).encode("utf-8"),
            )
            unrelated = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json",
                b'{"sources": []}',
            )
            future_validation = self._write(
                root,
                "Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json",
                (
                    '{"profile_id":"kazan_territory_v1","profiles":{"kazan":{'
                    '"source_profile_path":"Plugins/World/ProjectWorldData/Data/Profiles/'
                    'SourceIngestion/not_created_yet.source.json"}}}'
                ).encode("utf-8"),
            )

            fixture = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Fixtures/Provider/source.osm",
            )
            generated_map = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Content/Generated/P0/L_Test.umap",
            )
            manifest = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Manifests/active_set.json",
            )
            baseline = common_contract_hash(root)
            profile_baseline = profile_input_contract(validation_profile, root)

            game_config.write_bytes(b"changed")
            self.assertNotEqual(baseline, common_contract_hash(root))
            game_config.write_bytes(b"baseline")
            self.assertEqual(baseline, common_contract_hash(root))

            fixture.write_bytes(b"changed")
            self.assertNotEqual(baseline, common_contract_hash(root))
            fixture.write_bytes(b"baseline")
            self.assertEqual(baseline, common_contract_hash(root))

            generated_map.write_bytes(b"changed")
            self.assertEqual(baseline, common_contract_hash(root))
            manifest.write_bytes(b"changed")
            self.assertEqual(baseline, common_contract_hash(root))

            source_profile.write_bytes(b'{"sources": [{"source_id": "changed"}]}')
            self.assertNotEqual(baseline, common_contract_hash(root))
            self.assertNotEqual(profile_baseline, profile_input_contract(validation_profile, root))
            source_profile.write_bytes(b'{"sources": []}')
            self.assertEqual(baseline, common_contract_hash(root))

            unrelated.write_bytes(b'{"profile_id": "future"}')
            self.assertEqual(baseline, common_contract_hash(root))
            future_validation.write_bytes(future_validation.read_bytes() + b" ")
            self.assertEqual(baseline, common_contract_hash(root))

    def test_profile_contract_tracks_authored_package_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._write(root, "Plugins/World/ProjectWorldTestData/ProjectWorldTestData.uplugin")
            authored_map = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Content/Authored/Fixtures/L_Marker.umap",
            )
            authored_profile = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Authored/synthetic.json",
                (
                    '{"world_data_plugin":"ProjectWorldTestData","overlays":['
                    '{"authored_package":"/ProjectWorldTestData/Authored/Fixtures/L_Marker"}]}'
                ).encode("utf-8"),
            )
            validation_profile = self._write(
                root,
                "Plugins/World/ProjectWorldTestData/Data/Profiles/EndToEndValidation/synthetic.validation.json",
                (
                    '{"profile_id":"synthetic","profiles":{"synthetic":{'
                    f'"authored_overlay_profile":"{authored_profile.relative_to(root).as_posix()}"'
                    '}}}'
                ).encode("utf-8"),
            )

            baseline = profile_input_contract(validation_profile, root)
            authored_map.write_bytes(b"changed")
            self.assertNotEqual(baseline, profile_input_contract(validation_profile, root))
            authored_map.unlink()
            with self.assertRaises(ProfileInputError):
                profile_input_contract(validation_profile, root)


if __name__ == "__main__":
    unittest.main()
