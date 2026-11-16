"""Regressions for capture authentication.

Every case below is a way a capture run can report success while the evidence is
worthless. They are written against synthesized PNGs so they need no engine.
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "app"))

import verify_capture
import plan_vantages


def write_png(path, width, height, fill):
    """Smallest valid PNG: one IDAT of solid colour, no interlace."""
    raw = b"".join(b"\x00" + bytes(fill) * width for _ in range(height))

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    with open(path, "wb") as handle:
        handle.write(verify_capture.PNG_SIGNATURE)
        handle.write(chunk(b"IHDR", header))
        handle.write(chunk(b"IDAT", zlib.compress(raw)))
        handle.write(chunk(b"IEND", b""))
    return path


def sha256_of(path):
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()


class CaptureAuthenticationTests(unittest.TestCase):
    def setUp(self):
        scratch = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                               "../../../../tmp/world/visual_verification/tests"))
        os.makedirs(scratch, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.directory.cleanup)

    def test_paired_review_requires_normal_render_of_the_same_pose(self):
        receipt, _ = self.build(self.directory.name, self.healthy())
        with open(receipt, encoding="utf-8") as handle:
            document = json.load(handle)
        for view in document["views"]:
            view["render_mode"] = "base_color"
        with open(receipt, "w", encoding="utf-8") as handle:
            json.dump(document, handle)
        passed, findings, _ = verify_capture.verify(receipt)
        self.assertFalse(passed)
        self.assertTrue(any(not item["ok"] and item["check"] == "paired_presentation"
                            for item in findings))

    def paired_receipt(self):
        modes = ("lit", "base_color", "lighting_only", "world_normal")
        views = [(mode, 32, 20, (20 + index * 40, 40, 60), (32, 20))
                 for index, mode in enumerate(modes)]
        receipt, _ = self.build(self.directory.name, views)
        with open(receipt, encoding="utf-8") as handle:
            document = json.load(handle)
        for view, mode in zip(document["views"], modes):
            view.update(render_mode=mode, evidence_role="product" if mode == "lit" else "diagnostic")
        self.save(receipt, document)
        return receipt, document

    def save(self, path, document):
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(document, handle)

    def test_complete_paired_views_pass_and_shifted_camera_refuses(self):
        receipt, document = self.paired_receipt()
        self.assertTrue(verify_capture.verify(receipt)[0])
        document["views"][1]["location"][0] += 10
        self.save(receipt, document)
        self.assertIn("paired_presentation", self.failed_checks(receipt))

    def test_diagnostic_cannot_be_labelled_product_or_unknown_mode(self):
        receipt, document = self.paired_receipt()
        document["views"][1]["evidence_role"] = "product"
        self.save(receipt, document)
        self.assertIn("evidence_role:base_color", self.failed_checks(receipt))
        document["views"][1]["render_mode"] = "brightened"
        self.save(receipt, document)
        self.assertIn("render_mode:base_color", self.failed_checks(receipt))

    def test_normal_overview_can_accompany_a_local_diagnostic_pair(self):
        receipt, document = self.paired_receipt()
        path = os.path.join(self.directory.name, "screenshots", "overview.png")
        write_png(path, 32, 20, (0, 255, 0))
        overview = dict(document["views"][0], name="overview", file="overview.png",
                        image_sha256=sha256_of(path), location=[1000, 0, 500])
        document["views"].append(overview)
        self.save(receipt, document)
        self.assertTrue(verify_capture.verify(receipt)[0])

    def test_uniform_diagnostic_buffers_do_not_imply_a_stale_product_frame(self):
        receipt, document = self.paired_receipt()
        second = []
        for original in document["views"]:
            view = dict(original, name="second_" + original["name"], location=[100, 0, 0])
            if view["render_mode"] == "lit":
                path = os.path.join(self.directory.name, "screenshots", "second_lit.png")
                write_png(path, 32, 20, (255, 128, 0))
                view.update(file="second_lit.png", image_sha256=sha256_of(path))
            second.append(view)
        document["views"].extend(second)
        self.save(receipt, document)
        self.assertTrue(verify_capture.verify(receipt)[0])

    def test_planner_preserves_product_framing_and_input(self):
        original = dict(name="rim", x=1, y=2, z=3, pitch=-65, yaw=90, fov=75)
        plan = dict(capture_width=1280, capture_height=720, vantages=[original])
        expanded = plan_vantages.with_diagnostics(plan)
        self.assertEqual(4, len(expanded["vantages"]))
        self.assertNotIn("render_mode", original)
        self.assertEqual([original], plan["vantages"])
        for view in expanded["vantages"]:
            self.assertEqual({key: view[key] for key in original if key != "name"},
                             {key: value for key, value in original.items() if key != "name"})
        with self.assertRaises(ValueError):
            plan_vantages.with_diagnostics(expanded)

    def build(self, directory, views, control_matches=True, status="accepted"):
        screenshots = os.path.join(directory, "screenshots")
        os.makedirs(screenshots, exist_ok=True)
        entries = []
        for name, width, height, fill, declared in views:
            path = write_png(os.path.join(screenshots, name + ".png"), width, height, fill)
            entries.append(dict(
                name=name, file=name + ".png",
                requested_width=declared[0], requested_height=declared[1],
                image_sha256=sha256_of(path), fov_degrees=90.0,
                location=[0, 0, 0], rotation_pitch_yaw_roll=[-90, 0, 0]))
        receipt = dict(
            schema_version=1, map_package="/ProjectWorldData/Generated/Territory/L_Test",
            vantage_plan_sha256="a" * 64, screenshot_dir=screenshots,
            capture_width=views[0][1], capture_height=views[0][2], status=status,
            message="", views_pairwise_distinct=True, views=entries,
            determinism_control=dict(name=views[0][0], repeat_sha256=entries[0]["image_sha256"],
                                     matches_first_capture=control_matches))
        path = os.path.join(directory, "capture.json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(receipt, handle)
        return path, screenshots

    def healthy(self):
        return [("01_overview", 320, 200, (10, 20, 30), (320, 200)),
                ("02_oblique", 320, 200, (40, 50, 60), (320, 200))]

    def failed_checks(self, receipt_path):
        _, findings, _ = verify_capture.verify(receipt_path)
        return [f["check"] for f in findings if not f["ok"]]

    def test_authentic_capture_set_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt, _ = self.build(directory, self.healthy())
            self.assertEqual(self.failed_checks(receipt), [])
            self.assertEqual(verify_capture.main(receipt), 0)

    def test_missing_frame_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt, screenshots = self.build(directory, self.healthy())
            os.remove(os.path.join(screenshots, "02_oblique.png"))
            self.assertIn("exists:02_oblique", self.failed_checks(receipt))

    def test_wrong_dimensions_are_rejected(self):
        # The engine reported 320x200; the file is 160x100. A framing solve that
        # describes a different frame than the one rendered is not evidence.
        with tempfile.TemporaryDirectory() as directory:
            views = [("01_overview", 160, 100, (10, 20, 30), (320, 200)),
                     ("02_oblique", 160, 100, (40, 50, 60), (320, 200))]
            receipt, _ = self.build(directory, views)
            self.assertIn("dimensions:01_overview", self.failed_checks(receipt))

    def test_identical_frames_across_poses_are_rejected(self):
        # The stale-frame signature: two different camera poses, one image.
        with tempfile.TemporaryDirectory() as directory:
            views = [("01_overview", 320, 200, (10, 20, 30), (320, 200)),
                     ("02_oblique", 320, 200, (10, 20, 30), (320, 200))]
            receipt, _ = self.build(directory, views)
            self.assertIn("views_distinct", self.failed_checks(receipt))

    def test_repeat_pose_difference_is_reported_not_rejected(self):
        # Real captures differ slightly between repeats because the renderer carries temporal
        # state. That is reported for the operator, never a rejection: the stale-frame property
        # is carried by views_distinct above.
        with tempfile.TemporaryDirectory() as directory:
            receipt, _ = self.build(directory, self.healthy(), control_matches=False)
            self.assertEqual(self.failed_checks(receipt), [])
            _, findings, _ = verify_capture.verify(receipt)
            control = [f for f in findings if f["check"] == "repeat_pose_control"]
            self.assertEqual(len(control), 1)
            self.assertIn("temporal state", control[0]["detail"])

    def test_rewritten_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt, screenshots = self.build(directory, self.healthy())
            write_png(os.path.join(screenshots, "02_oblique.png"), 320, 200, (99, 99, 99))
            self.assertIn("unmodified:02_oblique", self.failed_checks(receipt))

    def test_rejected_receipt_is_not_authenticated(self):
        with tempfile.TemporaryDirectory() as directory:
            receipt, _ = self.build(directory, self.healthy(), status="rejected")
            self.assertIn("receipt_status", self.failed_checks(receipt))


if __name__ == "__main__":
    unittest.main()
