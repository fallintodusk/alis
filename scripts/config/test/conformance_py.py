"""Conformance runner for the Python strict conf parser (ue_conf.py).

Drives the SAME shared fixture corpus as the ps1/bat Pester suite
(scripts/config/test/fixtures/), plus normalization and stale-env unit
checks. Native runner on purpose: Pester must not require python.

Run: python scripts/config/test/conformance_py.py
Exit code: 0 = all pass, 1 = failures.
"""
from __future__ import annotations

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import ue_conf  # noqa: E402

FIXTURES = os.path.join(HERE, "fixtures")

failures = []


def check(name, cond, detail=""):
    if cond:
        print("[OK] %s" % name)
    else:
        print("[FAIL] %s %s" % (name, detail))
        failures.append(name)


def read_expected(case_dir):
    with open(os.path.join(case_dir, "expected.txt")) as fh:
        lines = [l.rstrip("\n") for l in fh]
    if lines and lines[0].strip() == "ERROR":
        return None
    return dict(l.split("=", 1) for l in lines if "=" in l)


def run_fixtures():
    for case in sorted(os.listdir(FIXTURES)):
        case_dir = os.path.join(FIXTURES, case)
        if not os.path.isdir(case_dir):
            continue
        expected = read_expected(case_dir)
        try:
            values, _ = ue_conf.resolve_conf(case_dir)
            error = None
        except ue_conf.UEConfError as exc:
            values, error = None, exc
        if expected is None:
            check("case %s (expect error)" % case, error is not None,
                  "parsed unexpectedly: %r" % (values,))
        else:
            ok = error is None and all(
                values.get(k) == v for k, v in expected.items())
            check("case %s" % case, ok,
                  "got %r error=%r expected %r" % (values, error, expected))


def run_units():
    n = ue_conf.normalize_engine_path
    check("normalize backslashes",
          n("C:\\TestEngine\\UE_X\\") == "c:/testengine/ue_x")
    check("normalize msys form",
          n("/c/TestEngine/UE_X") == "c:/testengine/ue_x")
    check("normalize case-insensitive equal",
          n("C:/A/B") == n("c:/a/b/"))
    check("stale mismatch detected",
          ue_conf.check_stale_env(
              "C:/New", env={"UE_PATH": "C:/Old"}) is not None)
    check("matching env passes",
          ue_conf.check_stale_env(
              "C:/Same/Path", env={"UE_PATH": "/c/Same/Path/"}) is None)
    check("unset env passes",
          ue_conf.check_stale_env("C:/Any", env={}) is None)


if __name__ == "__main__":
    run_fixtures()
    run_units()
    if failures:
        print("FAILED: %d case(s)" % len(failures))
        sys.exit(1)
    print("ALL PASS")
