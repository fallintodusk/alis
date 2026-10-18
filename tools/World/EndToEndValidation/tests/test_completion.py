import json
from pathlib import Path
import tempfile
import shutil
import unittest
from copy import deepcopy
from unittest.mock import patch

from World.EndToEndValidation.app.completion_archive import EvidenceArchive
from World.EndToEndValidation.app.completion_replay import ACCEPT,CLI,verify_file_maps
from World.EndToEndValidation.app.contracts import ValidationFailure,file_hash,validate_against
from World.EndToEndValidation.app.execution import REPO_ROOT
from World.EndToEndValidation.app.contracts import canonical_hash
from World.EndToEndValidation.app.completion import retain_completion,verify_completion_world,_target,_native


class CompletionFixture:
    def setUp(self):
        parent = REPO_ROOT/'tmp/world/completion_contract_tests'
        parent.mkdir(parents=True,exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name).resolve()
        self.assertTrue(self.repo.is_relative_to(parent.resolve()))

    def write(self,relative,text):
        path = self.repo/relative
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(text,encoding='utf-8')
        return path


class CompletionReplayTests(CompletionFixture,unittest.TestCase):
    def acceptance(self):
        old = self.write('proof/original.py', 'from __future__ import annotations\ndef produce(): return 1\ndef accept(value="base"): return 1\n')
        self.write(ACCEPT, 'from __future__ import annotations\ndef produce(): return 1\ndef accept(value="base", *, completion_evidence: str | None = None): return 2\n')
        return old

    def test_final_reader_body_preserves_executed_import_boundary(self):
        old = self.acceptance()
        before,current = {ACCEPT:file_hash(old)},{ACCEPT:file_hash(self.repo/ACCEPT)}
        self.assertEqual([ACCEPT],verify_file_maps(self.repo,before,current,{ACCEPT:old}))
        for text in (
            (self.repo/ACCEPT).read_text().replace('value="base"','value=produce()'),
            (self.repo/ACCEPT).read_text().replace('def accept','@produce()\ndef accept'),
            (self.repo/ACCEPT).read_text().replace('def produce(): return 1','def produce(): return 99'),
            (self.repo/ACCEPT).read_text().replace('completion_evidence: str | None = None','completion_evidence: str | None = produce()'),
        ):
            with self.subTest(text=text),self.assertRaises(ValidationFailure):
                self.write(ACCEPT,text)
                verify_file_maps(self.repo,before,{ACCEPT:file_hash(self.repo/ACCEPT)},{ACCEPT:old})

    def test_cli_hook_cannot_change_existing_producing_route(self):
        original = ('def _parser():\n    accept.add_argument("--run")\n'
                    'def main(args):\n    if args.operation == "run": return produce(args.profile)\n'
                    '    outcome = accept(args.run,args.profile,args.bootstrap_preflight)\n')
        modified = original.replace('    accept.add_argument("--run")',
            '    accept.add_argument("--run")\n    accept.add_argument("--completion-evidence")').replace(
            'args.bootstrap_preflight)','args.bootstrap_preflight, completion_evidence=args.completion_evidence)')
        old = self.write('proof/cli.py',original)
        self.write(CLI,modified)
        before = {CLI:file_hash(old)}
        self.assertEqual([CLI],verify_file_maps(self.repo,before,{CLI:file_hash(self.repo/CLI)},{CLI:old}))
        self.write(CLI,modified.replace('produce(args.profile)','produce("different")'))
        with self.assertRaises(ValidationFailure):
            verify_file_maps(self.repo,before,{CLI:file_hash(self.repo/CLI)},{CLI:old})

    def test_native_host_data_and_unknown_files_require_new_proof(self):
        for relative in ('scripts/ue/world/realize_canonical_world.ps1',
                         'Plugins/World/ProjectWorld/Source/Road.cpp',
                         'tools/World/EndToEndValidation/app/refinement_target.py',
                         'Plugins/World/ProjectWorldData/Data/Profiles/Budgets/city.json'):
            with self.subTest(relative=relative),self.assertRaises(ValidationFailure):
                verify_file_maps(self.repo,{relative:'a'*64},{relative:'b'*64},{})
        with self.assertRaises(ValidationFailure):
            verify_file_maps(self.repo,{}, {'tools/World/EndToEndValidation/app/new_producer.py':'a'*64},{})

    def test_frozen_entrypoint_source_must_match_recorded_input(self):
        old = self.acceptance()
        before = {ACCEPT:file_hash(old)}
        old.write_text('def accept(): pass\n')
        with self.assertRaises(ValidationFailure):
            verify_file_maps(self.repo,before,{ACCEPT:file_hash(self.repo/ACCEPT)},{ACCEPT:old})

    def test_retained_receipt_survives_original_path_removal(self):
        source = self.write('tmp/old/native.json',json.dumps({'status':'accepted','input':'a'*64}))
        expected = file_hash(source)
        archive = EvidenceArchive(self.repo,self.repo/'Saved/Validation/WorldAuthority/completion-test')
        archive.add(str(source),expected)
        source.unlink()
        self.assertEqual('a'*64,archive.read(str(source),expected)['input'])
        with self.assertRaises(ValidationFailure):
            archive.read('tmp/unknown/native.json',expected)
        archive.path(str(source),expected).write_text('{}')
        with self.assertRaises(ValidationFailure):
            archive.read(str(source),expected)

    def test_bad_pin_refuses_before_retention_writes(self):
        source = self.write('tmp/old/native.json','{}')
        target = self.repo/'Saved/Validation/WorldAuthority/completion-test'
        archive = EvidenceArchive(self.repo,target)
        with self.assertRaises(ValidationFailure):
            archive.add(str(source),'a'*64)
        self.assertFalse(target.exists())
        self.assertEqual('{}',source.read_text())

    def test_retained_execution_source_is_opaque_verified_bytes(self):
        source = self.write('proof/original.py','def accepted_producer(): return 1\n')
        expected = file_hash(source)
        archive = EvidenceArchive(self.repo,self.repo/'Saved/Validation/WorldAuthority/completion-test')
        archive.add(str(source),expected)
        self.assertEqual(source.read_bytes(),archive.path(str(source),expected).read_bytes())

    def test_retention_keeps_the_executed_road_locality_receipt(self):
        def pin(relative,document):
            source = self.write(relative,json.dumps(document))
            return {'path':relative,'sha256':file_hash(source)}
        active = pin('proof/active.json',{'scopes':[]})
        audit = pin('proof/audit.json',{'manifest_root':'state/manifests'})
        native = pin('proof/native.json',{})
        common = pin('proof/check.json',{})
        legs = {}
        for key in ('unreal_first','unreal_second','unreal_road_locality','unreal_rejected_apply','unreal_clean_rebuild'):
            receipt = pin('tmp/executed/'+key+'.json',{'native_leg':key})
            legs.update({key:receipt['path'],key+'_sha256':receipt['sha256']})
        matrix = pin('proof/matrix.json',{'evidence':{'profiles':{'city':legs},
            'common_checks':{'receipt':common['path'],'receipt_sha256':common['sha256']}}})
        selection = pin('tmp/selection.json',{})
        target_request = pin('tmp/target/request.json',{'selection_path':selection['path'],'selection_sha256':selection['sha256']})
        transport = pin('tmp/target/transport.json',{'request_sha256':target_request['sha256'],'receipts':{}})
        target = pin('proof/target.json',{'evidence':{'transport_path':transport['path'],
            'transport_sha256':transport['sha256'],'request':json.loads((self.repo/target_request['path']).read_text())}})
        frozen = pin('proof/inputs.json',{'base_authority_copies':{}})
        original = pin('proof/original.py',{'opaque':'retained source'})
        path = retain_completion(self.repo,self.repo/'Saved/Validation/WorldAuthority/retained-locality',{
            'schema_version':1,'matrix':matrix,'target':target,'execution_inputs':frozen,
            'original_sources':{ACCEPT:original,CLI:original},
            'rollout':{'prior_active':active,'active':active,'native':native,'audit':audit}})
        document = json.loads(path.read_text())
        archive = EvidenceArchive(self.repo,path.parent,document['retained_evidence'],document['execution_root'])
        locality = 'tmp/executed/unreal_road_locality.json'
        (self.repo/locality).unlink()
        self.assertEqual('unreal_road_locality',archive.read(locality,legs['unreal_road_locality_sha256'])['native_leg'])

    def test_original_absolute_labels_are_not_live_filesystem_dependencies(self):
        source = self.write('tmp/old/receipt.json','{"accepted":true}')
        expected = file_hash(source)
        archive = EvidenceArchive(self.repo,self.repo/'Saved/Validation/WorldAuthority/completion-test')
        archive.add(str(source),expected)
        relocated = self.repo/'relocated'
        destination = relocated/'Saved/Validation/WorldAuthority/completion-test'
        shutil.copytree(archive.root,destination)
        reader = EvidenceArchive(relocated,destination,archive.entries,execution_root=str(self.repo))
        source.unlink()
        self.assertTrue(reader.read(str(source),expected)['accepted'])

    def test_completion_contract_is_closed_and_requires_retained_request_link(self):
        pin = {'path':'Saved/proof.json','sha256':'a'*64}
        request = {'schema_version':1,'matrix':pin,'target':pin,'execution_inputs':pin,
            'original_sources':{ACCEPT:pin,CLI:pin},'rollout':{name:pin for name in ('prior_active','active','native','audit')}}
        validate_against(request,'completion-evidence.schema.json')
        for modified in ({**request,'skip_gate':True},{**request,'retained_evidence':{'Saved/proof.json':'a'*64}}):
            with self.subTest(modified=modified),self.assertRaises(ValidationFailure):
                validate_against(modified,'completion-evidence.schema.json')


class CompletionTargetTests(CompletionFixture,unittest.TestCase):
    def test_unknown_counter_is_not_a_measured_zero(self):
        native = {'status':'accepted','input_sha256':'c'*64,'execution_layer_ids':['roads'],
                  'layer_inventories':[{'final_dirty_units':[]}],'changes':{'updated_actors':0}}
        _native(native,{'result_sha256':'c'*64},['roads'],noop=True)
        native['changes']['updated_actors']=False
        with self.assertRaises(ValidationFailure): _native(native,{'result_sha256':'c'*64},['roads'],noop=True)

    def fixture(self,mutate=None):
        archive = EvidenceArchive(self.repo,self.repo/'Saved/Validation/WorldAuthority/completion-target')
        originals = []
        def pin(name,value):
            path = self.write('tmp/original/'+name+'.json',json.dumps(value))
            originals.append(path)
            result = {'path':str(path),'sha256':file_hash(path)}
            archive.add(**result)
            return result
        operations = {k:False for k in ('source_acquisition','base_compilation','canonical_promotion')}
        baseline = {key:'a'*64 for key in ('active_sha256','bundle_sha256','compile_result_sha256','run_contract_sha256',
            'historical_producer_implementation_sha256','historical_source_inputs_hash','historical_execution_identity','inputs_hash')}
        baseline.update(authority_id='fixture',active_path='authority/active.json',geographic_input_files={'source.json':'a'*64})
        base = {'$schema':'https://alis.world/schemas/world-validation/immutable-canonical-selection-v1.json',
            'schema_version':1,'mode':'immutable_canonical','changed_base':'a'*40,'profiles':{'city':baseline},
            'current_consumer_implementation_sha256':'a'*64,'executed_upstream_operations':operations,
            'validation_profile_path':'validation.json','validation_profile_sha256':'a'*64,'base_validation_profile_sha256':'a'*64}
        candidate = {key:'b'*64 for key in ('active_sha256','effective_identity','inputs_hash','bundle_sha256',
            'implementation_sha256','source_context_identity','execution_identity')}
        candidate.update(active_path='tmp/child/active.json',base_authority_id='fixture',base_result_sha256='a'*64,
                         result_sha256='c'*64,materialized_result_path='tmp/child/result.json')
        selection = pin('selection',{'$schema':'https://alis.world/schemas/world-validation/refinement-candidate-selection-v1.json',
            'schema_version':1,'mode':'refinement_candidate','base_selection':base,'candidates':{'city':candidate},
            'executed_upstream_operations':operations})
        request = {'selection_path':selection['path'],'selection_sha256':selection['sha256'],
                   'execute_layers':['roads','water','vegetation']}
        native = {'status':'accepted','input_sha256':'c'*64,'execution_layer_ids':request['execute_layers'],
                  'layer_inventories':[{'final_dirty_units':[]}],'changes':{'updated_actors':0}}
        receipts = {k:pin(k,native) for k in ('failure_native','saved_native','noop_native')}
        receipts['saved_active'] = pin('saved_active',{'canonical_input_compatibility':[{
            'candidate':{k:v for k,v in candidate.items() if k not in ('active_path','materialized_result_path')},
            'input_selection_sha256':selection['sha256']}]})
        world = receipts['saved_active']['sha256']
        receipts['saved_audit'] = pin('saved_audit',{'status':'accepted','active_set':{'sha256':world}})
        ticket = {'old_active_sha256':'a'*64,'new_active_sha256':'b'*64,'effective_result_sha256':'c'*64}
        receipts['failure_staging'] = pin('staging',{'pending':'World'})
        receipts['failure_journal'] = pin('journal',{'phase':'publishing','canonical_selection_transition':ticket,
            'prior_active_set_sha256':'d'*64,'expected_active_set_sha256':receipts['failure_staging']['sha256']})
        report = {'status':'accepted','transition':ticket,'receipts':receipts,
            'failure_recovery':{'status':'accepted','host_exit':1,'phase':'publishing','canonical_state':'selected','world_sha256':'d'*64},
            'saved_publication':{'status':'accepted','world_sha256':world,'canonical_sha256':'b'*64},
            'cold_noop':{'status':'accepted','world_sha256':world,'canonical_sha256':'b'*64},
            'restoration':{'status':'accepted','before_inventory_sha256':'e'*64,'after_inventory_sha256':'e'*64,
                'before_world_sha256':'d'*64,'after_world_sha256':'d'*64,'canonical_state':'prior'}}
        if mutate: mutate(report)
        transport = pin('transport',report)
        target = {'status':'accepted','evidence':{'matrix_mode':'refinement_selected',
            'scope':'selected_child_target_not_full_matrix','request':request,
            'transport_path':transport['path'],'transport_sha256':transport['sha256']}}
        return archive,target,originals

    def test_frozen_target_reads_its_exact_retained_closure_without_originals(self):
        archive,target,originals = self.fixture()
        self.assertEqual('city',_target(archive,target)[0])
        for path in originals: path.unlink()
        self.assertEqual('city',_target(archive,target)[0])
        receipt = next(iter(archive.entries))
        archive.path(receipt,archive.entries[receipt]).write_text('{}')
        with self.assertRaises(ValidationFailure): _target(archive,target)

    def test_status_only_target_cannot_replace_raw_native_proof(self):
        archive,target,_ = self.fixture(lambda report:report['receipts'].pop('saved_native'))
        with self.assertRaises(ValidationFailure): _target(archive,target)

    def test_paired_failure_must_reach_the_actual_saved_boundary(self):
        archive,target,_ = self.fixture(lambda report:report['failure_recovery'].update(phase='prepared'))
        with self.assertRaises(ValidationFailure): _target(archive,target)


class CompletionScopeTests(CompletionFixture,unittest.TestCase):
    def fixture(self,mutate=None):
        archive = EvidenceArchive(self.repo,self.repo/'Saved/Validation/WorldAuthority/completion-world')
        owner = 'state/manifests'
        def pin(relative,document):
            path = self.write(relative,json.dumps(document))
            value = {'path':relative,'sha256':file_hash(path)}
            archive.add(**value)
            return value
        base = {'compile_result_sha256':'a'*64,'presentation_profile_sha256':'b'*64,
                'runtime_profile_sha256':'d'*64,'authored_overlay_profile_sha256':'e'*64}
        candidate = {'result_sha256':'c'*64,'active_sha256':'f'*64}
        before,after,scopes,layers = [],[],[],{}
        for layer in ('map','terrain','roads','water','vegetation'):
            contract = {'canonical_inputs':[{'unit_id':'cell','sha256':'3'*64}],'dependency_inputs':[]}
            identity = {**base,'map_package':'/ProjectWorldData/City'}
            if layer!='map': identity['runtime_profile_sha256']='none'
            old = pin(owner+'/'+layer+'.1.json',{'input_identity':identity,'layer_contract':contract})
            prior_entry = {'scope_id':layer,'manifest_path':layer+'.1.json','manifest_sha256':old['sha256']}
            before.append(prior_entry)
            current_identity = deepcopy(identity)
            if layer in ('roads','water'):
                current_identity['compile_result_sha256']=candidate['result_sha256']
                new = pin(owner+'/'+layer+'.2.json',{'input_identity':current_identity,'layer_contract':contract})
                after.append({'scope_id':layer,'manifest_path':layer+'.2.json','manifest_sha256':new['sha256']})
            else:
                after.append(deepcopy(prior_entry))
            scopes.append({'scope_id':layer,'manifest_sha256':after[-1]['manifest_sha256'],
                'input_identity':current_identity,'owning_layer':layer,'generator_fingerprint':'1'*64})
            if layer!='map':
                layers[layer] = {'layer_id':layer,'scope_id':layer,'normalized_layer_contract_sha256':'2'*64,
                    'generator_id':'project_'+layer,'generator_version':1,'generator_fingerprint':'1'*64,
                    'artifact_root':'/ProjectWorldData/'+layer,'canonical_inputs':[{'unit_id':'cell','sha256':'3'*64}],
                    'dependency_inputs':[],'final_dirty_units':['cell'] if layer in ('roads','water') else []}
        other_base = {**base,'compile_result_sha256':'9'*64}
        for key,layer in (('other_map','map'),('other_terrain','terrain'),('presentation','presentation')):
            identity = {**other_base,'map_package':'/ProjectWorldData/OtherCity'}
            if layer!='map': identity['runtime_profile_sha256']='none'
            original = pin(owner+'/'+key+'.1.json',{'input_identity':identity})
            entry = {'scope_id':key,'manifest_path':key+'.1.json','manifest_sha256':original['sha256']}
            before.append(entry)
            after.append(deepcopy(entry))
            scopes.append({'scope_id':key,'manifest_sha256':entry['manifest_sha256'],'input_identity':identity,
                'owning_layer':layer,'generator_fingerprint':'1'*64,
                'consumer_references':['map','other_map'] if layer=='presentation' else []})
        prior = pin('proof/prior-active.json',{'scopes':before})
        binding = {'candidate':candidate,'input_selection_sha256':'4'*64,
                   'prior_active_set_sha256':prior['sha256'],'selected_scope_ids':['roads','water','vegetation'],
                   'layers':{key:{'accepted':{'accepted_manifest_sha256':before[index+1]['manifest_sha256'],
                       'accepted_generator_fingerprint':'1'*64},
                       'after_canonical_inputs_sha256':canonical_hash(row['canonical_inputs']),
                       'after_dependency_inputs_sha256':canonical_hash(row['dependency_inputs'])}
                       for index,(key,row) in enumerate(layers.items())}}
        saved = {'scopes':after,'canonical_input_compatibility':[binding]}
        native = {'status':'accepted','input_sha256':candidate['result_sha256'],
            'semantic_fingerprint':'7'*64,
            'execution_layer_ids':['roads','water','vegetation'],'layer_inventories':list(layers.values()),
            'map_package':'/ProjectWorldData/City','world_data_plugin':'ProjectWorldData',
            'presentation_profile_sha256':'b'*64,'runtime_profile_sha256':'d'*64,
            'authored_overlay_set_sha256':'e'*64,'realization_profile_sha256':'5'*64,'layer_dirty_input_sha256':'6'*64}
        target_native = deepcopy(native)
        if mutate: mutate(saved,native,scopes)
        active = pin('state/active.json',saved)
        audit = {'status':'accepted','manifest_root':owner,'active_set':active,'scopes':scopes}
        audit_pin = pin('proof/audit.json',audit)
        native_pin = pin('proof/native.json',native)
        completion = {'archive':archive,'document':{'rollout':{
            'prior_active':prior,'active':active,'audit':audit_pin,'native':native_pin}},
            'candidate':candidate,'request':{'execute_layers':['roads','water','vegetation'],'selection_sha256':'4'*64},
            'target_receipts':{'saved_native':target_native},'target':{'transition':{}},
            'records':{'/ProjectWorldData/City':base,'/ProjectWorldData/OtherCity':other_base}}
        return completion,audit

    def verify(self,completion,audit):
        with patch('World.EndToEndValidation.app.completion.validate_selection_transition',return_value={'state':'selected'}):
            return verify_completion_world(self.repo,completion,audit)

    def test_mixed_base_and_child_scopes_are_independently_bound(self):
        completion,audit = self.fixture()
        from World.EndToEndValidation.app.acceptance import verify_manifest_provenance
        self.assertTrue(verify_manifest_provenance(audit['scopes'],completion['records']))
        original = deepcopy(audit)
        result = self.verify(completion,audit)
        self.assertEqual(['roads','water'],result['changed_scope_ids'])
        self.assertEqual(['map','other_map','other_terrain','presentation','terrain','vegetation'],result['carried_scope_ids'])
        self.assertEqual(original,audit)

    def test_mixed_scope_reader_survives_relocation_without_original_manifests(self):
        completion,audit = self.fixture()
        archive = completion['archive']
        origin = self.repo
        relocated = origin/'moved'
        destination = relocated/'Saved/Validation/WorldAuthority/completion-world'
        shutil.copytree(archive.root,destination)
        shutil.copytree(origin/'state',relocated/'state')
        for path in (origin/'state',origin/'proof',archive.root):
            shutil.rmtree(path)
        self.repo = relocated
        completion['archive'] = EvidenceArchive(relocated,destination,archive.entries,execution_root=str(origin))
        result = self.verify(completion,audit)
        self.assertEqual(['roads','water'],result['changed_scope_ids'])

    def test_unselected_terrain_and_consumed_input_drift_refuse(self):
        def drift(saved,native,scopes):
            native['layer_inventories'][0]['canonical_inputs'][0]['sha256']='9'*64
        completion,audit = self.fixture(drift)
        with self.assertRaises(ValidationFailure): self.verify(completion,audit)

    def test_canonical_selection_and_live_world_drift_refuse(self):
        completion,audit = self.fixture()
        with patch('World.EndToEndValidation.app.completion.validate_selection_transition',return_value={'state':'prior'}):
            with self.assertRaises(ValidationFailure): verify_completion_world(self.repo,completion,audit)
        self.write('state/active.json','{}')
        with self.assertRaises(ValidationFailure): self.verify(completion,audit)

    def test_unselected_scope_update_cannot_borrow_selected_authority(self):
        def change(saved,native,scopes):
            saved['scopes'][1]['manifest_path']='terrain.2.json'
        completion,audit = self.fixture(change)
        with self.assertRaises(ValidationFailure): self.verify(completion,audit)

    def test_wrong_target_city_is_not_a_valid_combination(self):
        def change(saved,native,scopes):
            native['map_package']='/ProjectWorldData/OtherCity'
        completion,audit = self.fixture(change)
        with self.assertRaises(ValidationFailure): self.verify(completion,audit)


if __name__=='__main__':
    unittest.main()
