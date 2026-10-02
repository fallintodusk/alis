from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path


ASSETS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ASSETS_ROOT))

from check_primary_assets import main, read_directories


SCANNED_TYPE = (
    '+PrimaryAssetTypesToScan=(PrimaryAssetType="FixtureType",'
    'AssetBaseClass="/Script/Engine.Object",bHasBlueprintClasses=False,'
    'bIsEditorOnly=False,Directories=((Path="/FixturePlugin/Data")),SpecificAssets=,'
    'Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=Unknown))'
)
# The form the GameFeatures editor module writes: a rule with no directories.
DIRECTORYLESS_TYPE = (
    '+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",'
    'AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,'
    'bIsEditorOnly=False,Directories=,SpecificAssets=,'
    'Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))'
)


class PrimaryAssetPresenceTests(unittest.TestCase):
    def _run(self, *, scan_directory: str, scanned_type: str = SCANNED_TYPE) -> int:
        """Run the check with the scanned type's directory 'filled', 'empty', or 'missing'."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "Config" / "DefaultGame.ini"
            config.parent.mkdir(parents=True)
            config.write_text(
                "[/Script/Engine.AssetManagerSettings]\n"
                f"{scanned_type}\n{DIRECTORYLESS_TYPE}\n",
                encoding="utf-8",
            )
            content = root / "Plugins" / "FixturePlugin" / "Content"
            content.mkdir(parents=True)
            if scan_directory != "missing":
                (content / "Data").mkdir()
            if scan_directory == "filled":
                (content / "Data" / "Thing.uasset").write_bytes(b"")
            with contextlib.redirect_stdout(io.StringIO()):
                return main(root)

    def test_type_without_directories_has_nothing_to_check(self) -> None:
        self.assertEqual(0, self._run(scan_directory="filled"))

    def test_missing_scan_directory_still_fails(self) -> None:
        self.assertEqual(1, self._run(scan_directory="missing"))

    def test_empty_scan_directory_still_fails(self) -> None:
        self.assertEqual(1, self._run(scan_directory="empty"))

    def _variant(self, directories: str) -> str:
        variant = SCANNED_TYPE.replace('Directories=((Path="/FixturePlugin/Data"))', directories)
        self.assertNotEqual(SCANNED_TYPE, variant)
        return variant

    def test_engine_spellings_are_read(self) -> None:
        for spelling in (
            'Directories=( (Path="/FixturePlugin/Data") )',
            'Directories = ((Path="/FixturePlugin/Data"))',
            'directories=((path="/FixturePlugin/Data"))',
        ):
            with self.subTest(spelling=spelling):
                scanned = self._variant(spelling)
                self.assertEqual(0, self._run(scan_directory="filled", scanned_type=scanned))
                self.assertEqual(1, self._run(scan_directory="missing", scanned_type=scanned))

    def test_unreadable_directories_fail(self) -> None:
        # The engine imports a bare string inside a struct as empty, so it scans nothing.
        for spelling in (
            "Directories=((Bogus))",
            "Directories=((Path=/FixturePlugin/Data))",
            'Directories=((Path="/FixturePlugin/Data"),(Path=/FixturePlugin/Other))',
        ):
            with self.subTest(spelling=spelling):
                scanned = self._variant(spelling)
                self.assertEqual(1, self._run(scan_directory="filled", scanned_type=scanned))

    def test_entry_without_directories_key_names_none(self) -> None:
        self.assertEqual([], read_directories('PrimaryAssetType="Fixture",SpecificAssets='))


if __name__ == "__main__":
    unittest.main()
