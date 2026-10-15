from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from World.CanonicalCompilation.api import prepare_terrain_refinement
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash, read_json
from World.CanonicalCompilation.app.terrain_refinement_artifacts import validate_refinement
from World.CanonicalCompilation.tests.refinement_fixture import make_base
from World.ExecutionEnvironment.api import ExecutionEnvironmentError


def hashes(root):
    return {p.relative_to(root).as_posix(): file_hash(p) for p in root.rglob("*") if p.is_file()}


class RefinementArtifactTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / "tmp/world/terrain_refinement_unit"
        parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.cells, self.water, self.policy, self.selected = make_base(self.root / "base", cross_cell=False)

    def prepare(self, target="candidate"):
        return prepare_terrain_refinement(ROOT, self.root / "base/compile_result.json", self.root / target,
                                           self.water, self.policy, self.selected)

    def test_sparse_publication_identity_noop_and_removal(self):
        before = hashes(self.root / "base")
        result, candidate = self.prepare()
        self.assertEqual("refine", result["operation"])
        self.assertEqual(before, hashes(self.root / "base"))
        for name, digest in before.items():
            if name not in {"compile_result.json", "canonical/coverage.json", "canonical/cells/cell_0.json"}:
                self.assertEqual(digest, file_hash(candidate / name), name)
        original = read_json(self.root / "base/canonical/cells/cell_0.json")
        current = read_json(candidate / "canonical/cells/cell_0.json")
        self.assertEqual(original["terrain_lineage"], current["terrain_lineage"])
        self.assertEqual(original["feature_artifact"], current["feature_artifact"])
        self.assertNotEqual(original["terrain_artifact"]["content_hash"], current["terrain_artifact"]["content_hash"])
        saved = hashes(candidate)
        self.assertEqual(result, self.prepare()[0])
        self.assertEqual(saved, hashes(candidate))
        self.selected = {}
        _, removed = self.prepare("removed")
        for name in ("canonical/cells/cell_0.json", "canonical/cells/cell_1.json"):
            self.assertEqual(before[name], file_hash(removed / name))

    def test_injected_promotion_failure_keeps_base_and_authority_exact(self):
        before = hashes(self.root / "base")
        with patch("World.CanonicalCompilation.app.terrain_refinement_artifacts.promote_staging", side_effect=CompilerError("injected", "fixture refusal")):
            with self.assertRaises(CompilerError):
                self.prepare()
        self.assertFalse((self.root / "candidate").exists())
        self.assertEqual(before, hashes(self.root / "base"))
        self.assertEqual({"base"}, {p.name for p in self.root.iterdir()})

    def test_native_failure_and_changed_water_refuse_without_writes(self):
        before = hashes(self.root / "base")
        with patch("World.CanonicalCompilation.app.terrain_refinement.partition_triangles", side_effect=ExecutionEnvironmentError("native_failure", "fixture")):
            with self.assertRaises(CompilerError) as failure:
                self.prepare()
        self.assertEqual("native_failure", failure.exception.code)
        self.water["attributes"]["surface_function"]["level_m"] = 10.5
        with self.assertRaises(CompilerError) as failure:
            self.prepare()
        self.assertEqual("refinement_water_identity", failure.exception.code)
        self.assertEqual(before, hashes(self.root / "base"))
        self.assertFalse((self.root / "candidate").exists())

    def test_bad_replacement_coverage_and_weights_refuse(self):
        _, candidate = self.prepare()
        original = read_json(candidate / "canonical/refined_terrain/cell_0.json")
        for defect in ("missing", "overlap", "index", "weights", "version"):
            surface = deepcopy(original)
            quad = surface["quads"][0]
            if defect == "missing":
                quad["triangles"].pop()
                quad["water_triangles"] = []
            elif defect == "overlap":
                quad["triangles"][0] = quad["triangles"][1]
            elif defect == "index":
                quad["triangles"][0][0] = len(quad["vertices"])
            elif defect == "weights":
                quad["vertices"][0][3:] = [0, 0]
            else:
                surface["schema_version"] = 2
            with self.subTest(defect=defect), self.assertRaises(CompilerError):
                validate_refinement(ROOT, self.cells[next(iter(self.cells))], surface)

    def test_scope_and_existing_unowned_directory_refuse_unchanged(self):
        before = hashes(self.root / "base")
        for target in (ROOT / "tools", self.root / "base", self.root / "base/nested"):
            with self.subTest(target=target), self.assertRaises(CompilerError) as failure:
                prepare_terrain_refinement(ROOT, self.root / "base/compile_result.json", target,
                                           self.water, self.policy, self.selected)
            self.assertEqual("refinement_scope_invalid", failure.exception.code)
        (self.root / "candidate").mkdir()
        marker = self.root / "candidate/operator.txt"
        marker.write_text("retain", encoding="ascii")
        with self.assertRaises(CompilerError):
            self.prepare()
        self.assertEqual({"operator.txt": file_hash(marker)}, hashes(self.root / "candidate"))
        self.assertEqual(before, hashes(self.root / "base"))


if __name__ == "__main__":
    unittest.main()
