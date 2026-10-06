# Copyright ALIS. All Rights Reserved.
# License terms: see repository root LICENSE.

from __future__ import annotations

import argparse
import json
import sys
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
TEST_DATA = REPO_ROOT / "Plugins" / "World" / "ProjectWorldTestData" / "Data"
SOURCE_PROFILE = TEST_DATA / "Profiles" / "SourceIngestion" / "synthetic_territory_twin.source.json"
COMPILER_PROFILE = (
    TEST_DATA / "Profiles" / "CanonicalCompilation" / "synthetic_territory_twin.compile.json"
)
COMPILER_SCHEMA = "https://alis.world/schemas/world-compiler/compiler-profile-v1.json"

sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.CanonicalCompilation.app.pipeline import compile_world
from World.SourceIngestion.app.cli import main as source_main


def compile_twin(output_root: Path) -> Path:
    output_root = output_root.resolve()
    if not output_root.is_relative_to((REPO_ROOT / "tmp").resolve()):
        raise ValueError("Twin fixture output must stay under the repository tmp root")

    source_output = output_root / "source"
    with redirect_stdout(StringIO()):
        source_exit = source_main([
            "run", "--profile", str(SOURCE_PROFILE), "--output-root", str(source_output)
        ])
    if source_exit != 0:
        raise RuntimeError("Synthetic twin source ingestion failed")

    compiler_profile = json.loads(COMPILER_PROFILE.read_text(encoding="utf-8"))
    compiler_profile["$schema"] = COMPILER_SCHEMA
    base_root = output_root / "base"
    base_root.mkdir(parents=True, exist_ok=True)
    compiler_profile_path = base_root / "compiler_profile.json"
    compiler_profile_path.write_text(
        json.dumps(compiler_profile, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    compile_output = base_root / "compile"
    result, _ = compile_world(
        str(compiler_profile_path),
        source_result=source_output / "run_result.json",
        output_root_value=compile_output,
    )
    if result["status"] != "accepted":
        raise RuntimeError("Synthetic twin canonical compilation failed")
    return compile_output / "compile_result.json"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile the synthetic territory twin fixture")
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()
    print(compile_twin(Path(args.output_root)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
