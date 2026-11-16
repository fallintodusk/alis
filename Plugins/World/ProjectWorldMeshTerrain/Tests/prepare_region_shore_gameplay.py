"""Add explicitly inferred Ground/contact traversals to unchanged source-axis diagnostics."""
import argparse
import json
import math
from pathlib import Path
from prepare_region_gameplay import REPO,prepare as prepare_source,read,digest,fields,top_support
from prepare_region_shore import cap_edges


def contacts(deck, width=2., distance=8.):
    if not math.isfinite(distance) or not 1<=distance<=20:
        raise ValueError('Contact diagnostics need a bounded one-to-twenty metre approach')
    result=[]
    for index,edge in enumerate(cap_edges(deck,width)):
        a,b=edge;middle=[(a[k]+b[k])/2 for k in (0,1)]
        adjacent=[f for f in deck if tuple(a) in map(tuple,f) and tuple(b) in map(tuple,f)]
        if len(adjacent)!=1:raise ValueError('A contact edge must have one unambiguous incident upper face')
        inward=[(b[1]-a[1])/width,-(b[0]-a[0])/width]
        center=[sum(p[k] for p in adjacent[0])/3-middle[k] for k in (0,1)]
        if sum(x*y for x,y in zip(inward,center))<0:inward=[-v for v in inward]
        points=[[middle[k]+sign*distance*inward[k] for k in (0,1)] for sign in (-1,0,1)]
        result.append({'id':f'inferred_ground_contact_{index+1}','width_m':width,'cap':edge,'points':points})
    return result


def prepare(repo, view_path, route_path, output):
    repo,view_path,route_path,output=[Path(p).resolve() for p in (repo,view_path,route_path,output)]
    if output.exists() or not output.is_relative_to(repo/'tmp/world'):
        raise ValueError('Shore gameplay preparation needs fresh repository scratch')
    base=prepare_source(repo,view_path,route_path,output/'source_axes')
    payload=read(repo/base['input']);view=read(view_path);original=read(repo/view['original_fixture'])
    result=repo/original['canonical_result']['path'];receipt=read(result)
    entries={e['path']:e for e in receipt['outputs']};features=[]
    for name,entry in sorted(entries.items()):
        if not name.startswith('canonical/features/'):continue
        path=result.parent/name
        if digest(path)!=entry['sha256']:raise ValueError('The authenticated structural input changed')
        features.extend(f for f in read(path)['features'] if f['feature_class']=='constructed_surface')
    if len(features)!=1:raise ValueError('This contact diagnostic selects one admitted separate structure')
    coverage=read(result.parent/'canonical/coverage.json');origin=coverage['engine_georeference_origin'];height_origin=coverage['grid']['vertical_origin_m']
    ground,structures,water=fields(read(repo/view['sections']),origin,height_origin)
    derived=contacts(features[0]['attributes']['constructed_surface']['triangles'])
    cameras=[];proof=[]
    for contact in derived:
        positions=[];count=0;clearances=[]
        for point in contact['points']:
            z,_,_=top_support(ground,structures,water,point)
            positions.append([(point[0]-origin[0])*100,-(point[1]-origin[1])*100,(z-height_origin)*100])
        for a,b in zip(contact['points'],contact['points'][1:]):
            length=math.dist(a,b);normal=[-(b[1]-a[1])/length,(b[0]-a[0])/length]
            steps=math.ceil(length/.25)
            for step in range(steps+1):
                for lateral in (-contact['width_m']/2+.23,0.,contact['width_m']/2-.23):
                    xy=[a[k]+(b[k]-a[k])*step/steps+lateral*normal[k] for k in (0,1)]
                    z,wet,_=top_support(ground,structures,water,xy);count+=1
                    if wet is not None:clearances.append(z-wet)
        payload['routes'].append({'id':contact['id'],'width_m':contact['width_m'],'points_world_cm':positions,
            'inferred_ground_contact':{'feature_id':features[0]['feature_id'],'cap':contact['cap'],
                'basis':'authored_game_world_contact_not_mapped_road'}})
        eye=[*positions[0]];eye[2]+=170;target=[*positions[-1]];target[2]+=80
        delta=[target[k]-eye[k] for k in (0,1,2)]
        cameras.append({'name':contact['id'],'x':eye[0],'y':eye[1],'z':eye[2],
            'pitch':math.degrees(math.atan2(delta[2],math.hypot(*delta[:2]))),'yaw':math.degrees(math.atan2(delta[1],delta[0])),
            'note':'Actual saved corrected scene; inferred full-width Ground/Upper contact viewed from dry Ground at player height.'})
        proof.append({'id':contact['id'],'sampled_support_witnesses':count,'minimum_sampled_water_clearance_m':min(clearances) if clearances else None})
    source={'basis':'source_axis_subsets_plus_explicit_inferred_upper_ground_contacts',
        'source_axis_selection':{'path':route_path.relative_to(repo).as_posix(),'sha256':digest(route_path)},
        'canonical_result_sha256':digest(result),'constructed_feature_id':features[0]['feature_id'],'contacts':derived}
    source_path=output/'route-source.json';source_path.write_text(json.dumps(source,indent=2)+'\n',encoding='ascii')
    payload.update(route_source={'path':source_path.relative_to(repo).as_posix(),'sha256':digest(source_path)},
        scope='Actual Hero diagnostic on the unchanged saved candidate; mapped-axis intervals and separately identified inferred contact approaches. No city spawn, navigation or streaming acceptance.')
    path=output/'input.json';path.write_text(json.dumps(payload,indent=2)+'\n',encoding='ascii')
    (output/'contact-support.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='ascii')
    (output/'vantages.json').write_text(json.dumps({'capture_width':1280,'capture_height':720,'fov_degrees':75,'vantages':cameras},indent=2)+'\n',encoding='ascii')
    launch={**base,'input':path.relative_to(repo).as_posix(),'sha256':digest(path),'contacts':proof}
    (output/'launch.json').write_text(json.dumps(launch,indent=2)+'\n',encoding='ascii')
    return launch


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',type=Path)
    parser.add_argument('--scene-input',type=Path,required=True);parser.add_argument('--routes',type=Path,required=True);args=parser.parse_args()
    print(json.dumps(prepare(REPO,args.scene_input,args.routes,args.output),indent=2))


if __name__=='__main__':main()
