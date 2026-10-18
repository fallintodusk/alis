from __future__ import annotations

from pathlib import Path
from typing import Any

from .contracts import ValidationFailure, file_hash, read_json


def validate_source_payload(
    result_path: Path,
    result: dict[str, Any],
    source_profile_path: Path,
    budget_bytes: int,
) -> int:
    profile = read_json(source_profile_path)
    outputs = [entry for entry in result.get("outputs", [])
               if entry.get("kind") == "source_ledger"]
    if len(outputs) != 1 or result.get("operation") != "run":
        raise ValidationFailure("source_payload_ledger_missing",
                                "Full replay requires one source-run ledger")
    entry = outputs[0]
    relative = entry.get("path")
    root = result_path.parent.resolve()
    ledger_path = (root / relative).resolve() if isinstance(relative, str) else root
    if (not ledger_path.is_relative_to(root) or not ledger_path.is_file()
            or ledger_path.stat().st_size != entry.get("byte_size")
            or file_hash(ledger_path) != entry.get("sha256")):
        raise ValidationFailure("source_payload_ledger_changed",
                                "Source payload ledger is missing, outside its run, or changed")
    ledger = read_json(ledger_path)
    declared = profile.get("sources", [])
    snapshots = ledger.get("snapshots", [])
    if (not isinstance(declared, list) or not declared or not isinstance(snapshots, list)
            or not all(isinstance(item, dict) for item in declared + snapshots)):
        raise ValidationFailure("source_payload_identity_mismatch",
                                "Consumed source inventory is incomplete")
    sources = {item.get("source_id"): item for item in declared}
    consumed = {item.get("source_id"): item for item in snapshots}
    if (len(sources) != len(declared) or len(consumed) != len(snapshots)
            or set(sources) != set(consumed)
            or result.get("profile_id") != profile.get("profile_id")
            or ledger.get("profile_id") != profile.get("profile_id")):
        raise ValidationFailure("source_payload_identity_mismatch",
                                "Consumed sources do not identify the selected source profile")
    payloads: dict[str, int] = {}
    for source_id, source in sources.items():
        snapshot = consumed[source_id]
        size = snapshot.get("byte_size")
        hashes = snapshot.get("hashes", {})
        expected_hashes = source.get("hashes", {})
        digest = expected_hashes.get("sha256")
        if (type(size) is not int or size < 0 or size != source.get("expected_bytes")
                or not isinstance(hashes, dict) or not isinstance(digest, str)
                or hashes.get("sha256") != digest
                or any(hashes.get(key) != value for key, value in expected_hashes.items()
                       if key in {"sha256", "md5"})
                or (digest in payloads and payloads[digest] != size)):
            raise ValidationFailure("source_payload_identity_mismatch",
                                    "Consumed source size or digest differs from its pin",
                                    source_id=source_id)
        payloads[digest] = size
    # Cache residency is independent of the immutable payload consumed by this profile.
    payload_bytes = sum(payloads.values())
    if payload_bytes > budget_bytes:
        raise ValidationFailure("provider_budget_exceeded",
                                "Consumed provider payload exceeded its frozen per-profile budget",
                                profile_id=profile["profile_id"], payload_bytes=payload_bytes,
                                budget_bytes=budget_bytes)
    return payload_bytes
