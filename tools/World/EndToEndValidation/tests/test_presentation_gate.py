from __future__ import annotations

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from World.EndToEndValidation.app.contracts import ValidationFailure, load_profile, validate_against
from World.EndToEndValidation.app.package_gate import _validate_fresh_world_partition_cook
from World.EndToEndValidation.app.presentation import (
    _command,
    _launch_executable,
    _shipping_executable,
    _verify_measured_process,
)
from World.EndToEndValidation.app.validation import _validate_package_map_argument


class PresentationGateTests(unittest.TestCase):
    def test_representative_profile_pins_rendered_gate(self) -> None:
        profile = load_profile(
            REPO_ROOT
            / "Plugins"
            / "World"
            / "ProjectWorldData"
            / "Data"
            / "Profiles"
            / "EndToEndValidation"
            / "representative_v1.validation.json"
        )
        gate = profile["presentation_gate"]
        self.assertEqual("kazan", gate["world_profile"])
        self.assertEqual([1920, 1080], gate["resolution"])
        self.assertEqual(
            ["overview", "terrain_oblique", "road_oblique"],
            gate["camera_roles"],
        )

    def test_packaged_command_is_rendered_and_identity_pinned(self) -> None:
        command = _command(
            Path("C:/package/Alis.exe"),
            "run-test",
            Path("C:/evidence/presentation_gate.json"),
            "/ProjectWorldTestData/Generated/Representative/L_Test",
            "ProjectWorldTestData",
            {"profile_id": "presentation_v1", "sha256": "a" * 64},
            {"profile_id": "runtime_v1", "sha256": "b" * 64},
            {
                "machine_profile_id": "operator_reference_win64",
                "resolution": [1920, 1080],
                "scalability_level": 3,
                "warmup_frames": 60,
                "sample_frames": 180,
                "camera_roles": ["overview", "road_oblique"],
            },
            33.34,
        )
        joined = " ".join(command)
        self.assertEqual("/ProjectWorldTestData/Generated/Representative/L_Test", command[1])
        self.assertIn("-ProjectWorldPresentationGate", command)
        self.assertIn("-ProjectWorldGateWorldDataPlugin=ProjectWorldTestData", command)
        self.assertIn("-ProjectSkipFrontEnd", command)
        self.assertIn("-ProjectWorldGateCameras=overview,road_oblique", command)
        self.assertIn("-ProjectWorldGateResX=1920", command)
        self.assertIn("-ProjectWorldGateResY=1080", command)
        self.assertIn("-ProjectWorldGateBudgetMs=33.34", command)
        self.assertNotIn("NullRHI", joined)

    def test_shipping_executable_is_exact_and_unambiguous(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            executable = root / "Windows" / "Alis" / "Binaries" / "Win64" / "Alis-Win64-Shipping.exe"
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b"shipping")
            self.assertEqual(executable, _shipping_executable(root))
            other = root / "Windows" / "Other" / "Alis-Win64-Shipping.exe"
            other.parent.mkdir(parents=True)
            other.write_bytes(b"duplicate")
            with self.assertRaises(ValidationFailure):
                _shipping_executable(root)

    def test_staged_project_launcher_is_required(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            root = Path(directory)
            launcher = root / "Windows" / "Alis.exe"
            launcher.parent.mkdir(parents=True)
            launcher.write_bytes(b"launcher")
            self.assertEqual(launcher, _launch_executable(root))
            launcher.unlink()
            with self.assertRaises(ValidationFailure):
                _launch_executable(root)

    def test_presentation_result_contract_accepts_structured_rejection(self) -> None:
        result = {
            "$schema": "https://alis.world/schemas/world-presentation/presentation-result-v1.json",
            "schema_version": 1,
            "operation_id": "run-test",
            "status": "rejected",
            "map_package": "/ProjectWorldTestData/Generated/Representative/L_Test",
            "presentation_profile": "presentation_v1",
            "presentation_profile_sha256": "a" * 64,
            "runtime_profile": "runtime_v1",
            "runtime_profile_sha256": "b" * 64,
            "executable": "C:/package/Alis.exe",
            "build_configuration": "Shipping",
            "engine_version": "5.8.1",
            "machine_profile_id": "operator_reference_win64",
            "gpu_adapter": "GPU",
            "gpu_driver": "1.0",
            "rhi": "NullRHI",
            "resolution_x": 1920,
            "resolution_y": 1080,
            "scalability_level": 3,
            "warmup_frames": 60,
            "sample_frames_per_camera": 180,
            "p95_frame_time_budget_ms": 33.34,
            "worst_p95_frame_time_ms": 10.0,
            "requested_camera_roles": ["overview"],
            "viewpoints": [],
            "errors": [],
        }
        result["errors"] = [{"code": "presentation_gate_config_invalid"}]
        validate_against(result, "presentation-result.schema.json")

    def test_package_log_proves_map_list_at_both_uat_layers(self) -> None:
        config = (REPO_ROOT / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
        maps = re.findall(
            r'^\+MapsToCook=\(FilePath="(?P<map>/[A-Za-z0-9_/-]+)"\)\s*$',
            config,
            re.MULTILINE,
        )
        required = "/ProjectWorldTestData/Generated/Representative/L_Test"
        map_list = "+".join([*maps, required])
        uat_line = f"Parsing command line: BuildCookRun -MapsToCook={map_list} -cook\n"
        cook_line = (
            'Running: UnrealEditor-Cmd.exe "Alis.uproject" -run=Cook '
            f"-Map={map_list} -TargetPlatform=Windows -unattended\n"
        )
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            log = Path(directory) / "package.log"
            log.write_text(uat_line + cook_line, encoding="utf-8")
            self.assertIn(required, _validate_package_map_argument(log, required))

            # UAT parsing line without -MapsToCook (the refuted -map form) fails.
            log.write_text(
                f"Parsing command line: BuildCookRun -map={map_list} -cook\n" + cook_line,
                encoding="utf-8",
            )
            with self.assertRaises(ValidationFailure) as raised:
                _validate_package_map_argument(log, required)
            self.assertEqual("package_map_argument_missing", raised.exception.code)

            # Missing cook-commandlet translation fails independently.
            log.write_text(uat_line, encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                _validate_package_map_argument(log, required)
            self.assertEqual("package_cook_map_argument_missing", raised.exception.code)

            # An incomplete commandlet map list fails even when UAT parsed the full set.
            log.write_text(
                uat_line
                + 'Running: UnrealEditor-Cmd.exe "Alis.uproject" -run=Cook '
                + f"-Map={maps[0] if maps else required} -TargetPlatform=Windows\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValidationFailure) as raised:
                _validate_package_map_argument(log, required)
            self.assertEqual("package_cook_map_set_incomplete", raised.exception.code)

    def test_package_log_requires_fresh_world_partition_discovery(self) -> None:
        fresh = (
            'Running: UnrealEditor-Cmd.exe "Alis.uproject" -run=Cook '
            '-NoAssetRegistryCache -TargetPlatform=Windows\n'
        )
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            log = Path(directory) / "package.log"
            log.write_text(fresh, encoding="utf-8")
            _validate_fresh_world_partition_cook(log)

            log.write_text(fresh.replace(" -NoAssetRegistryCache", ""), encoding="utf-8")
            with self.assertRaises(ValidationFailure) as raised:
                _validate_fresh_world_partition_cook(log)
            self.assertEqual("package_asset_registry_cache_enabled", raised.exception.code)

            log.write_text(
                fresh + "LogWorldPartition: Warning: Duplicate actor descriptor guid `1234`\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValidationFailure) as raised:
                _validate_fresh_world_partition_cook(log)
            self.assertEqual("package_world_partition_descriptor_stale", raised.exception.code)

    def test_measured_process_identity_is_enforced(self) -> None:
        with tempfile.TemporaryDirectory(dir=REPO_ROOT / "tmp") as directory:
            shipping = Path(directory) / "Alis" / "Binaries" / "Win64" / "Alis-Win64-Shipping.exe"
            shipping.parent.mkdir(parents=True)
            shipping.write_bytes(b"shipping")
            result = {
                "operation_id": "run-test",
                "build_configuration": "Shipping",
                "executable": str(shipping).replace("\\", "/").upper(),
            }
            _verify_measured_process(result, shipping, "run-test")

            with self.assertRaises(ValidationFailure) as raised:
                _verify_measured_process({**result, "operation_id": "other"}, shipping, "run-test")
            self.assertEqual("presentation_gate_operation_mismatch", raised.exception.code)

            with self.assertRaises(ValidationFailure) as raised:
                _verify_measured_process(
                    {**result, "build_configuration": "Development"}, shipping, "run-test"
                )
            self.assertEqual("presentation_gate_configuration_mismatch", raised.exception.code)

            launcher = Path(directory) / "Alis.exe"
            launcher.write_bytes(b"launcher")
            with self.assertRaises(ValidationFailure) as raised:
                _verify_measured_process(
                    {**result, "executable": str(launcher)}, shipping, "run-test"
                )
            self.assertEqual("presentation_gate_executable_mismatch", raised.exception.code)


if __name__ == "__main__":
    unittest.main()
