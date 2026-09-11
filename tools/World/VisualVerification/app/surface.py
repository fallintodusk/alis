"""Surface-quality gate for canonical terrain.

WHY THIS EXISTS
---------------
The realization height check compares realized samples to canonical samples with
a tolerance derived from the canonical profile's own `height_quantization`. That
gate is self-referential: it can only ever prove "the engine faithfully
reproduced canonical". It reported 0/215040 mismatches at 3 mm maximum error on
a territory whose rendered surface was visibly terraced, because the terracing
is IN canonical and sits exactly on the tolerance lattice.

This module asks the question that check structurally cannot: is the canonical
surface a plausible terrain, independent of whether the engine copied it
correctly. It reads canonical cell documents directly and never looks at the
engine, so a green result here plus a green height check together mean "correct
data, correctly realized", which neither proves alone.

MEASURES
--------
lattice_step_m       Detected vertical lattice the samples snap to.
terrace_ratio        Fraction of adjacent sample pairs that are EXACTLY equal.
                     Flat plateaus separated by lattice-sized cliffs produce
                     constant-normal bands with hard discontinuities between
                     them, which is what reads as corduroy striping.
level_utilisation    Distinct height levels / samples. DIAGNOSTIC ONLY. It is
                     bounded above by (relief / quantization + 1) / samples, so
                     for a low-relief cell it is small no matter how good the
                     data is. It must never be compared to an absolute floor.
supported_level_ratio
                     Distinct levels / the levels the cell's own relief and
                     DECLARED quantization can support. This is the gated form:
                     it asks whether the surface delivers the vertical
                     resolution the document claims, which is scale-free. A
                     genuinely flat lake scores 1.0; a surface secretly sitting
                     on a 1.0 m lattice while declaring 0.1 m scores ~0.1.
step_ratio           Fraction of adjacent pairs differing by exactly one
                     lattice step. High values with high terrace_ratio is the
                     signature of quantisation-limited relief.

The two gates are deliberately independent. terrace_ratio catches repeated
samples (including nearest-neighbour upsampling, which leaves the level count
untouched); supported_level_ratio catches undelivered vertical resolution
(which leaves adjacent samples distinct). Neither subsumes the other.
"""
from __future__ import annotations

import glob
import json
import os
import sys
from collections import Counter

# A terrain whose adjacent samples are this often bit-identical is terraced
# rather than sampled; the surface normal is piecewise constant.
MAX_TERRACE_RATIO = 0.45
# Distinct levels as a fraction of the levels the cell's relief and declared
# quantization can support. Measured on the accepted Kazan territory the real
# surface scores 0.738; re-emitting the same data on a 0.5 m lattice while still
# declaring 0.1 m scores 0.266 and on a 1.0 m lattice 0.188. This floor sits in
# that gap with margin on both sides.
MIN_SUPPORTED_LEVEL_RATIO = 0.50


def detect_lattice(values):
    """Smallest positive gap the sorted distinct values consistently snap to."""
    levels = sorted(set(values))
    if len(levels) < 3:
        return 0.0
    gaps = [round(b - a, 6) for a, b in zip(levels, levels[1:])]
    common = Counter(gaps).most_common(1)[0][0]
    return float(common)


def measure_cell(path):
    document = json.load(open(path, encoding="utf-8"))
    rows = document.get("core_samples")
    if not rows or not isinstance(rows, list) or not isinstance(rows[0], list):
        return None
    height = len(rows)
    width = len(rows[0])
    flat = [v for row in rows for v in row]

    equal = 0
    stepped = 0
    total = 0
    lattice = detect_lattice(flat)
    for row in rows:
        for c in range(width - 1):
            delta = abs(row[c + 1] - row[c])
            total += 1
            if delta == 0.0:
                equal += 1
            elif lattice > 0 and abs(delta - lattice) < lattice * 1e-6:
                stepped += 1
    for c in range(width):
        for r in range(height - 1):
            delta = abs(rows[r + 1][c] - rows[r][c])
            total += 1
            if delta == 0.0:
                equal += 1
            elif lattice > 0 and abs(delta - lattice) < lattice * 1e-6:
                stepped += 1

    relief = max(flat) - min(flat) if flat else 0.0
    declared = float(document.get("height_quantization") or 0.0)
    # A cell can never hold more levels than its own relief spans, nor more
    # than it has samples. Comparing against that ceiling instead of against a
    # constant is what makes the measure independent of how flat the cell is.
    supported = min(relief / declared + 1.0, len(flat)) if declared > 0 and flat else 0.0
    distinct = len(set(flat))

    return dict(
        cell=os.path.basename(path),
        samples=len(flat),
        lattice_step_m=lattice,
        declared_quantization_m=document.get("height_quantization"),
        terrace_ratio=equal / total if total else 0.0,
        step_ratio=stepped / total if total else 0.0,
        level_utilisation=distinct / len(flat) if flat else 0.0,
        supported_levels=supported,
        supported_level_ratio=distinct / supported if supported > 0 else 0.0,
        relief_m=relief,
    )


def select_paths(terrain_dir, sample_limit):
    """Every cell by default; an explicit subsample is spread, never the head.

    Cell documents are named by grid coordinate, so the first N filenames are a
    contiguous column strip. Taking the head silently turned a territory verdict
    into a verdict about its flattest western edge.
    """
    paths = sorted(glob.glob(os.path.join(terrain_dir, "*.json")))
    if sample_limit is None or sample_limit >= len(paths) or sample_limit < 1:
        return paths, "complete"
    stride = len(paths) / float(sample_limit)
    strided = [paths[min(int(index * stride), len(paths) - 1)] for index in range(sample_limit)]
    return strided, "strided_subsample"


def main(terrain_dir, out_path, sample_limit=None):
    paths, sampling = select_paths(terrain_dir, sample_limit)
    if not paths:
        print(f"  FATAL: no canonical cell documents under {terrain_dir}")
        return 2

    cells = [m for m in (measure_cell(p) for p in paths) if m]
    if not cells:
        print("  FATAL: no cell document exposed a core_samples grid")
        return 2

    def mean(key):
        return sum(c[key] for c in cells) / len(cells)

    terrace = mean("terrace_ratio")
    utilisation = mean("level_utilisation")
    supported = mean("supported_level_ratio")
    lattice = Counter(c["lattice_step_m"] for c in cells).most_common(1)[0][0]
    declared = cells[0]["declared_quantization_m"]

    findings = [
        dict(ok=terrace <= MAX_TERRACE_RATIO, check="terrace_ratio",
             detail=f"{terrace:.3f} (max {MAX_TERRACE_RATIO})"),
        dict(ok=supported >= MIN_SUPPORTED_LEVEL_RATIO, check="supported_level_ratio",
             detail=f"{supported:.3f} (min {MIN_SUPPORTED_LEVEL_RATIO})"),
    ]
    document = dict(
        terrain_dir=terrain_dir, cells_measured=len(cells), sampling=sampling,
        detected_lattice_step_m=lattice, declared_quantization_m=declared,
        mean_terrace_ratio=terrace, mean_step_ratio=mean("step_ratio"),
        mean_supported_level_ratio=supported,
        mean_level_utilisation=utilisation, mean_relief_m=mean("relief_m"),
        findings=findings, passed=all(f["ok"] for f in findings))
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2)

    print(f"  cells measured           {len(cells)} ({sampling})")
    print(f"  declared quantization    {declared} m")
    print(f"  detected lattice step    {lattice} m")
    print(f"  mean relief              {mean('relief_m'):.2f} m")
    print(f"  mean step_ratio          {mean('step_ratio'):.3f}")
    print(f"  mean level_utilisation   {utilisation:.3f} (diagnostic; relief-bound)")
    for finding in findings:
        print(f"  [{'PASS' if finding['ok'] else 'FAIL'}] "
              f"{finding['check']:22s} {finding['detail']}")
    print(f"\n  SURFACE {'PASSED' if document['passed'] else 'FAILED'} -> {out_path}")
    return 0 if document["passed"] else 1


if __name__ == "__main__":
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else None
    sys.exit(main(sys.argv[1], sys.argv[2], limit))
