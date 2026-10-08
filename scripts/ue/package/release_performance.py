"""Explicit release acceptance of an inconclusive absolute performance result."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path


WAIVED_STATUS = "accepted_with_performance_waiver"
WAIVER_REASON = (
    "Absolute performance certification inconclusive on an unqualified busy host; "
    "operator accepts residual risk. This waiver certifies neither absolute performance "
    "nor historical runtime-byte equivalence."
)


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("Performance evidence must be a JSON object")
    return value


def _measurement_decision(aggregate: dict, allow_inconclusive: bool) -> dict:
    for name in ("base_frame_p95_budget_ms", "frame_p95_budget_ms", "frame_p95_ms"):
        value = aggregate.get(name)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f"Performance evidence has invalid {name}")
    base, budget, measured = (aggregate[name] for name in (
        "base_frame_p95_budget_ms", "frame_p95_budget_ms", "frame_p95_ms"))
    if base != 16.67 or not base <= budget <= round(base * 1.1, 3) or measured <= 0:
        raise ValueError("Performance evidence has an invalid measurement budget")
    for name, length in (("source_revision", 40), ("source_state_sha256", 64),
                         ("runtime_profile_sha256", 64), ("development_executable_sha256", 64),
                         ("development_package_sha256", 64)):
        if not re.fullmatch(r"[a-f0-9]{" + str(length) + "}", str(aggregate.get(name, ""))):
            raise ValueError(f"Performance evidence has invalid {name}")
    rejected = measured > budget
    if aggregate.get("status") != ("rejected" if rejected else "accepted"):
        raise ValueError("Performance status disagrees with the retained measurement")
    if rejected and (not allow_inconclusive or not aggregate.get("acceptance_reason")):
        raise ValueError("Performance rejection requires explicit inconclusive release acceptance")
    return {
        "status": "inconclusive" if rejected else "accepted",
        "operator_decision": "accepted_residual_risk" if rejected else "not_required",
        "reason": WAIVER_REASON if rejected else "",
        "measurement_status": aggregate["status"],
        "measurement_reason": aggregate.get("acceptance_reason", ""),
        **{name: aggregate[name] for name in (
            "source_revision", "source_state_sha256", "runtime_profile_sha256",
            "development_executable_sha256", "development_package_sha256",
            "base_frame_p95_budget_ms", "frame_p95_budget_ms", "frame_p95_ms")},
    }


def performance_decision(aggregate: dict, allow_inconclusive: bool = False) -> dict:
    if (aggregate.get("schema_version") != 1 or aggregate.get("execution_count") != 3 or
            len(aggregate.get("children", [])) != 3 or aggregate.get("total_sample_count", 0) < 900 or
            aggregate.get("streaming_failures") != 0):
        raise ValueError("Performance evidence is incomplete or has a non-budget failure")
    return _measurement_decision(aggregate, allow_inconclusive)


def authenticate_artifacts(document: dict, source_root: Path) -> None:
    artifacts, hashes = document.get("artifacts"), document.get("artifact_sha256")
    if not isinstance(artifacts, dict) or not artifacts or not isinstance(hashes, dict) or set(artifacts) != set(hashes):
        raise ValueError("Performance evidence inventory is incomplete")
    for key, recorded in artifacts.items():
        path = Path(recorded)
        if not path.is_absolute():
            path = source_root / path
        if file_hash(path) != hashes[key]:
            raise ValueError(f"Candidate evidence artifact changed: {key}")


def validate_machine_performance(machine: dict, allow_inconclusive: bool = False,
                                 source_root: Path = Path("."), require_recorded: bool = True) -> dict | None:
    status = machine.get("status")
    if require_recorded and status == "accepted" and "performance_review" not in machine:
        return None
    if status not in ("accepted", WAIVED_STATUS) or (status == WAIVED_STATUS and not allow_inconclusive):
        raise ValueError("Player machine acceptance requires explicit performance waiver authorization")
    authenticate_artifacts(machine, source_root)
    decisions = {}
    for city, key in (("kazan", "development_performance_aggregate"),
                      ("manhattan", "manhattan_development_performance")):
        path = Path(machine.get("artifacts", {}).get(key, ""))
        if not path.is_absolute():
            path = source_root / path
        digest = file_hash(path)
        if digest != machine.get("artifact_sha256", {}).get(key):
            raise ValueError(f"Performance artifact changed: {city}")
        document = read_json(path)
        aggregate = document if city == "kazan" else document.get("performance", {})
        decision = performance_decision(aggregate, allow_inconclusive)
        expected = {
            "source_revision": machine.get("revision"),
            "source_state_sha256": machine.get("source_state_sha256"),
            "development_executable_sha256": machine.get("development_executable_sha256"),
            "development_package_sha256": machine.get("development_package_sha256"),
        }
        if city == "kazan":
            expected["runtime_profile_sha256"] = machine.get("runtime_profile_sha256")
        else:
            authenticate_artifacts(document, source_root)
            expected["runtime_profile_sha256"] = document.get("runtime_profile_sha256")
            if document.get("performance_review") != decision:
                raise ValueError("Manhattan performance decision changed")
        if any(aggregate.get(name) != value or value is None for name, value in expected.items()):
            raise ValueError(f"Performance evidence belongs to another candidate: {city}")
        decisions[city] = {**decision, "aggregate_sha256": digest}
    waived = any(value["status"] == "inconclusive" for value in decisions.values())
    if status != (WAIVED_STATUS if waived else "accepted"):
        raise ValueError("Player machine status disagrees with its performance decisions")
    review = {
        "status": "inconclusive" if waived else "accepted",
        "operator_decision": "accepted_residual_risk" if waived else "not_required",
        "source_revision": machine["revision"], "source_state_sha256": machine["source_state_sha256"],
        "shipping_executable_sha256": machine["shipping_executable_sha256"],
        "shipping_package_sha256": machine["shipping_package_sha256"], "cities": decisions,
    }
    if require_recorded and machine.get("performance_review") != review:
        raise ValueError("Player performance review changed")
    return review


def validate_release_performance(manifest: dict) -> None:
    review = manifest.get("performance_review")
    if review is None:
        return
    source = manifest.get("player_source") or manifest.get("player_sources", {}).get("windows-x86_64", {})
    for review_key, source_key in (("source_revision", "revision"),
                                   ("source_state_sha256", "source_state_sha256"),
                                   ("shipping_executable_sha256", "shipping_executable_sha256")):
        if review.get(review_key) != source.get(source_key) or source.get(source_key) is None:
            raise ValueError("Release performance review belongs to another player candidate")
    if "package_tree_sha256" in source and review.get("shipping_package_sha256") != source["package_tree_sha256"]:
        raise ValueError("Release performance review package changed")
    cities = review.get("cities", {})
    if set(cities) != {"kazan", "manhattan"}:
        raise ValueError("Release performance review requires both city decisions")
    for decision in cities.values():
        aggregate = {
            **decision, "status": decision.get("measurement_status"),
            "acceptance_reason": decision.get("measurement_reason"),
        }
        if (_measurement_decision(aggregate, True) !=
                {key: value for key, value in decision.items() if key != "aggregate_sha256"} or
                not re.fullmatch(r"[a-f0-9]{64}", str(decision.get("aggregate_sha256", ""))) or
                decision.get("source_revision") != review["source_revision"] or
                decision.get("source_state_sha256") != review["source_state_sha256"]):
            raise ValueError("Release performance decision changed")
    waived = any(decision["status"] == "inconclusive" for decision in cities.values())
    if (review.get("status") != ("inconclusive" if waived else "accepted") or
            review.get("operator_decision") != ("accepted_residual_risk" if waived else "not_required")):
        raise ValueError("Release performance review must preserve the inconclusive result")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("decide", "compose", "validate"))
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=Path("."))
    parser.add_argument("--accept-inconclusive-performance", action="store_true")
    args = parser.parse_args()
    try:
        document = read_json(args.receipt)
        result = (performance_decision(document, args.accept_inconclusive_performance)
                  if args.mode == "decide" else validate_machine_performance(
                      document, args.accept_inconclusive_performance, args.source_root,
                      require_recorded=args.mode != "compose"))
        print(json.dumps(result))
        return 0
    except (ValueError, OSError, TypeError, KeyError) as error:
        print(f"Release performance acceptance refused: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
