# License terms: see repository root LICENSE.
import sys
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

import check_pr_affirmation


def affirmation(states: str, statements: tuple[str, ...] | None = None) -> str:
    statements = statements or check_pr_affirmation.REQUIRED_STATEMENTS
    items = "\n".join(
        f"- [{state}] {statement}"
        for state, statement in zip(states, statements, strict=True)
    )
    return "\n".join(
        (
            check_pr_affirmation.START_MARKER,
            items,
            check_pr_affirmation.END_MARKER,
        )
    )


class RightsAffirmationTests(unittest.TestCase):
    def test_accepts_completed_affirmation(self) -> None:
        self.assertEqual([], check_pr_affirmation.validate_affirmation(affirmation("xXxx")))

    def test_rejects_unchecked_item(self) -> None:
        errors = check_pr_affirmation.validate_affirmation(affirmation("xxx "))

        self.assertTrue(any("unchecked" in error for error in errors))

    def test_rejects_missing_block(self) -> None:
        errors = check_pr_affirmation.validate_affirmation("ordinary body")

        self.assertIn("Pull request is missing the rights affirmation block", errors)

    def test_rejects_reworded_statement(self) -> None:
        statements = list(check_pr_affirmation.REQUIRED_STATEMENTS)
        statements[0] = "I checked something else."

        errors = check_pr_affirmation.validate_affirmation(
            affirmation("xxxx", tuple(statements))
        )

        self.assertTrue(any("canonical statements" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
