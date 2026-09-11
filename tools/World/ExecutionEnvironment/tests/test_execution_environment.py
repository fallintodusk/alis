from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "tools"
ENVIRONMENT_ROOT = REPO_ROOT / "tools" / "World" / "ExecutionEnvironment"
sys.path.insert(0, str(TOOLS_ROOT))

from World.ExecutionEnvironment.app.contracts import ExecutionEnvironmentError, validate_document
from World.ExecutionEnvironment.app.dependencies import dependency_lock
from World.ExecutionEnvironment.app.identity import execution_identity
from World.ExecutionEnvironment.app.file_lock import exclusive_file_lock
from World.ExecutionEnvironment.app.python_environment import BootstrapFailure, ensure_environment
import World.ExecutionEnvironment.app.python_environment as python_environment
from World.ExecutionEnvironment.app.toolchain import (
    bootstrap_tools,
    require_tools,
    run_tool,
    tool_environment,
    tool_install_root,
)
import World.ExecutionEnvironment.app.toolchain as toolchain


class ExecutionEnvironmentTests(unittest.TestCase):
    def test_exclusive_file_lock_serializes_writers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            lock_path = Path(directory) / "shared.lock"
            first_acquired = threading.Event()
            release_first = threading.Event()
            second_acquired = threading.Event()

            def first() -> None:
                with exclusive_file_lock(lock_path):
                    first_acquired.set()
                    release_first.wait(2.0)

            def second() -> None:
                first_acquired.wait(2.0)
                with exclusive_file_lock(lock_path):
                    second_acquired.set()

            workers = [threading.Thread(target=first), threading.Thread(target=second)]
            for worker in workers:
                worker.start()
            self.assertTrue(first_acquired.wait(2.0))
            self.assertFalse(second_acquired.wait(0.1))
            release_first.set()
            self.assertTrue(second_acquired.wait(2.0))
            for worker in workers:
                worker.join()

    def test_execution_identity_covers_runtime_locks_and_implementation(self) -> None:
        identity = execution_identity(REPO_ROOT, False)
        self.assertEqual(sys.version.split()[0], identity["python_runtime"])
        self.assertEqual(dependency_lock(REPO_ROOT)["sha256"], identity["python_dependency_lock_sha256"])
        self.assertEqual(64, len(identity["toolchain_lock_sha256"]))
        self.assertEqual(64, len(identity["implementation_sha256"]))
        self.assertEqual(64, len(identity["identity_sha256"]))
        self.assertIsNone(identity["toolchain_receipt_sha256"])

    def test_dependency_lock_is_hash_and_license_complete(self) -> None:
        lock = dependency_lock(REPO_ROOT)
        self.assertEqual("3.14", lock["metadata"]["python-version"])
        self.assertEqual("windows-x86_64", lock["metadata"]["platform"])
        self.assertTrue(all(item["license"] == "MIT" and len(item["sha256"]) == 64 for item in lock["packages"]))
        completed = subprocess.run(
            [sys.executable, str(ENVIRONMENT_ROOT / "bootstrap.py"), "--check"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)

    def test_failed_python_install_removes_incomplete_environment(self) -> None:
        lock = dependency_lock(REPO_ROOT)
        with tempfile.TemporaryDirectory() as directory:
            fake_repo = Path(directory)
            environment_root = fake_repo / "tmp" / "world" / "execution_environment" / "python" / lock["sha256"]

            def fake_create(target: Path) -> None:
                target.mkdir(parents=True)
                (target / "Scripts").mkdir()
                (target / "Scripts" / "python.exe").write_bytes(b"incomplete")

            failed_install = subprocess.CompletedProcess([], 1, stdout="", stderr="offline")
            with (
                patch.object(python_environment, "REPO_ROOT", fake_repo),
                patch.object(python_environment, "dependency_lock", return_value=lock),
                patch.object(python_environment, "verify_host", return_value=lock),
                patch.object(python_environment.venv.EnvBuilder, "create", side_effect=fake_create),
                patch.object(python_environment.subprocess, "run", return_value=failed_install),
            ):
                with self.assertRaises(BootstrapFailure) as context:
                    ensure_environment()
            self.assertEqual("python_dependency_install_failed", context.exception.code)
            self.assertFalse(environment_root.exists())

    def test_gdal_plugins_are_disabled_and_builtin_drivers_remain(self) -> None:
        require_tools(REPO_ROOT)
        environment, tools = tool_environment(REPO_ROOT)
        self.assertEqual("disable", environment["GDAL_DRIVER_PATH"])
        self.assertEqual("disable", environment["GDAL_PYTHON_DRIVER_PATH"])
        self.assertTrue(tools["gdalbuildvrt"].is_file())
        self.assertTrue(tools["gdal_rasterize"].is_file())
        self.assertIn("GDAL 3.11.5", run_tool(REPO_ROOT, "gdalbuildvrt", ["--version"]))
        formats = run_tool(REPO_ROOT, "gdalinfo", ["--formats"])
        self.assertIn("GTiff", formats)
        self.assertIn("COG", formats)

    def test_tool_install_is_lock_addressed_and_concurrent_safe(self) -> None:
        self.assertNotEqual(tool_install_root(REPO_ROOT, "a" * 64), tool_install_root(REPO_ROOT, "b" * 64))
        receipts: list[dict] = []
        errors: list[Exception] = []

        def bootstrap() -> None:
            try:
                receipts.append(bootstrap_tools(REPO_ROOT))
            except Exception as error:
                errors.append(error)

        workers = [threading.Thread(target=bootstrap) for _ in range(2)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join()
        self.assertFalse(errors)
        self.assertEqual(1, len({receipt["lock_sha256"] for receipt in receipts}))

    def test_tool_promotion_tolerates_a_transient_windows_file_lock(self) -> None:
        original_replace = toolchain.os.replace
        attempts = 0

        def locked_then_available(source: Path, target: Path) -> None:
            nonlocal attempts
            attempts += 1
            if attempts <= 4:
                raise PermissionError("scanner still owns a newly extracted file")
            original_replace(source, target)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            staging = root / "staging"
            install = root / "installs" / "digest"
            staging.mkdir()
            (staging / "probe.txt").write_text("ready", encoding="utf-8")
            with (
                patch.object(toolchain.os, "replace", side_effect=locked_then_available),
                patch.object(toolchain.time, "sleep"),
            ):
                toolchain._promote_install(staging, install)

            self.assertEqual(5, attempts)
            self.assertEqual("ready", (install / "probe.txt").read_text(encoding="utf-8"))

    def test_require_tools_rejects_receipt_that_differs_from_verified_state(self) -> None:
        lock_sha256 = "a" * 64
        lock = {"components": []}
        probes = [{"component_id": "verified"}]
        with tempfile.TemporaryDirectory() as directory:
            install_root = Path(directory)
            receipt_path = install_root / "installed.json"
            receipt_path.write_text(json.dumps({"lock_sha256": lock_sha256, "components": probes}), encoding="utf-8")
            with (
                patch.object(toolchain, "_load_lock", return_value=(lock, lock_sha256)),
                patch.object(toolchain, "tool_install_root", return_value=install_root),
                patch.object(toolchain, "validate_document"),
                patch.object(toolchain, "_probe_components", return_value=probes),
            ):
                toolchain._VERIFIED_TOOL_LOCKS.clear()
                require_tools(REPO_ROOT)
                toolchain._VERIFIED_TOOL_LOCKS.clear()
                receipt_path.write_text(
                    json.dumps({"lock_sha256": lock_sha256, "components": [{"component_id": "stale"}]}),
                    encoding="utf-8",
                )
                with self.assertRaises(ExecutionEnvironmentError):
                    require_tools(REPO_ROOT)
        toolchain._VERIFIED_TOOL_LOCKS.clear()

    def test_bootstrap_repairs_corrupt_installed_receipt(self) -> None:
        lock_sha256 = "b" * 64
        lock = {"platform": "windows-x86_64", "components": []}
        with tempfile.TemporaryDirectory() as directory:
            fake_repo = Path(directory)
            install_root = tool_install_root(fake_repo, lock_sha256)
            install_root.mkdir(parents=True)
            (install_root / "installed.json").write_text("{broken", encoding="utf-8")

            def plain_write(path: Path, value: dict) -> None:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(value), encoding="utf-8")

            with (
                patch.object(toolchain, "_load_lock", return_value=(lock, lock_sha256)),
                patch.object(toolchain, "_probe_components", return_value=[]),
                patch.object(toolchain, "write_json", side_effect=plain_write),
            ):
                receipt = bootstrap_tools(fake_repo)
            self.assertEqual(lock_sha256, receipt["lock_sha256"])
            self.assertEqual(lock_sha256, json.loads((install_root / "installed.json").read_text())["lock_sha256"])
        toolchain._VERIFIED_TOOL_LOCKS.clear()

    def test_every_environment_json_declares_a_schema(self) -> None:
        for path in ENVIRONMENT_ROOT.rglob("*.json"):
            with self.subTest(path=path):
                value = json.loads(path.read_text(encoding="utf-8"))
                self.assertIn("$schema", value)
                if path.parent.name != "contracts":
                    validate_document(value, path)


if __name__ == "__main__":
    unittest.main()
