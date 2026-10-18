"""One base authority and an isolated, exactly replayed effective selection."""
from copy import deepcopy
from pathlib import Path
import os
import shutil
import sys
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash, file_hash, read_json, write_json
from World.CanonicalCompilation.app import promotion
from World.CanonicalCompilation.tests import test_road_refinement as roads
from World.CanonicalCompilation.refinement.app.contracts import write_json as write_child


class RefinementAuthorityTests(unittest.TestCase):
    def setUp(self):
        self.fixture = roads.RoadRefinementTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root, self.base = self.fixture.root, self.fixture.base
        self.profile_path = self.root / 'profile.json'
        self.profile_path.write_text('{}', encoding='ascii')
        self.profile = {'profile_id': 'refinement_fixture', 'world_data_plugin': 'ProjectWorldTestData'}
        self.external = {
            'source_profile_path': 'fixture/source.json', 'source_profile_sha256': '0'*64,
            'source_run_inputs_hash': '1'*64,
            'authored_overlay_path': 'fixture/overlay.json', 'authored_overlay_sha256': '0'*64,
            'fixture_features_path': None, 'fixture_features_sha256': None,
            'budget_profile_path': None, 'budget_profile_sha256': None,
            'control_profile_path': None, 'control_profile_sha256': None,
        }
        for name, value in (('load_profile', self.profile), ('current_external_inputs',
                {k: v for k, v in self.external.items() if k != 'source_run_inputs_hash'})):
            mock = patch.object(promotion, name, return_value=value)
            mock.start()
            self.addCleanup(mock.stop)
        result = read_json(self.base / 'compile_result.json')
        identity = canonical_hash({'compile_inputs_hash': result['inputs_hash'], 'admission_receipt_sha256': None})
        relative = f'bundles/{identity}.zip'
        bundle = self.root / 'authority' / relative
        bundle.parent.mkdir(parents=True)
        promotion._write_deterministic_bundle(bundle,
            [(e['path'], self.base / e['path']) for e in result['outputs']], result)
        self.active = bundle.parent.parent / 'active.json'
        derive = patch.object(promotion, 'canonical_active_path', return_value=self.active)
        derive.start()
        self.addCleanup(derive.stop)
        authority = {
            '$schema': Path(os.path.relpath(ROOT / 'tools/World/CanonicalCompilation/contracts/canonical-authority.schema.json', self.active.parent)).as_posix(),
            'schema_version': 1, 'authority_id': 'refinement_fixture:' + identity,
            **self.profile, 'inputs_hash': result['inputs_hash'], 'profile_sha256': file_hash(self.profile_path),
            'external_inputs': self.external, 'admission_receipt_sha256': None, 'admission_artifacts': [],
            'compile_result_sha256': file_hash(self.base / 'compile_result.json'), 'output_count': len(result['outputs']),
            'bundle': {'format': 'deterministic_zip_v1', 'path': relative,
                'sha256': file_hash(bundle), 'byte_size': bundle.stat().st_size},
        }
        write_json(self.active, authority)
        self.base_authority = promotion.validate_canonical_authority(ROOT, self.active, self.profile_path)
        self.cache_roots = []
        self.addCleanup(self.cleanup_cache)

    def cleanup_cache(self):
        scratch = (ROOT / 'tmp/world/canonical_compilation/materialized').resolve()
        for root in self.cache_roots:
            if not root.resolve().is_relative_to(scratch):
                raise AssertionError('Fixture cleanup escaped its materializer cache')
            if root.exists():
                shutil.rmtree(root)

    def materialize(self, active=None):
        active = active or self.active
        authority = read_json(active)
        identity = authority.get('selected_refinement', {}).get('effective_identity', authority['bundle']['sha256'])
        self.cache_roots.append(ROOT / 'tmp/world/canonical_compilation/materialized' / authority['profile_id'] / identity)
        return promotion.materialize_canonical(ROOT, active, self.profile_path)

    def prepare(self, output='selected'):
        from World.CanonicalCompilation.refinement.api import prepare_surface_authority
        _, child = self.fixture.prepare()
        return prepare_surface_authority(ROOT, self.active, self.profile_path,
            child / 'refinement_result.json', self.root / output)

    def test_base_cache_receipt_must_match_authenticated_authority(self):
        cached = self.materialize()
        receipt = read_json(cached)
        receipt['operation_id'] += ':corrupt'
        write_json(cached, receipt)
        with self.assertRaises(CompilerError):
            self.materialize()

    def test_selected_pointer_keeps_relative_schema_after_durable_activation(self):
        from World.CanonicalCompilation.refinement.app import activation
        schema = ROOT / 'tools/World/CanonicalCompilation/contracts/canonical-authority.schema.json'
        original = read_json(self.active)
        original['$schema'] = Path(os.path.relpath(schema, self.active.parent)).as_posix()
        write_json(self.active, original)
        selected, candidate = self.prepare()
        with patch.object(promotion, 'canonical_active_path', return_value=self.active):
            ticket = activation.prepare_selection_transition(ROOT, self.profile_path, candidate, file_hash(self.active))
            activation.publish_selection_transition(ROOT, ticket)
        self.assertEqual(original['$schema'], read_json(self.active)['$schema'])
        self.assertEqual(schema.resolve(), (self.active.parent / selected['$schema']).resolve())

    def test_isolated_selection_retains_base_and_materializes_actual_child(self):
        base_bytes = self.active.read_bytes()
        original = self.materialize()
        selected, active = self.prepare()
        self.assertEqual(base_bytes, self.active.read_bytes())
        self.assertEqual(self.base_authority, {k: v for k, v in selected.items() if k != 'selected_refinement'})
        self.assertEqual(selected, promotion.validate_canonical_authority(ROOT, active, self.profile_path))
        effective = self.materialize(active)
        self.assertEqual('refinement_result.json', effective.name)
        self.assertNotEqual(original.parent, effective.parent)
        self.assertEqual(selected['selected_refinement']['result_sha256'], file_hash(effective))
        self.assertEqual(file_hash(original.parent / 'canonical/terrain/cell_0.json'),
            file_hash(effective.parent / 'canonical/terrain/cell_0.json'))
        self.assertEqual(effective, self.materialize(active))
        with patch('World.CanonicalCompilation.refinement.app.authority.implementation_identity',
                side_effect=AssertionError('Historical producer must not be rerun')), \
                patch('World.CanonicalCompilation.refinement.app.authority.osm_context_identity',
                    side_effect=AssertionError('Historical source admission must not be rerun')), \
                patch('World.CanonicalCompilation.refinement.app.authority.execution_identity',
                    side_effect=AssertionError('Historical native execution must not be rerun')):
            self.assertEqual(selected, promotion.validate_canonical_authority(ROOT, active, self.profile_path))

    def test_stale_or_forged_child_refuses_without_selection_or_base_mutation(self):
        _, child = self.fixture.prepare()
        before = roads.inventory(self.active.parent)
        path = child / 'canonical/terrain/cell_0.json'
        terrain = read_json(path)
        terrain['core_samples'][0][0] += 1
        write_json(path, terrain)
        result_path = child / 'refinement_result.json'
        result = read_json(result_path)
        entry = next(e for e in result['outputs'] if e['path'] == path.relative_to(child).as_posix())
        entry.update(sha256=file_hash(path), byte_size=path.stat().st_size)
        write_child(result_path, result)
        from World.CanonicalCompilation.refinement.api import prepare_surface_authority
        with self.assertRaises(CompilerError):
            prepare_surface_authority(ROOT, self.active, self.profile_path, result_path, self.root / 'forged')
        self.assertEqual(before, roads.inventory(self.active.parent))
        self.assertFalse((self.root / 'forged').exists())

    def test_child_archive_damage_and_wrong_identity_refuse(self):
        selected, active = self.prepare()
        damaged = deepcopy(selected)
        damaged['selected_refinement']['effective_identity'] = '0'*64
        write_json(active, damaged)
        with self.assertRaises(CompilerError):
            promotion.validate_canonical_authority(ROOT, active, self.profile_path)
        write_json(active, selected)
        archive = active.parent / selected['selected_refinement']['bundle']['path']
        archive.write_bytes(archive.read_bytes()[:-1])
        with self.assertRaises(CompilerError):
            self.materialize(active)

    def test_preparation_failure_leaves_prior_selection_and_active_unchanged(self):
        _, active = self.prepare()
        before = roads.inventory(self.active.parent), roads.inventory(active.parent)
        with patch('World.CanonicalCompilation.refinement.app.authority.validate_selection_bundle',
                side_effect=CompilerError('injected', 'fixture')):
            with self.assertRaises(CompilerError):
                self.prepare('failed')
        self.assertFalse((self.root / 'failed').exists())
        self.assertEqual(before, (roads.inventory(self.active.parent), roads.inventory(active.parent)))

    def test_concurrent_child_materialization_and_cached_payload_refusal(self):
        selected, active = self.prepare()
        self.cache_roots.append(ROOT / 'tmp/world/canonical_compilation/materialized' /
            selected['profile_id'] / selected['selected_refinement']['effective_identity'])
        outputs, errors = [], []

        def consume():
            try:
                outputs.append(promotion.materialize_canonical(ROOT, active, self.profile_path))
            except Exception as error:
                errors.append(error)

        workers = [threading.Thread(target=consume) for _ in range(2)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join()
        self.assertFalse(errors)
        self.assertEqual(2, len(outputs))
        self.assertEqual(outputs[0], outputs[1])
        payload = outputs[0].parent / 'refinement/context.json'
        payload.write_bytes(payload.read_bytes() + b' ')
        with self.assertRaises(CompilerError):
            self.materialize(active)

    def test_changed_external_source_and_base_replacement_do_not_drop_child(self):
        _, active = self.prepare()
        before = roads.inventory(active.parent)
        changed = {k: v for k, v in self.external.items() if k != 'source_run_inputs_hash'}
        changed['source_profile_sha256'] = 'f'*64
        with patch.object(promotion, 'current_external_inputs', return_value=changed):
            with self.assertRaises(CompilerError) as raised:
                promotion.validate_canonical_authority(ROOT, active, self.profile_path)
            self.assertEqual('canonical_external_input_changed', raised.exception.code)
        with self.assertRaises(CompilerError) as raised:
            promotion.promote_canonical(ROOT, self.profile_path, self.base / 'compile_result.json', active.parent)
        self.assertEqual('canonical_refinement_retirement_required', raised.exception.code)
        self.assertEqual(before, roads.inventory(active.parent))


if __name__ == '__main__':
    unittest.main()
