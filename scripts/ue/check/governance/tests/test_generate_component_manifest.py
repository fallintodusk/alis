import subprocess
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


GOVERNANCE_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOVERNANCE_DIR))

import generate_component_manifest


class ComponentManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.repo_root = Path(self.temp_dir.name)
        self._git("init", "-q")
        self._git("config", "user.name", "ALIS Test")
        self._git("config", "user.email", "test@localhost")
        self._git("config", "core.autocrlf", "false")
        self._write(
            "LICENSE",
            "\n".join(
                (
                    "[MPL](LICENSES/MPL-2.0.txt)",
                    "[AGPL](LICENSES/AGPL-3.0-only.txt)",
                    "[Policy](docs/legal/policy.md)",
                    "| `ue-in-process` | Unreal | MPL-2.0 | boundary |",
                    "| `separate-process` | Tools | AGPL-3.0-only | boundary |",
                    "| `documentation` | Docs | MPL-2.0 | boundary |",
                )
            ),
        )
        self._write("LICENSES/MPL-2.0.txt", "MPL" * 400)
        self._write("LICENSES/AGPL-3.0-only.txt", "AGPL" * 300)
        self._write("Alis.uproject", "{}")
        self._write("Source/Alis/Alis.cpp", "int main_module = 0;")
        self._write("Plugins/UI/Test/Test.uplugin", "{}")
        self._write(
            "Plugins/UI/Test/Source/Test/Private/Test.cpp",
            "int plugin_module = 0;",
        )
        self._write("Plugins/UI/Test/README.md", "# Test")
        self._write("docs/README.md", "# Docs")
        self._write("docs/legal/policy.md", "# Policy")
        self._write(
            "scripts/ue/editor/NOTICE",
            "ALIS-Component-Class: ue-in-process\n",
        )
        self._write("scripts/README.md", "# Scripts")
        self._write("scripts/tool.py", "print('tool')")
        self._write("tools/helper.py", "print('tool')")
        self._write(".claude/settings.json", "{}")
        self._write(".codex/config.toml", "")
        self._write(".githooks/pre-commit", "#!/bin/sh\n")
        self._write("scripts/ue/editor/editor.py", "import unreal\n")
        self._write("scripts/git/mirror/mirror.exclude", "")
        self._write(".github/workflows/check.yml", "name: check")
        self._commit("initial")
        self._git("tag", "release-test")

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def _git(self, *args: str) -> str:
        result = subprocess.run(
            ["git", *args],
            cwd=self.repo_root,
            check=True,
            capture_output=True,
            text=True,
        )
        return result.stdout.strip()

    def _write(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")

    def _commit(self, message: str) -> None:
        self._git("add", "-A")
        self._git("commit", "-q", "-m", message)

    def _retag(self) -> None:
        self._git("tag", "-f", "release-test")

    def _generate(self) -> dict[str, object]:
        return generate_component_manifest.generate_manifest(
            self.repo_root,
            "release-test",
        )

    def test_generates_assignments_from_native_boundaries(self) -> None:
        manifest = self._generate()
        entries = {
            entry["path"]: entry
            for entry in manifest["entries"]
        }

        self.assertEqual(
            "ue-in-process",
            entries["Plugins/UI/Test/Source/Test/Private/Test.cpp"][
                "component_class"
            ],
        )
        self.assertEqual(
            "documentation",
            entries["Plugins/UI/Test/README.md"]["component_class"],
        )
        self.assertEqual(
            "separate-process",
            entries["scripts/tool.py"]["component_class"],
        )
        self.assertNotIn("evidence", entries["scripts/tool.py"])
        for path in (
            ".claude/settings.json",
            ".codex/config.toml",
            ".githooks/pre-commit",
            ".github/workflows/check.yml",
            "tools/helper.py",
        ):
            self.assertEqual(
                "separate-process",
                entries[path]["component_class"],
            )
        self.assertEqual(
            "ue-in-process",
            entries["scripts/ue/editor/editor.py"]["component_class"],
        )
        self.assertEqual(
            "documentation",
            entries["scripts/README.md"]["component_class"],
        )
        self.assertEqual(
            "legal-evidence",
            entries["LICENSE"]["kind"],
        )

    def test_output_is_reproducible(self) -> None:
        self.assertEqual(self._generate(), self._generate())

    def test_plugin_tools_inherit_separate_process_without_notice(self) -> None:
        self._write("Plugins/UI/Test/Tools/check.py", "print('check')")
        self._commit("plugin tool")
        self._retag()
        entry = next(item for item in self._generate()["entries"]
                     if item["path"] == "Plugins/UI/Test/Tools/check.py")
        self.assertEqual("separate-process", entry["component_class"])

    def test_definition_source_requires_authenticated_owner_manifest(self) -> None:
        source = "Plugins/UI/Test/Content/definition.json"
        manifest_path = "Plugins/UI/Test/Data/manifest.json"
        contract_path = "scripts/git/mirror/developer_asset_release.json"
        contract = json.dumps({"asset_authorities": [{
            "authority_kind": "generated_definition_manifest",
            "owner": "Test", "license_id": "MPL-2.0",
            "manifest_id": "test_definitions",
            "collections": [{"asset_class": "ObjectDefinition",
                             "source_root": "Plugins/UI/Test/Content",
                             "schema_path": "Plugins/UI/Test/Data/object.schema.json"}],
            "manifest_path": manifest_path,
        }]})
        self._write(source, "{}")
        self._write(contract_path, contract)
        self._write(manifest_path, json.dumps({
            "owner": "Test",
            "manifest_id": "test_definitions",
            "release_contract_sha256": hashlib.sha256(contract.encode()).hexdigest(),
            "assets": [{"source_path": source,
                        "asset_class": "ObjectDefinition",
                        "schema_path": "Plugins/UI/Test/Data/object.schema.json",
                        "source_sha256": hashlib.sha256(b"{}").hexdigest()}],
        }))
        self._commit("authenticated definition source")
        self._retag()
        entry = next(item for item in self._generate()["entries"] if item["path"] == source)
        self.assertEqual("ue-in-process", entry["component_class"])
        self.assertEqual(manifest_path, entry["evidence"])
        accepted = (self.repo_root / manifest_path).read_text()
        for mutation in ("root", "class", "schema", "duplicate", "identity"):
            invalid = json.loads(accepted)
            if mutation == "root":
                invalid["assets"][0]["source_path"] = "scripts/unrelated.json"
            elif mutation == "class":
                invalid["assets"][0]["asset_class"] = "Unknown"
            elif mutation == "schema":
                invalid["assets"][0]["schema_path"] = "unknown.schema.json"
            elif mutation == "duplicate":
                invalid["assets"].append(invalid["assets"][0].copy())
            else:
                invalid["manifest_id"] = "wrong"
            self._write(manifest_path, json.dumps(invalid))
            self._commit(mutation)
            self._retag()
            with self.subTest(mutation=mutation), self.assertRaises(
                generate_component_manifest.ManifestError
            ):
                self._generate()
        self._write(manifest_path, accepted)
        self._write(source, '{"changed": true}')
        self._commit("stale source")
        self._retag()
        with self.assertRaisesRegex(generate_component_manifest.ManifestError,
                                    "Stale definition source authority"):
            self._generate()

    def test_dirty_repository_fails(self) -> None:
        self._write("dirty.txt", "dirty")

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "must be clean",
        ):
            self._generate()

    def test_tag_must_identify_head(self) -> None:
        self._write("docs/next.md", "# Next")
        self._commit("next")

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "does not identify HEAD",
        ):
            self._generate()

    def test_unexpected_owner_notice_fails(self) -> None:
        self._write(
            "Plugins/UI/Test/Source/Test/Private/Test.cpp",
            "// Copyright Epic Games, Inc. All Rights Reserved.",
        )
        self._commit("owner notice")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "Unexpected ownership notice",
        ):
            self._generate()

    def test_unreal_declaration_does_not_allow_owner_notice(self) -> None:
        self._write(
            "scripts/ue/editor/editor.py",
            "# Copyright Epic Games, Inc. All Rights Reserved.\nimport unreal\n",
        )
        self._commit("declared Unreal owner notice")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "ownership notice",
        ):
            self._generate()

    def test_original_terms_declaration_allows_owner_notice(self) -> None:
        self._write(
            "vendor/NOTICE",
            "ALIS-Component-Class: original-terms\n",
        )
        self._write(
            "vendor/EpicDerived.py",
            "# Copyright Epic Games, Inc. All Rights Reserved.\n",
        )
        self._commit("reviewed original terms owner notice")
        self._retag()

        manifest = self._generate()
        entries = {
            entry["path"]: entry
            for entry in manifest["entries"]
        }
        self.assertEqual(
            "original-terms",
            entries["vendor/EpicDerived.py"]["component_class"],
        )

    def test_plugin_relative_resources_require_provenance(self) -> None:
        self._write("Plugins/UI/Test/Resources/Icon.txt", "icon")
        self._commit("resource")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "asset provenance is not yet supported",
        ):
            self._generate()

    def test_root_project_content_requires_supported_provenance(self) -> None:
        self._write("Content/PublicAsset.txt", "asset")
        self._commit("root asset")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "asset provenance is not yet supported",
        ):
            self._generate()

    def test_root_cargo_workspace_does_not_absorb_unreal_paths(self) -> None:
        self._write(
            "Cargo.toml",
            "\n".join(
                (
                    "[workspace]",
                    "members = []",
                    "[workspace.package]",
                    'license = "AGPL-3.0-only"',
                )
            ),
        )
        self._commit("root cargo workspace")
        self._retag()

        manifest = self._generate()
        entries = {
            entry["path"]: entry
            for entry in manifest["entries"]
        }

        self.assertEqual(
            "separate-process",
            entries["Cargo.toml"]["component_class"],
        )
        self.assertEqual(
            "ue-in-process",
            entries["Plugins/UI/Test/Source/Test/Private/Test.cpp"][
                "component_class"
            ],
        )

    def test_unreal_import_requires_local_unreal_declaration(self) -> None:
        self._write("scripts/new_editor.py", "import unreal\n")
        self._commit("unmarked Unreal script")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "requires a local ue-in-process declaration",
        ):
            self._generate()

    def test_original_terms_python_may_import_unreal(self) -> None:
        self._write(
            "vendor/NOTICE",
            "ALIS-Component-Class: original-terms\n",
        )
        self._write("vendor/editor.py", "import unreal\n")
        self._commit("original terms Unreal script")
        self._retag()

        manifest = self._generate()
        entries = {
            entry["path"]: entry
            for entry in manifest["entries"]
        }
        self.assertEqual(
            "original-terms",
            entries["vendor/editor.py"]["component_class"],
        )

    def test_documentation_python_may_import_unreal(self) -> None:
        self._write("docs/example.py", "import unreal\n")
        self._commit("documented Unreal example")
        self._retag()

        manifest = self._generate()
        entries = {
            entry["path"]: entry
            for entry in manifest["entries"]
        }
        self.assertEqual(
            "documentation",
            entries["docs/example.py"]["component_class"],
        )

    def test_excluded_declaration_cannot_survive_public_tree(self) -> None:
        self._write("scripts/git/mirror/mirror.exclude", "vendor/**\n")
        self._write(
            "vendor/NOTICE",
            "\n".join(
                (
                    "ALIS-Component-Class: original-terms",
                    "ALIS-Public-Source: excluded",
                )
            ),
        )
        self._commit("excluded component")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "survived public tree",
        ):
            self._generate()

    def test_unknown_root_fails_closed(self) -> None:
        self._write("unknown/payload.bin", "unknown")
        self._commit("unknown")
        self._retag()

        with self.assertRaisesRegex(
            generate_component_manifest.ManifestError,
            "Unknown component boundary",
        ):
            self._generate()


if __name__ == "__main__":
    unittest.main()
