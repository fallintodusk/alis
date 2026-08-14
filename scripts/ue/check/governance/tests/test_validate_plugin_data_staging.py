from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


GOVERNANCE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOVERNANCE_ROOT))

from validate_plugin_data_staging import find_plugin_data_readers


class PluginDataReaderDiscoveryTests(unittest.TestCase):
    def _module(self, plugin: Path, name: str, module_type: str, target: str) -> None:
        descriptor_path = plugin / f"{plugin.name}.uplugin"
        if descriptor_path.exists():
            descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
        else:
            descriptor = {"FileVersion": 3, "Modules": []}
        descriptor["Modules"].append({"Name": name, "Type": module_type})
        descriptor_path.parent.mkdir(parents=True, exist_ok=True)
        descriptor_path.write_text(json.dumps(descriptor), encoding="utf-8")
        source = plugin / "Source" / name / "Private" / f"{name}.cpp"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text(
            f'auto Path = FProjectPaths::GetPluginDataDir(TEXT("{target}"));',
            encoding="utf-8",
        )

    def test_only_shipping_modules_require_runtime_data_staging(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            plugins = Path(directory) / "Plugins"
            self._module(plugins / "ProjectWorld", "ProjectWorldEditor", "Editor", "ProjectWorld")
            self._module(plugins / "ProjectRuntime", "ProjectRuntime", "Runtime", "ProjectRuntime")

            readers = find_plugin_data_readers(plugins)

            self.assertNotIn("ProjectWorld", readers)
            self.assertEqual(1, len(readers["ProjectRuntime"]))


if __name__ == "__main__":
    unittest.main()
