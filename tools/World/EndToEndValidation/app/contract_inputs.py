from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path, PurePosixPath

from .producer_descriptors import load_producer_descriptors
from .profile_inputs import executable_profile_inputs


SOURCE_SUFFIXES = frozenset({".cpp", ".cs", ".h", ".inl"})
SCRIPT_SUFFIXES = frozenset({".bat", ".ps1", ".py", ".sh"})
# Explicit proof roots. The fitness check (proof_input_fitness.py) fails when UBT's
# resolved graph of a descriptor-named module reaches first-party code outside them.
CONTRACT_TREE_INPUTS = (
    ("tools/World", frozenset({".json", ".py", ".txt"})),
    ("scripts/ue/package", SCRIPT_SUFFIXES),
    ("scripts/ue/check", SCRIPT_SUFFIXES),
    ("scripts/ue/world", SCRIPT_SUFFIXES),
    ("scripts/ue/test", SCRIPT_SUFFIXES),
    ("scripts/ue/generated_content", SCRIPT_SUFFIXES),
    ("scripts/ue/material", SCRIPT_SUFFIXES),
    ("Plugins/World/ProjectWorld/Source", SOURCE_SUFFIXES),
    ("Plugins/World/ProjectWorld/Data/Schemas", frozenset({".json"})),
    ("Plugins/World/ProjectWorldMeshTerrain/Source", SOURCE_SUFFIXES),
    ("Plugins/Foundation/ProjectCore/Source", SOURCE_SUFFIXES),
    ("Plugins/Resources/ProjectObject/Source", SOURCE_SUFFIXES),
    ("Plugins/Gameplay/ProjectObjectCapabilities/Source", SOURCE_SUFFIXES),
)
# Gameplay placement resolves ProjectObject's spawn service through the service
# locator, a link UBT's module graph cannot show; the fitness check follows the
# graph from these modules as well as from the descriptor-named ones.
RUNTIME_PROVIDER_MODULES = ("ProjectObject",)
IMMUTABLE_TEST_INPUT_TREES = (
    ("Plugins/World/ProjectWorldTestData/Data", frozenset({"Manifests"}), None),
    ("Plugins/World/ProjectWorldTestData/Content/Authored", frozenset(), None),
)
CONTRACT_FILES = (
    "Alis.uproject", "Config/DefaultGame.ini", "Config/DefaultEngine.ini",
    "scripts/config/ue_path.conf",
)
COMMON_TEST_PROFILE_IDS = frozenset({"kazan_territory_v1"})


def _under(path: PurePosixPath, root: str) -> tuple[str, ...] | None:
    try:
        return path.relative_to(PurePosixPath(root)).parts
    except ValueError:
        return None


def _is_producer_record(path: PurePosixPath) -> bool:
    """A file under any plugin's Data/Producers/ or Data/TestFixtures/Verify/."""
    parts = path.parts
    if len(parts) < 6 or parts[0] != "Plugins" or parts[3] != "Data":
        return False
    return parts[4] == "Producers" or (len(parts) > 6 and parts[4:6] == ("TestFixtures", "Verify"))


def _is_verifier(path: PurePosixPath) -> bool:
    """The verify record and run scripts, named explicitly whatever the script roots become."""
    return (
        _under(path, "scripts/ue/world") is not None
        and path.suffix.lower() in {".ps1", ".py"}
        and "verify" in path.name.lower()
    )


def is_shared_contract_path(relative_path: str) -> bool:
    path = PurePosixPath(relative_path.replace("\\", "/").lstrip("./"))
    normalized = path.as_posix()
    if normalized in CONTRACT_FILES:
        return True
    if path.suffix.lower() == ".uplugin" and path.parts and path.parts[0] == "Plugins":
        return True
    if _is_producer_record(path) or _is_verifier(path):
        return True
    for root, suffixes in CONTRACT_TREE_INPUTS:
        if _under(path, root) is not None and path.suffix.lower() in suffixes:
            return True
    for root, excluded_roots, suffixes in IMMUTABLE_TEST_INPUT_TREES:
        parts = _under(path, root)
        if parts is not None and not excluded_roots.intersection(parts):
            return suffixes is None or path.suffix.lower() in suffixes
    return False


def covers_module_directory(relative_directory: str) -> bool:
    """True when one declared source root holds the whole module directory."""
    path = PurePosixPath(relative_directory)
    return any(
        SOURCE_SUFFIXES <= suffixes and _under(path, root) is not None
        for root, suffixes in CONTRACT_TREE_INPUTS
    )


def declared_input_files(
    repo_root: Path,
    profile_inputs: Mapping[str, frozenset[str]] | None = None,
) -> frozenset[str]:
    """Proof inputs named by data rather than location: descriptor data inputs and the
    transitive inputs of the profiles the common checks read. A caller that already
    holds executable_profile_inputs(repo_root) passes it to skip a second scan."""
    if profile_inputs is None:
        profile_inputs = executable_profile_inputs(repo_root, COMMON_TEST_PROFILE_IDS)
    data_inputs = {item for descriptor in load_producer_descriptors(repo_root) for item in descriptor.data_inputs}
    common_profile_inputs = {
        relative
        for profile_id, paths in profile_inputs.items()
        if profile_id in COMMON_TEST_PROFILE_IDS
        for relative in paths
    }
    return frozenset(data_inputs | common_profile_inputs)


def common_contract_files(repo_root: Path) -> list[Path]:
    # Imported here: plan imports this module without the validation dependencies.
    from .proof_input_fitness import assert_proof_input_fitness

    assert_proof_input_fitness(repo_root)
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
    for pattern in ("*/*/Data/Producers/*", "*/*/Data/TestFixtures/Verify/**/*"):
        candidates.update(path for path in (repo_root / "Plugins").glob(pattern) if path.is_file())
    candidates.update(path for path in (repo_root / "scripts/ue/world").rglob("*verify*") if path.is_file())
    shared = {
        path for path in candidates
        if is_shared_contract_path(path.relative_to(repo_root).as_posix())
    }
    return sorted(shared | {repo_root / relative for relative in declared_input_files(repo_root)})
