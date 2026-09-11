"""Scene-truth census of a realized World Partition territory.

Reads the CSV emitted by the editor console command:

    wp.Editor.DumpActorDescs <path>

World Partition actor descriptors are readable WITHOUT loading the actors, so a
full 210-cell territory is censused in milliseconds, with no rendering and no
editor build. This answers "is every layer actually present, registered, and
spatially placed".

SCOPE / KNOWN BLIND SPOT
------------------------
This proves PRESENCE and PLACEMENT only. It cannot prove the surface is
visually plausible, and it must never be reported as visual approval. A
territory can pass every check in this file and still render as an unusable
surface. See the component README for the terracing incident behind this note.
"""
from __future__ import annotations

import json
import math
import re
import sys

# Bounds are parsed SEPARATELY and are optional. Always-loaded actors such as
# DirectionalLight legitimately carry IsValid=false and emit no Min/Max pair.
# Requiring bounds inside this pattern silently drops those rows, which made an
# earlier revision of this file report the lighting set as missing.
DESC = re.compile(
    r"NativeClass:(?P<cls>\S+).*?"
    r"(?<= )Name:(?P<name>\S+).*?"
    r"SpatiallyLoaded:(?P<spatial>\w+)")

BOUNDS = re.compile(
    r"RuntimeBounds:IsValid=true, "
    r"Min=\(X=(?P<x0>[-\d.]+) Y=(?P<y0>[-\d.]+) Z=(?P<z0>[-\d.]+)\), "
    r"Max=\(X=(?P<x1>[-\d.]+) Y=(?P<y1>[-\d.]+) Z=(?P<z1>[-\d.]+)\)")

FOV_DEG = 90.0
CAPTURE_WIDTH = 1920
CAPTURE_HEIGHT = 1080


def required_overview_altitude(span_world_x, span_world_y):
    """Altitude at which BOTH world spans fit the captured frame.

    FOV_DEG is HORIZONTAL. On a 16:9 capture the vertical field is materially
    narrower, so solving only the horizontal axis under-reports the required
    altitude and silently crops the taller world dimension. Must stay in
    agreement with plan_vantages.altitude_for; two tools reporting different
    required altitudes is itself a defect.
    """
    half_h = math.radians(FOV_DEG / 2.0)
    aspect = CAPTURE_WIDTH / CAPTURE_HEIGHT
    half_v = math.atan(math.tan(half_h) / aspect)
    return max(span_world_y / 2.0 / math.tan(half_h),
               span_world_x / 2.0 / math.tan(half_v))


EXPECTATIONS = dict(
    landscape_proxies=210,
    water_actors=145,
    min_relief_m=50.0,
    max_buried_water=8,
)


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
            rows.append(row)
    return rows


def census(rows, expect):
    terrain = [r for r in rows
               if "LandscapeStreamingProxy" in r["cls"] and r["has_bounds"]]
    water = [r for r in rows if "Water" in r["name"] and r["has_bounds"]]
    logical = [r for r in rows if r["cls"].endswith("Landscape.Landscape")]

    findings = []

    def check(ok, label, detail):
        findings.append(dict(ok=bool(ok), check=label, detail=detail))

    check(len(terrain) == expect["landscape_proxies"], "landscape_proxy_count",
          f"{len(terrain)} (expected {expect['landscape_proxies']})")
    check(len(water) == expect["water_actors"], "water_actor_count",
          f"{len(water)} (expected {expect['water_actors']})")
    check(len(logical) == 1, "single_logical_landscape", str(len(logical)))

    # A generated layer actor that is not spatially loaded can never be streamed
    # by World Partition at runtime, however correct its geometry is.
    not_spatial = [r["name"] for r in terrain + water if r["spatial"] != "true"]
    check(not not_spatial, "generated_actors_spatially_loaded",
          f"{len(not_spatial)} not spatial")

    # Without this set the territory renders black, which silently degrades
    # every downstream screenshot check into a useless image.
    required = ["DirectionalLight", "SkyLight", "SkyAtmosphere"]
    present = {r["cls"].rsplit(".", 1)[-1] for r in rows}
    missing = [c for c in required if c not in present]
    check(not missing, "lighting_present",
          f"missing={missing}" if missing else "ok")

    if not terrain:
        return findings, {}

    x0 = min(r["x0"] for r in terrain)
    x1 = max(r["x1"] for r in terrain)
    y0 = min(r["y0"] for r in terrain)
    y1 = max(r["y1"] for r in terrain)
    z0 = min(r["z0"] for r in terrain)
    z1 = max(r["z1"] for r in terrain)
    span = max(x1 - x0, y1 - y0)
    relief = (z1 - z0) / 100.0

    check(relief >= expect["min_relief_m"], "terrain_has_relief",
          f"{relief:.2f} m (min {expect['min_relief_m']})")

    # Water below the LOWEST terrain sample of its own cell can never be seen.
    # This is a cheap proxy-level screen, not a per-sample submersion test.
    buried = 0
    for w in water:
        wx = (w["x0"] + w["x1"]) / 2.0
        wy = (w["y0"] + w["y1"]) / 2.0
        for t in terrain:
            if t["x0"] <= wx <= t["x1"] and t["y0"] <= wy <= t["y1"]:
                if w["z0"] < t["z0"]:
                    buried += 1
                break
    check(buried <= expect["max_buried_water"], "water_not_buried",
          f"{buried} below host cell minimum (max {expect['max_buried_water']})")

    geometry = dict(
        min=[x0, y0, z0], max=[x1, y1, z1],
        center=[(x0 + x1) / 2.0, (y0 + y1) / 2.0],
        span_uu=span, span_m=span / 100.0, relief_m=relief,
        required_overview_altitude_uu=required_overview_altitude(x1 - x0, y1 - y0),
        buried_water_actors=buried)
    return findings, geometry


def main(desc_path, out_path):
    rows = parse(desc_path)
    findings, geometry = census(rows, EXPECTATIONS)
    document = dict(total_descs=len(rows), expectations=EXPECTATIONS,
                    geometry=geometry, findings=findings,
                    passed=all(f["ok"] for f in findings))
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2)
    for finding in findings:
        print(f"  [{'PASS' if finding['ok'] else 'FAIL'}] "
              f"{finding['check']:34s} {finding['detail']}")
    if geometry:
        print(f"\n  territory {geometry['span_m']:.0f} m span, "
              f"relief {geometry['relief_m']:.1f} m")
        print(f"  overview altitude required: "
              f"{geometry['required_overview_altitude_uu'] / 100:.0f} m")
    print(f"\n  CENSUS {'PASSED' if document['passed'] else 'FAILED'} "
          f"-> {out_path}")
    return 0 if document["passed"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
