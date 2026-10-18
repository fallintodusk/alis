"""Identity of the refinement operation and the existing helpers it consumes."""
from pathlib import Path
from ...app.contracts import canonical_hash, file_hash


def implementation_identity() -> str:
    child = Path(__file__).resolve().parents[1]
    owner = child.parent
    files = [child / "api.py", *child.glob("app/*.py"), *child.glob("contracts/*.schema.json"),
             *owner.glob("app/*.py"), *owner.glob("contracts/*.schema.json")]
    return canonical_hash({path.relative_to(owner).as_posix(): file_hash(path) for path in sorted(files)})
