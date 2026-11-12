"""Exact boundaries of finite triangle fields; no source or geographic policy."""
from bisect import bisect_left,bisect_right
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path

from ..api import execution_identity as parent_identity
from ..app.contracts import ExecutionEnvironmentError


def execution_identity(repo_root,verify_native_tools):
    parent=parent_identity(repo_root,verify_native_tools)
    payload={'parent':parent,'triangle_boundary_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    return {**parent,'identity_sha256':hashlib.sha256(json.dumps(payload,sort_keys=True,separators=(',',':')).encode()).hexdigest()}


def _refuse(message):
    raise ExecutionEnvironmentError('triangle_boundary_invalid',message)


def _cross(a,b,c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def _area(ring):
    return sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(ring,(*ring[1:],ring[0])))/2


def _inside(point,ring,tick):
    inside=False
    for a,b in zip(ring,(*ring[1:],ring[0])):
        tick()
        side=_cross(a,b,point)
        if side==0 and all(min(a[k],b[k])<=point[k]<=max(a[k],b[k]) for k in (0,1)):
            _refuse('Boundary rings have a contact instead of strict containment')
        if (a[1]>point[1])!=(b[1]>point[1]):
            x=a[0]+(point[1]-a[1])*(b[0]-a[0])/(b[1]-a[1])
            if x>point[0]:inside=not inside
    return inside


def recover_triangle_boundary(triangles,*,maximum_work=10_000_000):
    """Return exact existing-vertex loops, including genuine holes and components.

    The caller owns expected topology and source-boundary departure. This does
    not certify an independently declared source footprint or invent missing faces.
    """
    if type(maximum_work) is not int or not 1<=maximum_work<=10_000_000:
        _refuse('Boundary recovery requires a bounded work limit')
    if not isinstance(triangles,list) or not 1<=len(triangles)<=4096:
        _refuse('Boundary recovery requires a bounded nonempty triangle field')
    work=0
    def tick():
        nonlocal work
        work+=1
        if work>maximum_work:_refuse('Triangle boundary recovery exceeds its work capacity')
    vertices={};faces=[];boxes=[]
    for triangle in triangles:
        if not isinstance(triangle,list) or len(triangle)!=3:_refuse('A boundary face needs three XY vertices')
        face=[]
        for point in triangle:
            if not isinstance(point,list) or len(point)!=2 or any(type(v) not in (int,float) for v in point):
                _refuse('Boundary coordinates require literal finite binary64 XY values')
            try:
                encoded=[float(v) for v in point]
                if any(not math.isfinite(v) for v in encoded) or any(F(v)!=F(p) for v,p in zip(encoded,point)):
                    _refuse('Boundary coordinates cannot be represented exactly as finite binary64')
            except (ValueError,OverflowError):_refuse('Boundary coordinate is not finite binary64')
            key=tuple(map(F,encoded));vertices.setdefault(key,encoded);face.append(key)
        area=_cross(*face)
        if area==0:_refuse('A boundary face is collapsed')
        if area<0:face.reverse()
        faces.append(face)
        boxes.append((min(p[0] for p in face),min(p[1] for p in face),max(p[0] for p in face),max(p[1] for p in face)))
    overlap_candidates=0
    def separates(a,b):
        return any(all(_cross(p,q,r)<=0 for r in b) for p,q in zip(a,(*a[1:],a[0])))
    for i,a in enumerate(faces):
        for j in range(i+1,len(faces)):
            tick();first,second=boxes[i],boxes[j]
            if first[2]<=second[0] or second[2]<=first[0] or first[3]<=second[1] or second[3]<=first[1]:continue
            overlap_candidates+=1;b=faces[j]
            if not separates(a,b) and not separates(b,a):_refuse('Triangle field has positive exact overlap')
    ordered=sorted(vertices);xs=[p[0] for p in ordered];incidence={};noded=0
    for face in faces:
        for a,b in zip(face,(*face[1:],face[0])):
            candidates=[];axis=0 if a[0]!=b[0] else 1
            for index in range(bisect_left(xs,min(a[0],b[0])),bisect_right(xs,max(a[0],b[0]))):
                tick();point=ordered[index]
                if min(a[1],b[1])<=point[1]<=max(a[1],b[1]) and _cross(a,b,point)==0:
                    candidates.append(point)
            candidates.sort(key=lambda point:(point[axis]-a[axis])/(b[axis]-a[axis]))
            noded+=len(candidates)-2
            for p,q in zip(candidates,candidates[1:]):
                edge=(p,q) if p<q else (q,p)
                incidence.setdefault(edge,[]).append(1 if p<q else -1)
    boundary=[];internal=0
    for (a,b),directions in incidence.items():
        if len(directions)==1:boundary.append((a,b) if directions[0]==1 else (b,a))
        elif len(directions)==2 and sum(directions)==0:internal+=1
        else:_refuse('Triangle field has overused or equally directed shared edges')
    outgoing={};incoming={}
    for a,b in boundary:
        if a in outgoing or b in incoming:_refuse('Triangle boundary has a nonmanifold vertex')
        outgoing[a]=b;incoming[b]=a
    if set(outgoing)!=set(incoming) or not outgoing:_refuse('Triangle boundary is not a closed finite manifold')
    remaining=set(outgoing);loops=[]
    while remaining:
        start=min(remaining);point=start;loop=[]
        while True:
            tick()
            if point not in remaining:_refuse('Triangle boundary cycles intersect or fail to close')
            remaining.remove(point);loop.append(point);point=outgoing[point]
            if point==start:break
        if len(loop)<3 or _area(loop)==0:_refuse('Triangle boundary contains a collapsed cycle')
        loops.append(loop)
    outers=sorted((r for r in loops if _area(r)>0),key=lambda r:r[0])
    holes=sorted((r for r in loops if _area(r)<0),key=lambda r:r[0])
    polygons=[[outer] for outer in outers]
    for hole in holes:
        containers=[]
        for index,outer in enumerate(outers):
            tick()
            if _inside(hole[0],outer,tick):containers.append(index)
        if not containers:_refuse('Triangle boundary hole lacks an enclosing component')
        owner=min(containers,key=lambda index:_area(outers[index]));polygons[owner].append(hole)
    face_area=sum(_cross(*f) for f in faces)/2
    boundary_area=sum(_area(r) for r in loops)
    if face_area!=boundary_area:_refuse('Exact field and boundary areas disagree')
    coordinates=[[[vertices[p] for p in (*ring,ring[0])] for ring in polygon] for polygon in polygons]
    geometry={'type':'Polygon','coordinates':coordinates[0]} if len(coordinates)==1 else {'type':'MultiPolygon','coordinates':coordinates}
    return {'geometry':geometry,'contract':'exact_existing_vertex_triangle_boundary_v1',
        'source_faces':len(faces),'source_vertices':len(vertices),'boundary_edges':len(boundary),
        'internal_edges':internal,'noded_edge_vertices':noded,'components':len(polygons),'holes':len(holes),
        'area_rational':str(face_area),'overlap_bbox_candidates':overlap_candidates,'work_count':work}
