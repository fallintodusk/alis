from pathlib import Path
from copy import deepcopy
import tempfile
import unittest

from .civil_grade_fixture import make_candidate
from ..app.contracts import CompilerError, read_json, file_hash
from ..refinement.api import prepare_terrain_refinement
from ..refinement.app.civil_grade import preserve_civil_grades
from ..app.surface_context import surface_context

ROOT = Path(__file__).resolve().parents[4]


class CivilGradeTests(unittest.TestCase):
    def test_whole_quad_preserves_road_and_dry_building_grade(self):
        parent = ROOT / 'tmp/world/civil_grade_unit'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root = Path(temporary)
            make_candidate(root)
            before = read_json(root / 'base/canonical/features/cell_0.json')['features']
            after = read_json(root / 'candidate/canonical/features/cell_0.json')['features']
            manifest = read_json(root / 'base/canonical/cells/cell_0.json')
            for old, current in zip(before, after, strict=True):
                if old['feature_class'] == 'water':
                    self.assertEqual(old, current)
                    continue
                reference = current['attributes'].get('civil_grade_reference')
                self.assertIsNotNone(reference, 'A refined whole quad loses its accepted civil grade')
                self.assertEqual({old['owner_cell_id']: manifest['terrain_artifact']['content_hash']},
                                 reference['terrain_artifacts'])
                self.assertEqual(old['geometry'], current['geometry'])

    def test_neighbor_support_conflicts_and_unrelated_anchor(self):
        parent = ROOT / 'tmp/world/civil_grade_unit'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root = Path(temporary)
            cells, _, _, _ = make_candidate(root)
            features = read_json(root / 'base/canonical/features/cell_0.json')['features']
            manifests = {key: read_json(root / f'base/canonical/cells/cell_{i}.json') for i, key in enumerate(cells)}
            surfaces = {next(iter(cells)): read_json(root / 'candidate/canonical/refined_terrain/cell_0.json')}
            road = features[1]
            road['geometry']['coordinates'][1][0] = 58
            extended = preserve_civil_grades(features, cells, surfaces, manifests)
            self.assertEqual(set(cells), set(extended[road['feature_id']]['attributes']['civil_grade_reference']['terrain_artifacts']))
            building = features[2]
            building['owner_cell_id'] = next(reversed(cells))
            self.assertNotIn(building['feature_id'], preserve_civil_grades(features, cells, surfaces, manifests))
            for context in ({'ford': 'present'}, {'tunnel': 'present'}, {'ground_relation': 'conflicting'}):
                road['attributes']['surface_context'] = context
                before = deepcopy(features)
                with self.subTest(context=context), self.assertRaises(CompilerError):
                    preserve_civil_grades(features, cells, surfaces, manifests)
                self.assertEqual(before, features)

    def test_public_noop_removal_and_refusal_keep_base_exact(self):
        parent = ROOT / 'tmp/world/civil_grade_unit'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root = Path(temporary)
            _, water, policy, selected = make_candidate(root)
            snapshots = lambda path: {p.relative_to(path).as_posix(): file_hash(p) for p in path.rglob('*') if p.is_file()}
            base_before, candidate_before = snapshots(root/'base'), snapshots(root/'candidate')
            prepare_terrain_refinement(ROOT, root/'base/compile_result.json', root/'candidate', water, policy, selected)
            self.assertEqual(candidate_before, snapshots(root/'candidate'))
            prepare_terrain_refinement(ROOT, root/'base/compile_result.json', root/'removed', water, policy, {})
            self.assertEqual(base_before['canonical/features/cell_0.json'], file_hash(root/'removed/canonical/features/cell_0.json'))
            policy['maximum_cut_m'] = 0
            with self.assertRaises(CompilerError):
                prepare_terrain_refinement(ROOT, root/'base/compile_result.json', root/'refused', water, policy, selected)
            self.assertFalse((root/'refused').exists())
            self.assertEqual(base_before, snapshots(root/'base'))

    def test_explicit_unsupported_crossing_is_not_missing_metadata(self):
        parent = ROOT / 'tmp/world/civil_grade_unit'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root = Path(temporary)
            cells, _, _, _ = make_candidate(root)
            features = read_json(root / 'base/canonical/features/cell_0.json')['features']
            manifests = {key: read_json(root / f'base/canonical/cells/cell_{i}.json') for i, key in enumerate(cells)}
            surfaces = {next(iter(cells)): read_json(root / 'candidate/canonical/refined_terrain/cell_0.json')}
            features[1]['attributes']['surface_context'] = surface_context({}, 'road')
            self.assertIn(features[1]['feature_id'], preserve_civil_grades(features, cells, surfaces, manifests))
            for tags in ({'ford': 'seasonal'}, {'tunnel': 'unsupported'}, {'bridge': 'unsupported'}):
                features[1]['attributes']['surface_context'] = surface_context(tags, 'road')
                with self.subTest(tags=tags), self.assertRaises(CompilerError):
                    preserve_civil_grades(features, cells, surfaces, manifests)


if __name__ == '__main__':
    unittest.main()
