"""Observe a composed Ground/bank/Pier child through the existing regional consumer."""
import argparse
from collections import Counter
from fractions import Fraction as F
import json
import math
from pathlib import Path
import sys

REPO=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(REPO/'tools'))
from prepare_region_pier import authenticated, digest, grid_values, prove_field, read
from World.CanonicalCompilation.refinement.app.shore_support import reference_faces
from World.ExecutionEnvironment.geometry.affine import _cross, _height


def cap_edges(deck, width):
    edges=Counter(tuple(sorted((tuple(a),tuple(b)))) for face in deck for a,b in zip(face,(*face[1:],face[0])))
    caps=sorted(edge for edge,count in edges.items() if count==1 and abs(math.dist(edge[0][:2],edge[1][:2])-width)<=1e-8)
    if len(caps)!=2:
        raise ValueError('This ribbon observation requires exactly two exposed full-width contact edges')
    return caps


def values(terrain, payload, xy):
    base,physical=grid_values(terrain,*xy)
    x,y=xy;west,_,_,north=terrain['bounds'];dx,dy=terrain['sample_spacing']
    i,j=math.floor((x-west)/dx),math.floor((north-y)/dy)
    quad=next((q for q in payload.get('quads',[]) if q['coordinate']==[i,j]),None)
    if quad is None:
        return base,base,physical
    point=list(map(F,xy));heights=[]
    for indices in quad['triangles']:
        face=[[F(v) for v in quad['vertices'][k][:3]] for k in indices]
        if all(_cross(a,b,point)>=0 for a,b in zip(face,(*face[1:],face[0]))):
            heights.append(float(_height(face,point)))
    if not heights or max(heights)-min(heights)>1e-8:
        raise ValueError('The encoded final field has missing or conflicting query support')
    return base,heights[0],heights[0]


def prepare(repo, result, result_hash, water_profile, output):
    repo,result,water_profile,output=[Path(p).resolve() for p in (repo,result,water_profile,output)]
    if not output.is_relative_to(repo/'tmp/world') or output.exists() or output.is_relative_to(result.parent):
        raise ValueError('The regional observation requires fresh disjoint repository scratch')
    _,entries=authenticated(result,result_hash)
    request=read(result.parent/'refinement/request.json')
    kinds=[op['kind'] for op in request['operations']]
    if kinds!=['qualified_ground_base','inferred_shore_transition','constructed_surface_import']:
        raise ValueError('This observation requires one exact Ground -> shore -> declared-Pier composition')
    manifests={};features={}
    for name in sorted(entries):
        if name.startswith('canonical/cells/'):
            cell=read(result.parent/name);manifests[cell['cell_id']]=cell
        if name.startswith('canonical/features/'):
            features.update({f['feature_id']:f for f in read(result.parent/name)['features']})
    structures=[f for f in features.values() if f['feature_class']=='constructed_surface']
    if len(structures)!=1:
        raise ValueError('The bounded observation selects exactly one source-bound constructed field')
    target=structures[0];ids=sorted(target['intersecting_cell_ids'])
    if len(ids)!=2 or len(set(ids))!=2:
        raise ValueError('This Pier observation needs exactly two complete membership cells')
    cells={};payloads={}
    for key in ids:
        manifest=manifests[key];descriptor=manifest['terrain_artifact'];terrain=read(result.parent/descriptor['path'])
        if manifest['terrain_lineage'].get('kind')!='qualified_ground_base':
            raise ValueError('The corrected scene requires explicit qualified Ground lineage')
        if 'construction' in terrain:
            if terrain['construction']!='inferred_east_bank_v1':raise ValueError('Unexpected corrected Terrain construction')
            payloads[key]=terrain;terrain=read(result.parent/terrain['base']['path'])
        cells[key]=terrain
        expected={'path':descriptor['path'],'sha256':descriptor['content_hash'],'byte_size':descriptor['byte_size']}
        if target['attributes']['constructed_surface']['model_provenance']['terrain_artifacts'].get(key)!=expected:
            raise ValueError('The observed Pier does not bind this exact final Terrain artifact')
    deck=target['attributes']['constructed_surface']['triangles'];caps=cap_edges(deck,2.)
    bounds=[min(p[k] for face in deck for p in face) for k in (0,1)]+[max(p[k] for face in deck for p in face) for k in (0,1)]
    ground=reference_faces(cells,payloads,ids,bounds)
    waters=[f for f in features.values() if f['feature_class']=='water' and set(f['intersecting_cell_ids']).intersection(ids)]
    profile=read(water_profile);offset=next(l for l in profile['layers'] if l['layer_id']=='water')['settings']['surface_offset_m']
    proof=prove_field(repo,deck,ground,waters,offset,caps)
    queries=[];physics=[]
    for key in ids:
        samples=[]
        for quad in payloads.get(key,{}).get('quads',[]):
            face=[quad['vertices'][i] for i in quad['triangles'][0]]
            samples.append([sum(p[k] for p in face)/3 for k in (0,1)])
        for face in deck[::10]:
            xy=[sum(p[k] for p in face)/3 for k in (0,1)]
            west,south,east,north=cells[key]['bounds']
            if west<xy[0]<east and south<xy[1]<north:samples.append(xy)
        if not samples:raise ValueError('A selected complete cell has no query witnesses')
        for xy in samples:
            base,final,physical=values(cells[key],payloads.get(key,{}),xy)
            queries.append({'cell_id':key,'xy':xy,'base_m':base,'final_m':final});physics.append([*xy,physical])
    if len(queries)>64:raise ValueError('The local witness budget is exceeded')
    a,b=caps[0];eye=[(a[k]+b[k])/2 for k in (0,1,2)];eye[2]+=1.7
    payload={'regional_observation':True,'water_preparation_scope':'selected_complete_cells',
        'canonical_result':{'path':result.relative_to(repo).as_posix(),'sha256':result_hash},'cell_id':ids[0],'cell_ids':ids,
        'constructed_feature_id':target['feature_id'],'terrain_queries':queries,'required_physics_points':physics,
        'water_profile':{'world_data_plugin':profile['world_data_plugin'],'path':water_profile.relative_to(water_profile.parents[2]).as_posix(),'sha256':digest(water_profile)},
        'player_eye':{'position':eye,'target':[eye[0]-30,eye[1]+5,eye[2]-1.7]},
        'classification':'Inferred bank and adapted separate Pier; qualified Ground retained outside selected transition. Native/Hero/acceptance pending.'}
    output.mkdir(parents=True)
    for name,value in [('input.json',payload),('l0.json',{**proof,'ground_contact_edges':caps,'query_witnesses':len(queries),
        'canonical_result_sha256':result_hash,'selected_complete_cells':ids,'authenticated_outputs':len(entries)})]:
        (output/name).write_text(json.dumps(value,indent=2)+'\n',encoding='ascii')
    return {'input':(output/'input.json').relative_to(repo).as_posix(),'sha256':digest(output/'input.json'),**proof}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',type=Path)
    parser.add_argument('--canonical-result',type=Path,required=True);parser.add_argument('--result-sha256',required=True)
    parser.add_argument('--water-profile',type=Path,required=True);args=parser.parse_args()
    print(json.dumps(prepare(REPO,args.canonical_result,args.result_sha256,args.water_profile,args.output),indent=2))


if __name__=='__main__':main()
