from __future__ import annotations

import ast
import json
import subprocess
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
WORLD_ROOT = REPO_ROOT / "tools" / "World"


class WorldToolArchitectureTests(unittest.TestCase):
    def test_project_world_logic_plugin_contains_no_instance_data(self) -> None:
        logic_root = REPO_ROOT / "Plugins" / "World" / "ProjectWorld"
        descriptor = json.loads((logic_root / "ProjectWorld.uplugin").read_text(encoding="utf-8"))
        self.assertFalse(descriptor["CanContainContent"])
        dependencies = {entry["Name"]: entry for entry in descriptor["Plugins"]}
        self.assertEqual(["Editor"], dependencies["GeometryProcessing"]["TargetAllowList"])

        content_root = logic_root / "Content"
        self.assertEqual([], [path for path in content_root.rglob("*") if path.is_file()])
        data_root = logic_root / "Data"
        data_files = [path.relative_to(data_root) for path in data_root.rglob("*") if path.is_file()]
        self.assertTrue(data_files)
        self.assertTrue(all(path.parts[0] == "Schemas" for path in data_files))
        self.assertTrue(all(path.name.endswith(".schema.json") for path in data_files))

    def test_world_test_data_is_editor_routed_and_not_production_cooked(self) -> None:
        plugins_root = REPO_ROOT / "Plugins"
        fixture_descriptor_path = plugins_root / "World" / "ProjectWorldTestData" / "ProjectWorldTestData.uplugin"
        fixture = json.loads(fixture_descriptor_path.read_text(encoding="utf-8"))
        self.assertFalse(fixture["EnabledByDefault"])
        self.assertEqual([], fixture["Modules"])

        dependents = []
        for descriptor_path in plugins_root.rglob("*.uplugin"):
            descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
            dependencies = {entry["Name"] for entry in descriptor.get("Plugins", [])}
            if "ProjectWorldTestData" in dependencies:
                dependents.append((descriptor_path, descriptor))
        self.assertEqual(["ProjectIntegrationTests.uplugin"], [path.name for path, _ in dependents])
        self.assertTrue(all(module["Type"] == "Editor" for module in dependents[0][1]["Modules"]))

        project = json.loads((REPO_ROOT / "Alis.uproject").read_text(encoding="utf-8"))
        project_plugins = {entry["Name"]: entry for entry in project["Plugins"]}
        self.assertNotIn("ProjectWorldTestData", project_plugins)
        self.assertEqual(["Editor"], project_plugins["ProjectIntegrationTests"]["TargetAllowList"])
        game_config = (REPO_ROOT / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
        self.assertNotIn("ProjectWorldTestData", game_config)
        gitignore = (REPO_ROOT / ".gitignore").read_text(encoding="utf-8")
        transient_roots = (
            "/Plugins/World/ProjectWorldTestData/Content/Generated/",
            "/Plugins/World/ProjectWorldTestData/Content/__ExternalActors__/Generated/",
            "/Plugins/World/ProjectWorldTestData/Content/__ExternalObjects__/Generated/",
            "/Plugins/World/ProjectWorldTestData/Data/Manifests/",
        )
        for transient_root in transient_roots:
            self.assertIn(transient_root, gitignore)
        tracked = subprocess.run(
            ["git", "ls-files", "--", *(root.removeprefix("/") for root in transient_roots)],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual("", tracked.stdout.strip())

    def test_component_layout_is_explicit(self) -> None:
        required = {
            "ExecutionEnvironment": {"README.md", "api.py", "app", "bootstrap.py", "contracts", "tests"},
            "SourceIngestion": {
                "README.md", "api.py", "app", "bootstrap.py", "contracts", "fixtures", "profiles", "run.py", "tests",
            },
            "CanonicalCompilation": {
                "README.md", "app", "bootstrap.py", "contracts", "fixtures", "profiles", "run.py", "tests",
            },
            "EndToEndValidation": {
                "README.md", "app", "bootstrap.py", "contracts", "profiles", "run.py", "tests",
            },
        }
        for component, children in required.items():
            root = WORLD_ROOT / component
            with self.subTest(component=component):
                self.assertTrue(root.is_dir())
                self.assertTrue(children.issubset({path.name for path in root.iterdir()}))
                normalized_component = component.replace("_", "").lower()
                repeated = [
                    path.name
                    for path in root.iterdir()
                    if path.is_dir() and path.name.replace("_", "").lower() == normalized_component
                ]
                self.assertEqual([], repeated, f"Redundant nested component name under {component}")
        self.assertFalse((WORLD_ROOT / "Compiler").exists())
        self.assertLessEqual(len((WORLD_ROOT / "README.md").read_text(encoding="utf-8").splitlines()), 200)

    def test_dependency_direction_uses_only_public_cross_component_symbols(self) -> None:
        roots = {
            "execution": WORLD_ROOT / "ExecutionEnvironment",
            "source": WORLD_ROOT / "SourceIngestion",
            "canonical": WORLD_ROOT / "CanonicalCompilation",
            "validation": WORLD_ROOT / "EndToEndValidation",
        }
        forbidden_modules = {
            "execution": ("World.SourceIngestion", "World.CanonicalCompilation", "World.EndToEndValidation"),
            "source": ("World.CanonicalCompilation", "World.EndToEndValidation"),
            "canonical": ("World.EndToEndValidation",),
            "validation": (),
        }
        module_owners = {
            "World.SourceIngestion": "source",
            "World.ExecutionEnvironment": "execution",
            "World.CanonicalCompilation": "canonical",
            "World.EndToEndValidation": "validation",
        }
        for owner, root in roots.items():
            for path in root.rglob("*.py"):
                tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
                for node in ast.walk(tree):
                    if isinstance(node, ast.Import):
                        modules = [alias.name for alias in node.names]
                    elif isinstance(node, ast.ImportFrom):
                        imported_module = node.module or ""
                        modules = [imported_module]
                        dependency_owner = next(
                            (value for module, value in module_owners.items() if imported_module.startswith(module)),
                            None,
                        )
                        if dependency_owner is not None and dependency_owner != owner:
                            self.assertFalse(
                                [alias.name for alias in node.names if alias.name.startswith("_")],
                                f"Private cross-component import in {path}",
                            )
                            if "tests" not in path.relative_to(root).parts:
                                bootstrap_api = "World.ExecutionEnvironment.app.python_environment"
                                self.assertTrue(
                                    imported_module.endswith(".api")
                                    or (path.name == "bootstrap.py" and imported_module == bootstrap_api),
                                    f"Cross-component import bypasses public API in {path}: {imported_module}",
                                )
                    else:
                        continue
                    for module in modules:
                        self.assertFalse(
                            module.startswith(forbidden_modules[owner]),
                            f"Invalid {owner} dependency {module} in {path}",
                        )

    def test_removed_component_names_do_not_return(self) -> None:
        forbidden = (
            "tools/World/" + "Compiler",
            "world_" + "ingestion",
            "world_" + "compiler",
            "bootstrap_" + "world_environment.py",
            "bootstrap_" + "source_ingestion.py",
            "bootstrap_" + "canonical_compilation.py",
            "run_" + "source_ingestion.py",
            "run_" + "canonical_compilation.py",
        )
        for path in WORLD_ROOT.rglob("*"):
            if not path.is_file() or path.suffix not in {".py", ".md", ".json", ".txt"}:
                continue
            text = path.read_text(encoding="utf-8")
            for token in forbidden:
                self.assertNotIn(token, text, f"Stale component name in {path}")


if __name__ == "__main__":
    unittest.main()
