#!/usr/bin/env python3
"""
UI Layout Report Tool - Automated widget tree analysis for agents.


Parses JSON dumps from UE's DumpTree command and validates layout invariants.
Designed for CI/agent consumption - returns structured output and exit codes.

Usage:
    python layout_report.py Saved/Dumps/Inventory.json
    python layout_report.py --format=json Saved/Dumps/Inventory.json
    python layout_report.py --severity=high Saved/Dumps/Inventory.json

Python Path:
    System Python (if in PATH): python, python3, py
    UE bundled Python: %UE_PATH%/Engine/Binaries/ThirdParty/Python3/Win64/python.exe

Exit codes:
    0 - No issues found
    1 - Issues found (check output)
    2 - File not found or parse error
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class Issue:
    """Detected layout issue."""
    tag: str
    severity: str  # high, medium, low
    widget_name: str
    widget_class: str
    slot_type: str
    slot_string: str
    message: str = ""


@dataclass
class WidgetInfo:
    """Parsed widget data."""
    name: str
    cls: str
    visible: bool
    hit_testable: bool
    desired_width: float
    desired_height: float
    arranged_width: float
    arranged_height: float
    slot_type: str
    slot_string: str
    position_x: float = 0
    position_y: float = 0
    parent_name: str = ""
    raw_issues: list = field(default_factory=list)
    anchor_key: str = ""  # Anchor string for position comparison


def walk_widgets(node: dict, out: list[tuple[dict, str]], children_count: dict[str, int], parent: str = "") -> None:
    """Recursively collect all widgets from tree with their parent name and children count."""
    out.append((node, parent))
    node_name = node.get("name", "?")
    children = node.get("children", [])
    children_count[node_name] = len(children)
    for child in children:
        walk_widgets(child, out, children_count, node_name)


def parse_widget(w: dict, parent_name: str = "") -> WidgetInfo:
    """Extract widget info from JSON node."""
    desired = w.get("desiredSize", {})
    arranged_w = w.get("width", w.get("arrangedSize", {}).get("width", 0))
    arranged_h = w.get("height", w.get("arrangedSize", {}).get("height", 0))
    pos = w.get("position", {})

    # Extract anchor key for position comparison (anchors dict or from slotString)
    anchors = w.get("anchors", {})
    if anchors:
        # Anchors use minX, minY, maxX, maxY keys
        anchor_key = f"{anchors.get('minX', 0)},{anchors.get('minY', 0)}-{anchors.get('maxX', 0)},{anchors.get('maxY', 0)}"
    else:
        # Try to extract from slotString (e.g., "Anch=0.5,0.5-0.5,0.5")
        slot_str = w.get("slotString", "")
        match = re.search(r"Anch=([0-9.,-]+)", slot_str)
        anchor_key = match.group(1) if match else ""

    return WidgetInfo(
        name=w.get("name", "?"),
        cls=w.get("class", "?"),
        visible=w.get("visible", False),
        hit_testable=w.get("hitTestable", False),
        desired_width=desired.get("width", 0),
        desired_height=desired.get("height", 0),
        arranged_width=arranged_w,
        arranged_height=arranged_h,
        slot_type=w.get("slotType", "?"),
        slot_string=w.get("slotString", ""),
        position_x=pos.get("x", 0),
        position_y=pos.get("y", 0),
        parent_name=parent_name,
        raw_issues=w.get("issues", []),
        anchor_key=anchor_key,
    )


def check_invariants(widgets: list[WidgetInfo], widget_children: dict[str, int]) -> list[Issue]:
    """Validate layout invariants from agent_ue_inspection.md."""
    issues: list[Issue] = []

    # Track positions for duplicate detection (keyed by actual parent name)
    positions_by_parent: dict[str, list[tuple[float, float, str]]] = {}

    # SEMANTIC RULE: Grid hosts that MUST have children when visible
    # These are containers that should always have grid cells inside
    required_content_widgets = {
        "GridHostPrimary",
        "GridHostSecondary",
        "NearbyGridHost",
        "LeftHandGridHost",
        "RightHandGridHost",
    }

    def is_required_host(name: str) -> bool:
        """Check if widget name matches a required host (exact or suffixed like 'GridHostPrimary_123')."""
        return any(name == base or name.startswith(base + "_") for base in required_content_widgets)

    for w in widgets:
        # ZERO_SIZE: visible widget with 0 in either dimension but positive desired
        if (w.visible and
            (w.arranged_width == 0 or w.arranged_height == 0) and
            (w.desired_width > 0 or w.desired_height > 0)):
            issues.append(Issue(
                tag="ZERO_SIZE",
                severity="high",
                widget_name=w.name,
                widget_class=w.cls,
                slot_type=w.slot_type,
                slot_string=w.slot_string,
                message=f"Desired={w.desired_width}x{w.desired_height}, Arranged={w.arranged_width}x{w.arranged_height}",
            ))

        # HIT_TEST_INVISIBLE: can receive input but not visible
        if w.hit_testable and not w.visible:
            issues.append(Issue(
                tag="HIT_TEST_INVISIBLE",
                severity="high",
                widget_name=w.name,
                widget_class=w.cls,
                slot_type=w.slot_type,
                slot_string=w.slot_string,
                message="Widget can receive input but is not visible",
            ))

        # NO_VIEWMODEL: trust the dump's issues array directly
        if "NO_VIEWMODEL" in w.raw_issues:
            issues.append(Issue(
                tag="NO_VIEWMODEL",
                severity="medium",
                widget_name=w.name,
                widget_class=w.cls,
                slot_type=w.slot_type,
                slot_string=w.slot_string,
                message="Widget missing ViewModel binding",
            ))

        # Track Canvas positions for SAME_POSITION check (use actual parent from tree)
        # Include anchor_key since same position with different anchors = different locations
        if w.slot_type == "Canvas" and w.visible:
            parent_key = w.parent_name if w.parent_name else "root"
            if parent_key not in positions_by_parent:
                positions_by_parent[parent_key] = []
            positions_by_parent[parent_key].append((w.position_x, w.position_y, w.name, w.anchor_key))

        # EMPTY_CONTAINER: Required grid hosts with no children
        if is_required_host(w.name) and w.visible:
            child_count = widget_children.get(w.name, 0)
            if child_count == 0:
                issues.append(Issue(
                    tag="EMPTY_CONTAINER",
                    severity="high",
                    widget_name=w.name,
                    widget_class=w.cls,
                    slot_type=w.slot_type,
                    slot_string=w.slot_string,
                    message=f"Grid host has no children (expected grid cells)",
                ))

        # Include pre-detected issues from dump
        # Skip: NO_VIEWMODEL (handled above), HIDDEN (intentional collapse is not an issue)
        for raw_issue in w.raw_issues:
            if raw_issue not in ("NO_VIEWMODEL", "HIDDEN"):
                # Severity mapping for Phase 4 validation rules
                high_issues = ["ZERO_SIZE", "HIT_TEST_INVISIBLE", "CANVAS_NO_SIZE", "EMPTY_CONTAINER"]
                medium_issues = ["FILL_WITH_OFFSET", "SAME_POSITION"]
                if raw_issue in high_issues:
                    severity = "high"
                elif raw_issue in medium_issues:
                    severity = "medium"
                else:
                    severity = "low"
                issues.append(Issue(
                    tag=str(raw_issue),
                    severity=severity,
                    widget_name=w.name,
                    widget_class=w.cls,
                    slot_type=w.slot_type,
                    slot_string=w.slot_string,
                    message="Pre-detected issue from dump",
                ))

    # SAME_POSITION: multiple siblings at same Canvas offset AND same anchor
    # Different anchors with same offset = different screen positions (not a collision)
    for parent, positions in positions_by_parent.items():
        seen: dict[tuple[float, float, str], str] = {}
        for x, y, name, anchor in positions:
            key = (x, y, anchor)  # Include anchor in key - different anchors = different positions
            if key in seen:
                issues.append(Issue(
                    tag="SAME_POSITION",
                    severity="medium",
                    widget_name=name,
                    widget_class="?",
                    slot_type="Canvas",
                    slot_string=f"Offset=({x},{y}) Anchor={anchor}",
                    message=f"Same position as {seen[key]}",
                ))
            else:
                seen[key] = name

    return issues


def format_text(det: dict, widgets: list[WidgetInfo], issues: list[Issue], max_issues: int) -> str:
    """Format output as human-readable text."""
    lines = [
        f"Viewport: {det.get('viewportWidth', '?')}x{det.get('viewportHeight', '?')} dpi={det.get('dpiScale', '?')}",
        f"Widgets: {len(widgets)}  Issues: {len(issues)}",
    ]

    if issues:
        for issue in issues[:max_issues]:
            lines.append(f"- {issue.tag}: {issue.widget_name} ({issue.widget_class}) "
                        f"slot={issue.slot_type}  {issue.slot_string}")
            if issue.message:
                lines.append(f"    {issue.message}")

        if len(issues) > max_issues:
            lines.append(f"... and {len(issues) - max_issues} more issues")
    else:
        lines.append("No issues detected.")

    return "\n".join(lines)


def format_json(det: dict, widgets: list[WidgetInfo], issues: list[Issue]) -> str:
    """Format output as JSON for programmatic consumption."""
    return json.dumps({
        "determinism": det,
        "widget_count": len(widgets),
        "issue_count": len(issues),
        "issues": [
            {
                "tag": i.tag,
                "severity": i.severity,
                "widget_name": i.widget_name,
                "widget_class": i.widget_class,
                "slot_type": i.slot_type,
                "slot_string": i.slot_string,
                "message": i.message,
            }
            for i in issues
        ],
    }, indent=2)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze widget tree JSON dump for layout issues.",
        epilog="See docs/testing/agent_ue_inspection.md for invariant definitions.",
    )
    parser.add_argument("file", nargs="?", default="Saved/Dumps/Inventory.json",
                       help="Path to JSON dump file (default: Saved/Dumps/Inventory.json)")
    parser.add_argument("--format", choices=["text", "json"], default="text",
                       help="Output format (default: text)")
    parser.add_argument("--severity", choices=["all", "high", "medium", "low"], default="all",
                       help="Filter issues by minimum severity (default: all)")
    parser.add_argument("--max-issues", type=int, default=40,
                       help="Maximum issues to display in text mode (default: 40)")
    args = parser.parse_args()

    path = Path(args.file)
    if not path.exists():
        print(f"ERROR: File not found: {path}", file=sys.stderr)
        return 2

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON: {e}", file=sys.stderr)
        return 2

    # Collect all widgets with their parent names and children counts
    raw_widgets: list[tuple[dict, str]] = []
    widget_children: dict[str, int] = {}
    for root in data.get("widgets", []):
        walk_widgets(root, raw_widgets, widget_children)

    widgets = [parse_widget(w, parent) for w, parent in raw_widgets]
    issues = check_invariants(widgets, widget_children)

    # Filter by severity
    severity_order = {"high": 0, "medium": 1, "low": 2}
    if args.severity != "all":
        min_severity = severity_order.get(args.severity, 2)
        issues = [i for i in issues if severity_order.get(i.severity, 2) <= min_severity]

    # Sort by severity
    issues.sort(key=lambda i: severity_order.get(i.severity, 2))

    det = data.get("determinism", {})

    if args.format == "json":
        print(format_json(det, widgets, issues))
    else:
        print(format_text(det, widgets, issues, args.max_issues))

    return 1 if issues else 0


if __name__ == "__main__":
    sys.exit(main())
