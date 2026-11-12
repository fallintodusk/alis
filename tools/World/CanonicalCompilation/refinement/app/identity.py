"""Identity of the refinement operation and the existing helpers it consumes."""
from pathlib import Path
from ...app.contracts import canonical_hash, file_hash


def implementation_identity() -> str:
    child = Path(__file__).resolve().parents[1]
    owner = child.parent
    files = [child / "api.py", *child.glob("app/*.py"), *child.glob("contracts/*.schema.json"),
             *owner.glob("app/*.py"), *owner.glob("contracts/*.schema.json")]
    return canonical_hash({path.relative_to(owner).as_posix(): file_hash(path) for path in sorted(files)})


def request_is_current(repo_root, request):
    from World.SourceIngestion.api import osm_context_identity
    from World.ExecutionEnvironment.geometry.api import execution_identity
    if (request['implementation_sha256'] != implementation_identity() or
            request['source_context_identity'] != osm_context_identity() or
            request['execution_identity'] != execution_identity(repo_root, True)['identity_sha256']):
        return False
    for operation in request['operations']:
        if operation['kind'] == 'identified_basin':
            from ..basin_admission import implementation_identity as basin_identity
            if operation['construction_identity'] != basin_identity(repo_root):
                return False
        if operation['kind'] == 'constructed_surface_import':
            from ..constructed_import import implementation_identity as constructor_identity
            if operation['construction_identity'] != constructor_identity(operation['source_qualification']):
                return False
    return True
