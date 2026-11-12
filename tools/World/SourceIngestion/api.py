"""Public accepted-source API consumed by Canonical Compilation."""

from .app.contracts import IngestionError, file_hash, read_json, validate_document, write_json
from .app.profiles import build_plan, validate_profile
from .app.run_identity import default_output_root, profile_path, run_contract
from .app.osm_context import osm_provider_namespace, osm_context_identity, osm_way_context


def osm_multipolygon_context(document, primitives, provider_feature_id):
    """Load the optional literal relation qualifier only for its actual consumers."""
    from .osm_multipolygon_context import osm_multipolygon_context as qualify
    return qualify(document, primitives, provider_feature_id)


def osm_multipolygon_context_identity():
    from .osm_multipolygon_context import osm_multipolygon_context_identity as identity
    return identity()


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
    "osm_provider_namespace",
    "osm_context_identity",
    "osm_way_context",
    "osm_multipolygon_context",
    "osm_multipolygon_context_identity",
]
