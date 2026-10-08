import importlib.util
import json
import sys
import subprocess
import tempfile
import runpy
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "audit_developer_payload.py"
SPEC = importlib.util.spec_from_file_location("audit_developer_payload", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DeveloperDependencyAuditTests(unittest.TestCase):
    def test_native_producer_input_class_is_authenticated(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds, inventory = self._write_inventory_fixture(root, [])
            document = json.loads(seeds.read_text())
            document["seeds"][0]["asset_class"] = "MeshPartitionDefinition"
            seeds.write_text(json.dumps(document))
            native = json.loads(inventory.read_text())
            native["assets"] = [{"package_name": "/ProjectWorldData/PublicMap",
                                 "asset_classes": ["MeshPartitionDefinition"]}]
            inventory.write_text(json.dumps(native))
            self.assertEqual("accepted", MODULE.validate_inventory(root, seeds, inventory)["status"])
            for classes in ([], ["Material"], ["MeshPartitionDefinition", "Material"]):
                with self.subTest(classes=classes):
                    native["assets"][0]["asset_classes"] = classes
                    inventory.write_text(json.dumps(native))
                    report = MODULE.validate_inventory(root, seeds, inventory)
                    self.assertEqual("rejected_asset_class", report["issues"][0]["code"])

    def test_selected_producer_inputs_enter_dependency_seeds(self):
        plan = MODULE.build_seed_plan(REPO_ROOT)
        inputs = [item for item in plan["seeds"] if item["owner"] == "ProjectWorldMeshTerrain"]
        self.assertEqual(1, len(inputs))
        self.assertEqual("MeshPartitionDefinition", inputs[0]["asset_class"])
        self.assertEqual("/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1", inputs[0]["package_name"])

    def test_rejected_cli_identifies_packages_after_report_cleanup(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds, inventory = self._write_inventory_fixture(
                root, [], ["/ProjectWorldData/PublicMap"]
            )
            report = root / "report.json"
            result = subprocess.run(
                [sys.executable, str(MODULE_PATH), "validate", "--repo-root", str(root),
                 "--seeds", str(seeds), "--inventory", str(inventory),
                 "--output", str(report), "--engine-root", str(root / "engine")],
                capture_output=True, text=True, check=False,
            )
            report.unlink()
            self.assertEqual(1, result.returncode)
            self.assertIn("rejected_missing_package", result.stderr)
            self.assertIn("/ProjectWorldData/PublicMap", result.stderr)

    def test_registry_gather_discovers_transitive_dependencies(self):
        class Registry:
            gathered = False

            def search_all_assets(self, synchronous):
                self.assert_synchronous = synchronous
                self.gathered = True

            def wait_for_completion(self):
                pass

            def scan_files_synchronous(self, files, force):
                pass

            def get_assets_by_package_name(self, package):
                if not self.gathered and package != "/Public/Map":
                    return []
                return [SimpleNamespace(asset_class_path=SimpleNamespace(asset_name="Object"))]

            def get_dependencies(self, package, options):
                if not options.include_hard_package_references:
                    return []
                return {"/Public/Map": ["/Public/Material"],
                        "/Public/Material": ["/Public/Texture"]}.get(package, []) if (
                            self.gathered or package == "/Public/Map"
                        ) else []

        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            seeds = root / "seeds.json"
            output = root / "inventory.json"
            seeds.write_text(json.dumps({"seeds": [{"package_name": "/Public/Map",
                                                   "artifact_path": "Map.umap"}]}))
            registry = Registry()
            unreal = SimpleNamespace(
                AssetRegistryHelpers=SimpleNamespace(get_asset_registry=lambda: registry),
                Paths=SimpleNamespace(project_dir=lambda: str(root),
                                      convert_relative_path_to_full=lambda value: value),
                AssetRegistryDependencyOptions=SimpleNamespace,
                log=lambda value: None,
            )
            with patch.dict(sys.modules, {"unreal": unreal}), patch.dict(
                "os.environ", {"ALIS_PUBLIC_DEPENDENCY_SEEDS": str(seeds),
                               "ALIS_PUBLIC_DEPENDENCY_OUTPUT": str(output)}
            ):
                runpy.run_path(str(REPO_ROOT / "scripts/ue/check/assets/export_public_dependency_inventory.py"))
            result = json.loads(output.read_text())
            self.assertEqual([], result["missing_packages"])
            self.assertEqual(["/Public/Map", "/Public/Material", "/Public/Texture"], result["packages"])
            self.assertTrue(registry.assert_synchronous)

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
