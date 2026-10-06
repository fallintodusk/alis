"""Rows of a World Partition actor descriptor dump.

Reads the file written by the editor console command:

    wp.Editor.DumpActorDescs <path>

The command writes one descriptor per line in the engine's Verbose format, which carries the
actor tags. Generated layers are found by the producer-neutral runtime roles their actors carry,
never by class or actor name, so a representation change cannot silently empty a reader.
"""
from __future__ import annotations

import re

TERRAIN_ROLE = "ProjectWorld.Terrain.v1"
WATER_ROLE = "ProjectWorld.Water.v1"

# Bounds and tags are parsed SEPARATELY and are optional. Always-loaded actors such as
# DirectionalLight legitimately carry IsValid=false and emit no Min/Max pair. Requiring bounds
# inside this pattern silently drops those rows, which once made the census report the lighting
# set as missing.
DESC = re.compile(
    r"NativeClass:(?P<cls>\S+).*?"
    r"(?<= )Name:(?P<name>\S+).*?"
    r"SpatiallyLoaded:(?P<spatial>\w+)")

BOUNDS = re.compile(
    r"RuntimeBounds:IsValid=true, "
    r"Min=\(X=(?P<x0>[-\d.]+) Y=(?P<y0>[-\d.]+) Z=(?P<z0>[-\d.]+)\), "
    r"Max=\(X=(?P<x1>[-\d.]+) Y=(?P<y1>[-\d.]+) Z=(?P<z1>[-\d.]+)\)")

TAGS = re.compile(r"(?<= )Tags:(?P<tags>\S+)")


def parse(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = DESC.search(line)
            if match is None:
                continue
            row = match.groupdict()
            bounds = BOUNDS.search(line)
            row["has_bounds"] = bounds is not None
            for key in ("x0", "y0", "z0", "x1", "y1", "z1"):
                row[key] = float(bounds.group(key)) if bounds else 0.0
            tags = TAGS.search(line)
            row["tags"] = tags.group("tags").split(",") if tags else []
            rows.append(row)
    return rows


def with_role(rows, role):
    """Rows whose actor carries the runtime role and has valid runtime bounds."""
    return [row for row in rows if role in row["tags"] and row["has_bounds"]]
