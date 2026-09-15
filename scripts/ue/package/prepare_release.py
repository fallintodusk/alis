#!/usr/bin/env python3
"""Prepare and verify one exact ALIS release bundle before signing."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


GITHUB_RELEASE_ASSET_LIMIT_BYTES = 2 * 1024 * 1024 * 1024
DEFAULT_RELEASE_PART_SIZE_MIB = 1900
MAX_RELEASE_PART_SIZE_MIB = 1900


class ReleaseError(RuntimeError):
    pass


@dataclass(frozen=True)
class ReleaseInputs:
    release_version: str
    release_tag: str
    private_source_root: Path
    public_source_root: Path
    player_package_root: Path
    player_evidence: Path
    developer_release_dir: Path
    developer_payload_manifest: Path
    component_manifest: Path
    dependency_report: Path
    privacy_report: Path
    map_load_report: Path
    attribution_notice: Path
    product_terms: Path


RUNTIME_STATE_PATHS = (
    "windows/alis/localappdata",
    "windows/alis/saved",
    "windows/engine/saved",
)
GENERATED_SIGNING_FILES = {
    "ALIS_PUBLIC_KEY.asc",
    "SHA256SUMS.txt",
    "SHA256SUMS.txt.asc",
    "VERIFY_RELEASE.bat",
    "VERIFY_RELEASE.ps1",
    "sign_release_summary.txt",
    "verify_release_summary.txt",
}
DEVELOPER_INPUT_ONLY_FILES = {
    "INSTALL_ALIS_DEVELOPER_PROJECT.bat",
    "INSTALL_ALIS_DEVELOPER_PROJECT.ps1",
    "LICENSE.txt",
    "LICENSE_MPL-2.0.txt",
    "VERIFY_RELEASE.ps1",
}
DEVELOPER_HELPER_FILES = {
    "INSTALL_ALIS_DEVELOPER_PROJECT.bat",
    "INSTALL_ALIS_DEVELOPER_PROJECT.ps1",
    "LICENSE.txt",
    "LICENSE_MPL-2.0.txt",
    "VERIFY_RELEASE.ps1",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise ReleaseError(f"Required JSON file is missing: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ReleaseError(f"Cannot read JSON file {path}: {error}") from error
    if not isinstance(value, dict):
        raise ReleaseError(f"JSON root must be an object: {path}")
    return value


def git_value(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", "-c", "core.fsmonitor=false", "-C", str(root), *arguments],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise ReleaseError(f"Git failed in {root}: {result.stderr.strip()}")
    return result.stdout.strip()


def source_state_digest(root: Path) -> str:
    root = root.resolve()
    command = ["git", "-c", "core.fsmonitor=false", "-C", str(root)]
    index_result = subprocess.run(
        [*command, "ls-files", "-s", "-z"],
        capture_output=True,
        check=False,
    )
    if index_result.returncode != 0:
        detail = index_result.stderr.decode("utf-8", errors="replace").strip()
        raise ReleaseError(f"Unable to read indexed source state: {detail}")
    worktree_result = subprocess.run(
        [*command, "diff", "--name-only", "-z", "--no-renames", "--no-ext-diff", "--"],
        capture_output=True,
        check=False,
    )
    if worktree_result.returncode != 0:
        detail = worktree_result.stderr.decode("utf-8", errors="replace").strip()
        raise ReleaseError(f"Unable to read working-tree source state: {detail}")
    untracked_result = subprocess.run(
        [*command, "ls-files", "-z", "--others", "--exclude-standard"],
        capture_output=True,
        check=False,
    )
    if untracked_result.returncode != 0:
        detail = untracked_result.stderr.decode("utf-8", errors="replace").strip()
        raise ReleaseError(f"Unable to read untracked source state: {detail}")

    entries: dict[bytes, tuple[bytes, bytes]] = {}
    for record in filter(None, index_result.stdout.split(b"\0")):
        metadata, separator, relative_bytes = record.partition(b"\t")
        fields = metadata.split()
        if not separator or len(fields) != 3:
            raise ReleaseError("Git returned an invalid index entry")
        mode, object_id, stage = fields
        if stage != b"0":
            raise ReleaseError("Release source contains an unresolved Git index entry")
        entries[relative_bytes] = (mode, object_id)

    changed_paths = set(filter(None, worktree_result.stdout.split(b"\0")))
    changed_paths.update(filter(None, untracked_result.stdout.split(b"\0")))
    for relative_bytes in changed_paths:
        try:
            relative = relative_bytes.decode("utf-8")
        except UnicodeDecodeError as error:
            raise ReleaseError("Changed Git path is not valid UTF-8") from error
        path = root / relative
        if not path.exists() and not path.is_symlink():
            entries.pop(relative_bytes, None)
            continue
        if not path.is_file() and not path.is_symlink():
            raise ReleaseError(f"Changed release input is not a file: {relative}")

        current_mode = entries.get(relative_bytes, (b"", b""))[0]
        if path.is_symlink():
            mode = b"120000"
        elif current_mode in {b"100644", b"100755"}:
            mode = current_mode
        else:
            executable = bool(path.stat().st_mode & 0o111)
            mode = b"100755" if executable else b"100644"
        object_result = subprocess.run(
            [*command, "hash-object", f"--path={relative}", "--", relative],
            capture_output=True,
            check=False,
        )
        if object_result.returncode != 0:
            detail = object_result.stderr.decode("utf-8", errors="replace").strip()
            raise ReleaseError(f"Unable to hash release input {relative}: {detail}")
        entries[relative_bytes] = (mode, object_result.stdout.strip())

    digest = hashlib.sha256()
    for relative_bytes, (mode, object_id) in sorted(entries.items()):
        digest.update(relative_bytes)
        digest.update(b"\0")
        digest.update(mode)
        digest.update(b"\0")
        digest.update(object_id)
        digest.update(b"\0")
    return digest.hexdigest()


def package_tree_digest(root: Path) -> str:
    if not root.is_dir():
        raise ReleaseError(f"Player package root is missing: {root}")
    lines: list[str] = []
    paths = sorted(
        (item for item in root.rglob("*") if item.is_file()),
        key=lambda item: item.relative_to(root).as_posix().encode("utf-8"),
    )
    for path in paths:
        relative = path.relative_to(root).as_posix()
        lowered = relative.lower()
        if any(lowered == prefix or lowered.startswith(prefix + "/") for prefix in RUNTIME_STATE_PATHS):
            continue
        lines.append(f"{relative}|{path.stat().st_size}|{sha256_file(path)}")
    return hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()


def resolve_recorded_path(value: str, source_root: Path) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = source_root / path
    return path.resolve()


def normalize_version(value: str) -> str:
    return value[1:] if value.startswith("v") else value


def require_equal(actual: Any, expected: Any, label: str) -> None:
    if actual != expected:
        raise ReleaseError(f"{label} mismatch: expected {expected!r}, found {actual!r}")


def require_safe_file_name(value: Any, label: str) -> str:
    name = str(value or "")
    if not name or Path(name).name != name or name in {".", ".."}:
        raise ReleaseError(f"Invalid {label}: {name!r}")
    return name


def validate_developer_release(inputs: ReleaseInputs, manifest: dict[str, Any]) -> list[Path]:
    root = inputs.developer_release_dir.resolve()
    manifest_path = inputs.developer_payload_manifest.resolve()
    if manifest_path.parent != root:
        raise ReleaseError("Developer payload manifest must be inside its release directory")

    payload_id = str(manifest.get("payload_id", ""))
    if not payload_id:
        raise ReleaseError("Developer payload manifest is missing payload_id")
    archive = manifest.get("archive")
    if not isinstance(archive, dict):
        raise ReleaseError("Developer payload manifest is missing archive authority")
    parts = archive.get("parts")
    if not isinstance(parts, list) or not parts:
        raise ReleaseError("Developer payload manifest contains no archive parts")

    selected: list[Path] = [manifest_path]
    seen: set[str] = {manifest_path.name}
    logical_digest = hashlib.sha256()
    logical_size = 0
    for index, part in enumerate(parts, start=1):
        if not isinstance(part, dict):
            raise ReleaseError(f"Developer archive part {index} is not an object")
        name = require_safe_file_name(part.get("name"), f"developer archive part {index} name")
        if name in seen:
            raise ReleaseError(f"Duplicate developer release file: {name}")
        path = root / name
        if not path.is_file():
            raise ReleaseError(f"Developer archive part is missing: {path}")
        require_equal(part.get("byte_size"), path.stat().st_size, f"developer archive part size for {name}")
        require_equal(part.get("sha256"), sha256_file(path), f"developer archive part hash for {name}")
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                logical_digest.update(chunk)
                logical_size += len(chunk)
        selected.append(path)
        seen.add(name)

    require_equal(archive.get("byte_size"), logical_size, "developer logical archive size")
    require_equal(archive.get("sha256"), logical_digest.hexdigest(), "developer logical archive hash")

    notices = sorted(root.glob("*.notices.json"))
    if len(notices) != 1:
        raise ReleaseError("Developer release must contain exactly one notices manifest")
    notice = read_json(notices[0])
    require_equal(notice.get("payload_id"), payload_id, "developer notices payload_id")
    require_equal(inputs.attribution_notice.resolve(), notices[0].resolve(), "developer attribution notice")
    selected.append(notices[0])
    seen.add(notices[0].name)

    for name in sorted(DEVELOPER_HELPER_FILES):
        path = root / name
        if not path.is_file():
            raise ReleaseError(f"Required developer release helper is missing: {path}")
        selected.append(path)
        seen.add(name)

    actual = {path.name for path in root.iterdir() if path.is_file()}
    directories = [path.name for path in root.iterdir() if path.is_dir()]
    if directories or actual != seen:
        extras = sorted(actual - seen) + sorted(directories)
        missing = sorted(seen - actual)
        raise ReleaseError(
            f"Developer release inventory mismatch: unexpected={extras}, missing={missing}"
        )
    return selected


def validate_inputs(inputs: ReleaseInputs, archive_report_path: Path) -> dict[str, Any]:
    private_root = inputs.private_source_root.resolve()
    public_root = inputs.public_source_root.resolve()
    package_root = inputs.player_package_root.resolve()
    private_revision = git_value(private_root, "rev-parse", "HEAD")
    private_state = source_state_digest(private_root)
    public_revision = git_value(public_root, "rev-parse", "HEAD")
    public_tree = git_value(public_root, "rev-parse", "HEAD^{tree}")
    tagged_revision = git_value(public_root, "rev-parse", f"{inputs.release_tag}^{{commit}}")
    require_equal(tagged_revision, public_revision, "public release tag")

    evidence = read_json(inputs.player_evidence)
    require_equal(evidence.get("schema_version"), 1, "player evidence schema")
    evidence_files: list[tuple[Path, str]]
    if evidence.get("status") == "operator_accepted":
        require_equal(evidence.get("product_decision"), "accepted", "player product decision")
        require_equal(evidence.get("source_revision"), private_revision, "player source revision")
        require_equal(evidence.get("source_state_sha256"), private_state, "player source state")
        recorded_package = resolve_recorded_path(str(evidence.get("package_root", "")), private_root)
        package_sha256 = evidence.get("package_tree_sha256")
        executable = resolve_recorded_path(str(evidence.get("shipping_executable", "")), private_root)
        composite = resolve_recorded_path(str(evidence.get("release_composite", "")), private_root)
        require_equal(evidence.get("release_composite_sha256"), sha256_file(composite), "player composite")
        machine = read_json(composite)
        operation_id = evidence.get("release_operation_id")
        shipping_executable_sha256 = evidence.get("shipping_executable_sha256")
        product_review = "accepted"
        evidence_files = [
            (inputs.player_evidence, "player-acceptance.json"),
            (composite, "player-machine-acceptance.json"),
        ]
    elif evidence.get("status") == "accepted":
        machine = evidence
        composite = inputs.player_evidence.resolve()
        require_equal(machine.get("revision"), private_revision, "player source revision")
        require_equal(machine.get("source_state_sha256"), private_state, "player source state")
        recorded_package = resolve_recorded_path(str(machine.get("final_package", "")), private_root)
        package_sha256 = machine.get("shipping_package_sha256")
        executable = package_root / "Windows/Alis/Binaries/Win64/Alis-Win64-Shipping.exe"
        operation_id = machine.get("operation_id")
        shipping_executable_sha256 = machine.get("shipping_executable_sha256")
        product_review = "pending_owner_approval"
        evidence_files = [(composite, "player-machine-acceptance.json")]
    else:
        raise ReleaseError(f"Unsupported player evidence status: {evidence.get('status')!r}")

    require_equal(machine.get("status"), "accepted", "player machine acceptance")
    require_equal(recorded_package, package_root, "player package root")
    require_equal(package_sha256, package_tree_digest(package_root), "player package tree")
    if not executable.is_file() or not executable.is_relative_to(package_root):
        raise ReleaseError("Player Shipping executable is missing or outside the package root")
    require_equal(
        shipping_executable_sha256,
        sha256_file(executable),
        "player Shipping executable",
    )

    developer = read_json(inputs.developer_payload_manifest)
    require_equal(developer.get("schema_version"), 2, "developer payload schema")
    require_equal(normalize_version(str(developer.get("release_version", ""))), inputs.release_version, "developer release version")
    developer_source = developer.get("public_source")
    if not isinstance(developer_source, dict):
        raise ReleaseError("Developer payload is missing public_source")
    require_equal(developer_source.get("tag"), inputs.release_tag, "developer source tag")
    require_equal(developer_source.get("revision"), public_revision, "developer source revision")
    developer_files = validate_developer_release(inputs, developer)

    component = read_json(inputs.component_manifest)
    require_equal(component.get("schema"), "alis-effective-component-manifest-v1", "component manifest schema")
    require_equal(component.get("source_commit"), public_revision, "component source revision")
    require_equal(component.get("source_tree"), public_tree, "component source tree")
    require_equal(component.get("source_tag"), inputs.release_tag, "component source tag")
    if int(component.get("entry_count", 0)) <= 0:
        raise ReleaseError("Component manifest contains no entries")

    dependency = read_json(inputs.dependency_report)
    if dependency.get("status") != "accepted" or dependency.get("issues") or int(dependency.get("hlod_package_count", -1)) != 0:
        raise ReleaseError("Developer dependency report is not accepted with zero issues and zero HLOD")
    require_equal(dependency.get("source_revision"), public_revision, "dependency source revision")
    require_equal(dependency.get("source_tree"), public_tree, "dependency source tree")

    privacy = read_json(inputs.privacy_report)
    if privacy.get("status") != "accepted" or int(privacy.get("issue_count", -1)) != 0:
        raise ReleaseError("Public source privacy report is not accepted with zero issues")
    require_equal(privacy.get("source_revision"), public_revision, "privacy source revision")
    require_equal(privacy.get("source_tree"), public_tree, "privacy source tree")

    map_load = read_json(inputs.map_load_report)
    require_equal(map_load.get("schema"), "alis-public-world-map-load-v1", "public map-load schema")
    require_equal(map_load.get("status"), "accepted", "public map-load status")
    required_maps = {
        "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory",
        "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase",
    }
    require_equal(set(map_load.get("maps", [])), required_maps, "public map-load maps")

    archive_report = read_json(archive_report_path)
    require_equal(archive_report.get("schema"), "alis-player-archive-v1", "player archive schema")
    require_equal(archive_report.get("status"), "accepted", "player archive status")
    require_equal(archive_report.get("package_tree_sha256"), package_sha256, "player archive package tree")
    archive_parts = archive_report.get("parts")
    if not isinstance(archive_parts, list) or not archive_parts:
        raise ReleaseError("Player archive report contains no parts")
    for part in archive_parts:
        path = archive_report_path.parent / str(part.get("name", ""))
        if not path.is_file():
            raise ReleaseError(f"Player archive part is missing: {path}")
        require_equal(part.get("byte_size"), path.stat().st_size, "player archive part size")
        require_equal(part.get("sha256"), sha256_file(path), "player archive part hash")

    if not inputs.product_terms.is_file() or not inputs.product_terms.read_text(encoding="utf-8-sig").strip():
        raise ReleaseError("Product terms are missing or empty")
    if not inputs.attribution_notice.is_file():
        raise ReleaseError("Generated attribution notice is missing")

    return {
        "private_revision": private_revision,
        "private_state": private_state,
        "public_revision": public_revision,
        "public_tree": public_tree,
        "player": {
            "operation_id": operation_id,
            "package_tree_sha256": package_sha256,
            "shipping_executable_sha256": shipping_executable_sha256,
            "product_review": product_review,
        },
        "evidence_files": evidence_files,
        "composite": composite,
        "archive_report": archive_report,
        "developer_manifest": developer,
        "developer_files": developer_files,
    }


def copy_asset(source: Path, output: Path, target_name: str, copied: dict[str, Path]) -> None:
    if not source.is_file():
        raise ReleaseError(f"Release asset is missing: {source}")
    if target_name in copied:
        if sha256_file(source) != sha256_file(copied[target_name]):
            raise ReleaseError(f"Conflicting release asset name: {target_name}")
        return
    target = output / target_name
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() == target.resolve():
        copied[target_name] = target
        return
    if target.exists():
        raise ReleaseError(f"Release output already contains unexpected asset: {target}")
    if source.resolve().is_relative_to(output.resolve()):
        source.replace(target)
    else:
        shutil.copy2(source, target)
    copied[target_name] = target


def write_release_text(output: Path, target_name: str, lines: list[str], copied: dict[str, Path]) -> None:
    target = output / target_name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines) + "\n", encoding="ascii")
    copied[target_name] = target


PLAYER_ARCHIVE_MANIFEST_MARKER = "__ALIS_PLAYER_ARCHIVE_MANIFEST_JSON__"


def write_player_installer(
    source: Path,
    output: Path,
    archive_report: dict[str, Any],
    copied: dict[str, Path],
) -> None:
    template = source.read_text(encoding="utf-8-sig")
    if template.count(PLAYER_ARCHIVE_MANIFEST_MARKER) != 1:
        raise ReleaseError("Player installer template has no unique archive-manifest marker")
    manifest_json = json.dumps(archive_report, sort_keys=True, separators=(",", ":"))
    rendered = template.replace(PLAYER_ARCHIVE_MANIFEST_MARKER, manifest_json.replace("'", "''"))
    target = output / "INSTALL_ALIS_PLAYER.ps1"
    target.write_text(rendered, encoding="utf-8")
    copied[target.name] = target


def write_release_guide(
    output: Path,
    version: str,
    tag: str,
    archive_report: dict[str, Any],
    developer_manifest: dict[str, Any],
    copied: dict[str, Path],
) -> None:
    repository = "https://github.com/fallintodusk/alis"
    if version == "2.0.0":
        player_changes = [
            "- Choose Kazan or Manhattan from the main menu.",
            "- Fly across the large-scale Kazan territory with fast overview controls.",
            "- Explore streamed terrain, water, roads, vegetation, and buildings rebuilt",
            "  from real geography.",
            "- Open the in-game pause menu with Escape.",
        ]
        developer_changes = [
            "- Build Kazan and Manhattan from real geographic data with the new",
            "  deterministic world pipeline.",
            "- Develop against Unreal Engine 5.8 with the generated world assets used by",
            "  the packaged release.",
            "- Validate world generation, streaming, gameplay routes, and performance with",
            "  expanded automated gates.",
        ]
        changes_heading = f"WHAT'S NEW {version}"
    else:
        player_changes = ["- See the Git history for changes in this release."]
        developer_changes = ["- See the Git history for changes in this release."]
        changes_heading = f"WHAT'S NEW {version}"
    player_parts = [
        require_safe_file_name(part.get("name"), "player archive part name")
        for part in archive_report["parts"]
    ]
    developer_parts = [
        require_safe_file_name(part.get("name"), "developer archive part name")
        for part in developer_manifest["archive"]["parts"]
    ]
    developer_payload = next(
        path.name for path in output.iterdir() if path.name.endswith(".developer-payload.json")
    )
    notices_artifact = next(
        path.name for path in output.iterdir() if path.name.endswith(".notices.json")
    )
    player_downloads = [f"   {name}" for name in player_parts]
    developer_downloads = [f"   {name}" for name in developer_parts]
    if len(player_parts) == 1 and not player_parts[0].endswith(".001"):
        player_extract = [f"2. With 7-Zip, extract {player_parts[0]}."]
    else:
        player_extract = [
            f"2. With 7-Zip, extract only {player_parts[0]}.",
            "   Keep the remaining numbered parts beside it.",
        ]
    if len(developer_parts) == 1 and not developer_parts[0].endswith(".001"):
        developer_extract = [f"3. With 7-Zip, extract {developer_parts[0]} into the source checkout."]
    else:
        developer_extract = [
            f"3. With 7-Zip, extract only {developer_parts[0]} into the source checkout.",
            "   Keep the remaining numbered parts beside it.",
        ]
    write_release_text(
        output,
        "README.txt",
        [
            f"ALIS {version}",
            "",
            "World Reborn",
            "",
            "Explore real geography rebuilt as playable, streamed worlds.",
            "",
            changes_heading,
            "",
            "Players",
            *player_changes,
            "",
            "Developers and contributors",
            *developer_changes,
            "",
            "PLAY ON WINDOWS",
            "",
            "1. Download these files into one folder:",
            "",
            *player_downloads,
            "",
            *player_extract,
            "",
            "3. Run Alis.exe from the extracted folder.",
            "",
            "Optional convenience",
            "Keep INSTALL_ALIS_PLAYER.bat and INSTALL_ALIS_PLAYER.ps1 beside the archive",
            "parts, then double-click INSTALL_ALIS_PLAYER.bat. It works offline, requires",
            "no administrator access, and only verifies, joins, and extracts the game.",
            "",
            "DEVELOP OR CONTRIBUTE",
            "",
            f"1. Clone the exact {tag} tag.",
            "",
            "2. Download:",
            "",
            *developer_downloads,
            "",
            *developer_extract,
            "",
            "4. Follow the Developer Quick Start:",
            f"   {repository}/blob/{tag}/developer/README.md",
            "",
            "Optional convenience",
            f"Keep those Developer archive files, {developer_payload}, the two Developer",
            "installer files, ALIS_PUBLIC_KEY.asc, SHA256SUMS.txt, and SHA256SUMS.txt.asc",
            "in one folder, then double-click INSTALL_ALIS_DEVELOPER.bat. It creates the exact",
            "tagged checkout, verifies the matching payload, and installs its generated assets.",
            "It does not install Unreal Engine or Visual Studio.",
            "",
            "OPTIONAL RELEASE VERIFICATION",
            "",
            "Advanced users can download the complete release and double-click",
            "VERIFY_RELEASE.bat to verify the publisher signature and every asset.",
            "",
            "LICENSES AND TERMS",
            "",
            "Product terms: PRODUCT_TERMS.txt",
            f"Data and third-party notices: {notices_artifact}",
            f"Open-source license: {repository}/blob/{tag}/LICENSE",
        ],
        copied,
    )


def prepare_release(inputs: ReleaseInputs, output: Path, archive_paths: Iterable[Path], archive_report_path: Path | None = None) -> Path:
    archive_paths = [Path(path).resolve() for path in archive_paths]
    if not archive_paths:
        raise ReleaseError("At least one player archive is required")
    if archive_report_path is None:
        archive_report_path = archive_paths[0].parent / "player-archive.json"
    validated = validate_inputs(inputs, archive_report_path.resolve())
    if output.exists():
        allowed = {path.resolve() for path in archive_paths}
        allowed.add(archive_report_path.resolve())
        existing = {path.resolve() for path in output.iterdir()}
        if existing != allowed or any(not path.is_file() for path in output.iterdir()):
            raise ReleaseError("Release output may contain only the prepared player archive and its report")

    sources: list[tuple[Path, str]] = []
    for archive in archive_paths:
        sources.append((archive, archive.name))
    for source in sorted(validated["developer_files"], key=lambda item: item.name.lower()):
        if source.name not in DEVELOPER_INPUT_ONLY_FILES:
            sources.append((source, source.name))
    sources.extend(
        [
            (inputs.component_manifest, "effective-component-manifest.json"),
            (inputs.product_terms, "PRODUCT_TERMS.txt"),
        ]
    )
    names: dict[str, Path] = {}
    for source, name in sources:
        if name in names and sha256_file(source) != sha256_file(names[name]):
            raise ReleaseError(f"Conflicting release asset name: {name}")
        names[name] = source

    output.mkdir(parents=True, exist_ok=True)
    copied: dict[str, Path] = {}
    try:
        for source, name in sources:
            copy_asset(source, output, name, copied)
        package_scripts = Path(__file__).resolve().parent
        mirror_scripts = package_scripts.parents[1] / "git" / "mirror"
        write_player_installer(
            package_scripts / "install_player_release.ps1",
            output,
            validated["archive_report"],
            copied,
        )
        copy_asset(package_scripts / "install_player_release.bat", output, "INSTALL_ALIS_PLAYER.bat", copied)
        copy_asset(mirror_scripts / "bootstrap_developer_release.ps1", output, "INSTALL_ALIS_DEVELOPER.ps1", copied)
        copy_asset(mirror_scripts / "bootstrap_developer_release.bat", output, "INSTALL_ALIS_DEVELOPER.bat", copied)
        write_release_guide(
            output,
            inputs.release_version,
            inputs.release_tag,
            validated["archive_report"],
            validated["developer_manifest"],
            copied,
        )
        if archive_report_path.is_relative_to(output.resolve()):
            archive_report_path.unlink()
        artifacts = [
            {
                "name": name,
                "byte_size": path.stat().st_size,
                "sha256": sha256_file(path),
            }
            for name, path in sorted(copied.items())
        ]
        manifest = {
            "schema": "alis-release-manifest-v3",
            "status": "pending_owner_approval",
            "release_version": inputs.release_version,
            "release_tag": inputs.release_tag,
            "public_source": {
                "revision": validated["public_revision"],
                "tree": validated["public_tree"],
            },
            "player_source": {
                "revision": validated["private_revision"],
                "source_state_sha256": validated["private_state"],
                "operation_id": validated["player"]["operation_id"],
                "package_tree_sha256": validated["player"]["package_tree_sha256"],
                "shipping_executable_sha256": validated["player"]["shipping_executable_sha256"],
            },
            "unresolved_count": 0,
            "product_review": {"status": validated["player"]["product_review"]},
            "rights_review": {"status": "pending_owner_approval"},
            "release_documents": {
                "component_manifest_artifact": "effective-component-manifest.json",
                "developer_guide_url": f"https://github.com/fallintodusk/alis/blob/{inputs.release_tag}/developer/README.md",
                "notices_artifact": inputs.attribution_notice.name,
                "product_terms_artifact": "PRODUCT_TERMS.txt",
                "source_license_url": f"https://github.com/fallintodusk/alis/blob/{inputs.release_tag}/LICENSE",
            },
            "artifacts": artifacts,
        }
        manifest_path = output / "release_manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return manifest_path
    except Exception:
        shutil.rmtree(output, ignore_errors=True)
        raise


def verify_artifacts(root: Path, manifest: dict[str, Any]) -> None:
    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        raise ReleaseError("Release manifest contains no artifacts")
    seen: set[str] = set()
    for item in artifacts:
        name = str(item.get("name", ""))
        relative = Path(name)
        if (
            not name
            or relative.is_absolute()
            or ".." in relative.parts
            or "\\" in name
            or relative.name != name
            or name in seen
        ):
            raise ReleaseError(f"Invalid or duplicate release artifact name: {name!r}")
        seen.add(name)
        path = root / name
        if not path.is_file():
            raise ReleaseError(f"Release artifact is missing: {name}")
        require_equal(item.get("byte_size"), path.stat().st_size, f"release artifact size for {name}")
        require_equal(item.get("sha256"), sha256_file(path), f"release artifact hash for {name}")
    expected = seen | {"release_manifest.json"}
    actual = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file()
    }
    directories = [path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_dir()]
    if directories or actual != expected:
        raise ReleaseError(
            "Release directory inventory mismatch: "
            f"unexpected={sorted(actual - expected) + sorted(directories)}, "
            f"missing={sorted(expected - actual)}"
        )


def verify_release_manifest(root: Path, require_ready: bool = False) -> dict[str, Any]:
    manifest = read_json(root / "release_manifest.json")
    require_equal(manifest.get("schema"), "alis-release-manifest-v3", "release manifest schema")
    expected = "ready_for_signature" if require_ready else manifest.get("status")
    if expected not in {"pending_owner_approval", "ready_for_signature"}:
        raise ReleaseError(f"Unsupported release manifest status: {expected!r}")
    if require_ready:
        require_equal(manifest.get("status"), "ready_for_signature", "release readiness")
        product = manifest.get("product_review")
        if not isinstance(product, dict) or product.get("status") != "accepted":
            raise ReleaseError("Release Product review is not accepted")
        rights = manifest.get("rights_review")
        if not isinstance(rights, dict) or rights.get("status") != "accepted":
            raise ReleaseError("Release rights review is not accepted")
    require_equal(manifest.get("unresolved_count"), 0, "release unresolved count")
    verify_artifacts(root, manifest)
    return manifest


def approve_release(root: Path, approve: bool) -> Path:
    if not approve:
        raise ReleaseError("Owner approval requires the explicit approval flag")
    manifest_path = root / "release_manifest.json"
    manifest = verify_release_manifest(root)
    require_equal(manifest.get("status"), "pending_owner_approval", "release approval state")
    terms = next((item for item in manifest["artifacts"] if item["name"] == "PRODUCT_TERMS.txt"), None)
    if terms is None:
        raise ReleaseError("Release manifest does not bind PRODUCT_TERMS.txt")
    rights = {
        "schema": "alis-release-rights-review-v1",
        "status": "accepted",
        "release_version": manifest["release_version"],
        "release_tag": manifest["release_tag"],
        "public_source_revision": manifest["public_source"]["revision"],
        "product_terms_sha256": terms["sha256"],
        "unresolved_count": 0,
        "review_role": "release_owner",
    }
    rights_path = root / "release-rights-review.json"
    if rights_path.exists():
        raise ReleaseError(f"Release rights review already exists: {rights_path}")
    rights_path.write_text(json.dumps(rights, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    manifest["artifacts"].append(
        {
            "name": rights_path.relative_to(root).as_posix(),
            "byte_size": rights_path.stat().st_size,
            "sha256": sha256_file(rights_path),
        }
    )
    manifest["artifacts"] = sorted(manifest["artifacts"], key=lambda item: item["name"])
    manifest["rights_review"] = {
        "status": "accepted",
        "artifact": rights_path.relative_to(root).as_posix(),
        "sha256": sha256_file(rights_path),
    }
    manifest["product_review"] = {
        "status": "accepted",
        "evidence": "owner approval of the exact prepared release directory",
    }
    manifest["status"] = "ready_for_signature"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    verify_release_manifest(root, require_ready=True)
    return manifest_path


def resolve_7zip(requested: str | None) -> str:
    candidates = [requested, shutil.which("7z.exe"), shutil.which("7z"), r"C:\Program Files\7-Zip\7z.exe"]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).resolve())
    raise ReleaseError("7-Zip was not found; pass --seven-zip")


def archive_player(package_root: Path, output: Path, version: str, split_size_mib: int, seven_zip: str | None) -> Path:
    package_root = package_root.resolve()
    if not 1 <= split_size_mib <= MAX_RELEASE_PART_SIZE_MIB:
        raise ReleaseError(
            f"Player archive split size must be between 1 and {MAX_RELEASE_PART_SIZE_MIB} MiB"
        )
    windows = package_root / "Windows"
    if not windows.is_dir():
        raise ReleaseError(f"Accepted player package has no Windows directory: {package_root}")
    if output.exists():
        raise ReleaseError(f"Player archive output must not exist: {output}")
    output.mkdir(parents=True)
    executable = list(windows.glob("Alis/Binaries/Win64/Alis-Win64-Shipping.exe"))
    if len(executable) != 1:
        shutil.rmtree(output, ignore_errors=True)
        raise ReleaseError("Accepted player package must contain exactly one Shipping executable")
    tool = resolve_7zip(seven_zip)
    base = output / f"ALIS_Win64_v{normalize_version(version)}.zip"
    command = [tool, "a", "-tzip", f"-v{split_size_mib}m", str(base), str(windows / "*")]
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        shutil.rmtree(output, ignore_errors=True)
        raise ReleaseError(f"7-Zip archive creation failed with exit code {result.returncode}")
    parts = sorted(output.glob(base.name + ".*"))
    if not parts:
        parts = [base] if base.is_file() else []
    if not parts:
        shutil.rmtree(output, ignore_errors=True)
        raise ReleaseError("7-Zip produced no player archive")
    oversized = [part for part in parts if part.stat().st_size >= GITHUB_RELEASE_ASSET_LIMIT_BYTES]
    if oversized:
        names = ", ".join(part.name for part in oversized)
        shutil.rmtree(output, ignore_errors=True)
        raise ReleaseError(f"Player archive exceeded the GitHub release asset limit: {names}")
    test = subprocess.run([tool, "t", str(parts[0])], check=False)
    if test.returncode != 0:
        shutil.rmtree(output, ignore_errors=True)
        raise ReleaseError(f"7-Zip archive verification failed with exit code {test.returncode}")
    report = {
        "schema": "alis-player-archive-v1",
        "status": "accepted",
        "package_tree_sha256": package_tree_digest(package_root),
        "parts": [
            {"name": part.name, "byte_size": part.stat().st_size, "sha256": sha256_file(part)}
            for part in parts
        ],
    }
    report_path = output / "player-archive.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    archive = commands.add_parser("archive-player")
    archive.add_argument("--package-root", type=Path, required=True)
    archive.add_argument("--output-dir", type=Path, required=True)
    archive.add_argument("--release-version", required=True)
    archive.add_argument("--split-size-mib", type=int, default=DEFAULT_RELEASE_PART_SIZE_MIB)
    archive.add_argument("--seven-zip")
    prepare = commands.add_parser("prepare")
    prepare.add_argument("--release-version", required=True)
    prepare.add_argument("--release-tag", required=True)
    prepare.add_argument("--private-source-root", type=Path, required=True)
    prepare.add_argument("--public-source-root", type=Path, required=True)
    prepare.add_argument("--player-package-root", type=Path, required=True)
    prepare.add_argument("--player-evidence", type=Path, required=True)
    prepare.add_argument("--player-archive-report", type=Path, required=True)
    prepare.add_argument("--developer-release-dir", type=Path, required=True)
    prepare.add_argument("--developer-payload-manifest", type=Path, required=True)
    prepare.add_argument("--component-manifest", type=Path, required=True)
    prepare.add_argument("--dependency-report", type=Path, required=True)
    prepare.add_argument("--privacy-report", type=Path, required=True)
    prepare.add_argument("--map-load-report", type=Path, required=True)
    prepare.add_argument("--attribution-notice", type=Path, required=True)
    prepare.add_argument("--product-terms", type=Path, required=True)
    prepare.add_argument("--output-dir", type=Path, required=True)
    approve = commands.add_parser("approve")
    approve.add_argument("--release-dir", type=Path, required=True)
    approve.add_argument("--approve-product-terms-and-rights", action="store_true")
    verify = commands.add_parser("verify")
    verify.add_argument("--release-dir", type=Path, required=True)
    verify.add_argument("--require-ready", action="store_true")
    source_state = commands.add_parser("source-state")
    source_state.add_argument("--source-root", type=Path, required=True)
    package_tree = commands.add_parser("package-tree")
    package_tree.add_argument("--package-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "source-state":
            print(source_state_digest(args.source_root))
            return 0
        if args.command == "package-tree":
            print(package_tree_digest(args.package_root))
            return 0
        if args.command == "archive-player":
            result = archive_player(args.package_root, args.output_dir, args.release_version, args.split_size_mib, args.seven_zip)
        elif args.command == "prepare":
            inputs = ReleaseInputs(
                args.release_version,
                args.release_tag,
                args.private_source_root,
                args.public_source_root,
                args.player_package_root,
                args.player_evidence,
                args.developer_release_dir,
                args.developer_payload_manifest,
                args.component_manifest,
                args.dependency_report,
                args.privacy_report,
                args.map_load_report,
                args.attribution_notice,
                args.product_terms,
            )
            archive_report = args.player_archive_report.resolve()
            report = read_json(archive_report)
            archives = [archive_report.parent / item["name"] for item in report.get("parts", [])]
            result = prepare_release(inputs, args.output_dir, archives, archive_report)
        elif args.command == "approve":
            result = approve_release(args.release_dir, args.approve_product_terms_and_rights)
        else:
            verify_release_manifest(args.release_dir, args.require_ready)
            result = args.release_dir / "release_manifest.json"
    except (ReleaseError, OSError, ValueError, KeyError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    print(f"[OK] {args.command}: {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
