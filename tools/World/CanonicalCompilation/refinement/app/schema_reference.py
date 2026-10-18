"""Closed schema-annotation repair of an already authenticated selected authority."""
import hashlib
import json
import os
from pathlib import Path

from . import activation
from ...app import promotion
from ...app.contracts import file_hash


def canonical_schema_reference(active):
    schema = Path(__file__).resolve().parents[2] / 'contracts/canonical-authority.schema.json'
    return Path(os.path.relpath(schema, active.parent)).as_posix()


def prepare_schema_reference_transition(repo, profile, expected_active_sha256):
    repo, profile = repo.resolve(), profile.resolve()
    with activation._locked(repo, profile):
        active = activation._active_path(repo, profile)
        old_bytes = active.read_bytes()
        if file_hash(active) != expected_active_sha256:
            activation._refuse('Schema repair requires the exact accepted prior pointer')
        old = promotion._validate_authority_document(repo, activation._pointer(old_bytes), active, profile)
        selected = old.get('selected_refinement')
        if (not selected or old.get('$schema') !=
                'https://alis.world/schemas/world-compiler/canonical-authority-v1.json'):
            activation._refuse('Schema repair only applies to the demonstrated selected-authority URL annotation')
        new = {**old, '$schema': canonical_schema_reference(active)}
        promotion._validate_authority_document(repo, new, active, profile)
        new_bytes = (json.dumps(new, indent=2, ensure_ascii=True)+'\n').encode('utf-8')
        if active.read_bytes() != old_bytes:
            activation._refuse('Canonical selection changed during schema repair preparation')
        return activation._transition(repo, profile, active, old_bytes, new_bytes, old, selected,
                                      kind='schema_reference_v1')


def validate_schema_reference_handoff(repo, original, repair):
    """Keep original producing bytes distinct from their exact metadata successor."""
    repo = repo.resolve()
    profile = repo / original['profile_path']
    with activation._locked(repo, profile):
        old_active, original_bytes, _ = activation._validated_documents(repo, original)
        active, repair_bytes, _, state = activation._validated(repo, repair)
        if (repair.get('transition_kind') != 'schema_reference_v1' or original.get('transition_kind') is not None or
                active != old_active or state != 'selected' or original_bytes['new'] != repair_bytes['old']):
            activation._refuse('Schema successor does not bind the exact original selected publication')
        return {'status':'accepted', 'state':'selected', 'active_sha256':file_hash(active),
                'historical_selected_sha256':hashlib.sha256(original_bytes['new']).hexdigest()}
