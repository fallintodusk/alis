from __future__ import annotations

import sys
from pathlib import Path


COMPONENT_ROOT = Path(__file__).resolve().parent
TOOLS_ROOT = COMPONENT_ROOT.parents[1]
sys.path.insert(0, str(TOOLS_ROOT))

from World.ExecutionEnvironment.app.python_environment import (
    BOOTSTRAP_FAILURE_EXIT,
    BootstrapFailure,
    failure_result,
    launch_cli,
)
from World.EndToEndValidation.app.bootstrap_preflight import capture


def main(argv: list[str] | None = None) -> int:
    try:
        arguments = list(argv or sys.argv[1:])
        if arguments[:1] == ["plan"]:
            from World.EndToEndValidation.app.planning import main as plan_main

            return plan_main(arguments[1:])
        preflight = capture()
        arguments.extend(["--bootstrap-preflight", str(preflight)])
        return launch_cli(COMPONENT_ROOT / "run.py", arguments, "validation_cli_launch_failed")
    except BootstrapFailure as error:
        import json

        print(json.dumps(failure_result(error), ensure_ascii=True, separators=(",", ":")), file=sys.stderr)
        return BOOTSTRAP_FAILURE_EXIT


if __name__ == "__main__":
    raise SystemExit(main())
