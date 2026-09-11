from __future__ import annotations

import ctypes
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Iterator

from World.CanonicalCompilation.api import (
    canonical_active_path,
    materialize_canonical,
    validate_canonical_authority,
)

from .contracts import ValidationFailure, canonical_hash, file_hash, read_json, tree_size, validate_against
from .roots import WorldDataRootError, resolve_world_data_roots


REPO_ROOT = Path(__file__).resolve().parents[4]
SOURCE_RUN = REPO_ROOT / "tools" / "World" / "SourceIngestion" / "run.py"
COMPILER_RUN = REPO_ROOT / "tools" / "World" / "CanonicalCompilation" / "run.py"
REALIZE_SCRIPT = REPO_ROOT / "scripts" / "ue" / "world" / "realize_canonical_world.ps1"
REALIZATION_EVIDENCE_ROOT = REPO_ROOT / "Saved" / "Validation" / "WorldRealization" / "EndToEnd"


CONTENT_LOCK_PATH = REPO_ROOT / "tmp" / "world" / "world_realization" / "content_mutation.lock"
CONTENT_LOCK_TOKEN_ENV = "ALIS_WORLD_CONTENT_LOCK_TOKEN"


@contextmanager
def _content_mutation_lock() -> Iterator[str]:
    # The E2E driver owns the ONE project-global generated-content mutation
    # lock for its full backup -> realize -> restore lifecycle. It holds the
    # file write-exclusive (share Read) and stamps a random owner token; the
    # realization wrapper it spawns verifies that token against the live lock
    # instead of self-acquiring, so parent and child never self-conflict.
    if os.name != "nt":
        raise ValidationFailure(
            "content_lock_platform_unsupported",
            "The generated-content mutation lock requires the pinned Windows runtime",
        )
    CONTENT_LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    generic_read_write = 0x80000000 | 0x40000000
    file_share_read = 0x00000001
    create_always = 2
    file_attribute_normal = 0x00000080
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateFileW.argtypes = [
        ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
    ]
    kernel32.CreateFileW.restype = ctypes.c_void_p
    kernel32.WriteFile.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p,
    ]
    kernel32.WriteFile.restype = ctypes.c_int
    kernel32.FlushFileBuffers.argtypes = [ctypes.c_void_p]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    invalid_handle = ctypes.c_void_p(-1).value
    handle = kernel32.CreateFileW(
        str(CONTENT_LOCK_PATH), generic_read_write, file_share_read, None,
        create_always, file_attribute_normal, None,
    )
    if handle == invalid_handle or handle is None:
        raise ValidationFailure(
            "content_lock_unavailable",
            "Another operation holds the ProjectWorld content mutation lock",
            path=str(CONTENT_LOCK_PATH),
        )
    token = uuid.uuid4().hex
    try:
        payload = token.encode("ascii")
        written = ctypes.c_uint32(0)
        if not kernel32.WriteFile(handle, payload, len(payload), ctypes.byref(written), None) or written.value != len(payload):
            raise ValidationFailure(
                "content_lock_token_write_failed",
                "Cannot stamp the content-lock owner token",
                path=str(CONTENT_LOCK_PATH),
            )
        kernel32.FlushFileBuffers(handle)
        os.environ[CONTENT_LOCK_TOKEN_ENV] = token
        yield token
    finally:
        os.environ.pop(CONTENT_LOCK_TOKEN_ENV, None)
        kernel32.CloseHandle(handle)


def _retry_locked_path(operation: Any, path: Path) -> None:
    for attempt in range(5):
        try:
            operation()
            return
        except PermissionError as error:
            if attempt == 4:
                raise ValidationFailure(
                    "generated_path_locked",
                    "Generated content remained locked after bounded retries",
                    path=str(path),
                ) from error
            time.sleep(0.25 * (attempt + 1))


def _run(name: str, command: list[str], log_root: Path, timeout: int = 7200) -> float:
    log_path = log_root / f"{name}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            command,
            cwd=REPO_ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=timeout,
        )
    duration = time.perf_counter() - started
    if completed.returncode != 0:
        raise ValidationFailure(
            "stage_failed",
            "World pipeline stage failed",
            stage=name,
            returncode=completed.returncode,
            log=str(log_path),
        )
    return duration


def _run_expected_rejection(name: str, command: list[str], log_root: Path) -> float:
    log_path = log_root / f"{name}.log"
    started = time.perf_counter()
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(
            command,
            cwd=REPO_ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=7200,
        )
    if completed.returncode == 0:
        raise ValidationFailure(
            "sabotage_not_rejected",
            "Authored-anchor sabotage unexpectedly succeeded",
            stage=name,
            log=str(log_path),
        )
    return time.perf_counter() - started


def _powershell() -> str:
    executable = shutil.which("powershell") or shutil.which("powershell.exe")
    if executable is None:
        raise ValidationFailure("powershell_missing", "PowerShell is required for Unreal validation")
    return executable


def _world_data_roots(plugin_name: str) -> tuple[Path, Path, Path]:
    try:
        roots = resolve_world_data_roots(plugin_name)
    except WorldDataRootError as error:
        raise ValidationFailure(
            "world_data_plugin_invalid",
            str(error),
            plugin=plugin_name,
        ) from error
    return roots.content_root, roots.presentation_root, roots.data_root


def _map_paths(
    map_package: str,
    content_root: Path,
    plugin_name: str,
) -> list[Path]:
    prefix = f"/{plugin_name}/Generated/"
    if not map_package.startswith(prefix) or re.fullmatch(
        rf"/{re.escape(plugin_name)}/Generated/[A-Za-z0-9_/]+", map_package
    ) is None:
        raise ValidationFailure("map_scope_invalid", "Generated map is outside its world-data mount", map=map_package)
    relative = Path("Generated") / Path(map_package[len(prefix):])
    map_base = content_root / relative
    paths = []
    for candidate in (
        Path(f"{map_base}.umap"),
        Path(f"{map_base}_BuiltData.uasset"),
    ):
        if candidate.is_file():
            paths.append(candidate)
    paths.extend(sorted(map_base.parent.glob(f"{map_base.name}_HLODLayer_*.uasset")))
    for external in ("__ExternalActors__", "__ExternalObjects__"):
        candidate = content_root / external / relative
        if candidate.exists():
            paths.append(candidate)
    for path in paths:
        try:
            path.resolve().relative_to(content_root.resolve())
        except ValueError as error:
            raise ValidationFailure("map_scope_invalid", "Generated map path escapes world-data content", path=str(path)) from error
    return sorted(set(paths))


def _backup_maps(
    map_packages: list[str], backup_root: Path, content_root: Path, plugin_name: str,
    moves: list[tuple[Path, Path]] | None = None,
) -> list[tuple[Path, Path]]:
    recorded = moves if moves is not None else []
    for map_package in map_packages:
        for source in _map_paths(map_package, content_root, plugin_name):
            relative = source.relative_to(content_root)
            destination = backup_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(source), str(destination))
            recorded.append((destination, source))
    return recorded


def _backup_generated(
    map_packages: list[str], backup_root: Path, content_root: Path,
    presentation_root: Path, plugin_name: str,
    realization_artifact_paths: list[Path] | None = None,
    moves: list[tuple[Path, Path]] | None = None,
) -> list[tuple[Path, Path]]:
    recorded = moves if moves is not None else []
    _backup_maps(map_packages, backup_root, content_root, plugin_name, recorded)
    if presentation_root.exists():
        destination = backup_root / presentation_root.relative_to(content_root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(presentation_root), str(destination))
        recorded.append((destination, presentation_root))
    for source in realization_artifact_paths or []:
        if not source.exists():
            continue
        destination = backup_root / source.relative_to(content_root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(source), str(destination))
        recorded.append((destination, source))
    return recorded


def _remove_owned_path(path: Path, content_root: Path) -> None:
    try:
        path.resolve().relative_to(content_root.resolve())
    except ValueError as error:
        raise ValidationFailure("generated_scope_invalid", "Generated path escapes world-data content", path=str(path)) from error
    if path.is_dir():
        _retry_locked_path(lambda: shutil.rmtree(path), path)
    elif path.exists():
        _retry_locked_path(path.unlink, path)


def _restore_recorded_moves(moves: list[tuple[Path, Path]]) -> None:
    for source, destination in moves:
        destination.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            _retry_locked_path(
                lambda source=source, destination=destination: shutil.copytree(
                    source,
                    destination,
                    dirs_exist_ok=True,
                ),
                source,
            )
        else:
            _retry_locked_path(
                lambda source=source, destination=destination: shutil.copy2(source, destination),
                source,
            )


def _restore_generated(
    moves: list[tuple[Path, Path]], map_packages: list[str], content_root: Path,
    presentation_root: Path, plugin_name: str,
    realization_artifact_paths: list[Path] | None = None,
) -> None:
    _remove_maps(map_packages, content_root, plugin_name)
    _remove_owned_path(presentation_root, content_root)
    for path in realization_artifact_paths or []:
        _remove_owned_path(path, content_root)
    _restore_recorded_moves(moves)


def _remove_maps(
    map_packages: list[str], content_root: Path, plugin_name: str
) -> None:
    for map_package in map_packages:
        for path in _map_paths(map_package, content_root, plugin_name):
            if path.is_dir():
                _retry_locked_path(lambda path=path: shutil.rmtree(path), path)
            else:
                _retry_locked_path(path.unlink, path)


def _generated_package_count(
    map_package: str, content_root: Path, plugin_name: str
) -> int:
    count = 0
    for path in _map_paths(map_package, content_root, plugin_name):
        if path.is_file():
            count += 1
        elif path.is_dir():
            count += sum(1 for item in path.rglob("*") if item.is_file())
    return count


def _realization_evidence_path(run_name: str, profile_name: str) -> Path:
    for value in (run_name, profile_name):
        if not value or Path(value).name != value:
            raise ValidationFailure("evidence_identity_invalid", "Realization evidence identity must be one path token")
    return REALIZATION_EVIDENCE_ROOT / run_name / profile_name / "unreal_emitted.json"


def _profile_contract(value: str, plugin_name: str, kind: str) -> dict[str, str]:
    relative = Path(value)
    path = (REPO_ROOT / relative).resolve()
    owned_root = _world_data_roots(plugin_name)[2]
    try:
        path.relative_to(owned_root.resolve())
    except ValueError as error:
        raise ValidationFailure(
            f"{kind}_profile_scope_invalid",
            f"{kind.capitalize()} profile must belong to its world-data plugin",
            path=value,
        ) from error
    if relative.is_absolute() or not path.is_file():
        raise ValidationFailure(
            f"{kind}_profile_missing",
            f"Pinned {kind} profile is unavailable",
            path=value,
        )
    document = read_json(path)
    profile_id = document.get("profile_id")
    if not isinstance(profile_id, str) or not profile_id:
        raise ValidationFailure(
            f"{kind}_profile_identity_invalid",
            f"Pinned {kind} profile has no stable identity",
            path=value,
        )
    return {
        "path": relative.as_posix(),
        "profile_id": profile_id,
        "sha256": file_hash(path),
    }


def _presentation_profile_contract(value: str, plugin_name: str = "ProjectWorld") -> dict[str, str]:
    return _profile_contract(value, plugin_name, "presentation")


def _runtime_profile_contract(value: str, plugin_name: str = "ProjectWorld") -> dict[str, str]:
    return _profile_contract(value, plugin_name, "runtime")


def _realization_profile_contract(
    value: str,
    plugin_name: str,
    map_package: str,
) -> dict[str, str]:
    contract = _profile_contract(value, plugin_name, "realization")
    document = read_json(REPO_ROOT / contract["path"])
    if (
        document.get("world_data_plugin") != plugin_name
        or document.get("map_package") != map_package
    ):
        raise ValidationFailure(
            "realization_profile_mismatch",
            "Pinned realization profile belongs to another owner or map",
            path=value,
        )
    return contract


def _realization_artifact_paths(
    contract: dict[str, str], content_root: Path, plugin_name: str,
) -> list[Path]:
    document = read_json(REPO_ROOT / contract["path"])
    package_prefix = f"/{plugin_name}/"
    generated_prefix = f"/{plugin_name}/Generated/"
    paths: list[Path] = []
    for layer in document.get("layers", []):
        artifact_root = layer.get("artifact_root")
        if not isinstance(artifact_root, str) or not artifact_root.startswith(generated_prefix):
            raise ValidationFailure(
                "realization_layer_root_invalid",
                "Realization layer artifact root must be inside its generated owner",
                artifact_root=artifact_root,
            )
        candidate = content_root / Path(artifact_root[len(package_prefix):].rstrip("/"))
        try:
            candidate.resolve().relative_to(content_root.resolve())
        except ValueError as error:
            raise ValidationFailure(
                "realization_layer_root_invalid",
                "Realization layer artifact root escapes world-data content",
                artifact_root=artifact_root,
            ) from error
        paths.append(candidate)
    return sorted(set(paths))


def _remove_realization_artifacts(
    contract: dict[str, str], content_root: Path, plugin_name: str,
) -> None:
    for path in _realization_artifact_paths(contract, content_root, plugin_name):
        _remove_owned_path(path, content_root)


def _authored_overlay_profile_contract(value: str, plugin_name: str) -> dict[str, Any]:
    relative = Path(value)
    path = (REPO_ROOT / relative).resolve()
    data_root = _world_data_roots(plugin_name)[2].resolve()
    try:
        path.relative_to(data_root / "Authored")
    except ValueError as error:
        raise ValidationFailure(
            "authored_overlay_profile_scope_invalid",
            "Authored overlay profile must belong to its world-data plugin",
            path=value,
        ) from error
    document = read_json(path)
    overlay_set_id = document.get("overlay_set_id")
    packages = sorted({item["authored_package"] for item in document.get("overlays", [])})
    if not path.is_file() or not isinstance(overlay_set_id, str):
        raise ValidationFailure("authored_overlay_profile_invalid", "Authored overlay profile is incomplete", path=value)
    return {
        "path": relative.as_posix(),
        "profile_id": overlay_set_id,
        "sha256": file_hash(path),
        "packages": packages,
    }


def _select_road_dirty_unit(compile_result: Path) -> str:
    compile_root = compile_result.parent
    receipt = read_json(compile_result)
    coverage_outputs = [
        item for item in receipt.get("outputs", [])
        if item.get("path") == "canonical/coverage.json"
    ]
    if len(coverage_outputs) != 1:
        raise ValidationFailure(
            "road_locality_input_missing",
            "Road-locality characterization requires one canonical coverage output",
        )
    coverage = read_json(compile_root / coverage_outputs[0]["path"])
    for descriptor in coverage.get("cells", []):
        cell = read_json(compile_root / descriptor["path"])
        features = read_json(compile_root / cell["feature_artifact"]["path"])
        if any(item.get("feature_class") == "road" for item in features.get("features", [])):
            return str(descriptor["cell_id"])
    raise ValidationFailure(
        "road_locality_input_missing",
        "Road-locality characterization found no canonical cell containing roads",
    )


def _owned_file_hashes(paths: list[Path], content_root: Path) -> dict[str, str]:
    hashes: dict[str, str] = {}
    for path in paths:
        if path.is_file():
            hashes[path.relative_to(content_root).as_posix()] = file_hash(path)
        elif path.is_dir():
            for item in sorted(candidate for candidate in path.rglob("*") if candidate.is_file()):
                hashes[item.relative_to(content_root).as_posix()] = file_hash(item)
    return hashes


def _generated_tree_hashes(content_root: Path) -> dict[str, str]:
    return _owned_file_hashes(
        [
            content_root / "Generated",
            content_root / "__ExternalActors__" / "Generated",
            content_root / "__ExternalObjects__" / "Generated",
        ],
        content_root,
    )


def _backup_guarded_generated(
    guarded: dict[str, str], backup_root: Path, content_root: Path,
) -> None:
    for relative, expected_hash in guarded.items():
        source = content_root / Path(relative)
        if not source.is_file() or file_hash(source) != expected_hash:
            raise ValidationFailure(
                "matrix_generated_guard_changed",
                "Generated owner bytes changed while preparing the Matrix guard",
                path=relative,
            )
        destination = backup_root / Path(relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        _retry_locked_path(
            lambda source=source, destination=destination: shutil.copy2(source, destination),
            source,
        )


def _repair_guarded_generated(
    expected: dict[str, str], guarded: dict[str, str], backup_root: Path,
    content_root: Path,
) -> list[str]:
    actual = _generated_tree_hashes(content_root)
    drift = sorted(
        relative for relative in set(expected) | set(actual)
        if expected.get(relative) != actual.get(relative)
    )
    for relative in sorted(set(actual) - set(expected)):
        _remove_owned_path(content_root / Path(relative), content_root)
    for relative, expected_hash in guarded.items():
        if actual.get(relative) == expected_hash:
            continue
        source = backup_root / Path(relative)
        if not source.is_file() or file_hash(source) != expected_hash:
            raise ValidationFailure(
                "matrix_generated_guard_invalid",
                "Matrix guard cannot restore pre-existing generated bytes",
                path=relative,
            )
        destination = content_root / Path(relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        _retry_locked_path(
            lambda source=source, destination=destination: shutil.copy2(source, destination),
            source,
        )
    return drift


def _authored_package_hashes(contract: dict[str, Any], content_root: Path, plugin_name: str) -> dict[str, str]:
    paths: list[Path] = []
    prefix = f"/{plugin_name}/"
    for package in contract["packages"]:
        if not package.startswith(prefix + "Authored/"):
            raise ValidationFailure("authored_package_scope_invalid", "Authored package escapes its plugin", package=package)
        relative = Path(package[len(prefix):])
        map_path = (content_root / relative).with_suffix(".umap")
        paths.append(map_path)
        for external in ("__ExternalActors__", "__ExternalObjects__"):
            paths.append(content_root / external / relative)
    hashes = _owned_file_hashes(paths, content_root)
    if contract["packages"] and not hashes:
        raise ValidationFailure("authored_package_missing", "No authored package bytes were found")
    return hashes


def _run_profile(
    name: str,
    settings: dict[str, Any],
    work_root: Path,
    logs: Path,
    powershell: str,
) -> dict[str, Any]:
    root = work_root / name
    world_data_plugin = settings["world_data_plugin"]
    content_root, presentation_root, _ = _world_data_roots(world_data_plugin)
    source_profile = settings.get("source_profile_path", settings["source_profile"])
    compiler_profile = settings.get("compiler_profile_path", settings["compiler_profile"])
    source_root = root / "source"
    source_seconds = _run(
        f"{name}_source",
        [sys.executable, str(SOURCE_RUN), "run", "--profile", source_profile, "--output-root", str(source_root)],
        logs,
    )
    source_result = source_root / "run_result.json"
    compile_roots = {key: root / key for key in ("compile_first", "compile_second", "incremental")}
    for key in ("compile_first", "compile_second"):
        _run(
            f"{name}_{key}",
            [
                sys.executable,
                str(COMPILER_RUN),
                "run",
                "--profile",
                compiler_profile,
                "--source-result",
                str(source_result),
                "--output-root",
                str(compile_roots[key]),
            ],
            logs,
        )
    bounds = ",".join(str(value) for value in settings["incremental_bounds"])
    _run(
        f"{name}_incremental",
        [
            sys.executable,
            str(COMPILER_RUN),
            "run",
            "--profile",
            compiler_profile,
            "--source-result",
            str(source_result),
            "--base-result",
            str(compile_roots["compile_first"] / "compile_result.json"),
            "--terrain-change-bounds",
            bounds,
            "--output-root",
            str(compile_roots["incremental"]),
        ],
        logs,
    )
    compile_result = compile_roots["compile_first"] / "compile_result.json"
    realization_compile_result = compile_result
    canonical_authority = None
    if settings.get("canonical_authority"):
        compiler_path = (REPO_ROOT / compiler_profile).resolve()
        active_path = canonical_active_path(REPO_ROOT, compiler_path)
        authority = validate_canonical_authority(REPO_ROOT, active_path, compiler_path)
        realization_compile_result = materialize_canonical(REPO_ROOT, active_path, compiler_path)
        canonical_authority = {
            "active_path": active_path.relative_to(REPO_ROOT).as_posix(),
            "active_sha256": file_hash(active_path),
            "authority_id": authority["authority_id"],
            "inputs_hash": authority["inputs_hash"],
            "bundle_sha256": authority["bundle"]["sha256"],
            "compile_result_sha256": authority["compile_result_sha256"],
        }
    emitted = _realization_evidence_path(work_root.name, name)
    emitted.parent.mkdir(parents=True, exist_ok=True)
    presentation = _presentation_profile_contract(settings["presentation_profile"], world_data_plugin)
    runtime = _runtime_profile_contract(settings["runtime_profile"], world_data_plugin) if settings.get("runtime_profile") else None
    realization = _realization_profile_contract(
        settings["realization_profile"], world_data_plugin, settings["map_package"]
    ) if settings.get("realization_profile") else None
    authored = _authored_overlay_profile_contract(settings["authored_overlay_profile"], world_data_plugin)
    authored_sabotage = _authored_overlay_profile_contract(
        settings["authored_overlay_sabotage_profile"], world_data_plugin
    )
    authored_hashes = _authored_package_hashes(authored, content_root, world_data_plugin)
    # Isolated runs use a transient sandbox manifest root; the durable
    # authority root tracks only the repository's tracked generated tree.
    sandbox_manifest_root = work_root / "manifests" / name
    command = [
        powershell,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(REALIZE_SCRIPT),
        "-CompileResult",
        str(realization_compile_result),
        "-Mode",
        "Apply",
        "-WorldDataPlugin",
        world_data_plugin,
        "-PresentationProfile",
        str(REPO_ROOT / presentation["path"]),
        "-AuthoredOverlayProfile",
        str(REPO_ROOT / authored["path"]),
        "-Map",
        settings["map_package"],
        "-EvidencePath",
        str(emitted),
        "-ManifestRoot",
        str(sandbox_manifest_root),
        "-NonInteractive",
    ]
    if settings["require_landscape"]:
        command.append("-RequireLandscape")
    if runtime is not None:
        command.extend(["-RuntimeProfile", str(REPO_ROOT / runtime["path"])])
    if realization is not None:
        command.extend(["-RealizationProfile", str(REPO_ROOT / realization["path"])])

    def _leg_command(compile_path: Path, overlay: dict[str, Any]) -> list[str]:
        leg = list(command)
        leg[leg.index("-CompileResult") + 1] = str(compile_path)
        leg[leg.index("-AuthoredOverlayProfile") + 1] = str(REPO_ROOT / overlay["path"])
        return leg

    def _copy_realization(target: Path) -> None:
        shutil.copy2(emitted, target)
        sidecar = Path(f"{emitted}.manifests.json")
        shutil.copy2(sidecar, Path(f"{target}.manifests.json"))

    first_command = _leg_command(realization_compile_result, authored)
    first_seconds = _run(f"{name}_unreal_first", [*first_command, "-EnrollManifests"], logs)
    first_result = root / "unreal_first.json"
    _copy_realization(first_result)
    authored_after_first = _authored_package_hashes(authored, content_root, world_data_plugin)
    second_seconds = _run(f"{name}_unreal_second", first_command, logs)
    second_result = root / "unreal_second.json"
    _copy_realization(second_result)
    authored_after_second = _authored_package_hashes(authored, content_root, world_data_plugin)

    road_dirty_unit = _select_road_dirty_unit(realization_compile_result)
    road_locality_command = [
        *first_command,
        "-DirtyUnit",
        f"roads={road_dirty_unit}",
    ]
    road_locality_seconds = _run(
        f"{name}_unreal_road_locality", road_locality_command, logs
    )
    road_locality_result = root / "unreal_road_locality.json"
    _copy_realization(road_locality_result)
    authored_after_road_locality = _authored_package_hashes(
        authored, content_root, world_data_plugin
    )

    incremental_command = _leg_command(
        compile_roots["incremental"] / "compile_result.json", authored
    )
    incremental_seconds = _run(f"{name}_unreal_incremental", incremental_command, logs)
    incremental_result = root / "unreal_incremental.json"
    _copy_realization(incremental_result)
    authored_after_incremental = _authored_package_hashes(authored, content_root, world_data_plugin)

    generated_before_rejection = _owned_file_hashes(
        [*_map_paths(settings["map_package"], content_root, world_data_plugin), presentation_root],
        content_root,
    )
    rejected_seconds = _run_expected_rejection(
        f"{name}_unreal_rejected_apply",
        _leg_command(compile_roots["incremental"] / "compile_result.json", authored_sabotage),
        logs,
    )
    rejected_result = root / "unreal_rejected_apply.json"
    shutil.copy2(emitted, rejected_result)
    generated_after_rejection = _owned_file_hashes(
        [*_map_paths(settings["map_package"], content_root, world_data_plugin), presentation_root],
        content_root,
    )
    authored_after_rejection = _authored_package_hashes(authored, content_root, world_data_plugin)

    _remove_maps([settings["map_package"]], content_root, world_data_plugin)
    if realization is not None:
        _remove_realization_artifacts(realization, content_root, world_data_plugin)
    _remove_owned_path(presentation_root, content_root)
    clean_seconds = _run(f"{name}_unreal_clean_rebuild", [*first_command, "-Reconstruct"], logs)
    clean_result = root / "unreal_clean_rebuild.json"
    _copy_realization(clean_result)
    authored_after_clean = _authored_package_hashes(authored, content_root, world_data_plugin)
    return {
        "source_root": str(source_root),
        "source_result": str(source_result),
        "source_seconds": source_seconds,
        # Freeze the child realization receipts INTO the accepted run. Their
        # paths alone are not evidence: a later reader would trust whatever
        # bytes happen to sit there, while the parent result.json hash still
        # looked valid. Acceptance verifies these before reading provenance.
        "unreal_first_sha256": file_hash(first_result),
        "unreal_second_sha256": file_hash(second_result),
        "unreal_road_locality_sha256": file_hash(road_locality_result),
        "unreal_incremental_sha256": file_hash(incremental_result),
        "unreal_rejected_apply_sha256": file_hash(rejected_result),
        "unreal_clean_rebuild_sha256": file_hash(clean_result),
        "compile_first_root": str(compile_roots["compile_first"]),
        "compile_second_root": str(compile_roots["compile_second"]),
        "incremental_root": str(compile_roots["incremental"]),
        "realization_compile_result": str(realization_compile_result),
        "realization_compile_result_sha256": file_hash(realization_compile_result),
        "canonical_authority": canonical_authority,
        "unreal_first": str(first_result),
        "unreal_second": str(second_result),
        "unreal_road_locality": str(road_locality_result),
        "road_locality_dirty_unit": road_dirty_unit,
        "unreal_incremental": str(incremental_result),
        "unreal_rejected_apply": str(rejected_result),
        "unreal_clean_rebuild": str(clean_result),
        "unreal_first_wall_seconds": first_seconds,
        "unreal_second_wall_seconds": second_seconds,
        "unreal_road_locality_wall_seconds": road_locality_seconds,
        "unreal_incremental_wall_seconds": incremental_seconds,
        "unreal_rejected_apply_wall_seconds": rejected_seconds,
        "unreal_clean_rebuild_wall_seconds": clean_seconds,
        "generated_package_count": _generated_package_count(
            settings["map_package"], content_root, world_data_plugin
        ),
        "presentation_profile": presentation,
        "runtime_profile": runtime,
        "realization_profile": realization,
        "authored_overlay_profile": authored,
        "authored_package_hashes": {
            "before": authored_hashes,
            "first": authored_after_first,
            "second": authored_after_second,
            "road_locality": authored_after_road_locality,
            "incremental": authored_after_incremental,
            "rejected": authored_after_rejection,
            "clean": authored_after_clean,
        },
        "rejected_generated_hashes": {
            "before": generated_before_rejection,
            "after": generated_after_rejection,
        },
    }


def execute(
    profile: dict[str, Any],
    operation_id: str,
    evidence_root: Path,
    preflight_path: Path,
    common_checks: dict[str, Any],
) -> dict[str, Any]:
    powershell = _powershell()
    preflight = read_json(preflight_path)
    validate_against(preflight, "bootstrap-preflight.schema.json")
    work_root = REPO_ROOT / "tmp" / "world" / "end_to_end_validation" / operation_id
    logs = evidence_root / "logs"
    work_root.mkdir(parents=True, exist_ok=False)
    cache_root = REPO_ROOT / "tmp" / "world" / "source_ingestion" / "cache"
    cache_before = tree_size(cache_root)
    ownership = []
    for name, settings in profile["profiles"].items():
        content_root, presentation_root, _ = _world_data_roots(settings["world_data_plugin"])
        realization_paths: list[Path] = []
        if settings.get("realization_profile"):
            realization = _realization_profile_contract(
                settings["realization_profile"],
                settings["world_data_plugin"],
                settings["map_package"],
            )
            realization_paths = _realization_artifact_paths(
                realization, content_root, settings["world_data_plugin"]
            )
        ownership.append((name, settings, content_root, presentation_root, realization_paths))
    with _content_mutation_lock():
        owner_states: dict[str, tuple[Path, dict[str, str]]] = {}
        owner_declared: dict[str, set[str]] = {}
        for _, settings, content_root, presentation_root, realization_paths in ownership:
            owner = settings["world_data_plugin"]
            owner_states.setdefault(owner, (content_root, _generated_tree_hashes(content_root)))
            scope_paths = _map_paths(settings["map_package"], content_root, owner)
            scope_paths.append(presentation_root)
            scope_paths.extend(realization_paths)
            owner_declared.setdefault(owner, set()).update(
                _owned_file_hashes(scope_paths, content_root)
            )
        guarded = {
            owner: {
                relative: digest for relative, digest in expected.items()
                if relative not in owner_declared.get(owner, set())
            }
            for owner, (_, expected) in owner_states.items()
        }
        backups: list[
            tuple[str, list[tuple[Path, Path]], dict[str, Any], Path, Path, list[Path]]
        ] = []
        prepared_backups: set[str] = set()
        restoration_owners: dict[str, dict[str, Any]] = {}
        original_failure: BaseException | None = None
        try:
            for owner, (content_root, _) in owner_states.items():
                _backup_guarded_generated(
                    guarded[owner], work_root / "map_backup" / "owner_guard" / owner,
                    content_root,
                )
            for name, settings, content_root, presentation_root, realization_paths in ownership:
                moves: list[tuple[Path, Path]] = []
                backups.append((
                    name, moves, settings, content_root, presentation_root, realization_paths,
                ))
                _backup_generated(
                    [settings["map_package"]], work_root / "map_backup" / name,
                    content_root, presentation_root, settings["world_data_plugin"],
                    realization_paths, moves,
                )
                prepared_backups.add(name)
            profiles = {
                name: _run_profile(name, settings, work_root, logs, powershell)
                for name, settings in profile["profiles"].items()
            }
        except BaseException as error:
            original_failure = error
            raise
        finally:
            restoration_failures = []
            for name, moves, settings, content_root, presentation_root, realization_paths in reversed(backups):
                try:
                    if name in prepared_backups:
                        _restore_generated(
                            moves, [settings["map_package"]], content_root,
                            presentation_root, settings["world_data_plugin"], realization_paths,
                        )
                    else:
                        _restore_recorded_moves(moves)
                except Exception:
                    restoration_failures.append(settings["map_package"])
            out_of_scope_mutations: dict[str, list[str]] = {}
            for owner, (content_root, expected) in owner_states.items():
                try:
                    drift = _repair_guarded_generated(
                        expected,
                        guarded[owner],
                        work_root / "map_backup" / "owner_guard" / owner,
                        content_root,
                    )
                    if drift:
                        out_of_scope_mutations[owner] = drift
                except Exception:
                    restoration_failures.append(owner)
            for owner, (content_root, expected) in owner_states.items():
                actual = _generated_tree_hashes(content_root)
                restoration_owners[owner] = {
                    "before_sha256": canonical_hash(expected),
                    "after_sha256": canonical_hash(actual),
                    "file_count": len(expected),
                }
                if actual != expected:
                    restoration_failures.append(owner)
            if restoration_failures:
                raise ValidationFailure(
                    "matrix_generated_restore_mismatch",
                    "Matrix could not restore the exact pre-run generated tree",
                    maps=restoration_failures,
                    backup=str(work_root / "map_backup"),
                )
            shutil.rmtree(work_root / "map_backup", ignore_errors=True)
            if original_failure is None and out_of_scope_mutations:
                raise ValidationFailure(
                    "matrix_generated_out_of_scope_mutation",
                    "Matrix changed generated bytes outside its declared scopes",
                    owners=out_of_scope_mutations,
                )

    return {
        "test_suites": common_checks["test_suites"],
        "common_checks": {
            "receipt": common_checks["receipt"],
            "receipt_sha256": common_checks["receipt_sha256"],
            "operation_id": common_checks["operation_id"],
            "common_contract_sha256": common_checks["common_contract_sha256"],
        },
        "profiles": profiles,
        "environment": {
            **common_checks["environment"],
            "immutable_cache_bytes": tree_size(cache_root),
            "network_transfer_bytes": max(0, tree_size(cache_root) - cache_before),
            "bootstrap_preflight_path": str(preflight_path),
            "bootstrap_preflight": preflight,
        },
        "generated_roots": sorted({str(item[2]) for item in ownership}),
        "generated_tree_restoration": {
            "status": "accepted",
            "owners": restoration_owners,
        },
        "work_root": str(work_root),
    }
