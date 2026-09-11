from __future__ import annotations

import json
import sys
from pathlib import Path


COMPILATION_ROOT = Path(__file__).resolve().parent
TOOLS_ROOT = COMPILATION_ROOT.parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

from World.ExecutionEnvironment.app.python_environment import (
    BOOTSTRAP_FAILURE_EXIT,
    BootstrapFailure,
    failure_result,
    launch_cli,
)


def main(argv: list[str] | None = None) -> int:
    arguments = sys.argv[1:] if argv is None else argv
    try:
        return launch_cli(
            COMPILATION_ROOT / "run.py",
            arguments,
            "canonical_compilation_launch_failed",
        )
    except BootstrapFailure as error:
        print(json.dumps(failure_result(error), sort_keys=True), file=sys.stderr)
        return BOOTSTRAP_FAILURE_EXIT
    except Exception as error:
        failure = BootstrapFailure(
            "python_bootstrap_internal_error", "Unexpected Python bootstrap failure", type=type(error).__name__
        )
        print(json.dumps(failure_result(failure), sort_keys=True), file=sys.stderr)
        return BOOTSTRAP_FAILURE_EXIT


if __name__ == "__main__":
    raise SystemExit(main())
