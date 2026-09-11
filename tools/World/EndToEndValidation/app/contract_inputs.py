from __future__ import annotations

from pathlib import Path, PurePosixPath

from .profile_inputs import executable_profile_inputs


CONTRACT_TREE_INPUTS = (
    ("tools/World", frozenset({".json", ".py", ".txt"})),
    ("scripts/ue/package", frozenset({".bat", ".ps1", ".py", ".sh"})),
    ("scripts/ue/check", frozenset({".bat", ".ps1", ".py", ".sh"})),
    ("scripts/ue/world", frozenset({".bat", ".ps1", ".py", ".sh"})),
    ("scripts/ue/test", frozenset({".bat", ".ps1", ".py", ".sh"})),
    ("Plugins/World/ProjectWorld/Source", frozenset({".cpp", ".cs", ".h"})),
    ("Plugins/World/ProjectWorld/Data/Schemas", frozenset({".json"})),
)
IMMUTABLE_TEST_INPUT_TREES = (
    ("Plugins/World/ProjectWorldTestData/Data", frozenset({"Manifests"}), None),
    ("Plugins/World/ProjectWorldTestData/Content/Authored", frozenset(), None),
)
CONTRACT_FILES = ("Alis.uproject", "Config/DefaultGame.ini", "Config/DefaultEngine.ini")
COMMON_TEST_PROFILE_IDS = frozenset({"p0", "representative_v1"})


def _under(path: PurePosixPath, root: str) -> tuple[str, ...] | None:
    try:
        return path.relative_to(PurePosixPath(root)).parts
    except ValueError:
        return None


def is_shared_contract_path(relative_path: str) -> bool:
    path = PurePosixPath(relative_path.replace("\\", "/").lstrip("./"))
    normalized = path.as_posix()
    if normalized in CONTRACT_FILES:
        return True
    if path.suffix.lower() == ".uplugin" and path.parts and path.parts[0] == "Plugins":
        return True
    for root, suffixes in CONTRACT_TREE_INPUTS:
        if _under(path, root) is not None and path.suffix.lower() in suffixes:
            return True
    for root, excluded_roots, suffixes in IMMUTABLE_TEST_INPUT_TREES:
        parts = _under(path, root)
        if parts is not None and not excluded_roots.intersection(parts):
            return suffixes is None or path.suffix.lower() in suffixes
    return False


def common_contract_files(repo_root: Path) -> list[Path]:
    candidates = {
        path
        for relative_root, _ in CONTRACT_TREE_INPUTS
        for path in (repo_root / relative_root).rglob("*")
        if path.is_file()
    }
    candidates.update(
        path
        for relative_root, _, _ in IMMUTABLE_TEST_INPUT_TREES
        for path in (repo_root / relative_root).rglob("*")
        if path.is_file()
    )
    candidates.update(path for path in (repo_root / "Plugins").rglob("*.uplugin") if path.is_file())
    candidates.update(repo_root / path for path in CONTRACT_FILES if (repo_root / path).is_file())
    shared = {
        path for path in candidates
        if is_shared_contract_path(path.relative_to(repo_root).as_posix())
    }
    profile_paths = {
        repo_root / relative
        for paths in executable_profile_inputs(repo_root, COMMON_TEST_PROFILE_IDS).values()
        for relative in paths
    }
    return sorted(shared | profile_paths)
