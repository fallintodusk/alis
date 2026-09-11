"""Regressions for the canonical surface-quality gate.

The original gate reported terrace_ratio 0.498 / level_utilisation 0.111 for the
Kazan territory. Both figures were artifacts of the gate, not the terrain:

  - the default sample took the FIRST 40 filenames, which is three of fifteen
    territory columns - a contiguous strip of the flattest, water-dominated
    ground;
  - level_utilisation was compared against an absolute 0.20, but it is bounded
    above by (relief / quantization + 1) / samples, so 94 of 210 legitimate
    cells could never reach it at any data quality.

Measured over all 210 cells the territory scores terrace_ratio 0.269 (passing),
and raw-source analysis proved the terracing in the failing cells is inherited
from Copernicus GLO-30 rather than produced by the 0.1 m lattice.
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "app"))

import surface


def write_cell(directory, name, rows, quantization=0.1):
    path = os.path.join(directory, name)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump({"core_samples": rows, "height_quantization": quantization}, handle)
    return path


def ramp(width, height, step, base=50.0):
    """A genuine surface whose samples advance one lattice step at a time."""
    return [[round(base + (row * width + column) * step, 6) for column in range(width)]
            for row in range(height)]


def lattice(rows, step):
    """Re-emit a surface on a coarser lattice than the document declares."""
    return [[round(round(value / step) * step, 6) for value in row] for row in rows]


class SurfaceGateTests(unittest.TestCase):
    def measure(self, directory, limit=None):
        out = os.path.join(directory, "receipt.json")
        code = surface.main(directory, out, limit) if limit is not None else surface.main(directory, out)
        with open(out, encoding="utf-8") as handle:
            return code, json.load(handle)

    def test_low_relief_cell_is_not_rejected_for_being_low_relief(self):
        # Floodplain: only 0.4 m of relief, but every adjacent sample really
        # varies and the cell populates every level its range supports. This is
        # good data that an absolute distinct-levels-per-sample threshold
        # rejects purely for being flat - 0.005 against a required 0.20.
        with tempfile.TemporaryDirectory() as directory:
            for index in range(4):
                rows = [[round(50.0 + ((row + column + index) % 5) * 0.1, 6)
                         for column in range(32)] for row in range(32)]
                write_cell(directory, "cell_x0_y%d.json" % index, rows)
            code, document = self.measure(directory)
            self.assertLess(document["mean_relief_m"], 1.0)
            self.assertLess(document["mean_level_utilisation"], surface.MIN_SUPPORTED_LEVEL_RATIO)
            self.assertTrue(document["passed"], document["findings"])
            self.assertEqual(code, 0)

    def test_surface_delivered_coarser_than_declared_is_rejected(self):
        # The defect this dimension exists to catch: the document claims a
        # 0.1 m lattice while the samples only ever land on 1.0 m.
        with tempfile.TemporaryDirectory() as directory:
            rows = ramp(32, 32, 0.1)
            write_cell(directory, "cell_x0_y0.json", lattice(rows, 1.0), quantization=0.1)
            code, document = self.measure(directory)
            self.assertFalse(document["passed"])
            self.assertEqual(code, 1)
            failed = [f["check"] for f in document["findings"] if not f["ok"]]
            self.assertIn("supported_level_ratio", failed)

    def test_terrace_ratio_remains_an_independent_signal(self):
        # Wide plateaus separated by cliffs keep full relief and can populate
        # their supported levels, so only the direct terracing signal sees it.
        with tempfile.TemporaryDirectory() as directory:
            rows = [[round(50.0 + (row // 8) * 4.0, 6) for _ in range(32)] for row in range(32)]
            write_cell(directory, "cell_x0_y0.json", rows)
            code, document = self.measure(directory)
            self.assertFalse(document["passed"])
            failed = [f["check"] for f in document["findings"] if not f["ok"]]
            self.assertIn("terrace_ratio", failed)

    def test_default_measurement_covers_every_cell(self):
        # The first filenames must not decide the territory verdict.
        with tempfile.TemporaryDirectory() as directory:
            for index in range(3):
                write_cell(directory, "cell_x-1_y%d.json" % index,
                           lattice(ramp(32, 32, 0.1), 1.0))
            for index in range(45):
                write_cell(directory, "cell_x2_y%d.json" % index, ramp(32, 32, 0.1))
            code, document = self.measure(directory)
            self.assertEqual(document["cells_measured"], 48)
            self.assertEqual(document["sampling"], "complete")

    def test_explicit_subsample_is_strided_and_declared(self):
        # A caller may still subsample, but it must be spread across the
        # territory and the receipt must say so.
        with tempfile.TemporaryDirectory() as directory:
            for index in range(3):
                write_cell(directory, "cell_x-1_y%d.json" % index,
                           lattice(ramp(32, 32, 0.1), 1.0))
            for index in range(45):
                write_cell(directory, "cell_x2_y%d.json" % index, ramp(32, 32, 0.1))
            code, document = self.measure(directory, limit=8)
            self.assertEqual(document["cells_measured"], 8)
            self.assertEqual(document["sampling"], "strided_subsample")
            self.assertTrue(document["passed"], document["findings"])


if __name__ == "__main__":
    unittest.main()
