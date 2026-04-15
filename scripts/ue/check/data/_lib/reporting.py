"""Error collection and formatted reporting for data cross-reference validation."""

from __future__ import annotations


class ErrorCollector:
    """Accumulates validation errors with file and field-path context."""

    def __init__(self) -> None:
        self._errors: list[str] = []

    def add(self, file_label: str, field_path: str, message: str) -> None:
        self._errors.append(f"{file_label}: {field_path} {message}")

    def count(self) -> int:
        return len(self._errors)

    def print_summary(self, label: str) -> int:
        """Print results and return exit code (0=pass, 1=fail)."""
        if not self._errors:
            print(f"{label} passed.")
            return 0

        print(f"\n{label} FAILED ({len(self._errors)} errors):")
        for error in self._errors:
            print(f"  [X] {error}")
        return 1
