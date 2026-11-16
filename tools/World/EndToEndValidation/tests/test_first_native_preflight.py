from contextlib import ExitStack, nullcontext
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from World.EndToEndValidation.app import execution
from World.EndToEndValidation.app.contracts import ValidationFailure
from World.EndToEndValidation.app.layered_validation import validate_first_layered_receipt

ROOT=Path(__file__).resolve().parents[4]


class FirstNativePreflightTests(unittest.TestCase):
    def test_nonlayered_profiles_retain_the_existing_route(self):
        validate_first_layered_receipt({}, {})

    def test_first_metric_refusal_stops_dispatch_and_restores_original_world(self):
        parent=ROOT/'tmp/world/first_native_preflight_tests';parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root=Path(temporary);content=root/'Content'
            original=content/'Generated/First/L_Test.umap';original.parent.mkdir(parents=True)
            original.write_bytes(b'original-map')
            neighbor=content/'Generated/Protected/Sentinel.uasset';neighbor.parent.mkdir(parents=True)
            neighbor.write_bytes(b'protected-neighbor')
            absent=content/'Generated/Second/L_Test.umap'
            for name in ('preflight.json','active.json','compile.json'):(root/name).write_text('{}')
            contract={'layer_id':'terrain','generator_id':'project_mesh_terrain','generator_version':1,
                'artifact_root':'/ProjectWorldTestData/Generated/First/Terrain/','canonical_input_count':1,
                'artifact_count':1,'metrics':{'sections':1}}
            settings={'world_data_plugin':'ProjectWorldTestData','source_profile':'source','compiler_profile':'compiler.json',
                'canonical_authority':True,'presentation_profile':'presentation.json','realization_profile':'realization.json',
                'authored_overlay_profile':'authored.json','authored_overlay_sabotage_profile':'sabotage.json',
                'map_package':'/ProjectWorldTestData/Generated/First/L_Test','expected_layers':[contract]}
            profile={'profiles':{'first':settings,'second':{**settings,'map_package':'/ProjectWorldTestData/Generated/Second/L_Test'}}}
            dispatched=[]
            def run(name,command,logs):
                dispatched.append(name)
                if name!='first_unreal_first':raise AssertionError('Later World dispatch occurred before first census validation')
                path=Path(command[command.index('-EvidencePath')+1]);path.parent.mkdir(parents=True,exist_ok=True)
                inventory={**contract,'canonical_inputs':[{'unit_id':'cell','sha256':'a'*64}],
                    'artifacts':[{'path':'mesh'}],'metrics':{'sections':1,'unexpected_counter':0}}
                path.write_text(json.dumps({'status':'accepted','layer_inventories':[inventory]}))
                Path(str(path)+'.manifests.json').write_text('{}')
                original.parent.mkdir(parents=True,exist_ok=True);original.write_bytes(b'replacement-map')
                return 0
            values={'REPO_ROOT':root,'_powershell':lambda:'powershell','validate_against':lambda *a:None,
                '_world_data_roots':lambda *a:(content,'/ProjectWorldTestData'),
                '_content_mutation_lock':lambda:nullcontext(),
                '_realization_profile_contract':lambda *a:{'path':'realization.json','profile_id':'fixture','sha256':'a'*64},
                '_realization_artifact_paths':lambda *a:[],
                'canonical_active_path':lambda *a:root/'active.json',
                'validate_canonical_authority':lambda *a:{'authority_id':'fixture','inputs_hash':'a'*64,
                    'bundle':{'sha256':'b'*64},'compile_result_sha256':'c'*64},
                'materialize_canonical':lambda *a:root/'compile.json',
                '_presentation_profile_contract':lambda *a:{'path':'presentation.json'},
                '_authored_overlay_profile_contract':lambda *a:{'path':'authored.json'},
                '_authored_package_hashes':lambda *a:{},
                '_realization_evidence_path':lambda op,name:root/'evidence'/name/'native.json','_run':run}
            caught=None
            with ExitStack() as stack:
                for key,value in values.items():stack.enter_context(patch.object(execution,key,value))
                projection=stack.enter_context(patch.object(execution,'capture_saved_projections',
                    side_effect=AssertionError('Expensive projection dispatched before first census validation')))
                try:execution.execute(profile,'run-fixture',root/'result',root/'preflight.json',{},mode='immutable_canonical')
                except BaseException as error:caught=error
            self.assertEqual(b'original-map',original.read_bytes())
            self.assertFalse(absent.exists())
            self.assertEqual(b'protected-neighbor',neighbor.read_bytes())
            self.assertIsInstance(caught,ValidationFailure,str(caught))
            self.assertEqual('layer_metrics_mismatch',caught.code)
            self.assertEqual(['first_unreal_first'],dispatched)
            projection.assert_not_called()


if __name__=='__main__':unittest.main()
