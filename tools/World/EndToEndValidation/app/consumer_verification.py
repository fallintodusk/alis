"""Authenticate the optional selected-city consumer envelope, fresh or retained."""
import hashlib
import math
from pathlib import Path, PurePosixPath
import struct

from .contracts import ValidationFailure, file_hash, read_json


def require(value, message):
    if not value:
        raise ValidationFailure('selected_consumers_unproven', message)


def _expected_ground(selection, city):
    name=city.lower()
    if name in selection['candidates']:
        return selection['candidates'][name]['result_sha256']
    base=selection['base_selection']['profiles'][name]
    return base.get('selected_refinement',{}).get('result_sha256',base['compile_result_sha256'])


def _scratch_document(repo, pin):
    require(isinstance(pin,dict),'Gameplay references need explicit file pins')
    label=pin.get('path');sha=pin.get('sha256')
    require(isinstance(label,str) and label.startswith('tmp/world/') and '\\' not in label and
            PurePosixPath(label).as_posix()==label and '..' not in PurePosixPath(label).parts and
            isinstance(sha,str) and len(sha)==64 and all(c in '0123456789abcdef' for c in sha),
            'Gameplay references need canonical scratch paths and hashes')
    path=(repo/label).resolve()
    require(path.is_relative_to((repo/'tmp/world').resolve()) and path.is_file() and file_hash(path)==sha,
            'A gameplay reference is missing, foreign or changed')
    return read_json(path)


def _gameplay_rows(plan):
    rows=plan.get('gameplay_scenes',[])
    require(isinstance(rows,list) and (not rows or len(rows)==2 and
            all(isinstance(row,dict) and set(row)=={'city','canonical_result_sha256','reference'} and
                row['city'] in ('Kazan','Manhattan') and isinstance(row['reference'],dict) and
                set(row['reference'])=={'path','sha256'} for row in rows) and
            {row['city'] for row in rows}=={'Kazan','Manhattan'}), 'Saved gameplay must identify both product cities exactly')
    return rows


def preflight(repo, request, profile, name):
    spec = request['consumer_verification']
    label=spec['vantage_plan_path']
    require(isinstance(label,str) and not Path(label).is_absolute() and '\\' not in label and
            PurePosixPath(label).as_posix()==label and '..' not in PurePosixPath(label).parts,
            'Runtime plan path must be canonical and repository-relative')
    path = (repo / spec['vantage_plan_path']).resolve()
    require(path.is_relative_to(repo.resolve()) and path.is_file() and
            file_hash(path) == spec['vantage_plan_sha256'], 'Selected runtime plan is missing, foreign or changed')
    settings = profile['profiles'][name]
    require(settings['world_data_plugin'] == 'ProjectWorldData' and
            all(city in profile['profiles'] and profile['profiles'][city]['world_data_plugin'] == 'ProjectWorldData'
                and profile['profiles'][city].get('runtime_profile') for city in ('kazan', 'manhattan')),
            'Runtime verification requires the existing two product cities')
    plan = read_json(path)
    views = plan.get('vantages', [])
    require(isinstance(views, list) and 1 <= len(views) <= 16 and
            len({row.get('name') for row in views}) == len(views), 'Runtime views must be nonempty and uniquely named')
    require(all(type(plan.get(key)) is int and plan[key] > 0 for key in ('capture_width', 'capture_height')) and
            type(plan.get('fov_degrees')) in (int,float) and math.isfinite(plan['fov_degrees']) and
            0 < plan['fov_degrees'] < 180 and all('fov' not in row and row.get('roll',0)==0 for row in views) and
            all(type(row.get(key)) in (int, float) and math.isfinite(row[key])
                for row in views for key in ('x', 'y', 'z', 'pitch', 'yaw')),
            'Runtime view geometry or dimensions are unknown')
    rows=_gameplay_rows(plan)
    if rows:
        selection=read_json(repo/request['selection_path'])
        require(file_hash(repo/request['selection_path'])==request['selection_sha256'],'Gameplay selection changed')
        for row in rows:
            expected=_expected_ground(selection,row['city'])
            routes=_scratch_document(repo,row['reference'])
            require(isinstance(routes,dict) and {'scene_input','route_source'}<=routes.keys(),'Saved gameplay needs pinned scene and route sources')
            scene=_scratch_document(repo,routes['scene_input'])
            require(isinstance(scene,dict) and isinstance(scene.get('saved_files'),dict) and
                    isinstance(scene.get('original_fixture'),str) and scene['original_fixture'] in scene['saved_files'],
                    'Saved gameplay has no pinned original source')
            original=_scratch_document(repo,{'path':scene['original_fixture'],'sha256':scene['saved_files'][scene['original_fixture']]})
            _scratch_document(repo,routes['route_source'])
            require(isinstance(original,dict) and isinstance(original.get('canonical_result'),dict),'Saved gameplay has no canonical reference')
            require(row['canonical_result_sha256']==original['canonical_result']['sha256']==expected,
                    'Saved gameplay references another canonical child')


def validate_consumers(request, transport, documents, selection, read, resolve):
    if 'consumer_verification' not in request:
        return
    proof = documents.get('consumer_verification', {})
    saved = transport['saved_publication']
    require(proof.get('status') == 'accepted' and proof.get('unchanged') is True and
            proof.get('world_sha256') == saved['world_sha256'] and
            proof.get('canonical_sha256') == saved['canonical_sha256'] and
            isinstance(proof.get('authority_before_sha256'), str) and len(proof['authority_before_sha256']) == 64 and
            proof.get('authority_before_sha256') == proof.get('authority_after_sha256'),
            'Requested selected consumers did not prove the unchanged saved authority pair')
    pins = proof.get('files', {})
    required = {'plan','profile','postbuild','terrain_manifest','terrain','world','capture','tour',
                'runtime_Kazan','runtime_Manhattan','partition_Kazan','partition_Manhattan'}
    require(required <= pins.keys(), 'Selected consumer envelope is incomplete')
    for pin in pins.values():
        resolve(pin['path'], pin['sha256'])
    def doc(key):
        return read(**pins[key])
    spec = request['consumer_verification']
    require(pins['plan'] == {'path': spec['vantage_plan_path'], 'sha256': spec['vantage_plan_sha256']} and
            pins['profile']['path'] == request['validation_profile_path'] and
            pins['profile']['sha256'] == selection['base_selection']['validation_profile_sha256'],
            'Selected consumer plan or profile was substituted')
    profile, plan = doc('profile'), doc('plan')
    _validate_gameplay(plan,profile,selection,saved,pins,doc)
    name, candidate = next(iter(selection['candidates'].items()))
    settings = profile['profiles'][name]
    map_name = settings['map_package']
    post, terrain, world = doc('postbuild'), doc('terrain'), doc('world')
    require(post.get('status') == terrain.get('status') == 'accepted' and
            post.get('compile_result_sha256') == terrain.get('compile_result_sha256') == candidate['result_sha256'] and
            post.get('map') == terrain.get('map') == map_name and
            terrain.get('schema') == 'project-world-mesh-terrain-audit:v2' and
            bool(post.get('sections')) and len(post['sections']) == len(terrain.get('sections', [])),
            'Selected saved Terrain projection has the wrong child or compiled census')
    projection = terrain.get('terrain_projection_json', '')
    require(isinstance(projection, str) and hashlib.sha256(projection.encode()).hexdigest() ==
            terrain.get('terrain_projection_sha256') and
            world.get('status') == 'accepted' and world.get('map_package') == map_name and
            world.get('schema') == 'project-world-saved-projection:v1',
            'Selected saved projections are missing or unauthenticated')
    import json
    records=json.loads(projection)
    require(isinstance(records,list) and len(records)==terrain.get('terrain_projection_record_count') and
            len({(row.get('kind'),row.get('key')) for row in records})==len(records),
            'Selected Terrain projection record census is incomplete or duplicated')
    layers=[row for row in documents.get('saved_native',{}).get('layer_inventories',[]) if row.get('layer_id')=='terrain']
    require(len(layers)==1,'Selected native Terrain input census is missing')
    cells=[row['unit_id'] for row in layers[0].get('canonical_inputs',[])]
    observed=[row['key'] for row in records if row['kind']=='base']
    tags=[row['cell'] for row in terrain.get('terrain_base_identity_tags',[])]
    require(bool(cells) and len(set(cells))==len(cells) and sorted(cells)==sorted(observed)==sorted(tags),
            'Selected Terrain does not exactly cover its admitted base cells')
    def section_paths(value):
        rows=value.get('sections',[]);paths=[row.get('artifact_path') for row in rows]
        require(bool(paths) and all(isinstance(path,str) and path for path in paths) and len(set(paths))==len(paths),
                'Selected section inventory is missing or duplicated')
        return set(paths)
    sections=section_paths(post)
    require(sections==section_paths(terrain) and len(sections)==len([row for row in records if row['kind']=='section']),
            'Selected compiled section sets disagree')
    manifest=doc('terrain_manifest')
    scopes=[row for row in documents.get('saved_active',{}).get('scopes',[]) if row.get('scope_id')==layers[0].get('scope_id')]
    artifacts=[row.get('path') for row in manifest.get('artifacts',[])]
    require(len(scopes)==1 and scopes[0].get('manifest_sha256')==pins['terrain_manifest']['sha256'] and
            manifest.get('owning_layer')=='terrain' and
            manifest.get('input_identity',{}).get('compile_result_sha256')==candidate['result_sha256'] and
            manifest.get('input_identity',{}).get('map_package')==map_name and
            sections <= set(artifacts) and len(set(artifacts))==len(artifacts)==len(cells)+1+len(sections),
            'Selected Terrain census is not bound to its saved manifest')
    for key in ('map','buildings'):
        part=world.get(key,{})
        text=part.get('projection_json','')
        require(isinstance(text,str) and hashlib.sha256(text.encode()).hexdigest()==part.get('sha256') and
                isinstance(json.loads(text),list) and 0 < len(json.loads(text))==part.get('record_count'),
                'Saved World projection digest or record count is invalid')
    operations = proof.get('operations', [])
    require(len(operations) == 2 and {row.get('city') for row in operations} == {'Kazan','Manhattan'},
            'Selected consumers omit or duplicate a product operation')
    expected = {}
    for operation in operations:
        city = operation['city']
        runtime = doc('runtime_' + city)
        config = profile['profiles'][city.lower()]
        require(operation['map'] == config['map_package'] and
                pins['runtime_' + city]['path'] == config['runtime_profile'] and
                operation['runtime'] == runtime['profile_id'] and
                operation['runtime_sha256'] == pins['runtime_' + city]['sha256'], 'Product operation identity differs')
        partition = doc('partition_' + city)
        selected = [row for row in partition.get('profiles', []) if row.get('profile_id') == operation['runtime']]
        require(partition.get('schema_version') == 3 and partition.get('status') == 'accepted' and
                partition.get('map_package') == operation['map'] and
                partition.get('selected_profile_id') == operation['runtime'] and len(selected) == 1 and
                selected[0].get('profile_sha256') == operation['runtime_sha256'] and selected[0].get('status') == 'accepted',
                'Product partition audit is missing or bound to another runtime')
        expected[city] = operation
    tour = doc('tour')
    rows = tour.get('cities', [])
    require(tour.get('schema') == 'project-world-uncooked-gameplay-admission:v1' and
            tour.get('status') == 'accepted' and tour.get('unchanged') is True and tour.get('known_bad_control') is False and
            tour.get('authority_before_sha256') == tour.get('authority_after_sha256') == proof['authority_before_sha256'] and
            len(rows) == 2 and {row.get('city') for row in rows} == set(expected), 'Both-city product tour is unproved')
    all_pins = {(str(Path(pin['path']).as_posix()), pin['sha256']) for pin in pins.values()}
    for row in rows:
        operation = expected[row['city']]
        require(row.get('status') == 'accepted' and row.get('map') == operation['map'] and
                row.get('runtime') == operation['runtime'] and row.get('runtime_sha256') == operation['runtime_sha256'] and
                bool(row.get('artifact_sha256')), 'Product tour consumed a different city or runtime')
        for path, sha in row['artifact_sha256'].items():
            # Tour records absolute native paths; the evidence inventory carries portable labels.
            native_path = path.replace('\\','/')
            require(any(label == native_path or native_path.endswith('/' + label)
                        for label, digest in all_pins if digest == sha), 'A product tour artifact was not retained')
    capture = doc('capture')
    views = capture.get('views', [])
    require(capture.get('status') == 'accepted' and capture.get('schema_version') == 1 and
            capture.get('map_package') == map_name and capture.get('vantage_plan_sha256') == spec['vantage_plan_sha256'] and
            len(views) == len(plan['vantages']) and {view['name'] for view in views} == {view['name'] for view in plan['vantages']},
            'Selected capture differs from the authenticated view plan')
    images = set()
    for view in views:
        pose = next(row for row in plan['vantages'] if row['name'] == view['name'])
        def close(actual, expected):
            return isinstance(actual,list) and len(actual)==len(expected) and all(
                type(a) in (int,float) and math.isfinite(a) and abs(a-b)<=1e-8 for a,b in zip(actual,expected))
        require(close(view.get('location'), [pose[key] for key in ('x','y','z')]) and
                close(view.get('rotation_pitch_yaw_roll'), [pose['pitch'],pose['yaw'],pose.get('roll',0)]) and
                view.get('fov_degrees') == plan['fov_degrees'] and
                view.get('requested_width') == plan['capture_width'] and view.get('requested_height') == plan['capture_height'],
                'Selected capture pose or dimensions differ')
        pin = pins.get('image_' + view['name'], {})
        require(pin.get('sha256') == view.get('image_sha256') and pin['sha256'] not in images,
                'Selected image is missing, stale or duplicated')
        with resolve(pin['path'], pin['sha256']).open('rb') as stream:
            header = stream.read(24)
        require(header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR' and
                struct.unpack('>II', header[16:24]) == (plan['capture_width'],plan['capture_height']),
                'Selected image bytes have the wrong PNG dimensions')
        images.add(pin['sha256'])


def _validate_gameplay(plan,profile,selection,saved,pins,doc):
    for row in _gameplay_rows(plan):
        prefix='gameplay_'+row['city']+'_'
        require(all(prefix+key in pins for key in ('input','proof','receipt','active','manifest_map','manifest_terrain')),
                'Saved product Hero evidence is missing')
        source,contract,native=[doc(prefix+key) for key in ('input','proof','receipt')]
        expected=_expected_ground(selection,row['city'])
        map_name=profile['profiles'][row['city'].lower()]['map_package']
        require(pins[prefix+'input']['sha256']==row['reference']['sha256'] and
                contract.get('schema')=='project-world-saved-region-gameplay:v1' and
                contract.get('map')==native.get('saved_product_map')==map_name and
                contract.get('compile_result_sha256')==native.get('compile_result_sha256')==row['canonical_result_sha256']==expected and
                contract.get('reference_gameplay_sha256')==native.get('input_sha256')==row['reference']['sha256'] and
                native.get('saved_product_proof_sha256')==pins[prefix+'proof']['sha256'] and
                contract.get('world_active')==pins[prefix+'active'] and
                native.get('world_active_sha256')==pins[prefix+'active']['sha256']==saved['world_sha256'],
                'Hero walking is not bound to the actual saved product child and World authority')
        manifests=[pins[key] for key in pins if key.startswith(prefix+'manifest_')]
        require(sorted(contract.get('manifests',[]),key=lambda pin:pin['path'])==sorted(manifests,key=lambda pin:pin['path']),
                'Saved walking manifests were substituted')
        active=doc(prefix+'active')
        for key in [key for key in pins if key.startswith(prefix+'manifest_')]:
            manifest=doc(key)
            require(manifest.get('input_identity',{}).get('map_package')==map_name and
                    (manifest.get('owning_layer')=='map' or manifest.get('input_identity',{}).get('compile_result_sha256')==expected) and
                    any(entry.get('scope_id')==manifest.get('scope_id') and entry.get('manifest_sha256')==pins[key]['sha256']
                        for entry in active.get('scopes',[])), 'A walked floor manifest is outside saved World authority')
        routes=source.get('routes',[]);frames=native.get('walking_frames',0)
        require(native.get('status')=='accepted' and native.get('foreign_floor_actual_hero_control_passed') is True and
                isinstance(routes,list) and 0<len(routes)<=8 and
                native.get('completed_bidirectional_routes')==len(routes) and type(frames) is int and frames>0 and
                native.get('ground_supported_frames',0)+native.get('structure_supported_frames',0)==frames and
                native.get('walking_segments')==2*sum(len(route['points_world_cm'])-1 for route in routes) and
                native.get('actual_hero_class')=='/Script/ProjectCharacter.DefinitionCharacter' and
                native.get('expected_definition_id')=='ObjectDefinition:Hero', 'Actual saved-scene Hero walking is incomplete')
        minimum,maximum,bound=[native.get(key) for key in ('minimum_foot_offset_cm','maximum_foot_offset_cm','maximum_native_floor_envelope_cm')]
        require(all(type(value) in (int,float) and math.isfinite(value) for value in (minimum,maximum,bound)) and
                -.05<=minimum<=maximum<=bound and bound>=0, 'Saved Hero foot contacts exceed their native floor envelope')
