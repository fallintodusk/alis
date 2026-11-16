"""Finite east-bank affine envelopes; no source admission or publication."""
from collections import Counter
from copy import deepcopy
from fractions import Fraction as F
import math

from World.ExecutionEnvironment.geometry.api import covered_points, validate_triangle_cover
from ...app.contracts import CompilerError
from .geometry import _domain, _external_edges, _point, _check_outer, _check_seams

Q = lambda value: F(str(value))


def refuse(message):
    raise CompilerError('shore_transition_invalid', message)


def value(plane, point):
    return plane[0]*point[0]+plane[1]*point[1]+plane[2]


def plane(a, b, c, channel=2):
    determinant = (b[0]-a[0])*(c[1]-a[1])-(c[0]-a[0])*(b[1]-a[1])
    gx = ((b[channel]-a[channel])*(c[1]-a[1])-(c[channel]-a[channel])*(b[1]-a[1]))/determinant
    gy = ((b[0]-a[0])*(c[channel]-a[channel])-(c[0]-a[0])*(b[channel]-a[channel]))/determinant
    return gx, gy, a[channel]-gx*a[0]-gy*a[1]


def area(poly):
    return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(poly, (*poly[1:],poly[0])))/2 if poly else F(0)


def clip(poly, line, sign):
    result = []
    if not poly:
        return result
    previous = poly[-1]; before = sign*value(line,previous)
    for point in poly:
        after = sign*value(line,point)
        if before*after < 0:
            t = before/(before-after)
            result.append(tuple(previous[k]+t*(point[k]-previous[k]) for k in (0,1)))
        if after >= 0:
            result.append(point)
        previous,before = point,after
    result = list(dict.fromkeys(result))
    return result if area(result) != 0 else []


def envelope(pieces, candidate, minimum):
    result = []
    for poly,prior in pieces:
        line = tuple(a-b for a,b in zip(prior,candidate))
        if not any(line):
            result.append((poly,prior)); continue
        for sign,chosen in ((-1,prior),(1,candidate)) if minimum else ((1,prior),(-1,candidate)):
            part = clip(poly,line,sign)
            if part:
                result.append((part,chosen))
    return result


def on_edge(point, a, b):
    return ((b[0]-a[0])*(point[1]-a[1]) == (b[1]-a[1])*(point[0]-a[0]) and
        min(a[0],b[0]) <= point[0] <= max(a[0],b[0]) and min(a[1],b[1]) <= point[1] <= max(a[1],b[1]))


def _strips(repo, water, bounds, policy, fraction=Q):
    west,south,east,north = map(fraction,bounds); segments = []
    polygons = water['geometry']['coordinates']
    if water['geometry']['type'] == 'Polygon':
        polygons = [polygons]
    for polygon in polygons:
        for ring in polygon:
            for a,b in zip(ring,ring[1:]):
                a,b = tuple(map(fraction,a)),tuple(map(fraction,b))
                if min(a[0],b[0])<east and max(a[0],b[0])>west and min(a[1],b[1])<north and max(a[1],b[1])>south:
                    if a[1] == b[1]:
                        refuse('An east-bank domain cannot contain horizontal or ambiguous Water edges')
                    segments.append((a,b))
    levels = sorted({south,north,*[p[1] for edge in segments for p in edge if south<p[1]<north]})
    strips = []; probes = []
    level = Q(water['attributes']['surface_function']['level_m'])
    for lo,hi in zip(levels,levels[1:]):
        mid = (lo+hi)/2
        match = [(a,b) for a,b in segments if min(a[1],b[1])<mid<max(a[1],b[1])]
        if len(match) != 1:
            refuse('The selected domain needs one complete unambiguous east bank per Y interval')
        a,b = match[0]; slope = (b[0]-a[0])/(b[1]-a[1]); intercept = a[0]-slope*a[1]
        coast = (F(1),-slope,-intercept)
        bank = tuple(Q(policy['bank_grade_x'])*v for v in coast)
        bank = (*bank[:2],bank[2]+level-Q(policy['bank_wet_offset_m']))
        strips.append((lo,hi,coast,bank))
        probes.extend([[float(west),float(mid)],[float(east),float(mid)]])
    membership = covered_points(repo,probes,water['geometry'])
    if membership != [inside for _ in strips for inside in (True,False)]:
        refuse('Water must occupy the west side and Ground the east side throughout this bounded model')
    return strips


def _encoded_metrics(repo, outputs, water, bounds, policy):
    strips = _strips(repo,water,bounds,policy,F); gradient_squared = F(0); wet_max = None
    for payload in outputs.values():
        for quad in payload['quads']:
            for indices in quad['triangles']:
                face = [[F(v) for v in quad['vertices'][i]] for i in indices]
                if area(face)<=0:
                    refuse('The packed field contains a collapsed or reversed triangle')
                height = plane(*face); gradient_squared = max(gradient_squared,height[0]**2+height[1]**2)
                for lo,hi,coast,_ in strips:
                    wet = clip(clip(clip([tuple(p[:2]) for p in face],(F(0),F(1),-lo),1),
                        (F(0),F(1),-hi),-1),coast,-1)
                    for point in wet:
                        actual = value(height,point)
                        wet_max = actual if wet_max is None else max(wet_max,actual)
    level = F(water['attributes']['surface_function']['level_m'])
    if (wet_max is None or wet_max>level or
            gradient_squared>F(policy['maximum_bank_gradient'])**2):
        refuse('The complete packed field violates its Water support or bank grade')
    return math.nextafter(math.sqrt(float(gradient_squared)),math.inf),float(wet_max)


def build(repo, cells, water, selected, policy):
    """Restore conditioned Ground with a bounded inferred bank, preserving its exterior."""
    names = {'bank_grade_x','bank_wet_offset_m','south_collar_zero_y','north_collar_zero_y',
        'collar_grade','maximum_bank_gradient','maximum_triangles','maximum_restore_m'}
    def finite(v):
        try:return type(v) in (int,float) and math.isfinite(v)
        except OverflowError:return False
    if (set(policy) != names or any(not finite(v) for v in policy.values()) or
        any(policy[k]<=0 for k in names-{'south_collar_zero_y','north_collar_zero_y'}) or
        type(policy['maximum_triangles']) is not int or not 2<=policy['maximum_triangles']<=4096 or
        policy['south_collar_zero_y']>=policy['north_collar_zero_y']):
        refuse('The affine bank requires explicit finite geometry and bounded quality limits')
    domain = _domain(cells,selected)
    if not 1<=len(domain)<=64:
        refuse('This local transition supports at most 64 original quads')
    bounds = [min(b[0] for b in domain),min(b[1] for b in domain),max(b[2] for b in domain),max(b[3] for b in domain)]
    if sum((b[2]-b[0])*(b[3]-b[1]) for b in domain) != (bounds[2]-bounds[0])*(bounds[3]-bounds[1]):
        refuse('The selected transition must be a complete rectangle with no missing neighbor')
    level = Q(water['attributes']['surface_function']['level_m'])
    strips = _strips(repo,water,bounds,policy)
    south = (F(0),Q(policy['collar_grade']),level-Q(policy['collar_grade'])*Q(policy['south_collar_zero_y']))
    north = (F(0),-Q(policy['collar_grade']),level+Q(policy['collar_grade'])*Q(policy['north_collar_zero_y']))
    pieces = []; restore = F(0); wet_max = level
    for _,(key,x,y,original) in sorted(domain.items()):
        cell = cells[key]
        if cell.get('raster_alignment',{}).get('authority') != 'qualified_ground_grid_nodes':
            refuse('A shore transition requires qualified Ground rather than an unqualified surface raster')
        raw = deepcopy(cell); raw['core_samples'] = raw['water_fit']['core_samples']
        corners = [list(map(Q,p)) for p in original]
        source = [_point(raw,i,j) for i,j in ((x,y),(x+1,y),(x,y+1),(x+1,y+1))]
        if any(not finite(v) for point in source for v in point):
            refuse('Unknown or nonfinite Ground is not an admitted bank control')
        source = [list(map(Q,p)) for p in source]
        if any(a[2]>b[2] for a,b in zip(corners,source)):
            refuse('Restoration cannot override a final height above its original qualified Ground')
        for indices in ((0,2,1),(1,2,3)):
            tri = [corners[i] for i in indices]; observed = plane(*[source[i] for i in indices])
            old = plane(*tri); poly = [tuple(p[:2]) for p in tri]; parts = []
            semantic = [plane(*tri,k) for k in (3,4)]
            for lo,hi,coast,bank in strips:
                part = clip(clip(poly,(F(0),F(1),-lo),1),(F(0),F(1),-hi),-1)
                if not part:
                    continue
                branch = [(part,observed)]
                for candidate in (bank,south,north):
                    branch = envelope(branch,candidate,True)
                branch = envelope(branch,old,False)
                for p,h in branch:
                    wet = clip(p,coast,-1)
                    if wet:
                        wet_max = max(wet_max,*[value(h,v) for v in wet])
                    if any(not value(old,v)<=value(h,v)<=value(observed,v) for v in p):
                        refuse('The bank must stay between the retained field and qualified Ground')
                    restore = max(restore,*[value(h,v)-value(old,v) for v in p])
                parts.extend(branch)
            if len({p for _,p in parts}) == 1:
                parts = [(poly,parts[0][1])]
            if sum(area(p) for p,_ in parts) != area(poly):
                refuse('Affine envelope lost complete original-half coverage')
            pieces.extend((key,x,y,p,h,semantic) for p,h in parts)
    if wet_max>level or restore>Q(policy['maximum_restore_m']) or len(pieces)>4096:
        refuse('The complete bank violates wet support, restoration or bounded-work limits')
    nodes = {p for _,_,_,poly,_,_ in pieces for p in poly}
    grouped = {(key,x,y):[] for key,x,y,_ in domain.values()}; gradient = 0.
    for key,x,y,poly,h,semantic in pieces:
        ring = []
        for a,b in zip(poly,(*poly[1:],poly[0])):
            matching = sorted((p for p in nodes if on_edge(p,a,b)),
                key=lambda p:(p[0]-a[0])*(b[0]-a[0])+(p[1]-a[1])*(b[1]-a[1]))
            ring.extend(matching[:-1])
        center = tuple(sum(p[k] for p in ring)/len(ring) for k in (0,1))
        # Every shared boundary node participates: point-cloud triangulation can
        # discard a nearly collinear mandatory node and leave a microscopic gap.
        for a,b in zip(ring,(*ring[1:],ring[0])):
            face = [[float(p[0]),float(p[1]),float(value(h,p)),*[float(value(s,p)) for s in semantic]] for p in (a,b,center)]
            p,q,r = face; determinant = (q[0]-p[0])*(r[1]-p[1])-(r[0]-p[0])*(q[1]-p[1])
            if determinant<=1e-12:
                refuse('Canonical restoration collapsed a positive-area envelope face')
            gx = ((q[2]-p[2])*(r[1]-p[1])-(r[2]-p[2])*(q[1]-p[1]))/determinant
            gy = ((q[0]-p[0])*(r[2]-p[2])-(r[0]-p[0])*(q[2]-p[2]))/determinant
            gradient = max(gradient,math.hypot(gx,gy)); grouped[key,x,y].append(face)
    if gradient>policy['maximum_bank_gradient']:
        refuse('The actual encoded triangles exceed their declared bank gradient')
    outputs = {}; outer = _external_edges(domain)
    for _,(key,x,y,(a,b,c,d)) in sorted(domain.items()):
        faces = grouped[key,x,y]; vertices = []; index = {}; triangles = []
        if len(faces)>policy['maximum_triangles']:
            refuse('A final quad exceeds its declared triangle capacity')
        validate_triangle_cover(repo,{'type':'Polygon','coordinates':[[p[:2] for p in (a,c,d,b,a)]]},[[p[:2] for p in f] for f in faces])
        for face in faces:
            ids = []
            for point in face:
                xy = tuple(point[:2])
                if xy not in index:
                    index[xy] = len(vertices); vertices.append(point)
                elif max(abs(a-b) for a,b in zip(vertices[index[xy]],point))>1e-8:
                    refuse('Adjacent affine pieces disagree on height or semantic weights')
                ids.append(index[xy])
            triangles.append(ids)
        for point in vertices:
            _check_outer(point,outer)
        outputs.setdefault(key,{'quads':[]})['quads'].append({'coordinate':[x,y],
            'vertices':vertices,'triangles':triangles,'water_triangles':[]})
    _check_seams(outputs,domain)
    gradient,wet_height = _encoded_metrics(repo,outputs,water,bounds,policy)
    return outputs, {'affected_quads':len(domain),'triangles':sum(len(f) for f in grouped.values()),
        'maximum_bank_gradient':gradient,'maximum_restore_m':float(restore),'wet_height_upper_m':wet_height}
