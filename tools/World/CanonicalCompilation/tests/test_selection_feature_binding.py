"""A newly resealed child is not evidence that its producer consumed the claimed feature."""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.tests import test_refinement_authority as authority_fixture
from World.CanonicalCompilation.app.contracts import CompilerError, canonical_hash, file_hash, read_json, write_json
from World.CanonicalCompilation.refinement.app import activation, authority, identity
from World.CanonicalCompilation.refinement.app.contracts import write_json as write_child
from World.CanonicalCompilation.app import promotion
from World.EndToEndValidation.app import refinement_selection
from World.EndToEndValidation.app.contracts import ValidationFailure


class SelectionFeatureBindingTests(unittest.TestCase):
    def setUp(self):
        self.fixture = authority_fixture.RefinementAuthorityTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.original, self.candidate = self.fixture.prepare()
        self.prior = self.fixture.active.read_bytes()

    def reseal(self, field):
        candidate = deepcopy(self.original)
        selected = candidate['selected_refinement']
        archive = self.candidate.parent / selected['bundle']['path']
        staged = self.fixture.root / 'resealed'
        staged.mkdir()
        with zipfile.ZipFile(archive) as source:
            for item in source.namelist():
                target = staged / item
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.read(item))
        shard_path = staged / 'canonical/features/cell_0.json'
        original_feature_digest = file_hash(shard_path)
        shard = read_json(shard_path)
        road = next(f for f in shard['features'] if f['feature_class'] == 'road')
        profile = road['attributes']['road_elevation_profile']
        if field == 'base_feature_sha256':
            profile[field] = 'f' * 64
        else:
            profile['surface'][field] = 'e' * 64
        write_json(shard_path, shard)
        manifest_path = staged / 'canonical/cells/cell_0.json'
        manifest = read_json(manifest_path)
        manifest['feature_artifact'].update(content_hash=file_hash(shard_path), byte_size=shard_path.stat().st_size)
        write_json(manifest_path, manifest)
        coverage_path = staged / 'canonical/coverage.json'
        coverage = read_json(coverage_path)
        for entry in coverage['cells']:
            if entry['path'] == 'canonical/cells/cell_0.json':
                entry.update(sha256=file_hash(manifest_path), byte_size=manifest_path.stat().st_size)
        for entry in coverage['artifact_descriptors']:
            if entry.get('content_hash') == original_feature_digest:
                entry.update(content_hash=file_hash(shard_path), byte_size=shard_path.stat().st_size)
        write_json(coverage_path, coverage)
        result_path = staged / 'refinement_result.json'
        result = read_json(result_path)
        for entry in result['outputs']:
            path = staged / entry['path']
            entry.update(sha256=file_hash(path), byte_size=path.stat().st_size)
        write_child(result_path, result)
        selected['result_sha256'] = file_hash(result_path)
        address = canonical_hash({key: selected[key] for key in ('base_result_sha256', 'inputs_hash', 'result_sha256')})
        relative = f'refinements/{address}.zip'
        target = self.candidate.parent / relative
        promotion._write_deterministic_bundle(target, [(e['path'], staged / e['path']) for e in result['outputs']],
                                             result, result_name='refinement_result.json')
        selected['bundle'].update(path=relative, sha256=file_hash(target), byte_size=target.stat().st_size)
        selected['effective_identity'] = authority._selection_identity(candidate, selected)
        write_json(self.candidate, candidate)
        return self.fixture.active.parent / relative

    def assert_first_selection_refuses_resealed_feature(self, field):
        installed = self.reseal(field)
        # Frozen integrity is deliberately distinct from fresh producer admission.
        promotion.validate_canonical_authority(ROOT, self.candidate, self.fixture.profile_path)
        with self.assertRaises(CompilerError):
            activation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate,
                                                   file_hash(self.fixture.active))
        self.assertFalse(installed.exists(), 'Unproven child must refuse before immutable installation')
        self.assertEqual(self.prior, self.fixture.active.read_bytes())

    def test_original_feature_digest_is_bound_at_first_selection(self):
        self.assert_first_selection_refuses_resealed_feature('base_feature_sha256')

    def test_modeled_consumed_feature_digest_is_bound_at_first_selection(self):
        self.assert_first_selection_refuses_resealed_feature('consumed_feature_sha256')

    def test_resealed_direct_publish_is_refused_without_changing_the_pointer(self):
        installed = self.reseal('consumed_feature_sha256')
        selected = read_json(self.candidate)['selected_refinement']
        activation._install(ROOT, self.candidate.parent / selected['bundle']['path'], installed, selected['bundle'])
        transition = activation._transition(ROOT, self.fixture.profile_path, self.fixture.active,
            self.prior, self.candidate.read_bytes(), self.fixture.base_authority, selected)
        with self.assertRaises(CompilerError):
            activation.publish_selection_transition(ROOT, transition)
        self.assertEqual(self.prior, self.fixture.active.read_bytes())

    def test_resealed_e2e_candidate_is_refused_without_changing_the_pointer(self):
        self.reseal('base_feature_sha256')
        baseline = {'active_path': self.fixture.active.relative_to(ROOT).as_posix(),
            'active_sha256': file_hash(self.fixture.active),
            'compile_result_sha256': self.fixture.base_authority['compile_result_sha256'],
            'authority_id': self.fixture.base_authority['authority_id']}
        settings = {'compiler_profile_path': self.fixture.profile_path.relative_to(ROOT).as_posix()}
        with self.assertRaises(ValidationFailure):
            refinement_selection._candidate(ROOT, settings, baseline, self.candidate)
        self.assertEqual(self.prior, self.fixture.active.read_bytes())

    def test_base_archive_changed_during_replay_refuses_before_installation(self):
        descriptor = self.original['bundle']
        base_zip = self.candidate.parent / descriptor['path']
        replay = authority.prepare_surface_refinement
        changed = []

        def replace_comment(*arguments):
            result = replay(*arguments)
            with zipfile.ZipFile(base_zip, 'a') as bundle:
                bundle.comment = b'changed while replaying'
            changed.append(file_hash(base_zip))
            return result

        with patch.object(authority, 'prepare_surface_refinement', side_effect=replace_comment), self.assertRaises(CompilerError):
            activation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate, file_hash(self.fixture.active))
        self.assertEqual(1, len(changed))
        self.assertEqual(changed[0], file_hash(base_zip))
        self.assertEqual(self.prior, self.fixture.active.read_bytes())
        self.assertFalse((self.fixture.active.parent / self.original['selected_refinement']['bundle']['path']).exists())

    def test_current_producer_drift_during_replay_refuses_before_installation(self):
        current = identity.implementation_identity()
        with patch.object(identity, 'implementation_identity', side_effect=[current, '0'*64]), self.assertRaises(CompilerError):
            activation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate, file_hash(self.fixture.active))
        self.assertEqual(self.prior, self.fixture.active.read_bytes())
        self.assertFalse((self.fixture.active.parent / self.original['selected_refinement']['bundle']['path']).exists())

    def test_selected_noop_and_restore_do_not_replay_producers(self):
        transition = activation.prepare_selection_transition(ROOT, self.fixture.profile_path, self.candidate, file_hash(self.fixture.active))
        activation.publish_selection_transition(ROOT, transition)
        with patch.object(authority, 'prepare_surface_refinement', side_effect=AssertionError('Frozen operation must not produce')):
            self.assertFalse(activation.publish_selection_transition(ROOT, transition)['changed'])
            self.assertTrue(activation.publish_selection_transition(ROOT, transition, restore=True)['changed'])
            self.assertFalse(activation.publish_selection_transition(ROOT, transition, restore=True)['changed'])
        self.assertEqual(self.prior, self.fixture.active.read_bytes())
