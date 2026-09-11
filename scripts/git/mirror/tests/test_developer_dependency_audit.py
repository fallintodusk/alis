import importlib.util
import json
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "audit_developer_payload.py"
SPEC = importlib.util.spec_from_file_location("audit_developer_payload", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DeveloperDependencyAuditTests(unittest.TestCase):
    @staticmethod
    def _write_inventory_fixture(root: Path, dependencies: list[str], missing: list[str] | None = None):
        (root / "Alis.uproject").write_text('{"Modules": []}\n', encoding="utf-8")
        seeds = root / "seeds.json"
        inventory = root / "inventory.json"
        package_name = "/ProjectWorldData/PublicMap"
        seeds.write_text(
            json.dumps(
                {
                    "public_roots": [package_name],
                    "seeds": [
                        {
                            "package_name": package_name,
                            "owner": "ProjectWorldData",
                            "authority_path": "authority.json",
                            "provenance": "owner_manifest_selected",
                            "distribution_class": "public",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        packages = [package_name, *dependencies]
        inventory.write_text(
            json.dumps(
                {
                    "roots": [package_name],
                    "packages": packages,
                    "edges": [{"from": package_name, "to": item} for item in dependencies],
                    "missing_packages": missing or [],
                }
            ),
            encoding="utf-8",
        )
        return seeds, inventory

    def test_unselected_project_content_fails_closed(self):
        classification = MODULE.classify_dependency(
            "/ProjectObject/Private/SM_Tree",
            {},
            {"ProjectObject"},
            set(),
        )
        self.assertEqual("rejected_required_project_content", classification["disposition"])

    def test_unknown_mount_is_not_assumed_to_be_engine_content(self):
        classification = MODULE.classify_dependency(
            "/LocallyInstalledPlugin/Private/Asset",
            {},
            {"Game"},
            set(),
            {"Engine"},
            set(),
        )
        self.assertEqual("rejected_unknown_mount", classification["disposition"])

    def test_missing_selected_package_rejects_report(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds, inventory = self._write_inventory_fixture(
                root, [], ["/ProjectWorldData/PublicMap"]
            )
            report = MODULE.validate_inventory(root, seeds, inventory)
            self.assertEqual("rejected", report["status"])
            self.assertEqual("rejected_missing_package", report["issues"][0]["code"])

    def test_hlod_dependency_rejects_report(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds, inventory = self._write_inventory_fixture(root, ["/Engine/Generated/HLOD_Test"])
            report = MODULE.validate_inventory(root, seeds, inventory)
            self.assertEqual("rejected", report["status"])
            self.assertEqual(1, report["hlod_package_count"])
            self.assertIn("hlod_forbidden", {issue["code"] for issue in report["issues"]})

    def test_restricted_embedded_dependency_rejects_before_payload(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            plugin = root / "Plugins" / "Resources" / "ProjectObject"
            plugin.mkdir(parents=True)
            (plugin / "ProjectObject.uplugin").write_text('{"Modules": []}\n', encoding="utf-8")
            seeds, inventory = self._write_inventory_fixture(
                root, ["/ProjectObject/Restricted/SM_ThirdPartyTree"]
            )
            report = MODULE.validate_inventory(root, seeds, inventory)
            self.assertEqual("rejected", report["status"])
            self.assertIn(
                "rejected_required_project_content",
                {issue["code"] for issue in report["issues"]},
            )

    def test_report_binds_git_source_identity(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds, inventory = self._write_inventory_fixture(root, [])
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.name", "test"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.email", "test@localhost"], cwd=root, check=True)
            subprocess.run(["git", "add", "."], cwd=root, check=True)
            subprocess.run(["git", "commit", "-q", "-m", "fixture"], cwd=root, check=True)
            revision = subprocess.run(
                ["git", "rev-parse", "HEAD"], cwd=root, check=True,
                capture_output=True, text=True,
            ).stdout.strip()
            tree = subprocess.run(
                ["git", "rev-parse", "HEAD^{tree}"], cwd=root, check=True,
                capture_output=True, text=True,
            ).stdout.strip()

            report = MODULE.validate_inventory(root, seeds, inventory)

            self.assertEqual(revision, report["source_revision"])
            self.assertEqual(tree, report["source_tree"])


if __name__ == "__main__":
    unittest.main()
