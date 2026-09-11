from __future__ import annotations

import os
import time
from contextlib import contextmanager
from pathlib import Path

from .contracts import ExecutionEnvironmentError


@contextmanager
def exclusive_file_lock(path: Path, timeout_seconds: float = 120.0):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    if path.stat().st_size == 0:
        handle.write(b"0")
        handle.flush()
    deadline = time.monotonic() + timeout_seconds
    while True:
        try:
            handle.seek(0)
            if os.name == "nt":
                import msvcrt

                msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl

                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except OSError as error:
            if time.monotonic() >= deadline:
                handle.close()
                raise ExecutionEnvironmentError(
                    "file_lock_timeout", "Timed out waiting for an exclusive file lock"
                ) from error
            time.sleep(0.05)
    try:
        yield
    finally:
        handle.seek(0)
        if os.name == "nt":
            import msvcrt

            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
        handle.close()
