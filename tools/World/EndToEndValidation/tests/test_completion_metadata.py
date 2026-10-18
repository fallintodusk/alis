"""Metadata equivalence refuses producing drift and forged successor links."""
from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.EndToEndValidation.app.contracts import ValidationFailure as CompletionError
from World.EndToEndValidation.app.completion_metadata import ADDED,EXISTING,TESTS,repair_payloads,verify_metadata_sources
from World.EndToEndValidation.app.contracts import file_hash


class MetadataCompletionTests(unittest.TestCase):
    def test_mixed_validation_function_keeps_all_original_semantic_checks(self):
        relative='tools/World/CanonicalCompilation/app/promotion.py'
        scratch=ROOT/'tmp/world/completion_metadata_tests'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            repo=Path(directory)
            old=repo/'original.py'
            old.write_text('def _validate_authority_document(repo_root, authority, active_path, profile_path):\n'
                '    validate_document(authority, active_path)\n    return authority["inputs_hash"]\n',encoding='ascii')
            current=repo/relative
            current.parent.mkdir(parents=True)
            header='def _validate_authority_schema(repo_root: Path, authority: dict[str, Any], active_path: Path, profile_path: Path) -> None:'
            source=header+'\n    pass\n\n'
            source+=old.read_text().replace('    validate_document(authority, active_path)\n',
                '    _validate_authority_schema(repo_root, authority, active_path, profile_path)\n')
            current.write_text(source,encoding='ascii')
            for name in (ADDED|EXISTING|TESTS)-{relative}:
                path=repo/name
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text('# isolated source closure fixture\n',encoding='ascii')
            frozen={relative:file_hash(old)}
            proof={'status':'accepted','kind':'canonical_schema_reference_v1','restored_fixture':False,
                   'cold_noop':True,'source_files':{name:file_hash(repo/name) for name in ADDED|EXISTING|TESTS}}
            verify_metadata_sources(repo,frozen,{relative:file_hash(current)},{relative:old},proof)
            # Even a newly measured operation cannot excuse a changed producing check.
            current.write_text(source.replace('return authority["inputs_hash"]','return "unverified"'),encoding='ascii')
            proof['source_files'][relative]=file_hash(current)
            with self.assertRaises(CompletionError):
                verify_metadata_sources(repo,frozen,{relative:file_hash(current)},{relative:old},proof)
            for bad_header in (header.replace('profile_path: Path)','profile_path: Path, _probe=alter_runtime_inputs())'),
                               header.replace(' -> None:', ' -> alter_runtime_inputs():')):
                current.write_text(source.replace(header,bad_header),encoding='ascii')
                proof['source_files'][relative]=file_hash(current)
                with self.subTest(header=bad_header),self.assertRaises(CompletionError):
                    verify_metadata_sources(repo,frozen,{relative:file_hash(current)},{relative:old},proof)
            current.write_text(source,encoding='ascii')
            proof['source_files'][relative]=file_hash(current)
            missing=repo/next(iter(ADDED))
            missing.unlink()
            with self.assertRaises(CompletionError):
                verify_metadata_sources(repo,frozen,{relative:file_hash(current)},{relative:old},proof)
            current.write_text(source,encoding='ascii')
            proof['source_files'][relative]='0'*64
            with self.assertRaises(CompletionError):
                verify_metadata_sources(repo,frozen,{relative:file_hash(current)},{relative:old},proof)

    def fixture(self):
        old={'scopes':[{'scope_id':'layer_a','manifest_sha256':'unchanged'}],
             'transaction_id':'old','accepted_operation_id':'old','prior_active_set_sha256':'base',
             'canonical_input_compatibility':[{'candidate':{'active_sha256':'a'*64},'input_selection_sha256':'original'}]}
        new=deepcopy(old)
        new['canonical_input_compatibility'][0]['candidate']['active_sha256']='b'*64
        new.update(transaction_id='operation',accepted_operation_id='operation',prior_active_set_sha256='c'*64)
        proof={'status':'accepted','kind':'canonical_schema_reference_v1','restored_fixture':False,
               'cold_noop':True,'world_audit_status':'accepted','operation_id':'operation',
               'prior_world_sha256':'c'*64,'current_world_sha256':'d'*64,'prior_canonical_sha256':'a'*64,
               'current_canonical_sha256':'b'*64,'world_path':'Data/Manifests/active_set.json','canonical_path':'Data/Canonical/active.json'}
        proof['changed_paths']=[proof['world_path'],proof['canonical_path']]
        proof['payloads']={'prior-world.json':'c'*64,'current-world.json':'d'*64,
            'prior-canonical.json':'a'*64,'current-canonical.json':'b'*64,'transition.json':'e'*64,'world-audit.json':'f'*64}
        values={'prior-world.json':old,'current-world.json':new,'prior-canonical.json':{},'current-canonical.json':{},
                'transition.json':{'old_active_sha256':'a'*64,'new_active_sha256':'b'*64,'active_path':proof['canonical_path']},
                'world-audit.json':{'status':'accepted','active_set':{'sha256':'d'*64}}}
        class Archive:
            def read(self,path,sha):
                return deepcopy(proof if Path(path).name=='repair.json' else values[Path(path).name])
        return proof,values,Archive()

    def test_exact_successor_accepts_without_rewriting_historical_scopes(self):
        proof,values,archive=self.fixture()
        actual,documents=repair_payloads(archive,{'path':'retained/repair.json','sha256':'1'*64})
        self.assertEqual(proof,actual)
        self.assertEqual(values['prior-world.json']['scopes'],documents['current-world.json']['scopes'])

    def test_manifest_input_ancestry_extra_writes_and_prepared_receipts_refuse(self):
        for change in ('manifest','input','parent','files','prepared','restored'):
            proof,values,archive=self.fixture()
            if change=='manifest': values['current-world.json']['scopes'][0]['manifest_sha256']='forged'
            elif change=='input': values['current-world.json']['canonical_input_compatibility'][0]['input_selection_sha256']='forged'
            elif change=='parent': values['current-world.json']['prior_active_set_sha256']='forged'
            elif change=='files': proof['changed_paths'].append('Terrain.uasset')
            elif change=='prepared': proof['status']='prepared'
            else: proof['restored_fixture']=True
            with self.subTest(change=change),self.assertRaises(CompletionError):
                repair_payloads(archive,{'path':'retained/repair.json','sha256':'1'*64})


if __name__=='__main__':
    unittest.main()
