"""Public accepted-source API consumed by Canonical Compilation."""

from .app.contracts import IngestionError, file_hash, read_json, validate_document, write_json
from .app.profiles import build_plan, validate_profile
from .app.run_identity import default_output_root, profile_path, run_contract


__all__ = [
    "IngestionError",
    "build_plan",
    "default_output_root",
    "file_hash",
    "profile_path",
    "read_json",
    "run_contract",
    "validate_document",
    "validate_profile",
    "write_json",
]
