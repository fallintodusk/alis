from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path
from typing import Any

from .contracts import file_hash, read_json, validate_document
from .dependencies import dependency_lock_hash, verify_python_runtime
from .toolchain import require_tools, tool_install_root


def _canonical_hash(value: Any) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def execution_identity(repo_root: Path, verify_native_tools: bool) -> dict[str, Any]:
    environment_root = repo_root / "tools" / "World" / "ExecutionEnvironment"
    toolchain_lock = environment_root / "toolchain.lock.json"
    toolchain_lock_sha256 = file_hash(toolchain_lock)
    implementation_files = [
        environment_root / "api.py",
        environment_root / "bootstrap.py",
        *sorted((environment_root / "app").glob("*.py")),
        *sorted((environment_root / "contracts").glob("*.json")),
    ]
    runtime = verify_python_runtime(repo_root)
    receipt_sha256: str | None = None
    if verify_native_tools:
        require_tools(repo_root)
        receipt_path = tool_install_root(repo_root, toolchain_lock_sha256) / "installed.json"
        receipt = read_json(receipt_path)
        validate_document(receipt, receipt_path)
        receipt_sha256 = file_hash(receipt_path)
    components = {
        "python_runtime": platform.python_version(),
        "python_dependency_lock_sha256": dependency_lock_hash(repo_root),
        "toolchain_lock_sha256": toolchain_lock_sha256,
        "toolchain_receipt_sha256": receipt_sha256,
        "implementation_sha256": _canonical_hash({
            "files": {
                path.relative_to(environment_root).as_posix(): file_hash(path)
                for path in implementation_files
            }
        }),
    }
    return {**components, "identity_sha256": _canonical_hash(components)}
