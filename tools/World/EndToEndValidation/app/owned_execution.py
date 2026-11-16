"""Contain a composed Windows acceptance operation using the shared test Job owner."""
import json
from pathlib import Path
import subprocess


def run(name: str, command: list[str], log_root: Path, timeout: int) -> float:
    from .execution import REPO_ROOT, _powershell, _run

    log_root.mkdir(parents=True, exist_ok=True)
    request_path = log_root / f'{name}.command.json'
    request_path.write_text(json.dumps({
        'executable': command[0], 'arguments': subprocess.list2cmdline(command[1:]),
        'cwd': str(REPO_ROOT), 'log': str(log_root / f'{name}.worker.log'),
    }, indent=2) + '\n', encoding='utf-8')
    # The supervisor owns the deadline, Job termination and drain. A killed
    # supervisor also closes the private Job; the outer backstop cannot orphan writers.
    return _run(name, [_powershell(), '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
        '-File', str(REPO_ROOT / 'scripts/ue/test/lib/run_owned_process.ps1'),
        '-RequestPath', str(request_path), '-TimeoutSeconds', str(timeout)], log_root, timeout=timeout + 30)
