"""Small saved-input envelope controls; native geometry is qualified by its owner."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from prepare_region_view import REPO, prepare, read


class RegionalViewPreparationTests(unittest.TestCase):
    def setUp(self):
        root = REPO/'tmp/world/regional_view_preparation_tests'
        root.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=root)
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        self.scene = self.root/'source'
        plugin = self.scene/'runtime_plugin'
        self.plugin = plugin
        (plugin/'Content').mkdir(parents=True)
        (plugin/'ProjectWorldBasinSectionsFixture.uplugin').write_text('{}')
        content = plugin/'Content'
        (content/'SectionMap.umap').write_bytes(b'opaque saved map')
        (content/'section.uasset').write_bytes(b'opaque native section')
        self.result = self.root/'result.json'
        self.result.write_text('{}')
        self.original = self.root/'original.json'
        self.original.write_text(json.dumps({'canonical_result': {
            'path': self.result.relative_to(REPO).as_posix(),
            'sha256': hashlib.sha256(self.result.read_bytes()).hexdigest()}}))
        (self.scene/'sections.json').write_text(json.dumps({
            'fixture_sha256': hashlib.sha256(self.original.read_bytes()).hexdigest()}))
        self.profile = self.root/'profile.json'
        self.profile.write_text('{}')
        self.plan = self.root/'plan.json'
        self.plan.write_text(json.dumps({'vantages': [{'x': 1., 'y': 2., 'z': 3.}]}))

    def prepare(self, output):
        return prepare(REPO, self.scene, self.original, self.profile, self.plan, output)

    def test_pins_consumed_scene_and_keeps_original_bytes(self):
        before = {p: p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        launch = self.prepare(self.root/'output')
        document = read(REPO/launch['input'])
        self.assertIn((self.plugin/'Content/SectionMap.umap').relative_to(REPO).as_posix(), document['saved_files'])
        self.assertIn(self.original.relative_to(REPO).as_posix(), document['saved_files'])
        self.assertEqual(launch['input_sha256'], hashlib.sha256((REPO/launch['input']).read_bytes()).hexdigest())
        self.assertTrue(all(p.read_bytes() == data for p, data in before.items()))

    def test_stale_original_or_result_refuses_without_output(self):
        original = self.original.read_bytes()
        self.original.write_bytes(original+b' ')
        output = self.root/'refused_original'
        with self.assertRaisesRegex(ValueError, 'Original canonical fixture'):
            self.prepare(output)
        self.assertFalse(output.exists())
        self.original.write_bytes(original)
        self.result.write_text('{"changed":true}')
        output = self.root/'refused_result'
        with self.assertRaisesRegex(ValueError, 'exact canonical result'):
            self.prepare(output)
        self.assertFalse(output.exists())

    def test_nested_output_refuses_before_modifying_source(self):
        output = self.scene/'nested_output'
        with self.assertRaisesRegex(ValueError, 'fresh separate output'):
            self.prepare(output)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
