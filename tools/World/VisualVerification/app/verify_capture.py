"""Authenticate an operator capture set against the files it claims.

WHY THIS EXISTS
---------------
A capture receipt written by the process that did the capturing is a claim, not
evidence. The failure modes this stage exists to catch all produce a receipt
that looks fine:

  missing frame        the engine reported success and wrote nothing
  wrong size           the render target silently fell back to another size, so
                       the framing solve no longer describes what was rendered
  stale frame          the same image served for several camera poses, which is
                       exactly what the editor viewport did when it was not the
                       OS foreground window
  rewritten file       the receipt describes bytes that are no longer on disk

So this reads the PNG headers itself and re-hashes the files. It never consults
the engine and never trusts a field it can measure instead.

The result is operator EVIDENCE. A green run here means the frames are real,
correctly sized, distinct and unmodified - it does NOT mean the territory looks
right. Only the operator decides that.
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_dimensions(path):
    """Width and height straight from IHDR, or None if this is not a PNG."""
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) < 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", header[16:24])
    return width, height


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify(receipt_path):
    with open(receipt_path, encoding="utf-8") as handle:
        receipt = json.load(handle)

    directory = os.path.dirname(os.path.abspath(receipt_path))
    screenshots = receipt.get("screenshot_dir") or os.path.join(
        os.path.dirname(directory), "screenshots")
    views = receipt.get("views") or []
    findings = []

    def finding(ok, check, detail):
        findings.append(dict(ok=bool(ok), check=check, detail=detail))

    finding(receipt.get("status") == "accepted", "receipt_status",
            str(receipt.get("status")))
    finding(bool(views), "views_present", f"{len(views)} views")

    seen = {}
    for view in views:
        name = view.get("name", "<unnamed>")
        path = os.path.join(screenshots, view.get("file", ""))
        if not os.path.isfile(path):
            finding(False, f"exists:{name}", path)
            continue
        finding(True, f"exists:{name}", os.path.basename(path))

        dimensions = png_dimensions(path)
        requested = (view.get("requested_width"), view.get("requested_height"))
        finding(dimensions == requested, f"dimensions:{name}",
                f"{dimensions} vs requested {requested}")

        actual = sha256_of(path)
        finding(actual == view.get("image_sha256"), f"unmodified:{name}",
                f"{actual[:12]} vs recorded {str(view.get('image_sha256'))[:12]}")
        seen.setdefault(actual, []).append(name)

    # Distinct poses that render identical bytes are the stale-frame signature.
    collisions = {h: names for h, names in seen.items() if len(names) > 1}
    finding(not collisions, "views_distinct",
            "all distinct" if not collisions else f"identical: {collisions}")

    # The control is the FIRST pose captured a second time. It is REPORTED, never gated:
    # UE carries temporal rendering state between frames (auto exposure reads the previous
    # frame), so healthy imagery legitimately fails a byte-identical repeat. Gating it would
    # reject good evidence and invite a pointless hunt for a bit-deterministic renderer.
    control = receipt.get("determinism_control") or {}
    findings.append(dict(
        ok=True, check="repeat_pose_control",
        detail=f"control {control.get('name')} "
               f"{'byte-identical' if control.get('matches_first_capture') else 'differs (temporal state)'}"))

    passed = all(f["ok"] for f in findings)
    return passed, findings, receipt


def main(receipt_path, out_path=None):
    if not os.path.isfile(receipt_path):
        print(f"  FATAL: no capture receipt at {receipt_path}")
        return 2
    passed, findings, receipt = verify(receipt_path)

    print(f"  map                      {receipt.get('map_package')}")
    print(f"  vantage plan             {str(receipt.get('vantage_plan_sha256'))[:12]}")
    print(f"  requested size           {receipt.get('capture_width')}x{receipt.get('capture_height')}")
    for item in findings:
        print(f"  [{'PASS' if item['ok'] else 'FAIL'}] {item['check']:28s} {item['detail']}")
    print(f"\n  CAPTURE {'AUTHENTICATED' if passed else 'REJECTED'}")
    print("  Authenticated frames are operator EVIDENCE, not an acceptance gate.")

    if out_path:
        with open(out_path, "w", encoding="utf-8") as handle:
            json.dump(dict(receipt=receipt_path, passed=passed, findings=findings),
                      handle, indent=2)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None))
