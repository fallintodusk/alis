"""A schema annotation repair cannot change geographic authority or evade admission."""
import base64
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
from World.CanonicalCompilation.tests import test_refinement_authority as fixtures
from World.CanonicalCompilation.refinement.app import activation, schema_reference
from World.CanonicalCompilation.app.contracts import CompilerError, file_hash
from World.CanonicalCompilation.app import promotion


class SchemaReferenceTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.RefinementAuthorityTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        _, candidate = self.fixture.prepare()
        self.original = activation.prepare_selection_transition(ROOT, self.fixture.profile_path,
            candidate, file_hash(self.fixture.active))
        activation.publish_selection_transition(ROOT, self.original)
        # Reproduce the frozen pointer emitted by the old writer, without fresh admission.
        historical = json.loads(candidate.read_bytes())
        historical['$schema'] = 'https://alis.world/schemas/world-compiler/canonical-authority-v1.json'
        payload = (json.dumps(historical, indent=2)+'\n').encode('ascii')
        self.fixture.active.write_bytes(payload)
        self.original['new_active_bytes_base64'] = base64.b64encode(payload).decode('ascii')
        self.original['new_active_sha256'] = hashlib.sha256(payload).hexdigest()
        self.before = self.fixture.active.read_bytes()

    def prepare(self):
        return schema_reference.prepare_schema_reference_transition(ROOT,
            self.fixture.profile_path, file_hash(self.fixture.active))

    def test_schema_only_save_repeat_restore_and_exact_historical_handoff(self):
        with patch.object(activation, '_current_producer', side_effect=AssertionError('No new geometry')):
            ticket = self.prepare()
            self.assertEqual(self.before, self.fixture.active.read_bytes())
            self.assertTrue(activation.publish_selection_transition(ROOT, ticket)['changed'])
            actual = json.loads(self.fixture.active.read_bytes())
            prior = json.loads(self.before)
            self.assertEqual({k:v for k,v in prior.items() if k != '$schema'},
                             {k:v for k,v in actual.items() if k != '$schema'})
            schema = ROOT / 'tools/World/CanonicalCompilation/contracts/canonical-authority.schema.json'
            self.assertEqual(schema.resolve(), (self.fixture.active.parent / actual['$schema']).resolve())
            self.assertFalse(activation.publish_selection_transition(ROOT, ticket)['changed'])
            link = schema_reference.validate_schema_reference_handoff(ROOT, self.original, ticket)
            self.assertEqual(file_hash(self.fixture.active), link['active_sha256'])
            self.assertEqual(self.original['new_active_sha256'], link['historical_selected_sha256'])
            activation.publish_selection_transition(ROOT, ticket, restore=True)
            self.assertEqual(self.before, self.fixture.active.read_bytes())
            self.assertFalse(activation.publish_selection_transition(ROOT, ticket, restore=True)['changed'])

    def test_stale_prior_unknown_current_and_base_only_refuse_unchanged(self):
        with self.assertRaises(CompilerError):
            schema_reference.prepare_schema_reference_transition(ROOT, self.fixture.profile_path, '0'*64)
        ticket = self.prepare()
        self.fixture.active.write_bytes(self.before+b' ')
        foreign = self.fixture.active.read_bytes()
        for restore in (False, True):
            with self.assertRaises(CompilerError):
                activation.publish_selection_transition(ROOT, ticket, restore=restore)
            self.assertEqual(foreign, self.fixture.active.read_bytes())
        self.fixture.active.write_bytes(self.before)
        activation.publish_selection_transition(ROOT, self.original, restore=True)
        base = self.fixture.active.read_bytes()
        with self.assertRaises(CompilerError):
            self.prepare()
        self.assertEqual(base, self.fixture.active.read_bytes())

    def test_forged_metadata_repair_cannot_relabel_semantics_or_schema(self):
        ticket = self.prepare()
        for field, value in (('output_count', 999), ('$schema', '../unknown.schema.json')):
            forged = deepcopy(ticket)
            document = json.loads(base64.b64decode(forged['new_active_bytes_base64']))
            document[field] = value
            payload = (json.dumps(document)+'\n').encode('ascii')
            forged['new_active_bytes_base64'] = base64.b64encode(payload).decode('ascii')
            forged['new_active_sha256'] = hashlib.sha256(payload).hexdigest()
            with self.subTest(field=field), self.assertRaises(CompilerError):
                activation.publish_selection_transition(ROOT, forged)
            self.assertEqual(self.before, self.fixture.active.read_bytes())
        normal = {k:v for k,v in ticket.items() if k != 'transition_kind'}
        with self.assertRaises(CompilerError):
            activation.publish_selection_transition(ROOT, normal)
        self.assertEqual(self.before, self.fixture.active.read_bytes())

    def test_changed_external_source_and_broken_bundle_still_refuse(self):
        ticket = self.prepare()
        external = {k:v for k,v in self.fixture.external.items() if k != 'source_run_inputs_hash'}
        external['source_profile_sha256'] = 'f'*64
        with patch.object(promotion, 'current_external_inputs', return_value=external):
            for action in (self.prepare, lambda: activation.publish_selection_transition(ROOT, ticket)):
                with self.assertRaises(CompilerError):
                    action()
        child = self.fixture.active.parent / ticket['bundle']['path']
        child.write_bytes(child.read_bytes()[:-1])
        with self.assertRaises(CompilerError):
            activation.publish_selection_transition(ROOT, ticket)
        self.assertEqual(self.before, self.fixture.active.read_bytes())

    def test_replace_failure_preserves_exact_prior_and_allows_retry(self):
        ticket = self.prepare()
        with patch.object(activation.os, 'replace', side_effect=OSError('injected')), self.assertRaises(CompilerError):
            activation.publish_selection_transition(ROOT, ticket)
        self.assertEqual(self.before, self.fixture.active.read_bytes())
        activation.publish_selection_transition(ROOT, ticket)
        with self.assertRaises(CompilerError):
            self.prepare()

    def test_handoff_rejects_original_with_different_selected_bytes(self):
        ticket = self.prepare()
        activation.publish_selection_transition(ROOT, ticket)
        forged = deepcopy(self.original)
        payload = base64.b64decode(forged['new_active_bytes_base64']) + b' '
        forged['new_active_bytes_base64'] = base64.b64encode(payload).decode('ascii')
        forged['new_active_sha256'] = hashlib.sha256(payload).hexdigest()
        with self.assertRaises(CompilerError):
            schema_reference.validate_schema_reference_handoff(ROOT, forged, ticket)

    def test_unknown_schema_cannot_select_an_arbitrary_validation_contract(self):
        unknown = json.loads(self.before)
        unknown['$schema'] = 'https://example.invalid/permissive.json'
        self.fixture.active.write_text(json.dumps(unknown), encoding='ascii')
        before = self.fixture.active.read_bytes()
        with self.assertRaises(CompilerError):
            self.prepare()
        self.assertEqual(before, self.fixture.active.read_bytes())

    def test_fresh_url_admission_and_forged_mutating_publish_refuse_without_writes(self):
        activation.publish_selection_transition(ROOT, self.original, restore=True)
        base = self.fixture.active.read_bytes()
        candidate = self.fixture.root / 'selected/active.json'
        document = json.loads(candidate.read_bytes())
        document['$schema'] = 'https://alis.world/schemas/world-compiler/canonical-authority-v1.json'
        candidate.write_text(json.dumps(document)+'\n', encoding='ascii')
        target = self.fixture.active.parent / self.original['bundle']['path']
        payload = target.read_bytes()
        target.unlink()
        with self.assertRaises(CompilerError):
            activation.prepare_selection_transition(ROOT, self.fixture.profile_path, candidate, file_hash(self.fixture.active))
        self.assertFalse(target.exists())
        target.write_bytes(payload)
        with self.assertRaises(CompilerError):
            activation.publish_selection_transition(ROOT, self.original)
        self.assertEqual(base, self.fixture.active.read_bytes())


if __name__ == '__main__':
    unittest.main()
