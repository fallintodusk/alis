from __future__ import annotations

import json
import os
import shutil
import subprocess
import time
import venv
from contextlib import contextmanager
from pathlib import Path


ENVIRONMENT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = ENVIRONMENT_ROOT.parents[2]
TOOLS_ROOT = ENVIRONMENT_ROOT.parents[1]

from .dependencies import dependency_lock, verify_python_host
from .paths import environment_data_root


BOOTSTRAP_FAILURE_EXIT = 7


class BootstrapFailure(RuntimeError):
    def __init__(self, code: str, message: str, **details: object) -> None:
        super().__init__(message)
        self.code = code
        self.details = details


@contextmanager
def _exclusive_lock(path: Path, timeout_seconds: float = 300.0):
    import msvcrt

    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    if path.stat().st_size == 0:
        handle.write(b"0")
        handle.flush()
    deadline = time.monotonic() + timeout_seconds
    while True:
        try:
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            break
        except OSError:
            if time.monotonic() >= deadline:
                handle.close()
                raise RuntimeError("Timed out waiting for the Python environment lock")
            time.sleep(0.1)
    try:
        yield
    finally:
        handle.seek(0)
        msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        handle.close()


def _environment_python(root: Path) -> Path:
    return root / "Scripts" / "python.exe"


def probe_runtime(python: Path, lock_sha256: str) -> dict[str, object] | None:
    if not python.is_file():
        return None
    probe = (
        "import json,sys; from pathlib import Path; "
        f"sys.path.insert(0, {str(TOOLS_ROOT)!r}); "
        "from World.ExecutionEnvironment.app.dependencies import verify_python_runtime; "
        f"print(json.dumps(verify_python_runtime(Path({str(REPO_ROOT)!r})), sort_keys=True))"
    )
    try:
        result = subprocess.run([str(python), "-c", probe], capture_output=True, text=True, check=False)
    except OSError:
        return None
    if result.returncode != 0:
        return None
    try:
        runtime = json.loads(result.stdout)
        return runtime if runtime["lock_sha256"] == lock_sha256 else None
    except (json.JSONDecodeError, KeyError, TypeError):
        return None


def _environment_ready(environment_root: Path, lock_sha256: str) -> bool:
    receipt_path = environment_root / "installed.json"
    try:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    if receipt.get("status") != "accepted" or receipt.get("lock_sha256") != lock_sha256:
        return False
    return probe_runtime(_environment_python(environment_root), lock_sha256) is not None


def _write_environment_receipt(environment_root: Path, lock_sha256: str, runtime: dict[str, object]) -> None:
    receipt_path = environment_root / "installed.json"
    staging = receipt_path.with_suffix(".json.tmp")
    staging.write_text(json.dumps({
        "status": "accepted",
        "lock_sha256": lock_sha256,
        "python": runtime["python"],
        "packages": runtime["packages"],
    }, sort_keys=True), encoding="utf-8")
    os.replace(staging, receipt_path)


def verify_host() -> dict[str, object]:
    try:
        return verify_python_host(REPO_ROOT)
    except (OSError, RuntimeError, ValueError) as exc:
        raise BootstrapFailure(
            "python_host_invalid", "Python host or dependency lock is unsupported", type=type(exc).__name__
        ) from exc


def ensure_environment() -> Path:
    try:
        lock = dependency_lock(REPO_ROOT)
    except (OSError, ValueError) as exc:
        raise BootstrapFailure(
            "python_lock_invalid", "Python dependency lock is unavailable or invalid", type=type(exc).__name__
        ) from exc
    verify_host()
    python_root = environment_data_root(REPO_ROOT) / "python"
    environment_root = python_root / lock["sha256"]
    try:
        with _exclusive_lock(python_root / "locks" / f"{lock['sha256']}.lock"):
            if _environment_ready(environment_root, lock["sha256"]):
                return _environment_python(environment_root)
            try:
                if environment_root.exists():
                    shutil.rmtree(environment_root)
                venv.EnvBuilder(with_pip=True, clear=True).create(environment_root)
            except (OSError, subprocess.SubprocessError) as exc:
                raise BootstrapFailure(
                    "python_environment_create_failed", "Isolated Python environment creation failed",
                    type=type(exc).__name__,
                ) from exc
            python = _environment_python(environment_root)
            try:
                install = subprocess.run([
                    str(python), "-m", "pip", "install", "--disable-pip-version-check",
                    "--require-hashes", "--only-binary=:all:", "--requirement",
                    str(ENVIRONMENT_ROOT / "requirements.lock.txt"),
                ], capture_output=True, text=True, check=False)
            except OSError as exc:
                raise BootstrapFailure(
                    "python_dependency_install_failed", "Hash-locked Python dependency installation failed",
                    type=type(exc).__name__,
                ) from exc
            if install.returncode != 0:
                raise BootstrapFailure(
                    "python_dependency_install_failed", "Hash-locked Python dependency installation failed",
                    returncode=install.returncode,
                )
            runtime = probe_runtime(python, lock["sha256"])
            if runtime is None:
                raise BootstrapFailure(
                    "python_environment_probe_failed", "Installed Python environment verification failed"
                )
            try:
                _write_environment_receipt(environment_root, lock["sha256"], runtime)
            except OSError as exc:
                raise BootstrapFailure(
                    "python_environment_receipt_failed", "Python environment receipt could not be committed",
                    type=type(exc).__name__,
                ) from exc
            return python
    except BootstrapFailure as exc:
        try:
            if environment_root.exists():
                shutil.rmtree(environment_root)
        except OSError:
            exc.details["cleanup_failed"] = True
        raise
    except (OSError, RuntimeError) as exc:
        raise BootstrapFailure(
            "python_environment_lock_failed", "Python environment lock failed", type=type(exc).__name__
        ) from exc


def failure_result(error: BootstrapFailure) -> dict[str, object]:
    return {
        "status": "failed",
        "operation": "bootstrap-python",
        "error": {"code": error.code, "message": str(error), "details": error.details},
    }


def launch_cli(entrypoint: Path, arguments: list[str], error_code: str) -> int:
    python = ensure_environment()
    try:
        completed = subprocess.run([str(python), str(entrypoint), *arguments], check=False)
    except OSError as exc:
        raise BootstrapFailure(
            error_code, "World tool CLI could not be launched", type=type(exc).__name__
        ) from exc
    return completed.returncode
