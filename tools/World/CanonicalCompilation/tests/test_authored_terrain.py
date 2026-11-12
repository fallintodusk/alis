"""Authored dry Terrain is admitted by the existing selected-candidate owner."""
from copy import deepcopy
from pathlib import Path
import tempfile
import unittest
import io
import json
import shutil
import zipfile
from unittest.mock import patch

from .madison_ground_replay import ROOT, inputs
from .refinement_fixture import make_base
from .test_refinement_artifacts import hashes
from ..app.contracts import CompilerError, canonical_hash, file_hash, read_json
from ..app.artifacts import output_descriptor
from ..app import promotion
from ..refinement.api import prepare_surface_refinement
from ..refinement.app.contracts import write_json
from ..refinement.app.authority import _declared_terrain, _replay_result, _selection_identity, validate_selection_bundle


class AuthoredTerrainTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT/'tmp/world/authored_terrain_tests'
        parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        data = inputs()
        self.cell = data['terrain']
        # Mechanical accepted-artifact fixture; the real authority is tested separately.
        remote = deepcopy(self.cell)
        remote['cell_id'] = remote['cell_id'].replace(':x1:', ':x11:')
        remote['bounds'] = [value+9300 if index in (0, 2) else value
                            for index, value in enumerate(remote['bounds'])]
        make_base(self.root/'base', values=({self.cell['cell_id']: self.cell, remote['cell_id']: remote},
                  data['water']['features'][0], {}, {}))
        self.base = self.root/'base/compile_result.json'
        self.intent = self.root/'authored.json'
        self.overlay = data['overlay']
        self.overlay['$schema'] = 'https://alis.world/schemas/world-compiler/authored-overlay-v2.json'
        self.save()

    def save(self):
        write_json(self.intent, self.overlay)
        self.operation = {'kind': 'authored_dry_terrain', 'feature_ids': [], 'input': {
            'path': self.intent.relative_to(ROOT).as_posix(), 'sha256': file_hash(self.intent),
            'byte_size': self.intent.stat().st_size}}

    def prepare(self, name='candidate'):
        return prepare_surface_refinement(ROOT, self.base, self.root/name, [self.operation])

    def test_authenticated_dry_patch_preserves_base_and_water_and_replays(self):
        before = hashes(self.base.parent)
        result, candidate = self.prepare()
        self.assertEqual('accepted', result['status'])
        self.assertEqual(before, hashes(self.base.parent))
        for name, digest in before.items():
            if name.startswith(('canonical/terrain/', 'canonical/features/')):
                self.assertEqual(digest, file_hash(candidate/name), name)
        report = read_json(candidate/'reports/refinement.json')
        self.assertEqual([self.cell['cell_id']], report['changed_cells'])
        surface = read_json(candidate/'canonical/refined_terrain/cell_0.json')
        self.assertEqual('authored_dry_terrain_v1', surface['construction'])
        self.assertEqual(9, len(surface['quads']))
        self.assertEqual(4, surface['metrics']['changed_nodes'])
        repeated, cold = self.prepare('cold')
        self.assertEqual(result, repeated)
        self.assertEqual(hashes(candidate), hashes(cold))
        saved = hashes(candidate)
        self.assertEqual(result, self.prepare()[0])
        self.assertEqual(saved, hashes(candidate))

    def test_changed_intent_pin_refuses_before_publication(self):
        self.intent.write_bytes(self.intent.read_bytes()+b' ')
        before = hashes(self.base.parent)
        with self.assertRaises(CompilerError):
            self.prepare()
        self.assertFalse((self.root/'candidate').exists())
        self.assertEqual(before, hashes(self.base.parent))

    def test_water_intersection_refuses_without_hiding_the_feature(self):
        self.overlay['terrain_patches'][0]['center'] = [585223.41, 4511202.45]
        self.overlay['terrain_patches'][0]['radius_m'] = 30.
        self.save()
        with self.assertRaisesRegex(CompilerError, 'Water'):
            self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_surface_weights_and_unresolved_provenance_refuse(self):
        for case in ('weights', 'provenance'):
            original = deepcopy(self.overlay)
            if case == 'weights':
                self.overlay['terrain_patches'][0]['surface_weights'] = {'ground': 1., 'hydro_transition': 0.}
            else:
                self.overlay['terrain_patches'][0]['provenance_ref'] = 'unknown'
            self.save()
            with self.subTest(case=case), self.assertRaises(CompilerError):
                self.prepare()
            self.assertFalse((self.root/'candidate').exists())
            self.overlay = original

    def test_failed_shared_promotion_keeps_inputs_and_no_candidate(self):
        before = hashes(self.base.parent)
        with patch('World.CanonicalCompilation.refinement.app.surface_publication.promote_staging',
                   side_effect=CompilerError('injected', 'promotion refusal')):
            with self.assertRaisesRegex(CompilerError, 'promotion refusal'):
                self.prepare()
        self.assertEqual(before, hashes(self.base.parent))
        self.assertFalse((self.root/'candidate').exists())

    def cross_cell(self, missing=False):
        grid = 'grid_0000000000000000'
        cells = {}
        for column in (0, 10) if missing else (0, 1):
            key = f'{grid}:x{column}:y0'
            cells[key] = {'grid_id': grid, 'cell_id': key,
                'bounds': [column*60, 0, column*60+60, 60], 'sample_spacing': [30, 30],
                'height_quantization': .1, 'core_samples': [[12.]*3 for _ in range(3)],
                'surface_semantics': {'roles': ['ground', 'hydro_transition'],
                    'core_samples': {'ground': [[1.]*3 for _ in range(3)],
                                     'hydro_transition': [[0.]*3 for _ in range(3)]}}}
        water = deepcopy(inputs()['water']['features'][0])
        water['geometry'] = {'type': 'Polygon', 'coordinates': [[[8, 12], [22, 12], [22, 18], [8, 18], [8, 12]]]}
        base = self.root/('missing-base' if missing else 'cross-base')
        make_base(base, values=(cells, water, {}, {}))
        self.base = base/'compile_result.json'
        self.overlay['grid_id'] = grid
        self.overlay['terrain_patches'][0].update(center=[60., 30.], radius_m=0., delta_m=1.)
        self.save()

    def test_shared_node_changes_both_authenticated_neighbors(self):
        self.cross_cell()
        before = hashes(self.base.parent)
        _, candidate = self.prepare()
        surfaces = [read_json(candidate/f'canonical/refined_terrain/cell_{i}.json') for i in range(2)]
        self.assertEqual([2, 2], [len(s['quads']) for s in surfaces])
        edges = [sorted({tuple(p) for q in surface['quads'] for p in q['vertices'] if p[0] == 60})
                 for surface in surfaces]
        self.assertEqual(edges[0], edges[1])
        self.assertIn((60., 30., 13., 1., 0.), edges[0])
        self.assertEqual(before, hashes(self.base.parent))

    def test_missing_neighbor_cannot_publish_a_changed_outer_edge(self):
        self.cross_cell(missing=True)
        with self.assertRaisesRegex(CompilerError, 'unselected boundary'):
            self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_same_cell_operations_refuse_without_partial_publication(self):
        with self.assertRaisesRegex(CompilerError, 'disjoint'):
            prepare_surface_refinement(ROOT, self.base, self.root/'candidate', [self.operation, self.operation])
        self.assertFalse((self.root/'candidate').exists())

    def test_late_terrain_bound_support_is_rechecked_before_publication(self):
        from ..refinement.app import authored_terrain
        construct = authored_terrain.construct

        def changed_support(repo, operation, document, coverage, cells, features):
            result = construct(repo, operation, document, coverage, cells, features)
            next(iter(features.values()))['attributes']['road_elevation_profile'] = {
                'support_terrain_artifacts': {self.cell['cell_id']: 'a'*64}}
            return result

        with patch.object(authored_terrain, 'construct', side_effect=changed_support):
            with self.assertRaisesRegex(CompilerError, 'retained Terrain-bound'):
                self.prepare()
        self.assertFalse((self.root/'candidate').exists())

    def test_frozen_domain_binding_and_current_replay(self):
        _, candidate = self.prepare()
        _replay_result(ROOT, self.base, candidate/'refinement_result.json', self.root/'current-replay')
        request = read_json(candidate/'refinement/request.json')

        def archive(root, corrupt=None):
            buffer = io.BytesIO()
            with zipfile.ZipFile(buffer, 'w') as bundle:
                for p in root.rglob('*.json'):
                    payload = p.read_bytes()
                    if corrupt and 'refined_terrain' in p.parts:
                        document = json.loads(payload)
                        if corrupt == 'domain':
                            document['consumed']['domain_sha256'] = '0'*64
                        else:
                            document['quads'][0]['vertices'][0][2] += .1
                        payload = json.dumps(document).encode('ascii')
                    bundle.writestr(p.relative_to(root).as_posix(), payload)
            return zipfile.ZipFile(io.BytesIO(buffer.getvalue()))

        with archive(self.base.parent) as base, archive(candidate) as child:
            self.assertEqual(1, len(_declared_terrain(base, child, request)))
        for corrupt in ('domain', 'vertex'):
            with self.subTest(corrupt=corrupt):
                with archive(self.base.parent) as base, archive(candidate, corrupt=corrupt) as child:
                    with self.assertRaisesRegex(CompilerError, 'immutable input'):
                        _declared_terrain(base, child, request)

    def frozen_authority(self, child, name):
        root = self.root/name
        root.mkdir()
        base = read_json(self.base)
        promotion._write_deterministic_bundle(root/'base.zip',
            [(e['path'], self.base.parent/e['path']) for e in base['outputs']], base)
        result = read_json(child/'refinement_result.json')
        request = read_json(child/'refinement/request.json')
        selected = {'base_result_sha256': result['base_result_sha256'], 'inputs_hash': result['inputs_hash'],
            'result_sha256': file_hash(child/'refinement_result.json'), 'output_count': len(result['outputs']),
            **{k: request[k] for k in ('implementation_sha256', 'source_context_identity', 'execution_identity')}}
        address = canonical_hash({k: selected[k] for k in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
        relative = f'refinements/{address}.zip'
        target = root/relative
        target.parent.mkdir()
        promotion._write_deterministic_bundle(target,
            [(e['path'], child/e['path']) for e in result['outputs']], result, result_name='refinement_result.json')
        selected['bundle'] = {'path': relative, 'sha256': file_hash(target), 'byte_size': target.stat().st_size}
        authority = {'authority_id': 'authored-fixture', 'profile_id': result['profile_id'],
            'compile_result_sha256': file_hash(self.base),
            'bundle': {'path': 'base.zip', 'sha256': file_hash(root/'base.zip')}, 'selected_refinement': selected}
        selected['effective_identity'] = _selection_identity(authority, selected)
        return authority, root

    def test_frozen_authored_only_preserves_original_feature_and_terrain_bytes(self):
        _, child = self.prepare()
        authority, root = self.frozen_authority(child, 'valid-authority')
        validate_selection_bundle(authority, root)
        before = hashes(self.base.parent)
        for case in ('water', 'terrain'):
            bad = self.root/f'changed-{case}'
            shutil.copytree(child, bad)
            if case == 'water':
                path = bad/'canonical/features/cell_0.json'
                shard = read_json(path)
                shard['features'][0]['attributes']['surface_function']['level_m'] += 1.
                write_json(path, shard)
                manifest_path = bad/'canonical/cells/cell_0.json'
                manifest = read_json(manifest_path)
                manifest['feature_artifact'].update(content_hash=file_hash(path), byte_size=path.stat().st_size)
                write_json(manifest_path, manifest)
                coverage_path = bad/'canonical/coverage.json'
                coverage = read_json(coverage_path)
                coverage['cells'][0].update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
                for descriptor in coverage['artifact_descriptors']:
                    if descriptor['artifact_id'] == manifest['feature_artifact']['artifact_id']:
                        descriptor.update({k: v for k, v in manifest['feature_artifact'].items() if k != 'path'})
                write_json(coverage_path, coverage)
            else:
                path = bad/'canonical/terrain/cell_0.json'
                terrain = read_json(path)
                terrain['core_samples'][0][0] += 1.
                write_json(path, terrain)
            result = read_json(bad/'refinement_result.json')
            result['outputs'] = [output_descriptor('compiler_document', p, bad)
                for p in sorted(bad.rglob('*.json')) if p.name != 'refinement_result.json']
            write_json(bad/'refinement_result.json', result)
            authority, root = self.frozen_authority(bad, f'{case}-authority')
            with self.subTest(case=case), self.assertRaisesRegex(CompilerError, 'original feature|unrelated base artifact'):
                validate_selection_bundle(authority, root)
        self.assertEqual(before, hashes(self.base.parent))
