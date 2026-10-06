"""Regressions for finding generated layers in a descriptor dump.

The census and the vantage planner read `wp.Editor.DumpActorDescs` lines. Terrain and water are
found by the producer-neutral runtime roles their actors carry, never by class or actor name, so
a terrain representation change cannot silently empty either tool. The lines below follow the
engine's Verbose descriptor format, which includes actor tags.
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "app"))

import census
import plan_vantages

TERRAIN_ROLE = "ProjectWorld.Terrain.v1"
WATER_ROLE = "ProjectWorld.Water.v1"


def descriptor(index, cls, name, bounds=None, tags=(), spatial="true"):
    text = (f"Guid:{index:032X} NativeClass:{cls} Name:{name} Label:{name} "
            f"SpatiallyLoaded:{spatial} ")
    if bounds is None:
        text += "RuntimeBounds:IsValid=false "
    else:
        (x0, y0, z0), (x1, y1, z1) = bounds
        text += (f"RuntimeBounds:IsValid=true, Min=(X={x0:.3f} Y={y0:.3f} Z={z0:.3f}), "
                 f"Max=(X={x1:.3f} Y={y1:.3f} Z={z1:.3f}) ")
    text += "RuntimeGrid:None EditorOnly:false RuntimeOnly:false HLODRelevant:false"
    if tags:
        text += " Tags:" + ",".join(tags)
    return text + " DataLayerNames:\n"


def lighting(start):
    return [descriptor(start + offset, f"/Script/Engine.{cls}", f"{cls}_0", spatial="false")
            for offset, cls in enumerate(("DirectionalLight", "SkyLight", "SkyAtmosphere"))]


def mesh_terrain_territory():
    """Four tagged compiled sections, one tagged water body, and untagged decoys."""
    lines = []
    for index, (x, y) in enumerate(((0, 0), (50000, 0), (0, 50000), (50000, 50000))):
        lines.append(descriptor(
            index, "/Script/MeshPartition.CompiledSection", f"CompiledSection_{index}",
            bounds=((x, y, -1000.0), (x + 50000, y + 50000, 9000.0)),
            tags=(TERRAIN_ROLE, "ProjectWorld.MeshTerrain.CompiledSection")))
    # Water named without "Water" and lying below every terrain sample: found by its role and
    # not judged by its height, which canonical compilation owns.
    lines.append(descriptor(
        10, "/Script/Engine.StaticMeshActor", "River_01",
        bounds=((20000, 20000, -3000.0), (30000, 30000, -2500.0)), tags=(WATER_ROLE,)))
    # Decoys: an untagged actor far outside the terrain and an untagged actor named like water.
    lines.append(descriptor(
        11, "/Script/Engine.StaticMeshActor", "Tower_01",
        bounds=((400000, 400000, 0.0), (401000, 401000, 90000.0))))
    lines.append(descriptor(
        12, "/Script/Engine.StaticMeshActor", "WaterTower_01",
        bounds=((10000, 10000, 0.0), (11000, 11000, 3000.0))))
    return lines + lighting(20)


def landscape_era_territory():
    """Untagged Landscape proxies: no actor carries the terrain role."""
    lines = [descriptor(
        index, "/Script/Landscape.LandscapeStreamingProxy", f"LandscapeStreamingProxy_{index}",
        bounds=((index * 1000, 0, -500.0), (index * 1000 + 1000, 1000, 8000.0)))
        for index in range(3)]
    lines.append(descriptor(9, "/Script/Landscape.Landscape", "Landscape_0", spatial="false"))
    return lines + lighting(20)


class DescriptorRoleTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write_dump(self, lines):
        path = os.path.join(self.directory.name, "actor_descs.csv")
        with open(path, "w", encoding="utf-8") as handle:
            handle.writelines(lines)
        return path

    def plan(self, lines):
        out = os.path.join(self.directory.name, "vantages.json")
        status = plan_vantages.main(self.write_dump(lines), out)
        document = None
        if os.path.exists(out):
            with open(out, encoding="utf-8") as handle:
                document = json.load(handle)
        return status, document

    def test_planner_frames_terrain_found_by_role(self):
        status, document = self.plan(mesh_terrain_territory())
        self.assertEqual(status, 0)
        self.assertEqual(document["territory"]["min"], [0.0, 0.0, -1000.0])
        self.assertEqual(document["territory"]["max"], [100000.0, 100000.0, 9000.0])
        self.assertEqual(document["counts"]["terrain_descriptors"], 4)
        self.assertEqual(document["counts"]["water_actors"], 1)

    def test_planner_refuses_a_dump_without_the_terrain_role(self):
        status, document = self.plan(landscape_era_territory())
        self.assertNotEqual(status, 0)
        self.assertIsNone(document)

    def test_census_passes_a_mesh_terrain_territory(self):
        rows = census.parse(self.write_dump(mesh_terrain_territory()))
        findings, geometry = census.census(rows, census.EXPECTATIONS)
        failed = [f["check"] for f in findings if not f["ok"]]
        self.assertEqual(failed, [])
        checks = {f["check"] for f in findings}
        self.assertTrue({"terrain_present", "water_present"} <= checks)
        self.assertFalse([c for c in checks if "landscape" in c or "buried" in c])
        self.assertEqual(geometry["relief_m"], 100.0)

    def test_census_fails_when_no_actor_carries_the_terrain_role(self):
        rows = census.parse(self.write_dump(landscape_era_territory()))
        findings, _ = census.census(rows, census.EXPECTATIONS)
        by_check = {f["check"]: f["ok"] for f in findings}
        self.assertIn("terrain_present", by_check)
        self.assertFalse(by_check["terrain_present"])


if __name__ == "__main__":
    unittest.main()
