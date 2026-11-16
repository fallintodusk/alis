import copy
import hashlib
import json
import shutil
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from World.EndToEndValidation.app.consumer_verification import preflight, validate_consumers
from World.EndToEndValidation.app.contracts import ValidationFailure, file_hash, read_json
from World.EndToEndValidation.app.completion_archive import EvidenceArchive


ROOT=Path(__file__).resolve().parents[4]


class ConsumerVerificationTests(unittest.TestCase):
    def setUp(self):
        parent=ROOT/'tmp/world/consumer_verification_tests';parent.mkdir(parents=True,exist_ok=True)
        temporary=tempfile.TemporaryDirectory(dir=parent);self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.pins={}
        self.plan={'capture_width':2,'capture_height':1,'fov_degrees':75,
            'vantages':[dict(name='shore_'+str(i),x=i,y=0,z=100,pitch=0,yaw=0) for i in (1,2)]}
        self.pin('plan',self.plan)
        self.profile={'profiles':{city.lower():{'map_package':'/'+city,'world_data_plugin':'ProjectWorldData',
            'runtime_profile':self.relative(self.root/('runtime_'+city+'.json'))} for city in ('Kazan','Manhattan')}}
        self.pin('profile',self.profile)
        self.request={'validation_profile_path':self.pins['profile']['path'],
            'consumer_verification':{'vantage_plan_path':self.pins['plan']['path'],
                'vantage_plan_sha256':self.pins['plan']['sha256']}}
        self.selection={'base_selection':{'validation_profile_sha256':self.pins['profile']['sha256']},
            'candidates':{'manhattan':{'result_sha256':'a'*64}}}
        self.proof={'status':'accepted','unchanged':True,'world_sha256':'b'*64,'canonical_sha256':'c'*64,
            'authority_before_sha256':'d'*64,'authority_after_sha256':'d'*64,'files':self.pins,'operations':[]}
        self.transport={'saved_publication':{'world_sha256':'b'*64,'canonical_sha256':'c'*64}}
        sections=[{'artifact_path':'section/one'},{'artifact_path':'section/two'}]
        self.native={'layer_inventories':[{'layer_id':'terrain','scope_id':'terrain_scope','canonical_inputs':[{'unit_id':'cell'}]}]}
        self.active={'scopes':[]}
        self.pin('terrain_manifest',{'owning_layer':'terrain',
            'input_identity':{'compile_result_sha256':'a'*64,'map_package':'/Manhattan'},
            'artifacts':[{'path':path} for path in ('base','partition','section/one','section/two')]})
        self.active['scopes']=[{'scope_id':'terrain_scope','manifest_sha256':self.pins['terrain_manifest']['sha256']}]
        self.pin('postbuild',{'status':'accepted','compile_result_sha256':'a'*64,'map':'/Manhattan','sections':sections})
        text=json.dumps([{'kind':'base','key':'cell'},{'kind':'section','key':'one'},{'kind':'section','key':'two'}])
        self.pin('terrain',{'status':'accepted','schema':'project-world-mesh-terrain-audit:v2',
            'compile_result_sha256':'a'*64,'map':'/Manhattan','sections':sections,
            'terrain_projection_record_count':3,'terrain_base_identity_tags':[{'cell':'cell'}],
            'terrain_projection_json':text,'terrain_projection_sha256':hashlib.sha256(text.encode()).hexdigest()})
        part={'projection_json':'[{}]','record_count':1,'sha256':hashlib.sha256(b'[{}]').hexdigest()}
        self.pin('world',{'status':'accepted','map_package':'/Manhattan','schema':'project-world-saved-projection:v1','map':part,'buildings':part})
        cities=[]
        for city in ('Kazan','Manhattan'):
            self.pin('runtime_'+city,{'profile_id':city+'_runtime'})
            operation=dict(city=city,map='/'+city,runtime=city+'_runtime',runtime_sha256=self.pins['runtime_'+city]['sha256'])
            self.proof['operations'].append(operation)
            self.pin('partition_'+city,dict(schema_version=3,status='accepted',map_package='/'+city,
                selected_profile_id=operation['runtime'],profiles=[dict(profile_id=operation['runtime'],
                    profile_sha256=operation['runtime_sha256'],status='accepted')]))
            artifact=self.pin('tour_'+city,{'native':'authenticated runtime route'})
            cities.append({**operation,'status':'accepted','native_status':'rejected',
                'artifact_sha256':{str(self.root/('tour_'+city+'.json')):artifact['sha256']}})
        # The owning tour admits gameplay after its separately validated FPS-only diagnostic.
        self.pin('tour',dict(schema='project-world-uncooked-gameplay-admission:v1',status='accepted',unchanged=True,
            known_bad_control=False,authority_before_sha256='d'*64,authority_after_sha256='d'*64,cities=cities))
        views=[]
        for pose in self.plan['vantages']:
            name=pose['name'];path=self.root/(name+'.png')
            def chunk(kind,body):
                return struct.pack('>I',len(body))+kind+body+struct.pack('>I',zlib.crc32(kind+body))
            color=bytes([pose['x']*60,0,0])*2
            path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,1,8,2,0,0,0))+
                chunk(b'IDAT',zlib.compress(b'\0'+color))+chunk(b'IEND',b''))
            self.pins['image_'+name]={'path':self.relative(path),'sha256':file_hash(path)}
            views.append(dict(name=name,file=path.name,requested_width=2,requested_height=1,fov_degrees=75,
                location=[pose[k] for k in ('x','y','z')],rotation_pitch_yaw_roll=[0,0,0],image_sha256=file_hash(path)))
        self.pin('capture',dict(schema_version=1,status='accepted',map_package='/Manhattan',
            vantage_plan_sha256=self.pins['plan']['sha256'],screenshot_dir=str(self.root),views=views))

    def relative(self,path):return path.relative_to(ROOT).as_posix()

    def pin(self,key,document):
        path=self.root/(key+'.json');path.write_text(json.dumps(document))
        self.pins[key]={'path':self.relative(path),'sha256':file_hash(path)}
        return self.pins[key]

    def validate(self,read=None,resolve=None):
        def file(path,sha256):
            value=ROOT/path
            if not value.is_file() or file_hash(value)!=sha256:raise ValidationFailure('test_artifact_changed','changed')
            return value
        validate_consumers(self.request,self.transport,{'consumer_verification':self.proof,'saved_native':self.native,'saved_active':self.active},self.selection,
            read or (lambda path,sha256:read_json(file(path,sha256))),resolve or file)

    def add_saved_gameplay(self):
        self.selection['base_selection']['profiles']={'kazan':{'compile_result_sha256':'e'*64}}
        terrain=read_json(ROOT/self.pins['terrain_manifest']['path']);terrain['scope_id']='terrain_scope'
        self.pin('terrain_manifest',terrain);self.active['scopes'][0]['manifest_sha256']=self.pins['terrain_manifest']['sha256']
        rows=[];snapshots={}
        for city,child in [('Kazan','e'*64),('Manhattan','a'*64)]:
            prefix='gameplay_'+city+'_'
            source=self.pin('reference_source_'+city,{'canonical_result':{'sha256':child}})
            scene=self.pin('reference_scene_'+city,{'original_fixture':source['path'],'saved_files':{source['path']:source['sha256']}})
            route_source=self.pin('reference_route_'+city,{'source':'faithful axis'})
            routes={'scene_input':scene,'route_source':route_source,
                'routes':[{'id':'contact','width_m':2,'points_world_cm':[[0,0,0],[100,0,0]]}]}
            reference=self.pin('reference_'+city,routes);self.pin(prefix+'input',routes)
            rows.append({'city':city,'canonical_result_sha256':child,'reference':reference})
            for owner in ('map','terrain'):
                manifest=terrain if city=='Manhattan' and owner=='terrain' else {
                    'scope_id':city+'_'+owner,'owning_layer':owner,'input_identity':{'map_package':'/'+city,'compile_result_sha256':child}}
                pin=self.pin(prefix+'manifest_'+owner,manifest)
                if not (city=='Manhattan' and owner=='terrain'):
                    self.active['scopes'].append({'scope_id':manifest['scope_id'],'manifest_sha256':pin['sha256']})
            snapshots[city]=[self.pins[prefix+'manifest_'+owner] for owner in ('map','terrain')]
        self.transport['saved_publication']['world_sha256']=self.proof['world_sha256']=hashlib.sha256(json.dumps(self.active).encode()).hexdigest()
        for row in rows:
            city=row['city'];prefix='gameplay_'+city+'_'
            active=self.pin(prefix+'active',self.active)
            contract=self.pin(prefix+'proof',{'schema':'project-world-saved-region-gameplay:v1','map':'/'+city,
                'compile_result_sha256':row['canonical_result_sha256'],'reference_gameplay_sha256':row['reference']['sha256'],
                'world_active':active,'manifests':snapshots[city]})
            self.pin(prefix+'receipt',{'status':'accepted','saved_product_map':'/'+city,
                'compile_result_sha256':row['canonical_result_sha256'],'input_sha256':row['reference']['sha256'],
                'world_active_sha256':active['sha256'],'saved_product_proof_sha256':contract['sha256'],
                'foreign_floor_actual_hero_control_passed':True,'completed_bidirectional_routes':1,'walking_frames':100,
                'ground_supported_frames':100,'structure_supported_frames':0,'walking_segments':2,
                'actual_hero_class':'/Script/ProjectCharacter.DefinitionCharacter','expected_definition_id':'ObjectDefinition:Hero',
                'minimum_foot_offset_cm':1.9,'maximum_foot_offset_cm':2.4,'maximum_native_floor_envelope_cm':2.41})
        self.plan['gameplay_scenes']=rows;self.pin('plan',self.plan)
        self.request['consumer_verification']['vantage_plan_sha256']=self.pins['plan']['sha256']
        pin=self.pin('selection',self.selection);self.request.update(selection_path=pin['path'],selection_sha256=pin['sha256'])
        capture=read_json(ROOT/self.pins['capture']['path']);capture['vantage_plan_sha256']=self.pins['plan']['sha256']
        self.pin('capture',capture)

    def test_saved_product_hero_requires_current_child_floor_control_and_complete_walking(self):
        self.add_saved_gameplay();preflight(ROOT,self.request,self.profile,'manhattan');self.validate()
        key='gameplay_Manhattan_receipt';original=read_json(ROOT/self.pins[key]['path'])
        for field,bad in [('saved_product_map','/temporary-region'),('foreign_floor_actual_hero_control_passed',False),
                          ('completed_bidirectional_routes',0),('maximum_foot_offset_cm',10)]:
            with self.subTest(field=field):
                self.pin(key,{**original,field:bad})
                with self.assertRaises(ValidationFailure):self.validate()
        self.pin(key,original);self.validate()

    def test_saved_gameplay_wrong_child_or_changed_reference_refuses_before_mutation(self):
        self.add_saved_gameplay()
        self.plan['gameplay_scenes'][0]['canonical_result_sha256']='0'*64;self.pin('plan',self.plan)
        self.request['consumer_verification']['vantage_plan_sha256']=self.pins['plan']['sha256']
        with self.assertRaisesRegex(ValidationFailure,'another canonical child'):preflight(ROOT,self.request,self.profile,'manhattan')
        self.plan['gameplay_scenes'][0]['canonical_result_sha256']='e'*64;self.pin('plan',self.plan)
        self.request['consumer_verification']['vantage_plan_sha256']=self.pins['plan']['sha256']
        (ROOT/self.plan['gameplay_scenes'][0]['reference']['path']).write_text('{}')
        with self.assertRaisesRegex(ValidationFailure,'missing, foreign or changed'):preflight(ROOT,self.request,self.profile,'manhattan')

    def test_complete_scoped_consumers_and_existing_fps_diagnostic_semantics(self):
        preflight(ROOT,self.request,self.profile,'manhattan')
        self.validate()

    def test_publication_or_status_only_cannot_substitute_for_consumers(self):
        self.proof={'status':'accepted'}
        with self.assertRaises(ValidationFailure):self.validate()

    def test_changed_authority_and_missing_nested_artifact_refuse(self):
        self.proof['authority_after_sha256']='e'*64
        with self.assertRaises(ValidationFailure):self.validate()
        self.proof['authority_after_sha256']='d'*64
        (ROOT/self.pins['image_shore_1']['path']).write_bytes(b'invalid')
        with self.assertRaises(ValidationFailure):self.validate()

    def test_wrong_child_and_substituted_runtime_refuse(self):
        original=read_json(ROOT/self.pins['terrain']['path'])
        self.pin('terrain',{**original,'compile_result_sha256':'f'*64})
        with self.assertRaises(ValidationFailure):self.validate()
        self.pin('terrain',original)
        self.pin('runtime_Kazan',{'profile_id':'wrong'})
        with self.assertRaises(ValidationFailure):self.validate()

    def test_duplicate_city_or_wrong_view_pose_cannot_pass(self):
        tour=read_json(ROOT/self.pins['tour']['path']);original=copy.deepcopy(tour)
        tour['cities'][1]=tour['cities'][0];self.pin('tour',tour)
        with self.assertRaises(ValidationFailure):self.validate()
        self.pin('tour',original)
        capture=read_json(ROOT/self.pins['capture']['path']);capture['views'][0]['location'][2]+=1
        self.pin('capture',capture)
        with self.assertRaises(ValidationFailure):self.validate()

    def test_same_count_duplicate_sections_and_missing_base_cells_refuse(self):
        original=read_json(ROOT/self.pins['terrain']['path'])
        changed=copy.deepcopy(original);changed['sections'][1]=changed['sections'][0]
        self.pin('terrain',changed)
        with self.assertRaises(ValidationFailure):self.validate()
        self.pin('terrain',original)
        self.native['layer_inventories'][0]['canonical_inputs'][0]['unit_id']='missing-cell'
        with self.assertRaises(ValidationFailure):self.validate()

    def test_noncanonical_or_incompatible_camera_input_refuses_before_work(self):
        for field,value in [('fov_degrees',float('nan')),('fov_degrees',None)]:
            changed={**self.plan,field:value};self.pin('plan',changed)
            self.request['consumer_verification']['vantage_plan_sha256']=self.pins['plan']['sha256']
            with self.assertRaises(ValidationFailure):preflight(ROOT,self.request,self.profile,'manhattan')
        self.request['consumer_verification']['vantage_plan_path']='./'+self.pins['plan']['path']
        with self.assertRaises(ValidationFailure):preflight(ROOT,self.request,self.profile,'manhattan')

    def test_frozen_reads_survive_removal_of_original_nested_inputs(self):
        self.add_saved_gameplay()
        repo=self.root/'archive-repo'
        for pin in self.pins.values():
            target=repo/pin['path'];target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(ROOT/pin['path'],target)
        archive=EvidenceArchive(repo,repo/'Saved/Validation/WorldAuthority/retained');archive.retain_pins(self.pins)
        for pin in self.pins.values():(repo/pin['path']).unlink()
        for pin in self.pins.values():(ROOT/pin['path']).unlink()
        self.validate(lambda path,sha256:archive.read(path,sha256),archive.path)
        path=archive.path(**{'value':self.pins['image_shore_1']['path'],'expected':self.pins['image_shore_1']['sha256']})
        path.write_bytes(b'corrupt')
        with self.assertRaises(ValidationFailure):self.validate(lambda path,sha256:archive.read(path,sha256),archive.path)


if __name__=='__main__':unittest.main()
