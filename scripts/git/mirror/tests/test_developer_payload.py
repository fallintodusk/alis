import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[4]
MODULE_PATH = REPO_ROOT / "scripts" / "git" / "mirror" / "compose_developer_payload.py"
SPEC = importlib.util.spec_from_file_location("compose_developer_payload", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
RECIPE_OWNERS = ("ProjectTexture", "ProjectMaterial")


class DeveloperPayloadTests(unittest.TestCase):
    def test_allow_dirty_skips_checkout_cleanliness_but_not_tracked_admission(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            tracked = root / "tracked.uasset"
            tracked.write_bytes(b"tracked")
            subprocess.run(["git", "-C", str(root), "add", "tracked.uasset"], check=True)
            accepted = MODULE.Entry("tracked.uasset", MODULE.sha256_file(tracked), 7, "asset", "Owner")
            MODULE.ensure_tracked(root, [accepted])

            untracked = root / "untracked.uasset"
            untracked.write_bytes(b"untracked")
            rejected = MODULE.Entry("untracked.uasset", MODULE.sha256_file(untracked), 9, "asset", "Owner")
            with self.assertRaisesRegex(MODULE.PayloadError, "not tracked by git"):
                MODULE.ensure_tracked(root, [rejected])
            with self.assertRaisesRegex(MODULE.PayloadError, "not tracked by git"):
                MODULE.ensure_tracked(root, [rejected], allowed_untracked_paths={"other.uasset"})
            MODULE.ensure_tracked(root, [accepted, rejected], allowed_untracked_paths={"untracked.uasset"})

    def test_generated_source_sha_is_stable_across_checkout_line_endings(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            lf = root / "lf.json"
            crlf = root / "crlf.json"
            lf.write_bytes(b'{"value": 1}\n')
            crlf.write_bytes(b'{"value": 1}\r\n')

            self.assertEqual(MODULE.normalized_json_sha256(lf), MODULE.normalized_json_sha256(crlf))

    def test_active_authority_selects_only_production_generated_assets(self):
        entries = {}
        scopes = MODULE.collect_manifest_authority(REPO_ROOT, "ProjectWorldData", entries)
        canonical = MODULE.collect_canonical_authority(
            REPO_ROOT,
            "ProjectWorldData",
            entries,
            ["kazan_territory_v1", "manhattan_showcase_v1"],
        )

        self.assertTrue(scopes)
        self.assertTrue(canonical)
        self.assertTrue(any(entry.kind == "canonical_bundle" for entry in entries.values()))
        self.assertEqual(
            {"kazan_territory_v1", "manhattan_showcase_v1"},
            {item["profile_id"] for item in canonical},
        )
        self.assertFalse(any("kazan_p0" in entry.path for entry in entries.values()))
        self.assertTrue(
            all(Path(entry.path).suffix.lower() in {".uasset", ".umap", ".zip"} for entry in entries.values())
        )
        for entry in entries.values():
            lowered = entry.path.lower()
            self.assertNotIn("projectworldtestdata", lowered)
            self.assertNotIn("hlod", lowered)

    def test_world_payload_can_select_the_public_manifest_projection(self):
        source_root = REPO_ROOT / "Plugins/World/ProjectWorldData/Data/Manifests"
        active = json.loads((source_root / "active_set.json").read_text(encoding="utf-8-sig"))
        selected_scope = active["scopes"][0]
        with tempfile.TemporaryDirectory() as temp_value:
            manifest_root = Path(temp_value)
            manifest_path = manifest_root / selected_scope["manifest_path"]
            manifest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_root / selected_scope["manifest_path"], manifest_path)
            active["scopes"] = [selected_scope]
            (manifest_root / "active_set.json").write_text(json.dumps(active), encoding="utf-8")

            entries = {}
            accepted_generated_paths = set()
            selected = MODULE.collect_manifest_authority(
                REPO_ROOT, "ProjectWorldData", entries, manifest_root,
                accepted_generated_paths=accepted_generated_paths,
            )

            self.assertEqual([selected_scope["scope_id"]], [item["scope_id"] for item in selected])
            self.assertEqual(
                len(json.loads(manifest_path.read_text(encoding="utf-8-sig"))["artifacts"]),
                len(entries),
            )
            self.assertEqual(set(entries), accepted_generated_paths)

    def test_compose_accepts_only_digest_verified_untracked_projection(self):
        parent = REPO_ROOT / "tmp/release/tests/developer-payload"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temp_value:
            fixture = Path(temp_value)
            source = fixture / "source"
            owner = source / "Plugins/World/ProjectWorldData"
            artifact_relative = "Plugins/World/ProjectWorldData/Content/Generated/Territory/L_Public.umap"
            artifact = source / artifact_relative
            for path, content in (
                (source / "Alis.uproject", "{}"),
                (owner / "ProjectWorldData.uplugin", "{}"),
                (source / MODULE.ASSET_RELEASE_CONTRACT,
                 json.dumps({"world_authorities": [{"owner": "ProjectWorldData", "canonical_profiles": ["fixture"]}]})),
                (source / "LICENSE", "fixture"),
                (source / "LICENSES/MPL-2.0.txt", "fixture"),
                (source / "scripts/ue/package/verify_release.ps1", "fixture"),
            ):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            canonical = owner / "Data/Canonical/fixture"
            canonical.mkdir(parents=True)
            bundle = canonical / "bundle.zip"
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr("reports/attribution.json", "{}")
            (canonical / "active.json").write_text(json.dumps({
                "profile_id": "fixture", "authority_id": "fixture",
                "bundle": {"path": "bundle.zip", "sha256": MODULE.sha256_file(bundle),
                           "byte_size": bundle.stat().st_size},
            }), encoding="utf-8")
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            subprocess.run(["git", "-C", str(source), "add", "."], check=True)
            artifact.parent.mkdir(parents=True)
            artifact.write_bytes(b"projected-world-package")

            manifests = fixture / "projection"
            scope = manifests / "scopes/public.json"
            scope.parent.mkdir(parents=True)
            scope.write_text(json.dumps({"artifacts": [{
                "path": artifact_relative, "digest_kind": "sha256",
                "digest": MODULE.sha256_file(artifact),
            }]}), encoding="utf-8")
            (manifests / "active_set.json").write_text(json.dumps({"scopes": [{
                "scope_id": "public", "manifest_path": "scopes/public.json",
                "manifest_sha256": MODULE.sha256_file(scope),
            }]}), encoding="utf-8")

            def compose(output: str):
                return MODULE.compose(
                    source, fixture / output, "fixture", "vfixture", ["ProjectWorldData"],
                    1, True, "a" * 40, "main", manifests,
                )

            payload = json.loads(compose("accepted").read_text(encoding="utf-8"))
            self.assertIn(artifact_relative, {entry["path"] for entry in payload["entries"]})
            archive = fixture / "accepted" / payload["archive"]["logical_name"]
            with zipfile.ZipFile(archive) as payload_archive:
                self.assertEqual(artifact.read_bytes(), payload_archive.read(artifact_relative))

            artifact.write_bytes(b"changed-after-projection")
            with self.assertRaisesRegex(MODULE.PayloadError, "Generated artifact hash mismatch"):
                compose("wrong-digest")
            artifact.write_bytes(b"projected-world-package")

            other_relative = "Plugins/World/ProjectWorldData/Content/Generated/Territory/Other.umap"
            other = source / other_relative
            other.write_bytes(b"unselected-package")

            def add_other(repo_root, entries):
                MODULE.add_entry(entries, repo_root, other_relative, "generated_asset", "ProjectWorldData")
                return []

            with mock.patch.object(MODULE, "collect_public_asset_authority", side_effect=add_other):
                with self.assertRaisesRegex(MODULE.PayloadError, "Payload authority is not tracked by git"):
                    compose("unselected")

            if shutil.which("wsl.exe"):
                subprocess.run(["git", "-C", str(source), "-c", "user.name=fixture",
                                "-c", "user.email=fixture@localhost", "commit", "-qm", "fixture"], check=True)
                linked = fixture / "linked"
                subprocess.run(["git", "-C", str(source), "worktree", "add", "--quiet",
                                "--detach", str(linked), "HEAD"], check=True)
                try:
                    linked_artifact = linked / artifact_relative
                    linked_artifact.parent.mkdir(parents=True)
                    linked_artifact.write_bytes(artifact.read_bytes())
                    git_dir = subprocess.run(["git", "-C", str(linked), "rev-parse", "--absolute-git-dir"],
                                             check=True, capture_output=True, text=True).stdout.strip()

                    def wsl_path(path: Path | str):
                        return subprocess.run(["wsl.exe", "wslpath", "-a", "'" + str(path) + "'"],
                                              check=True, capture_output=True, text=True).stdout.strip()

                    wsl_output = fixture / "linked-output"
                    result = subprocess.run([
                        "wsl.exe", "env", f"GIT_DIR={wsl_path(git_dir)}",
                        f"GIT_WORK_TREE={wsl_path(linked)}", "python3",
                        wsl_path(MODULE_PATH), "--repo-root", wsl_path(linked),
                        "--output-dir", wsl_path(wsl_output), "--release-version", "fixture",
                        "--public-source-tag", "vfixture", "--public-source-revision", "a" * 40,
                        "--public-source-branch", "main", "--world-manifest-root", wsl_path(manifests),
                        "--owner", "ProjectWorldData", "--part-size-mib", "1", "--allow-dirty",
                    ], capture_output=True, text=True, check=False)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                    linked_payload = json.loads(next(wsl_output.glob("*.developer-payload.json")).read_text())
                    self.assertIn(artifact_relative, {entry["path"] for entry in linked_payload["entries"]})
                finally:
                    subprocess.run(["git", "-C", str(source), "worktree", "remove", "--force",
                                    str(linked)], check=True)

    def test_public_asset_authority_selects_every_generated_definition_pair(self):
        entries = {}
        authorities = MODULE.collect_public_asset_authority(REPO_ROOT, entries)

        counts = {item["owner"]: item["asset_count"] for item in authorities}
        contract = json.loads((REPO_ROOT / MODULE.ASSET_RELEASE_CONTRACT).read_text(encoding="utf-8"))
        expected = {}
        for authority in contract["asset_authorities"]:
            manifest = json.loads((REPO_ROOT / authority["manifest_path"]).read_text(encoding="utf-8-sig"))
            items = "assets" if authority["authority_kind"] == "generated_definition_manifest" else "records"
            expected[authority["owner"]] = (authority["authority_kind"], len(manifest[items]))
        self.assertEqual({owner: count for owner, (_, count) in expected.items()}, counts)
        for kind, entry_kind in (
            ("generated_definition_manifest", "generated_definition_asset"),
            ("generated_recipe_manifest", "generated_recipe_asset"),
        ):
            self.assertEqual(
                sum(count for owner_kind, count in expected.values() if owner_kind == kind),
                sum(entry.kind == entry_kind for entry in entries.values()))
        self.assertEqual(0, sum(entry.kind == "generated_definition_source" for entry in entries.values()))
        self.assertTrue(all(Path(entry.path).suffix.lower() in {".uasset", ".umap"} for entry in entries.values()))
        self.assertTrue(all("thirdparty" not in path.lower() for path in entries))
        self.assertTrue(all("projectworldtestdata" not in path.lower() for path in entries))

    def test_public_asset_authority_rejects_asset_hash_drift(self):
        manifest = json.loads(
            (REPO_ROOT / "Plugins/Resources/ProjectObject/Data/Manifests/public_generated_definitions.json").read_text()
        )
        target = REPO_ROOT / Path(manifest["assets"][0]["artifact_path"])
        original = MODULE.sha256_file

        def sabotaged(path):
            return "0" * 64 if Path(path).resolve() == target.resolve() else original(path)

        with mock.patch.object(MODULE, "sha256_file", side_effect=sabotaged):
            with self.assertRaises(MODULE.PayloadError):
                MODULE.collect_public_asset_authority(REPO_ROOT, {})

    @staticmethod
    def _recipe_authority(owner):
        contract = json.loads((REPO_ROOT / MODULE.ASSET_RELEASE_CONTRACT).read_text(encoding="utf-8"))
        return next(
            item for item in contract["asset_authorities"]
            if item["owner"] == owner and item["authority_kind"] == "generated_recipe_manifest"
        )

    def _write_recipe_fixture(self, root, owners=RECIPE_OWNERS):
        # Copies each owner's accepted manifest, recipe tree, and selected packages into an
        # isolated repository root whose release contract declares only those owners.
        authorities = [self._recipe_authority(owner) for owner in owners]
        for authority in authorities:
            manifest_path = REPO_ROOT / authority["manifest_path"]
            (root / authority["manifest_path"]).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(manifest_path, root / authority["manifest_path"])
            shutil.copytree(REPO_ROOT / authority["recipe_root"], root / authority["recipe_root"])
            manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
            for record in manifest["records"]:
                package = record["output_object_path"].partition(".")[0]
                artifact = f'{authority["artifact_root"]}/{package[len(authority["package_root"]) + 1:]}.uasset'
                (root / artifact).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(REPO_ROOT / artifact, root / artifact)
        contract_path = root / MODULE.ASSET_RELEASE_CONTRACT
        contract_path.parent.mkdir(parents=True, exist_ok=True)
        contract_path.write_text(json.dumps({"asset_authorities": authorities}), encoding="utf-8")
        return {authority["owner"]: authority for authority in authorities}

    def test_recipe_authorities_select_every_accepted_record(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            authorities = self._write_recipe_fixture(root)
            entries = {}

            selected = MODULE.collect_public_asset_authority(root, entries)

            for item in selected:
                manifest = json.loads((root / authorities[item["owner"]]["manifest_path"]).read_text())
                self.assertEqual(len(manifest["records"]), item["asset_count"])
            self.assertEqual(
                sum(item["asset_count"] for item in selected),
                sum(entry.kind == "generated_recipe_asset" for entry in entries.values()))

    def test_recipe_source_digest_ignores_line_endings_and_bom(self):
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            authorities = self._write_recipe_fixture(root)
            recipes = root / authorities["ProjectMaterial"]["recipe_root"]
            recipe = next(recipes.rglob("*" + authorities["ProjectMaterial"]["recipe_suffix"]))
            text = recipe.read_text(encoding="utf-8-sig").replace("\r\n", "\n")
            recipe.write_bytes(b"\xef\xbb\xbf" + text.replace("\n", "\r\n").encode("utf-8"))

            MODULE.collect_public_asset_authority(root, {})

    def test_recipe_authority_rejects_each_drift(self):
        def edit_recipe(root, authorities):
            recipe = next((root / authorities["ProjectMaterial"]["recipe_root"]).rglob("*.surface.json"))
            recipe.write_text(recipe.read_text(encoding="utf-8").replace('"compiler_version"', '"compiler_version" '))

        def add_unrecorded_recipe(root, authorities):
            recipe = next((root / authorities["ProjectMaterial"]["recipe_root"]).rglob("*.surface.json"))
            shutil.copy2(recipe, recipe.with_name("Unrecorded" + recipe.name))

        def remove_recorded_recipe(root, authorities):
            next((root / authorities["ProjectTexture"]["recipe_root"]).rglob("*.pattern.json")).unlink()

        def change_package(root, authorities):
            package = next((root / authorities["ProjectMaterial"]["artifact_root"]).rglob("*.uasset"))
            package.write_bytes(package.read_bytes() + b"\0")

        def change_pattern_hash(root, authorities):
            path = root / authorities["ProjectMaterial"]["manifest_path"]
            manifest = json.loads(path.read_text(encoding="utf-8-sig"))
            manifest["records"][0]["pattern_package_sha256"] = "0" * 64
            path.write_text(json.dumps(manifest), encoding="utf-8")

        cases = (
            ("recipe content", edit_recipe, RECIPE_OWNERS, "differs from its accepted source digest"),
            ("unrecorded recipe", add_unrecorded_recipe, RECIPE_OWNERS, "recipe authority is incomplete"),
            ("missing recipe", remove_recorded_recipe, RECIPE_OWNERS, r"Required payload file is missing: .*\.pattern\.json"),
            ("output package", change_package, RECIPE_OWNERS, "Generated recipe asset hash mismatch"),
            ("missing ProjectTexture authority", None, ("ProjectMaterial",), "not a selected payload package"),
            ("pattern package hash", change_pattern_hash, RECIPE_OWNERS, "not a selected payload package"),
        )
        for name, mutate, owners, message in cases:
            with self.subTest(name), tempfile.TemporaryDirectory() as temp_value:
                root = Path(temp_value)
                authorities = self._write_recipe_fixture(root, owners)
                if mutate is not None:
                    mutate(root, authorities)
                with self.assertRaisesRegex(MODULE.PayloadError, message):
                    MODULE.collect_public_asset_authority(root, {})

    def test_archive_split_is_bounded_and_byte_exact(self):
        with tempfile.TemporaryDirectory() as temp_value:
            archive = Path(temp_value) / "payload.zip"
            expected = bytes(range(35))
            archive.write_bytes(expected)

            parts = MODULE.split_archive(archive, 10)

            self.assertEqual([10, 10, 10, 5], [part.stat().st_size for part in parts])
            self.assertEqual(expected, b"".join(part.read_bytes() for part in parts))
            self.assertFalse(archive.exists())

    def test_unsafe_project_path_is_rejected(self):
        for value in ("../outside", "/rooted", ""):
            with self.subTest(value=value):
                with self.assertRaises(MODULE.PayloadError):
                    MODULE.safe_relative(value)

    def test_compose_rejects_non_public_revision_shape(self):
        with tempfile.TemporaryDirectory() as temp_value:
            with self.assertRaises(MODULE.PayloadError):
                MODULE.compose(
                    REPO_ROOT, Path(temp_value) / "release", "test", "v-test",
                    ["ProjectWorldData"], 1700, True, "private-head", "main",
                )

    def test_mirror_refuses_to_claim_payload_publication_on_direct_push(self):
        git_bash = Path("C:/Program Files/Git/bin/bash.exe")
        bash = str(git_bash) if git_bash.is_file() else shutil.which("bash")
        if not bash:
            self.skipTest("Bash is required")
        result = subprocess.run(
            [
                bash, (REPO_ROOT / "scripts/git/mirror/mirror_to_github.sh").as_posix(),
                "--push", "--remote-url", "https://example.invalid/alis.git",
                "--developer-release-dir", "unused", "--developer-version", "v-test",
            ],
            capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Developer payload publication is not implemented", result.stderr)

    def test_public_credential_patterns_match_literals_not_code_expressions(self):
        git_bash = Path("C:/Program Files/Git/bin/bash.exe")
        bash = str(git_bash) if git_bash.is_file() else shutil.which("bash")
        if not bash:
            self.skipTest("Bash is required")
        pattern_source = REPO_ROOT / "scripts/git/mirror/forbidden_text_patterns.regex"
        patterns = "\n".join(
            line for line in pattern_source.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        )
        with tempfile.TemporaryDirectory() as temp_value:
            pattern_file = Path(temp_value) / "patterns.regex"
            pattern_file.write_text(patterns + "\n", encoding="utf-8")

            # A non-login shell keeps the result independent of the caller's profile.
            def grep_patterns(value):
                return subprocess.run(
                    [bash, "-c", 'grep -E -f "$1"', "bash", pattern_file.as_posix()],
                    input=value + "\n", text=True, capture_output=True, check=False, timeout=30,
                )

            for value in (
                "SECURITY_TOKEN=abcdefgh",
                'SecurityToken = "dummy_private_value"',
                "$env:API_KEY='12345678'",
            ):
                with self.subTest(match=value):
                    result = grep_patterns(value)
                    self.assertEqual(0, result.returncode, result.stderr)
            for value in (
                "token = uuid.uuid4().hex",
                "token = widen_token(line, hit)",
                'CONTENT_LOCK_TOKEN_ENV = "PROJECT_GENERATED_CONTENT_LOCK_TOKEN"',
            ):
                with self.subTest(no_match=value):
                    result = grep_patterns(value)
                    self.assertEqual(1, result.returncode, result.stderr)

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_is_complete_and_conflict_safe(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            temp_root = Path(temp_value)
            seed = temp_root / "seed"
            revision = self._write_project_checkout(seed)
            release = temp_root / "release"
            MODULE.compose(REPO_ROOT, release, "installer-test", "installer-test", ["ProjectWorldData"], 1, True, revision, "main")
            developer_release = release / "developer"
            developer_release.mkdir()
            for path in list(release.iterdir()):
                if path != developer_release:
                    path.replace(developer_release / path.name)
            subprocess.run(["git", "tag", "installer-test"], cwd=seed, check=True)
            manifest_path = next(developer_release.glob("*.developer-payload.json"))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(2, manifest["schema_version"])
            self.assertEqual("installer-test", manifest["release_version"])
            self.assertEqual("installer-test", manifest["public_source"]["tag"])
            self.assertEqual(revision, manifest["public_source"]["revision"])
            self.assertGreater(len(manifest["archive"]["parts"]), 1)
            project = temp_root / "project"
            subprocess.run(["git", "clone", "-q", str(seed), str(project)], check=True)
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertIn("[INFO] Release signature was not required", result.stdout)
            self.assertNotIn("WARNING:", result.stdout + result.stderr)
            self.assertTrue(all((project / Path(entry["path"])).is_file() for entry in manifest["entries"]))

            conflict = temp_root / "conflict"
            self._write_project_checkout(conflict)
            bad_path = conflict / Path(manifest["entries"][0]["path"])
            bad_path.parent.mkdir(parents=True, exist_ok=True)
            bad_path.write_bytes(b"conflict")
            subprocess.run(["git", "add", "."], cwd=conflict, check=True)
            subprocess.run(["git", "commit", "-q", "-m", "conflicting public source"], cwd=conflict, check=True)
            conflict_revision = subprocess.run(
                ["git", "rev-parse", "HEAD"], cwd=conflict, check=True, capture_output=True, text=True
            ).stdout.strip()
            subprocess.run(["git", "tag", "conflict-test"], cwd=conflict, check=True)
            conflict_release = temp_root / "conflict-release"
            MODULE.compose(
                REPO_ROOT, conflict_release, "conflict-test", "conflict-test", ["ProjectWorldData"], 1700,
                True, conflict_revision, "main",
            )
            conflict_manifest = json.loads(next(conflict_release.glob("*.developer-payload.json")).read_text())
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(conflict / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(conflict), "-ReleaseDir", str(conflict_release),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(0, result.returncode)
            present = [entry for entry in conflict_manifest["entries"] if (conflict / Path(entry["path"])).exists()]
            self.assertEqual(1, len(present))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_wrong_public_revision_before_copy(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            subprocess.run(["git", "tag", "revision-test"], cwd=project, check=True)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "revision-test", "revision-test", ["ProjectWorldData"], 1700, True, "f" * 40, "main")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertIn(revision, result.stderr + result.stdout)
            manifest = json.loads(next(release.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_source_installer_never_executes_downloaded_verifier(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            subprocess.run(["git", "tag", "trust-test"], cwd=project, check=True)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "trust-test", "trust-test", ["ProjectWorldData"], 1700, True, revision, "main")
            marker = root / "downloaded-verifier-executed"
            (release / "VERIFY_RELEASE.ps1").write_text(f"Set-Content -Path '{marker}' -Value bad\n", encoding="utf-8")
            (release / "SHA256SUMS.txt").write_text("invalid\n", encoding="ascii")
            (release / "SHA256SUMS.txt.asc").write_text("invalid\n", encoding="ascii")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release), "-RequireReleaseSignature",
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(marker.exists())

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_manifest_outside_authenticated_release(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            authenticated = root / "authenticated-release"
            outside = root / "outside" / "untrusted.developer-payload.json"
            verifier_marker = root / "trusted-verifier-ran"
            outside.parent.mkdir()
            outside.write_text("not valid json", encoding="ascii")
            marker_value = str(verifier_marker).replace("'", "''")
            verifier = (
                "param([string]$ReleaseDir)\n"
                f"Set-Content -LiteralPath '{marker_value}' -Value verified\n"
                "& $env:ComSpec /c exit 0\n"
            )
            revision = self._write_project_checkout(project, verifier)
            subprocess.run(["git", "tag", "boundary-test"], cwd=project, check=True)
            MODULE.compose(
                REPO_ROOT, authenticated, "boundary-test", "boundary-test", ["ProjectWorldData"], 1700,
                True, revision, "main",
            )
            (authenticated / "SHA256SUMS.txt").write_text("test fixture\n", encoding="ascii")
            (authenticated / "SHA256SUMS.txt.asc").write_text("test fixture\n", encoding="ascii")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(authenticated),
                    "-ManifestPath", str(outside), "-RequireReleaseSignature",
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(verifier_marker.exists())
            self.assertIn("ManifestPath must be inside", result.stderr + result.stdout)
            manifest = json.loads(next(authenticated.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @unittest.skipUnless(shutil.which("powershell.exe") or shutil.which("pwsh"), "PowerShell is required")
    def test_installer_rejects_missing_public_tag_before_copy(self):
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        with tempfile.TemporaryDirectory() as temp_value:
            root = Path(temp_value)
            project = root / "project"
            revision = self._write_project_checkout(project)
            release = root / "release"
            MODULE.compose(REPO_ROOT, release, "missing-tag", "missing-tag", ["ProjectWorldData"], 1700, True, revision, "main")
            result = subprocess.run(
                [
                    powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                    str(project / "scripts/git/mirror/install_developer_payload.ps1"),
                    "-ProjectRoot", str(project), "-ReleaseDir", str(release),
                ],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertIn("missing-tag", result.stderr + result.stdout)
            manifest = json.loads(next(release.glob("*.developer-payload.json")).read_text())
            self.assertFalse(any((project / Path(entry["path"])).exists() for entry in manifest["entries"]))

    @staticmethod
    def _write_project_checkout(project: Path, verifier_text: str | None = None) -> str:
        project.mkdir(parents=True)
        shutil.copy2(REPO_ROOT / "Alis.uproject", project / "Alis.uproject")
        plugin = project / "Plugins" / "World" / "ProjectWorldData"
        plugin.mkdir(parents=True)
        shutil.copy2(
            REPO_ROOT / "Plugins" / "World" / "ProjectWorldData" / "ProjectWorldData.uplugin",
            plugin / "ProjectWorldData.uplugin",
        )
        object_plugin = project / "Plugins" / "Resources" / "ProjectObject"
        object_plugin.mkdir(parents=True)
        shutil.copy2(
            REPO_ROOT / "Plugins" / "Resources" / "ProjectObject" / "ProjectObject.uplugin",
            object_plugin / "ProjectObject.uplugin",
        )
        for owner in ("ProjectExperienceData", *RECIPE_OWNERS):
            owner_plugin = project / "Plugins" / "Resources" / owner
            owner_plugin.mkdir(parents=True)
            shutil.copy2(
                REPO_ROOT / "Plugins" / "Resources" / owner / f"{owner}.uplugin",
                owner_plugin / f"{owner}.uplugin",
            )
        mirror = project / "scripts/git/mirror"
        package = project / "scripts/ue/package"
        mirror.mkdir(parents=True)
        package.mkdir(parents=True)
        shutil.copy2(REPO_ROOT / "scripts/git/mirror/install_developer_payload.ps1", mirror)
        if verifier_text is None:
            shutil.copy2(REPO_ROOT / "scripts/ue/package/verify_release.ps1", package)
        else:
            (package / "verify_release.ps1").write_text(verifier_text, encoding="ascii")
        subprocess.run(["git", "init", "-q"], cwd=project, check=True)
        subprocess.run(["git", "config", "core.autocrlf", "false"], cwd=project, check=True)
        subprocess.run(["git", "config", "user.name", "payload-test"], cwd=project, check=True)
        subprocess.run(["git", "config", "user.email", "payload-test@localhost"], cwd=project, check=True)
        subprocess.run(["git", "add", "."], cwd=project, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "public source fixture"], cwd=project, check=True)
        return subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=project, check=True, capture_output=True, text=True
        ).stdout.strip()



if __name__ == "__main__":
    unittest.main()
