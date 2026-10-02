import contextlib
import importlib.util
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from fnmatch import fnmatchcase
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "prepare_public_source.py"
SPEC = importlib.util.spec_from_file_location("prepare_public_source", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
SANITIZER_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "sanitize_public_text.py"
SANITIZER_SPEC = importlib.util.spec_from_file_location("sanitize_public_text", SANITIZER_PATH)
SANITIZER = importlib.util.module_from_spec(SANITIZER_SPEC)
assert SANITIZER_SPEC.loader is not None
SANITIZER_SPEC.loader.exec_module(SANITIZER)
# The definition the sanitizer consumes; its file is asserted to be governance's.
GOVERNANCE = SANITIZER.machine_local_paths
GOVERNANCE_PATH = REPO_ROOT / "scripts" / "ue" / "check" / "governance" / "machine_local_paths.py"


def machine_path_samples() -> list[str]:
    """One machine-local path per governance rule, assembled at runtime so this
    file carries no literal for the path checks to find."""
    b = "\\"

    def win(*parts: str) -> str:
        return "E" + ":" + b + b.join(parts)

    def fwd(*parts: str) -> str:
        return "E" + ":" + "/" + "/".join(parts)

    version = "UE_" + "5.8"
    home = "/" + "home" + "/someone"
    wsl = b + b + "wsl.localhost" + b + "Ubuntu" + b + "home" + b + "someone"
    tilde = "~" + "/repos_alis/"
    return [
        win("Repos_Alis", "site"), fwd("Repos_Alis", "site"),
        win("Repos_Alis", "Alis"), fwd("Repos_Alis", "Alis"),
        "/mnt/e/Repos_Alis" + "/site", "/mnt/e/Repos_Alis" + "/Alis",
        wsl + b + "repos_alis" + b + "cdn", home + "/repos_alis/cdn",
        tilde + "cdn", tilde + "site", tilde + "Alis", tilde + "notes",
        win("UnrealEngine-" + "5.8"), fwd("UnrealEngine", version),
        win("Program Files (x86)", "Epic Games", version), fwd("Program Files", "Epic Games", version),
        win("Program Files", "Python311", "python.exe"), fwd("Program Files", "Python311", "python.exe"),
        win("Program Files (x86)", "Windows Kits", "10", "Debuggers", "x64", "cdb.exe"),
        fwd("Program Files", "Windows Kits", "10", "Debuggers", "x64", "cdb.exe"),
        win("Symbols"), fwd("Symbols"), win("Builds", "Alis-3.0.0"), fwd("Builds", "Alis-3.0.0"),
        win("Games", "Alis"), fwd("Games", "Alis"),
        win("Users", "someone", "AppData", "Local", "Temp", "x.log"),
        fwd("Users", "someone", "AppData", "Local", "Temp", "x.log"),
        win("Users", "someone", "AppData", "Local", "Alis"),
        fwd("Users", "someone", "AppData", "Local", "Alis"),
        win("Users", "someone"), fwd("Users", "someone"),
        wsl + b + "tools", home + "/tools",
        "%WSL_HOME%" + b + "repos_alis" + b + "cdn", "$HOME" + "/repos_alis/cdn",
    ]

# Markdown the mirror keeps out of public source, and why. Every other tracked
# .md file must survive mirror.exclude: documentation is public by default, and
# private values inside it are fixed at the source instead of excluded.
PRIVATE_MARKDOWN = {
    "AGENTS.md": "agent instructions, maintained apart from the documentation",
    "CLAUDE.md": "agent instructions adapter",
    "CODEX.md": "agent instructions adapter",
    "Plugins/ThirdParty/": "third-party components excluded by their license declaration",
}


def mirror_exclude_rules() -> tuple[list[str], list[str]]:
    # Same parsing as build_filtered_snapshot() in mirror_to_github.sh:
    # fnmatch rules over repo-relative POSIX paths; "!rule" re-includes.
    excluded, kept = [], []
    for raw_line in (REPO_ROOT / "scripts/git/mirror/mirror.exclude").read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        keep = line.startswith("!")
        line = (line[1:] if keep else line).replace("\\", "/")
        (kept if keep else excluded).append(line + "**" if line.endswith("/") else line)
    return excluded, kept


def mirror_excluded(path: str, rules: tuple[list[str], list[str]]) -> bool:
    excluded, kept = rules
    return any(fnmatchcase(path, rule) for rule in excluded) and not any(
        fnmatchcase(path, rule) for rule in kept
    )


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
            "City17",
            "InstanceArrayTool",
            "ProjectIntegrationTests",
            "ProjectOpenableTemplates",
            "ProjectPlacementEditor",
        ):
            plugin = next(item for item in descriptor["Plugins"] if item["Name"] == plugin_name)
            self.assertFalse(plugin["Enabled"])
        # The public worlds are realized by Mesh Terrain; the projection never disables it.
        terrain = next(item for item in descriptor["Plugins"] if item["Name"] == "ProjectWorldMeshTerrain")
        self.assertTrue(terrain["Enabled"])
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
        render.parent.mkdir(parents=True)
        render.write_text("SecurityToken=dummy_private_value\n", encoding="utf-8")

        MODULE.project(self.root)

        self.assertEqual("SecurityToken=<redacted>\n", render.read_text(encoding="utf-8"))

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
        rules = mirror_exclude_rules()

        def excluded(path: str) -> bool:
            return mirror_excluded(path, rules)

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
        self.assertTrue(excluded("scripts/config/ue_path.local.conf"))
        self.assertTrue(excluded("scripts/config/other.local.conf"))
        self.assertFalse(excluded("scripts/config/ue_path.conf.example"))
        self.assertFalse(
            excluded("scripts/config/test/fixtures/02_local_override/ue_path.local.conf")
        )

    def test_mirror_keeps_first_party_markdown_public(self):
        rules = mirror_exclude_rules()
        tracked = subprocess.run(
            ["git", "ls-files", "-z", "--", "*.md"],
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
        ).stdout.decode("utf-8").split("\0")
        hidden = [
            path
            for path in tracked
            if path
            and not path.startswith(tuple(PRIVATE_MARKDOWN))
            and mirror_excluded(path, rules)
        ]
        self.assertEqual([], hidden)

    def test_anonymizer_keeps_generic_environment_variables_in_scripts(self):
        published = {
            "tools/install.bat": "curl -o %TEMP%\\rustup-init.exe\r\necho %%LOCALAPPDATA%%\\Alis\r\n",
            "tools/build.ps1": '$cargoHome = Join-Path $HOME ".cargo"\n',
            "tools/setup.sh": 'export PATH="$HOME/.cargo/bin:$PATH"\n',
            "docs/setup.md": "Add `%USERPROFILE%\\.cargo\\bin` and `%WSL_HOME%\\tools` to PATH.\n",
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for relative, content in published.items():
                (root / relative).parent.mkdir(parents=True, exist_ok=True)
                (root / relative).write_bytes(content.encode("utf-8"))
            SANITIZER.sanitize_tree(root)
            for relative, content in published.items():
                self.assertEqual(content.encode("utf-8"), (root / relative).read_bytes(), relative)
            self.assertEqual([], SANITIZER.residual_machine_paths(root))

    def test_publication_rewrites_docs_and_refuses_code_with_machine_paths(self):
        literal = "E" + ":/Repos_Alis/Alis"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            script = root / "tools" / "run.bat"
            guide = root / "docs" / "guide.md"
            script.parent.mkdir(parents=True)
            guide.parent.mkdir(parents=True)
            script.write_text("set PROJ=" + literal + "/Alis.uproject\n", encoding="utf-8")
            guide.write_text("see " + literal + "/docs\n", encoding="utf-8")
            SANITIZER.sanitize_tree(root)
            self.assertEqual(
                "set PROJ=" + literal + "/Alis.uproject\n", script.read_text(encoding="utf-8")
            )
            self.assertEqual("see <repo>/docs\n", guide.read_text(encoding="utf-8"))
            self.assertEqual([("tools/run.bat", [1])], SANITIZER.residual_machine_paths(root))
            with contextlib.redirect_stderr(io.StringIO()) as stderr:
                status = SANITIZER.main(["sanitize_public_text.py", "--check", str(root)])
            self.assertEqual(1, status)
            self.assertIn("tools/run.bat", stderr.getvalue())

    def test_publication_refuses_every_path_governance_flags(self):
        samples = machine_path_samples()
        # A new governance rule fails here until it has a sample.
        for pattern, _ in GOVERNANCE.RULES:
            self.assertTrue(any(pattern.search(sample) for sample in samples), pattern.pattern)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "tools").mkdir()
            (root / "docs").mkdir()
            for index, sample in enumerate(samples):
                code = "x = '" + sample + "'\n"
                self.assertEqual([1], GOVERNANCE.machine_path_lines(code), sample)
                (root / "tools" / f"s{index}.py").write_text(code, encoding="utf-8")
                (root / "docs" / f"s{index}.md").write_text("see " + sample + " here\n", encoding="utf-8")
            SANITIZER.sanitize_tree(root)
            refused = {path for path, _ in SANITIZER.residual_machine_paths(root)}
        self.assertEqual({f"tools/s{index}.py" for index in range(len(samples))}, refused)

    def test_publication_follows_the_governance_definition(self):
        self.assertEqual(GOVERNANCE_PATH.resolve(), Path(GOVERNANCE.__file__).resolve())
        probe = "Q" + ":/ParityProbe"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "tools").mkdir()
            (root / "tools" / "probe.ps1").write_text("$x = '" + probe + "'\n", encoding="utf-8")
            self.assertEqual([], SANITIZER.residual_machine_paths(root))
            extended = GOVERNANCE.RULES + [(re.compile(re.escape(probe)), "<probe>")]
            with mock.patch.object(GOVERNANCE, "RULES", extended):
                self.assertEqual([("tools/probe.ps1", [1])], SANITIZER.residual_machine_paths(root))
                self.assertEqual("see <probe>\n", SANITIZER.sanitize_text("see " + probe + "\n", ".md"))

    def test_anonymizer_never_rewrites_across_lines(self):
        b = "\\"
        text = (
            "Mount points live under /" + "home/ on Linux.\nSee docs/build.md\n"
            "Profiles live in C:" + b + "Users" + b + " on Windows.\nRun scripts" + b + "x.bat\n"
        )
        self.assertEqual(text, SANITIZER.sanitize_text(text, ".md"))

    def test_anonymizer_replaces_concrete_machine_paths(self):
        b = "\\"
        user = "C:" + b + "Users" + b + "someone"
        cases = {
            user + b + "AppData" + b + "Local" + b + "Temp" + b + "x.log": "%TEMP%" + b + "x.log",
            user + b + ".cargo" + b + "bin": "%USERPROFILE%" + b + ".cargo" + b + "bin",
            "cd " + user: "cd %USERPROFILE%",
            "/" + "home/someone/tools": "$HOME/tools",
            "E:" + b + "Repos_Alis" + b + "Alis" + b + "Saved": "<repo>" + b + "Saved",
            "$HOME" + "/repos_alis/cdn/schema.json": "<cdn-repo>/schema.json",
            "by Alis Team": "by ALIS",
        }
        for text, expected in cases.items():
            self.assertEqual(expected, SANITIZER.sanitize_text(text, ".md"))

if __name__ == "__main__":
    unittest.main()
