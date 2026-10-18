"""Pure contained-standing construction from separately qualified Ground observations."""
from collections import Counter, defaultdict
from copy import deepcopy
import heapq
import math
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import triangulate_points, polygon_union, covered_points
from ...app.contracts import CompilerError, canonical_hash
from ...app.spatial import quantize
from ...app.water import linear_quantile
from .contracts import validate_document


def _refuse(message, **details):
    raise CompilerError('contained_water_unsupported',message,**details)


def _spill(points, neighbors, seed, boundary):
    costs,queue = {seed:points[seed][2]},[(points[seed][2],seed)]
    while queue:
        cost,node = heapq.heappop(queue)
        if cost!=costs[node]:
            continue
        if node in boundary:
            return cost
        for other in neighbors[node]:
            candidate = max(cost,points[other][2])
            if candidate<costs.get(other,math.inf):
                costs[other] = candidate
                heapq.heappush(queue,(candidate,other))
    _refuse('Ground TIN has no connected route to its observed boundary')


def build_contained_water(repo_root, water, ground_points, context, evidence, *, quantile, height_step, canonical_crs,
                          footprint_model='mapped_containment'):
    """No publication/admission: the caller must authenticate complete source/frame evidence."""
    attrs = water.get('attributes',{})
    geometry = water.get('geometry',{})
    if footprint_model not in ('mapped_containment','observed_ground_component'):
        _refuse('Footprint construction model is unknown')
    polygons = geometry.get('coordinates',[]) if geometry.get('type')=='MultiPolygon' else [geometry.get('coordinates',[])]
    if footprint_model=='observed_ground_component' and (len(polygons)!=1 or len(polygons[0])!=1):
        _refuse('Observed footprint cannot reinterpret disconnected or explicitly excluded source regions')
    if (water.get('feature_class')!='water' or attrs.get('surface_geometry')!='polygon' or
            attrs.get('surface_behavior')!='standing' or geometry.get('type') not in ('Polygon','MultiPolygon') or
            (geometry.get('type')=='MultiPolygon' and len(geometry.get('coordinates',[]))!=1) or
            context.get('geometry_sha256')!=canonical_hash(geometry) or context.get('amenity')!='fountain' or
            context.get('barrier')!='kerb' or any(key in context for key in ('ele','depth','height'))):
        _refuse('Standing constructed-basin context is missing, mismatched or needs metric interpretation')
    if (isinstance(quantile,bool) or not isinstance(quantile,(int,float)) or not math.isfinite(quantile) or not 0<=quantile<=1 or
            isinstance(height_step,bool) or not isinstance(height_step,(int,float)) or not math.isfinite(height_step) or height_step<=0):
        _refuse('Quantile and quantization must be explicit finite model inputs')
    if (evidence.get('classification')!=2 or type(evidence.get('classification')) is not int or
            not isinstance(canonical_crs,str) or not canonical_crs or evidence.get('canonical_crs')!=canonical_crs or
            evidence.get('vertical_datum')!='EPSG:3855' or
            evidence.get('point_content_sha256')!=canonical_hash(ground_points) or
            any(not isinstance(evidence.get(key),str) or len(evidence[key])!=64 or
                any(c not in '0123456789abcdef' for c in evidence[key]) for key in ('source_sha256','processing_sha256'))):
        _refuse('Ground class, point identity or qualified canonical frame is missing')
    if (not isinstance(ground_points,list) or len(ground_points)<3 or
            any(not isinstance(p,list) or len(p)!=3 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or
                not math.isfinite(v) for v in p) for p in ground_points)):
        _refuse('Ground observations are not finite XYZ positions')
    unique = {}
    for point in ground_points:
        xy = tuple(point[:2])
        if xy in unique and unique[xy]!=point[2]:
            _refuse('Ground observations have conflicting elevations at one XY')
        unique[xy] = point[2]
    points = [[*xy,z] for xy,z in sorted(unique.items())]
    faces = triangulate_points(repo_root,[p[:2] for p in points])
    membership = covered_points(repo_root,[p[:2] for p in points],geometry)
    interior = [i for i,inside in enumerate(membership) if inside]
    if not interior:
        _refuse('Mapped domain contains no qualified Ground observations')
    seed = min(interior,key=lambda i:(points[i][2],i))
    level = quantize(linear_quantile([points[i][2] for i in interior],quantile),height_step)
    edges = Counter()
    for face in faces:
        edges.update(tuple(sorted((face[i],face[(i+1)%3]))) for i in range(3))
    if any(count not in (1,2) for count in edges.values()):
        _refuse('Ground TIN has invalid edge incidence')
    boundary = {i for edge,count in edges.items() if count==1 for i in edge}
    neighbors = defaultdict(set)
    for a,b in edges:
        neighbors[a].add(b); neighbors[b].add(a)
    spill = _spill(points,neighbors,seed,boundary)
    if not points[seed][2]<level<spill:
        _refuse('Quantized model has no wet depth or reaches the observed spill',level_m=level,spill_level_m=spill)
    connected,pending = {seed},[seed]
    while pending:
        node = pending.pop()
        for other in neighbors[node]:
            if other not in connected and points[other][2]<=level:
                connected.add(other); pending.append(other)
    if connected.intersection(boundary):
        _refuse('Actual wet component reaches the observed Ground boundary')
    polygons = []
    for face in faces:
        if not connected.intersection(face):
            continue
        triangle = [points[i] for i in face]
        clipped = []
        for a,b in zip(triangle,[triangle[1],triangle[2],triangle[0]]):
            if a[2]<=level:
                clipped.append(a[:2])
            if min(a[2],b[2])<level<max(a[2],b[2]):
                fraction = (level-a[2])/(b[2]-a[2])
                clipped.append([a[i]+fraction*(b[i]-a[i]) for i in (0,1)])
        if len(clipped)>=3:
            origin = clipped[0]
            area = sum((a[0]-origin[0])*(b[1]-origin[1])-(b[0]-origin[0])*(a[1]-origin[1])
                       for a,b in zip(clipped,clipped[1:]+clipped[:1]))
            if area!=0:
                polygons.append({'type':'Polygon','coordinates':[[*clipped,clipped[0]]]})
    united = polygon_union(repo_root,polygons,geometry)
    if footprint_model=='mapped_containment' and united['outside_area_m2']>0:
        _refuse('Actual wet component leaves the mapped domain; truncation or expansion is not supported',
            outside_mapped_area_m2=united['outside_area_m2'],wet_area_m2=united['area_m2'],level_m=level,spill_level_m=spill)
    if united['geometry']['type']!='Polygon':
        _refuse('Wet component has only point contacts rather than one connected open interior')
    changed = deepcopy(water)
    changed['geometry'] = united['geometry']
    function = {'$schema':'https://alis.world/schemas/world-compiler/contained-water-v1.json',
        'function_id':'contained_ground_quantile','function_version':1,'vertical_basis':'qualified_ground_tin',
        'accuracy':'derived_not_surveyed','level_m':level,'spill_level_m':spill,'sample_count':len(interior),
        'quantile':quantile,'quantization_m':height_step,
        'ground_tin_sha256':canonical_hash({'points':points,'triangles':faces,'evidence':evidence}),
        'context_sha256':canonical_hash(context),'wet_geometry_sha256':canonical_hash(united['geometry']),
        'mapped_geometry_sha256':canonical_hash(geometry),'footprint_model':footprint_model,
        'baseline_function_sha256':canonical_hash(attrs['surface_function'])}
    validate_document(function,Path('contained-water.json'))
    changed['attributes']['surface_function'] = function
    return {'feature':changed,'ground':{'vertices':points,'triangles':faces,'evidence':deepcopy(evidence)},
        'wet_area_m2':united['area_m2'],'outside_mapped_area_m2':united['outside_area_m2']}
