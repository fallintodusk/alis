import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath


GOVERNANCE_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOVERNANCE_DIR))

import validate_licensing


class LicensingValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.repo_root = Path(self.temp_dir.name)
        self._write(
            "LICENSE",
            "\n".join(
                (
                    "[AGPL](LICENSES/AGPL-3.0-only.txt)",
                    "[Policy](docs/legal/policy.md)",
                    "| `ue-in-process` | Unreal modules | MPL-2.0 | boundary |",
                    "| `separate-process` | Separate tools | AGPL-3.0-only | boundary |",
                    "| `documentation` | Documentation | MPL-2.0 | boundary |",
                )
            ),
        )
        self._write("LICENSES/AGPL-3.0-only.txt", "x" * 1001)
        self._write("docs/legal/policy.md", "# Policy")
        self._write("Alis.uproject", "{}")
        self._write(
            "tools/BuildService/Cargo.toml",
            "\n".join(
                (
                    "[workspace]",
                    'members = ["crates/worker"]',
                    "[workspace.package]",
                    'license = "AGPL-3.0-only"',
                )
            ),
        )
        self._write(
            "tools/BuildService/crates/worker/Cargo.toml",
            "\n".join(
                (
                    "[package]",
                    'name = "worker"',
                    'license.workspace = true',
                )
            ),
        )
        self._write(
            "scripts/git/mirror/mirror.exclude",
            "Plugins/InstanceArrayTool/**",
        )
        self._write(
            "Plugins/InstanceArrayTool/NOTICE",
            "\n".join(
                (
                    "ALIS-Component-Class: original-terms",
                    "ALIS-Public-Source: excluded",
                )
            ),
        )

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def _write(self, relative_path: str, content: str) -> None:
        path = self.repo_root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _paths(self) -> list[str]:
        return [
            path.relative_to(self.repo_root).as_posix()
            for path in self.repo_root.rglob("*")
            if path.is_file()
        ]

    def _validate(self) -> list[str]:
        return validate_licensing.validate_repository(
            self.repo_root,
            self._paths(),
        )

    def test_valid_explicit_metadata(self) -> None:
        self.assertEqual([], self._validate())

    def test_unexpected_owner_notice_fails_closed(self) -> None:
        self._write(
            "Source/EpicDerived.cpp",
            "// Copyright Epic Games, Inc. All Rights Reserved.",
        )

        errors = self._validate()

        self.assertTrue(
            any(
                "Unexpected ownership notice requires explicit review" in error
                for error in errors
            )
        )

    def test_explicit_review_allows_owner_notice(self) -> None:
        self._write("Source/NOTICE", "ALIS-Component-Class: original-terms\n")
        self._write(
            "Source/EpicDerived.cpp",
            "// Copyright Epic Games, Inc. All Rights Reserved.",
        )

        self.assertEqual([], self._validate())

    def test_unreal_declaration_does_not_allow_owner_notice(self) -> None:
        self._write(
            "scripts/ue/editor/NOTICE",
            "ALIS-Component-Class: ue-in-process\n",
        )
        self._write(
            "scripts/ue/editor/EpicDerived.py",
            "# Copyright Epic Games, Inc. All Rights Reserved.",
        )

        errors = self._validate()

        self.assertTrue(
            any(
                "Unexpected ownership notice requires explicit review" in error
                for error in errors
            )
        )

    def test_unmarked_notice_does_not_declare_component(self) -> None:
        self._write("vendor/NOTICE", "Third-party attribution only.")

        self.assertEqual([], self._validate())

    def test_reports_unknown_explicit_component_class(self) -> None:
        self._write("vendor/NOTICE", "ALIS-Component-Class: invented\n")

        errors = self._validate()

        self.assertTrue(any("Unknown explicit component class" in e for e in errors))

    def test_reports_conflicting_explicit_metadata(self) -> None:
        self._write("vendor/LICENSE", "ALIS-Component-Class: ue-in-process\n")
        self._write("vendor/NOTICE", "ALIS-Component-Class: original-terms\n")

        errors = self._validate()

        self.assertTrue(
            any("Conflicting explicit legal metadata" in e for e in errors)
        )

    def test_reports_missing_local_public_exclusion(self) -> None:
        mirror = self.repo_root / "scripts/git/mirror/mirror.exclude"
        mirror.write_text("", encoding="utf-8")

        errors = self._validate()

        self.assertTrue(
            any("requires a public mirror exclusion" in e for e in errors)
        )

    def test_reports_cargo_workspace_assignment_drift(self) -> None:
        cargo = self.repo_root / "tools/BuildService/Cargo.toml"
        cargo.write_text(cargo.read_text().replace("AGPL-3.0-only", "MIT"))

        errors = self._validate()

        self.assertTrue(any("Cargo workspace license drift" in e for e in errors))

    def test_reports_cargo_package_assignment_drift(self) -> None:
        cargo = self.repo_root / "tools/BuildService/crates/worker/Cargo.toml"
        cargo.write_text(
            cargo.read_text().replace(
                "license.workspace = true",
                'license = "MIT"',
            )
        )

        errors = self._validate()

        self.assertTrue(any("Cargo package license drift" in e for e in errors))

    def test_reports_unreferenced_license_text(self) -> None:
        self._write("LICENSES/NOISE.txt", "x" * 1001)

        errors = self._validate()

        self.assertIn(
            "Unreferenced standard license text adds noise: LICENSES/NOISE.txt",
            errors,
        )

    def test_repository_resources_category_does_not_reclassify_plugin_source(self) -> None:
        plugin_root = PurePosixPath("Plugins/Resources/ProjectAudio")
        source = PurePosixPath(
            "Plugins/Resources/ProjectAudio/Source/ProjectAudio/Audio.cpp"
        )
        relative = source.relative_to(plugin_root).as_posix()

        self.assertEqual(
            "ue-in-process",
            validate_licensing.unreal_relative_class(relative),
        )

    def test_plugin_relative_resources_requires_provenance(self) -> None:
        self.assertEqual(
            "provenance-required",
            validate_licensing.unreal_relative_class("Resources/Icon.png"),
        )

    def test_plugin_build_metadata_inherits_unreal_component(self) -> None:
        self.assertEqual(
            "ue-in-process",
            validate_licensing.unreal_relative_class("BuildUnit.yaml"),
        )


if __name__ == "__main__":
    unittest.main()
