"""Small adapter refusals; native scene acceptance is a separate exact test."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

PATH = Path(__file__).with_name('prepare_region_pier.py')
SPEC = importlib.util.spec_from_file_location('prepare_region_pier', PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PierPreparationTests(unittest.TestCase):
    def test_complete_snapshot_pin_refuses_changed_output_without_mutation(self):
        root = MODULE.REPO/'tmp/world/regional_pier_preparation_tests'
        root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=root) as temporary:
            directory = Path(temporary)
            artifact = directory/'original.json'
            artifact.write_text('{}', encoding='ascii')
            result = directory/'result.json'
            document = {'status': 'accepted', 'outputs': [{'path': 'original.json',
                'sha256': MODULE.digest(artifact), 'byte_size': 2}]}
            result.write_text(json.dumps(document), encoding='ascii')
            pin = MODULE.digest(result)
            MODULE.authenticated(result, pin)
            artifact.write_text('[]', encoding='ascii')
            before = {p.name: p.read_bytes() for p in directory.iterdir()}
            with self.assertRaisesRegex(ValueError, 'complete snapshot'):
                MODULE.authenticated(result, pin)
            self.assertEqual(before, {p.name: p.read_bytes() for p in directory.iterdir()})
            with self.assertRaisesRegex(ValueError, 'selected identity'):
                MODULE.authenticated(result, '0'*64)

    def test_query_contract_retains_bilinear_and_physical_triangle_distinction(self):
        grid = {'bounds': [0, 0, 1, 1], 'sample_spacing': [1, 1], 'core_samples': [[0, 0], [0, 4]]}
        self.assertEqual(MODULE.grid_values(grid, .5, .5), (1, 0))
        with self.assertRaisesRegex(ValueError, 'escapes'):
            MODULE.grid_values(grid, 2, .5)

    def test_output_cannot_add_files_to_either_authenticated_canonical_root(self):
        root = MODULE.REPO/'tmp/world/regional_pier_preparation_tests'
        root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=root) as temporary:
            directory = Path(temporary)
            candidate, base = directory/'candidate', directory/'base'
            candidate.mkdir(); base.mkdir()
            result, original = candidate/'result.json', base/'result.json'
            result.write_text('{}'); original.write_text('{}')
            before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
            for source in (candidate, base):
                output = source/'nested_observation'
                with self.assertRaisesRegex(ValueError, 'disjoint'):
                    MODULE.prepare(MODULE.REPO, result, '0'*64, original, '0'*64, directory/'unused.json', output)
                self.assertFalse(output.exists())
                self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})

    def test_full_width_water_and_entire_cap_proof_has_a_real_intrusion_control(self):
        item, _, ground = MODULE.fixture()
        points = [p for face in item['triangles'] for p in face]
        west, east = min(p[0] for p in points)-1, max(p[0] for p in points)+1
        south, north = min(p[1] for p in points)-1, max(p[1] for p in points)+1
        water = {'feature_id': 'control:ordinary-water',
            'geometry': {'type': 'Polygon', 'coordinates': [[[west, south], [east, south], [east, north], [west, north], [west, south]]]},
            'attributes': {'surface_behavior': 'standing', 'surface_function': {'level_m': 0}}}
        proof = MODULE.prove(MODULE.REPO, item['triangles'], ground, [water], .25)
        self.assertGreater(proof['minimum_deck_above_overlapping_water_m'], .17)
        self.assertLess(max(proof['full_cap_ground_error_m']), 1e-8)
        with self.assertRaisesRegex(ValueError, 'positive clearance'):
            MODULE.prove(MODULE.REPO, item['triangles'], ground, [water], .9)


if __name__ == '__main__':
    unittest.main()
