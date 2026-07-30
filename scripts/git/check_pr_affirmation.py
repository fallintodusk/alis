#!/usr/bin/env python3
# License terms: see repository root LICENSE.
"""Verify that a pull request completed the rights affirmation block."""

from __future__ import annotations

import argparse
import os
import re
import sys


START_MARKER = "<!-- rights-affirmation:start -->"
END_MARKER = "<!-- rights-affirmation:end -->"
CHECKBOX_PATTERN = re.compile(
    r"^\s*-\s+\[([ xX])\]\s+(.*?)(?=^\s*-\s+\[|\Z)",
    re.MULTILINE | re.DOTALL,
)
REQUIRED_STATEMENTS = (
    "I have authority to submit every first-party portion under the component "
    "license assigned by the root `LICENSE`.",
    "I identified copied material, generated code, templates, dependencies, "
    "datasets, and assets, including their exact upstream terms.",
    "I retained applicable copyright, attribution, license, and proprietary "
    "notices.",
    "I separated or excluded anything with unknown or incompatible terms.",
)


def normalize_statement(statement: str) -> str:
    return re.sub(r"\s+", " ", statement).strip()


def validate_affirmation(body: str) -> list[str]:
    start = body.find(START_MARKER)
    end = body.find(END_MARKER)
    if start < 0 or end < 0 or end <= start:
        return ["Pull request is missing the rights affirmation block"]

    block = body[start + len(START_MARKER) : end]
    items = [
        (state, normalize_statement(statement))
        for state, statement in CHECKBOX_PATTERN.findall(block)
    ]
    errors: list[str] = []
    if len(items) != len(REQUIRED_STATEMENTS):
        errors.append(
            "Rights affirmation block must contain exactly "
            f"{len(REQUIRED_STATEMENTS)} checkboxes"
        )
    if {statement for _, statement in items} != set(REQUIRED_STATEMENTS):
        errors.append("Rights affirmation must preserve the canonical statements")
    unchecked = sum(state == " " for state, _ in items)
    if unchecked:
        errors.append(f"Rights affirmation has {unchecked} unchecked item(s)")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--body-env",
        default="PR_BODY",
        help="Environment variable containing the pull-request body",
    )
    args = parser.parse_args()
    errors = validate_affirmation(os.environ.get(args.body_env, ""))
    if errors:
        print("[X] Rights affirmation failed")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("[OK] Rights affirmation is complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
