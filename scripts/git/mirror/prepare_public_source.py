"""Apply deterministic public-only project descriptor and config projections."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path


class ProjectionError(RuntimeError):
    pass


KAZAN_MAP = "/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory"
MANHATTAN_MAP = "/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase"
FORBIDDEN_PUBLIC_CONFIG_TOKENS = (
    "/City17/",
    "/MainMenuWorld/",
    "/ProjectWorldData/Generated/P0/",
    "/ProjectWorldData/Authored/Survival/",
)
PUBLIC_WORLD_SCOPE_PREFIXES = (
    "layer_kazan_territory_public_v1_",
    "layer_manhattan_showcase_public_v1_",
    "map_territory_l_projectworldkazanterritory",
    "map_showcase_manhattan_l_projectworldmanhattanshowcase",
    "presentation_kazan_representative_v1",
)
PUBLIC_BINARY_IGNORE_START = "# BEGIN ALIS PUBLIC BINARY PAYLOAD"
PUBLIC_BINARY_IGNORE_END = "# END ALIS PUBLIC BINARY PAYLOAD"
PUBLIC_BINARY_IGNORE_LINES = (
    "*.uasset",
    "*.umap",
    "/Plugins/World/ProjectWorldData/Data/Canonical/*/bundles/*.zip",
)
PUBLIC_DISABLED_PLUGINS = (
    "InstanceArrayTool",
    "ProjectIntegrationTests",
    "ProjectOpenableTemplates",
)
PUBLIC_DISABLED_DEFAULT_PLUGINS = (
    "ProjectPlacementEditor",
)
PUBLIC_GITHUB_ROOT = "https://github.com/fallintodusk/alis"


def write_text_preserving_newlines(path: Path, original: str, text: str) -> None:
    newline = "\r\n" if "\r\n" in original else "\n"
    normalized = text.replace("\r\n", "\n").replace("\n", newline)
    path.write_text(normalized, encoding="utf-8", newline="")


def project_descriptor(root: Path) -> None:
    path = root / "Alis.uproject"
    document = json.loads(path.read_text(encoding="utf-8-sig"))
    for plugin_name in PUBLIC_DISABLED_PLUGINS:
        matches = [item for item in document.get("Plugins", []) if item.get("Name") == plugin_name]
        if len(matches) != 1:
            raise ProjectionError(f"Alis.uproject must declare {plugin_name} exactly once")
        matches[0]["Enabled"] = False
    for plugin_name in PUBLIC_DISABLED_DEFAULT_PLUGINS:
        matches = [item for item in document.get("Plugins", []) if item.get("Name") == plugin_name]
        if len(matches) > 1:
            raise ProjectionError(f"Alis.uproject declares {plugin_name} more than once")
        if matches:
            matches[0]["Enabled"] = False
        else:
            document.setdefault("Plugins", []).append({"Name": plugin_name, "Enabled": False})
    path.write_text(json.dumps(document, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def public_gitignore(root: Path) -> None:
    path = root / ".gitignore"
    original = path.read_text(encoding="utf-8-sig")
    block = "\n".join((PUBLIC_BINARY_IGNORE_START, *PUBLIC_BINARY_IGNORE_LINES, PUBLIC_BINARY_IGNORE_END))
    pattern = re.compile(
        rf"(?ms)^[ \t]*{re.escape(PUBLIC_BINARY_IGNORE_START)}[ \t]*$.*?"
        rf"^[ \t]*{re.escape(PUBLIC_BINARY_IGNORE_END)}[ \t]*(?:\r?\n)?"
    )
    if pattern.search(original):
        text = pattern.sub(block, original).rstrip() + "\n"
    else:
        text = original.rstrip() + "\n\n" + block + "\n"
    write_text_preserving_newlines(path, original, text)


def public_readme(root: Path) -> None:
    path = root / "README.md"
    original = path.read_text(encoding="utf-8-sig")
    text = original.replace("https://github.com/<user>/alis", PUBLIC_GITHUB_ROOT)
    write_text_preserving_newlines(path, original, text)


def public_documentation(root: Path) -> None:
    render_path = root / "docs" / "config" / "render" / "render.md"
    if render_path.is_file():
        original = render_path.read_text(encoding="utf-8-sig")
        text = re.sub(
            r"(?im)^(\s*SecurityToken\s*=\s*)[^\r\n]+$",
            r"\1<redacted>",
            original,
        )
        write_text_preserving_newlines(render_path, original, text)

    automation_path = root / "docs" / "testing" / "automation.md"
    if automation_path.is_file():
        original = automation_path.read_text(encoding="utf-8-sig")
        text = re.sub(
            r"[A-Za-z]:(?:\\\\|\\)Repos_Alis(?:\\\\|\\)Alis",
            "<repo>",
            original,
        )
        write_text_preserving_newlines(automation_path, original, text)


def default_engine(root: Path) -> None:
    path = root / "Config" / "DefaultEngine.ini"
    original = path.read_text(encoding="utf-8-sig")
    text = re.sub(
        r"(?m)^EditorStartupMap=.*$",
        f"EditorStartupMap={KAZAN_MAP}.{KAZAN_MAP.rsplit('/', 1)[1]}",
        original,
        count=1,
    )
    text = re.sub(
        r"(?m)^GameDefaultMap=.*$",
        f"GameDefaultMap={KAZAN_MAP}.{KAZAN_MAP.rsplit('/', 1)[1]}",
        text,
        count=1,
    )
    text = re.sub(r"(?m)^SecurityToken=.*(?:\r?\n)?", "", text)
    text = re.sub(
        r"(?m)^DefaultSoundClassName=/ProjectAudio/System/MasterSoundClass\.MasterSoundClass(?:\r?\n)?",
        "",
        text,
    )
    write_text_preserving_newlines(path, original, text)


def default_game(root: Path) -> None:
    path = root / "Config" / "DefaultGame.ini"
    original = path.read_text(encoding="utf-8-sig")
    kept = []
    map_scan = (
        '+PrimaryAssetTypesToScan=(PrimaryAssetType="Map",AssetBaseClass="/Script/Engine.World",'
        'bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/ProjectWorldData/Generated/Territory"),'
        '(Path="/ProjectWorldData/Generated/Showcase/Manhattan")),SpecificAssets=,'
        'Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=Unknown))'
    )
    for line in original.splitlines():
        if line.startswith('+PrimaryAssetTypesToScan=(PrimaryAssetType="Map"'):
            if map_scan not in kept:
                kept.append(map_scan)
            continue
        if any(token in line for token in FORBIDDEN_PUBLIC_CONFIG_TOKENS):
            continue
        if line.startswith("+DirectoriesToAlwaysCook="):
            continue
        if line.startswith("EntryPointExperience="):
            kept.append("EntryPointExperience=KazanTerritory")
            continue
        kept.append(line)
    text = "\n".join(kept) + "\n"
    write_text_preserving_newlines(path, original, text)


def project_world_manifests(root: Path, source_root: Path) -> None:
    active_path = source_root / "active_set.json"
    active = json.loads(active_path.read_text(encoding="utf-8-sig"))
    scopes = active.get("scopes", [])
    if len(scopes) != 11:
        raise ProjectionError("Public World authority must contain exactly 11 scopes")
    selected = []
    for scope in scopes:
        scope_id = str(scope.get("scope_id", ""))
        if not any(scope_id.startswith(prefix) for prefix in PUBLIC_WORLD_SCOPE_PREFIXES):
            raise ProjectionError(f"Non-public World scope entered projection: {scope_id}")
        relative = Path(str(scope.get("manifest_path", "")))
        if relative.is_absolute() or ".." in relative.parts or relative.parts[:1] != ("scopes",):
            raise ProjectionError(f"Unsafe public World manifest path: {relative}")
        source = source_root / relative
        if not source.is_file():
            raise ProjectionError(f"Public World manifest is missing: {source}")
        if "hlod" in source.read_text(encoding="utf-8-sig").lower():
            raise ProjectionError(f"HLOD entered public World manifest: {source}")
        selected.append((relative, source))
    target_root = root / "Plugins" / "World" / "ProjectWorldData" / "Data" / "Manifests"
    target_scopes = target_root / "scopes"
    target_scopes.mkdir(parents=True, exist_ok=True)
    for existing in target_scopes.glob("*.json"):
        existing.unlink()
    scope_hashes = {}
    for relative, source in selected:
        target = target_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        document = json.loads(source.read_text(encoding="utf-8-sig"))
        document["$schema"] = "../../../../ProjectWorld/Data/Schemas/project_world_generated_manifest.schema.json"
        target.write_text(json.dumps(document, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        scope_hashes[relative.as_posix()] = hashlib.sha256(target.read_bytes()).hexdigest()
    active["$schema"] = "../../../ProjectWorld/Data/Schemas/project_world_active_manifest_set.schema.json"
    for scope in active["scopes"]:
        scope["manifest_sha256"] = scope_hashes[str(scope["manifest_path"])]
    (target_root / "active_set.json").write_text(
        json.dumps(active, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )


def rebind_generated_definition_sources(root: Path) -> None:
    contract_path = root / "scripts" / "git" / "mirror" / "developer_asset_release.json"
    if not contract_path.is_file():
        return
    contract = json.loads(contract_path.read_text(encoding="utf-8-sig"))
    for authority in contract.get("asset_authorities", []):
        if authority.get("authority_kind") != "generated_definition_manifest":
            continue
        manifest_path = root / str(authority["manifest_path"])
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        for asset in manifest.get("assets", []):
            source_path = root / str(asset["source_path"])
            text = source_path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
            normalized = text.encode("utf-8")
            source_path.write_bytes(normalized)
            semantic_md5 = hashlib.md5(normalized, usedforsecurity=False).hexdigest()
            if semantic_md5 != asset.get("source_json_hash_md5"):
                raise ProjectionError(f"Public filtering changed generated source semantics: {asset['source_path']}")
            asset["source_sha256"] = hashlib.sha256(normalized).hexdigest()
        manifest_path.write_text(
            json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
        )


def verify(root: Path) -> None:
    descriptor = json.loads((root / "Alis.uproject").read_text(encoding="utf-8-sig"))
    for plugin_name in (*PUBLIC_DISABLED_PLUGINS, *PUBLIC_DISABLED_DEFAULT_PLUGINS):
        matches = [item for item in descriptor.get("Plugins", []) if item.get("Name") == plugin_name]
        if len(matches) != 1 or matches[0].get("Enabled") is not False:
            raise ProjectionError(f"{plugin_name} remains enabled in the public projection")
    config_text = "\n".join(
        path.read_text(encoding="utf-8-sig")
        for path in (root / "Config" / "DefaultEngine.ini", root / "Config" / "DefaultGame.ini")
    )
    for token in FORBIDDEN_PUBLIC_CONFIG_TOKENS:
        if token in config_text:
            raise ProjectionError(f"Excluded map/content token remains in public config: {token}")
    if "SecurityToken=" in config_text:
        raise ProjectionError("SecurityToken remains in public config")
    if "/ProjectAudio/System/MasterSoundClass" in config_text:
        raise ProjectionError("Unavailable ProjectAudio default remains in public config")
    for required in (KAZAN_MAP, MANHATTAN_MAP, "EntryPointExperience=KazanTerritory"):
        if required not in config_text:
            raise ProjectionError(f"Required public config route is absent: {required}")
    ignore_text = (root / ".gitignore").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    expected_block = "\n".join(
        (PUBLIC_BINARY_IGNORE_START, *PUBLIC_BINARY_IGNORE_LINES, PUBLIC_BINARY_IGNORE_END)
    )
    if ignore_text.count(expected_block) != 1:
        raise ProjectionError("Public binary payload ignore policy is absent or duplicated")
    readme = (root / "README.md").read_text(encoding="utf-8-sig")
    for suffix in (".git", "/releases/latest", "/security/advisories/new"):
        if PUBLIC_GITHUB_ROOT + suffix not in readme:
            raise ProjectionError(f"Required public GitHub route is absent: {suffix}")
    if "https://github.com/<user>/alis" in readme:
        raise ProjectionError("Placeholder GitHub route remains in the public README")


def project(root: Path, world_manifest_root: Path | None = None) -> None:
    project_descriptor(root)
    public_gitignore(root)
    public_readme(root)
    public_documentation(root)
    default_engine(root)
    default_game(root)
    if world_manifest_root is not None:
        project_world_manifests(root, world_manifest_root)
    rebind_generated_definition_sources(root)
    verify(root)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--world-manifest-root", type=Path)
    args = parser.parse_args()
    try:
        project(
            args.root.resolve(),
            args.world_manifest_root.resolve() if args.world_manifest_root else None,
        )
    except (OSError, ValueError, KeyError, json.JSONDecodeError, ProjectionError) as error:
        print(f"[FAIL] {error}")
        return 1
    print(f"[OK] Public source projection: {args.root.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
