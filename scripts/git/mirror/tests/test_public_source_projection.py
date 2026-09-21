import importlib.util
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from fnmatch import fnmatchcase
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "prepare_public_source.py"
SPEC = importlib.util.spec_from_file_location("prepare_public_source", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class PublicSourceProjectionTests(unittest.TestCase):

    def test_public_front_door_routes_and_commands_exist(self):
        missing = []
        for relative in (
            "README.md",
            "docs/README.md",
            "docs/quickstart/README.md",
            "docs/quickstart/player/README.md",
            "docs/quickstart/developer/README.md",
            "docs/quickstart/developer/world-generation.md",
            "SECURITY.md",
        ):
            document = REPO_ROOT / relative
            text = document.read_text(encoding="utf-8")
            local_targets = {
                match.group(1).split("#", 1)[0]
                for match in re.finditer(r"\]\(([^)]+)\)", text)
                if not match.group(1).startswith(("http://", "https://", "mailto:"))
            }
            missing.extend(
                f"{relative}: {target}"
                for target in local_targets
                if target and not (document.parent / target).exists()
            )
        self.assertEqual([], missing)
        for required in (
            "scripts/git/mirror/install_developer_payload.ps1",
            "scripts/config/ue_path.conf.example",
            "scripts/ue/standalone/build.ps1",
            "Plugins/World/README.md",
            *MODULE.REQUIRED_PUBLIC_COMMUNITY_PATHS,
        ):
            self.assertTrue((REPO_ROOT / required).is_file(), required)

    def test_official_release_refuses_noncanonical_remote_before_network_access(self):
        bash = Path("C:/Program Files/Git/bin/bash.exe")
        executable = str(bash) if bash.is_file() else shutil.which("bash")
        if not executable:
            self.skipTest("Bash is required")
        script = REPO_ROOT / "scripts/git/mirror/mirror_to_github.sh"
        result = subprocess.run(
            [
                executable,
                str(script),
                "--dry-run",
                "--remote-url",
                "https://github.com/example/alis.git",
                "--branch",
                "release/v2.0.0-source",
                "--official-release",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Official release remote must be fallintodusk/alis", result.stderr)

    def test_official_release_requires_public_world_authority_before_network_access(self):
        bash = Path("C:/Program Files/Git/bin/bash.exe")
        executable = str(bash) if bash.is_file() else shutil.which("bash")
        if not executable:
            self.skipTest("Bash is required")
        script = REPO_ROOT / "scripts" / "git" / "mirror" / "mirror_to_github.sh"
        result = subprocess.run(
            [
                executable,
                str(script),
                "--dry-run",
                "--remote-url",
                "https://github.com/fallintodusk/alis.git",
                "--branch",
                "release/v2.0.0-source",
                "--official-release",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Official release requires --public-world-manifest-root", result.stderr)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "Config").mkdir()
        for relative in (
            ".gitignore",
            "README.md",
            "SECURITY.md",
            "Alis.uproject",
            "Config/DefaultEngine.ini",
            "Config/DefaultGame.ini",
            *MODULE.REQUIRED_PUBLIC_COMMUNITY_PATHS,
        ):
            source = REPO_ROOT / relative
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)

    def tearDown(self):
        self.temp.cleanup()

    def test_projection_disables_private_plugin_and_routes_to_public_worlds(self):
        MODULE.project(self.root)

        descriptor = json.loads((self.root / "Alis.uproject").read_text(encoding="utf-8"))
        for plugin_name in (
            "InstanceArrayTool",
            "ProjectIntegrationTests",
            "ProjectOpenableTemplates",
            "ProjectPlacementEditor",
        ):
            plugin = next(item for item in descriptor["Plugins"] if item["Name"] == plugin_name)
            self.assertFalse(plugin["Enabled"])
        config = (self.root / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8")
        config += (self.root / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
        self.assertIn(MODULE.KAZAN_MAP, config)
        self.assertIn(MODULE.MANHATTAN_MAP, config)
        self.assertIn("EntryPointExperience=KazanTerritory", config)
        self.assertNotIn("SecurityToken=", config)
        self.assertNotIn("/ProjectAudio/System/MasterSoundClass", config)
        for token in MODULE.FORBIDDEN_PUBLIC_CONFIG_TOKENS:
            self.assertNotIn(token, config)
        ignore = (self.root / ".gitignore").read_text(encoding="utf-8")
        for pattern in MODULE.PUBLIC_BINARY_IGNORE_LINES:
            self.assertIn(pattern, ignore)
        readme = (self.root / "README.md").read_text(encoding="utf-8")
        self.assertIn(MODULE.PUBLIC_GITHUB_ROOT + "/releases/latest", readme)
        self.assertIn("[Security Policy](SECURITY.md)", readme)
        self.assertTrue((self.root / "SECURITY.md").is_file())
        for relative_path in MODULE.REQUIRED_PUBLIC_COMMUNITY_PATHS:
            self.assertTrue((self.root / relative_path).is_file(), relative_path)
        self.assertNotIn("https://github.com/<user>/alis", readme)

    def test_projection_refuses_missing_security_policy(self):
        (self.root / "SECURITY.md").unlink()

        with self.assertRaisesRegex(MODULE.ProjectionError, "security policy is absent"):
            MODULE.project(self.root)

    def test_projection_refuses_missing_public_community_file(self):
        missing_path = MODULE.REQUIRED_PUBLIC_COMMUNITY_PATHS[-1]
        (self.root / missing_path).unlink()

        with self.assertRaisesRegex(
            MODULE.ProjectionError,
            f"Required public community file is absent: {re.escape(missing_path)}",
        ):
            MODULE.project(self.root)

    def test_projection_repairs_sanitized_public_project_routes(self):
        path = self.root / "README.md"
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                MODULE.PUBLIC_GITHUB_ROOT, "https://github.com/<user>/alis"
            ),
            encoding="utf-8",
        )

        MODULE.project(self.root)

        readme = path.read_text(encoding="utf-8")
        for suffix in (".git", "/releases/latest"):
            self.assertIn(MODULE.PUBLIC_GITHUB_ROOT + suffix, readme)
        self.assertIn("[Security Policy](SECURITY.md)", readme)
        self.assertNotIn("https://github.com/<user>/alis", readme)

    def test_projection_redacts_private_documentation_examples(self):
        render = self.root / "docs/config/render/render.md"
        automation = self.root / "docs/testing/automation.md"
        render.parent.mkdir(parents=True)
        automation.parent.mkdir(parents=True)
        render.write_text("SecurityToken=dummy_private_value\n", encoding="utf-8")
        automation.write_text(
            r"-Project=E:\\Repos_Alis\\Alis\\Alis.uproject" + "\n",
            encoding="utf-8",
        )

        MODULE.project(self.root)

        self.assertEqual("SecurityToken=<redacted>\n", render.read_text(encoding="utf-8"))
        self.assertEqual("-Project=<repo>\\\\Alis.uproject\n", automation.read_text(encoding="utf-8"))

    def test_projection_is_idempotent(self):
        MODULE.project(self.root)
        before = {path: path.read_bytes() for path in self.root.rglob("*") if path.is_file()}
        MODULE.project(self.root)
        after = {path: path.read_bytes() for path in self.root.rglob("*") if path.is_file()}
        self.assertEqual(before, after)

    def test_verifier_rejects_reenabled_private_plugin(self):
        MODULE.project(self.root)
        path = self.root / "Alis.uproject"
        descriptor = json.loads(path.read_text(encoding="utf-8"))
        next(item for item in descriptor["Plugins"] if item["Name"] == "InstanceArrayTool")["Enabled"] = True
        path.write_text(json.dumps(descriptor), encoding="utf-8")
        with self.assertRaises(MODULE.ProjectionError):
            MODULE.verify(self.root)

    def test_world_manifest_projection_replaces_private_scope_set(self):
        source = self.root / "public-manifests"
        (source / "scopes").mkdir(parents=True)
        scopes = []
        ids = [
            *(f"layer_kazan_territory_public_v1_{name}" for name in ("terrain", "water", "roads", "buildings")),
            *(f"layer_manhattan_showcase_public_v1_{name}" for name in ("terrain", "water", "roads", "buildings")),
            "map_territory_l_projectworldkazanterritory",
            "map_showcase_manhattan_l_projectworldmanhattanshowcase",
            "presentation_kazan_representative_v1",
        ]
        for scope_id in ids:
            relative = f"scopes/{scope_id}.1.json"
            (source / relative).write_text(json.dumps({"scope_id": scope_id}), encoding="utf-8")
            scopes.append({"scope_id": scope_id, "manifest_path": relative})
        (source / "active_set.json").write_text(json.dumps({"scopes": scopes}), encoding="utf-8")
        target = self.root / "Plugins/World/ProjectWorldData/Data/Manifests/scopes"
        target.mkdir(parents=True)
        (target / "private.json").write_text("{}", encoding="utf-8")

        MODULE.project_world_manifests(self.root, source)

        self.assertFalse((target / "private.json").exists())
        self.assertEqual(11, len(list(target.glob("*.json"))))
        active = json.loads((target.parent / "active_set.json").read_text(encoding="utf-8"))
        self.assertEqual(
            "../../../ProjectWorld/Data/Schemas/project_world_active_manifest_set.schema.json",
            active["$schema"],
        )
        for scope in active["scopes"]:
            manifest_path = target.parent / scope["manifest_path"]
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(
                "../../../../ProjectWorld/Data/Schemas/project_world_generated_manifest.schema.json",
                manifest["$schema"],
            )
            self.assertEqual(hashlib.sha256(manifest_path.read_bytes()).hexdigest(), scope["manifest_sha256"])

    def test_generated_definition_projection_rebinds_public_source_bytes(self):
        source = self.root / "Data/Definition.json"
        source.parent.mkdir(parents=True)
        source.write_bytes(b'{\r\n  "id": "example"\r\n}')
        normalized = b'{\n  "id": "example"\n}'
        manifest_path = self.root / "Data/manifest.json"
        manifest_path.write_text(
            json.dumps(
                {
                    "assets": [
                        {
                            "source_path": "Data/Definition.json",
                            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                            "source_json_hash_md5": hashlib.md5(
                                normalized, usedforsecurity=False
                            ).hexdigest(),
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        contract = self.root / "scripts/git/mirror/developer_asset_release.json"
        contract.parent.mkdir(parents=True)
        contract.write_text(
            json.dumps(
                {
                    "asset_authorities": [
                        {
                            "authority_kind": "generated_definition_manifest",
                            "manifest_path": "Data/manifest.json",
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )

        MODULE.rebind_generated_definition_sources(self.root)

        self.assertEqual(normalized, source.read_bytes())
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(
            hashlib.sha256(normalized).hexdigest(), manifest["assets"][0]["source_sha256"]
        )

    def test_mirror_keeps_project_skills_and_excludes_claude_local_state(self):
        patterns = []
        for raw_line in (REPO_ROOT / "scripts/git/mirror/mirror.exclude").read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if line and not line.startswith("#"):
                patterns.append(line + "**" if line.endswith("/") else line)

        def excluded(path: str) -> bool:
            return any(fnmatchcase(path, pattern) for pattern in patterns)

        self.assertFalse(excluded(".claude/skills/world-engineering/SKILL.md"))
        self.assertFalse(excluded(".claude/skills/world-engineering/agents/openai.yaml"))
        for relative_path in MODULE.REQUIRED_PUBLIC_COMMUNITY_PATHS:
            self.assertFalse(excluded(relative_path), relative_path)
        self.assertTrue(excluded(".claude/settings.json"))
        self.assertTrue(excluded(".claude/settings.local.json.backup"))
        self.assertTrue(excluded(".claude/scheduled_tasks.lock"))
        self.assertTrue(excluded("Plugins/Gameplay/ProjectGAS/Content/.gitkeep"))
        self.assertTrue(
            excluded("Plugins/UI/ProjectUI/Content/Slate/Fonts/game-icons.css")
        )
        self.assertTrue(
            excluded("Plugins/World/City17/Content/Maps/City17_Persistent.ini")
        )
        self.assertFalse(
            excluded("Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json")
        )


if __name__ == "__main__":
    unittest.main()
