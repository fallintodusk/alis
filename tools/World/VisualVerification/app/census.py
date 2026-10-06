"""Scene-truth census of a realized World Partition territory.

Reads the CSV emitted by the editor console command:

    wp.Editor.DumpActorDescs <path>

World Partition actor descriptors are readable WITHOUT loading the actors, so a
whole territory is censused in milliseconds, with no rendering and no editor
build. This answers "is every layer actually present, registered, and spatially
placed". Terrain and water are found by their runtime roles
(`actor_descriptors`), so the census holds for any terrain representation.

SCOPE / KNOWN BLIND SPOT
------------------------
This proves PRESENCE and PLACEMENT only. It cannot prove the surface is
visually plausible, and it must never be reported as visual approval. A
territory can pass every check in this file and still render as an unusable
surface. See the component README for the terracing incident behind this note.
It does not judge terrain-to-water heights either: canonical compilation owns
that relation and fails closed when it breaks.
"""
from __future__ import annotations

import json
import math
import sys

from actor_descriptors import TERRAIN_ROLE, WATER_ROLE, parse, with_role

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
    min_relief_m=50.0,
)


def census(rows, expect):
    terrain = with_role(rows, TERRAIN_ROLE)
    water = with_role(rows, WATER_ROLE)

    findings = []

    def check(ok, label, detail):
        findings.append(dict(ok=bool(ok), check=label, detail=detail))

    check(terrain, "terrain_present",
          f"{len(terrain)} descriptors tagged {TERRAIN_ROLE}")
    check(water, "water_present",
          f"{len(water)} descriptors tagged {WATER_ROLE}")

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

    geometry = dict(
        min=[x0, y0, z0], max=[x1, y1, z1],
        center=[(x0 + x1) / 2.0, (y0 + y1) / 2.0],
        span_uu=span, span_m=span / 100.0, relief_m=relief,
        required_overview_altitude_uu=required_overview_altitude(x1 - x0, y1 - y0))
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
