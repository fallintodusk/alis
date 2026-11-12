"""Prepare a read-only canonical-region observation for the exact native test."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

REPO=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(REPO/'tools'))
from World.CanonicalCompilation.api import canonical_active_path, materialize_canonical

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path):
    return json.loads(path.read_text(encoding='utf-8'))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--compiler-profile',type=Path,required=True)
    parser.add_argument('--water-profile',type=Path,required=True)
    parser.add_argument('--xy',type=float,nargs=2,required=True)
    parser.add_argument('--look-offset',type=float,nargs=2,default=[0,-30])
    parser.add_argument('--canonical-result',type=Path)
    parser.add_argument('--result-sha256')
    args=parser.parse_args()
    out=args.output.resolve();profile=args.compiler_profile.resolve();water_profile=args.water_profile.resolve()
    if not out.is_relative_to(REPO/'tmp/world') or out.exists():
        raise ValueError('Observation output must be fresh below repository tmp/world')
    if args.canonical_result:
        result=args.canonical_result.resolve()
        if not args.result_sha256 or digest(result)!=args.result_sha256:
            raise ValueError('Supplied consumer input requires its exact hash')
    else:
        result=materialize_canonical(REPO,canonical_active_path(REPO,profile),profile)
    document=read(result);entries={e['path']:e for e in document['outputs']}
    def artifact(relative):
        path=result.parent/relative;pin=entries[relative]
        if digest(path)!=pin['sha256'] or path.stat().st_size!=pin['byte_size']:
            raise ValueError('Source artifact differs from its declared snapshot')
        return read(path)
    x,y=args.xy;selected=[]
    for name in sorted(entries):
        if name.startswith('canonical/cells/') and name.endswith('.json'):
            cell=artifact(name);west,south,east,north=cell['bounds']
            if west<=x<east and south<=y<north:
                selected.append(cell)
    if len(selected)!=1:
        raise ValueError('Focus must select exactly one original canonical cell')
    cell=selected[0];terrain=artifact(cell['terrain_artifact']['path'])
    if 'core_samples' not in terrain:
        raise ValueError('This preparer currently requires an unchanged grid; use the authored fixture for corrected input')
    west,south,east,north=terrain['bounds'];dx,dy=terrain['sample_spacing'];samples=terrain['core_samples']
    def values(px,py):
        u=(px-west)/dx;v=(north-py)/dy
        i=max(0,min(len(samples[0])-2,math.floor(u)));j=max(0,min(len(samples)-2,math.floor(v)))
        u,v=u-i,v-j;a,b=samples[j][i:i+2];c,d=samples[j+1][i:i+2]
        bilinear=a*(1-u)*(1-v)+b*u*(1-v)+c*(1-u)*v+d*u*v
        physical=a+(b-a)*u+(c-a)*v if u+v<=1 else b*(1-v)+c*(1-u)+d*(u+v-1)
        return bilinear,physical
    i=math.floor((x-west)/dx);j=math.floor((north-y)/dy);queries=[];physics=[]
    for row in range(max(0,j-1),min(len(samples)-1,j+2)):
        for col in range(max(0,i-1),min(len(samples[0])-1,i+2)):
            for u,v in ((1/3,1/3),(2/3,2/3)):
                px,py=west+(col+u)*dx,north-(row+v)*dy
                query,z=values(px,py);queries.append({'xy':[px,py],'base_m':query,'final_m':query});physics.append([px,py,z])
    query,floor=values(x,y);queries.append({'xy':[x,y],'base_m':query,'final_m':query});physics.append([x,y,floor])
    tx,ty=x+args.look_offset[0],y+args.look_offset[1]
    if not west<=tx<=east or not south<=ty<=north:
        raise ValueError('View target must retain selected complete-cell support')
    water=read(water_profile);data_root=water_profile.parents[2]
    payload={'regional_observation':True,'water_preparation_scope':'selected_complete_cell',
             'classification':'Native regional observation, not full-world Water equivalence or geographic acceptance',
             'canonical_result':{'path':result.relative_to(REPO).as_posix(),'sha256':digest(result)},
             'cell_id':cell['cell_id'],'terrain_queries':queries,'required_physics_points':physics,
             'water_profile':{'world_data_plugin':water['world_data_plugin'],'path':water_profile.relative_to(data_root).as_posix(),'sha256':digest(water_profile)},
             'player_eye':{'position':[x,y,floor+1.7],'target':[tx,ty,values(tx,ty)[1]]},
             'query_scope':'Unrefined public queries remain bilinear; collision witnesses use original physical PWA triangles'}
    out.mkdir(parents=True)
    target=out/'input.json';target.write_text(json.dumps(payload,indent=2)+'\n',encoding='ascii')
    print(json.dumps({'input':str(target.relative_to(REPO)),'sha256':digest(target),'cell_id':cell['cell_id'],
                      'physics_witnesses':len(physics),'parent_outputs':len(entries)}))

if __name__=='__main__':
    main()
