"""Pure raster-Ground construction; source authentication and publication are separate."""
import math
from pathlib import Path

from World.ExecutionEnvironment.api import ExecutionEnvironmentError
from World.ExecutionEnvironment.geometry.api import execution_identity,triangulate_points,validate_triangle_cover
from ..app.contracts import CompilerError,canonical_hash,file_hash
from .app.geometry import EPSILON,_domain,_external_edges,_check_seams
from .ground_resolution import station_sets


def _refuse(message):
    raise CompilerError('ground_surface_invalid',message)


def implementation_identity():
    """This optional capability does not change the unconsumed Water/feature child."""
    path=Path(__file__).resolve();world=path.parents[2];owner=path.parent
    files=[path,owner/'ground_resolution.py',owner/'app/geometry.py',owner.parent/'app/contracts.py',
        owner.parent/'contracts/terrain-cell.schema.json',
        world/'SourceIngestion/supplemental/contracts/ground-grid.schema.json']
    return canonical_hash({p.relative_to(world).as_posix():file_hash(p) for p in files})


def _grid(grid,evidence,canonical_crs):
    if (not isinstance(grid,dict) or not isinstance(evidence,dict) or
        grid.get('basis')!='raster_ground_model' or evidence.get('basis')!='raster_ground_model' or
        grid.get('sample_rule')!='grid_nodes' or
        grid.get('frame')!={'horizontal':evidence.get('canonical_crs'),'vertical':'EPSG:3855','unit':'m'} or
        evidence.get('vertical_datum')!='EPSG:3855' or
        not isinstance(canonical_crs,str) or not canonical_crs or evidence.get('canonical_crs')!=canonical_crs or
        'classification' in evidence):
        _refuse('Raster Ground basis, node rule and qualified canonical frame are required')
    if any(not isinstance(evidence.get(key),str) or len(evidence[key])!=64 or
           any(c not in '0123456789abcdef' for c in evidence[key])
           for key in ('grid_content_sha256','source_sha256','processing_sha256')):
        _refuse('Ground content and processing identities are missing')
    size,origin,spacing=grid.get('size'),grid.get('origin'),grid.get('spacing')
    if (not isinstance(size,list) or len(size)!=2 or any(type(v)is not int or v<2 for v in size) or
        any(not isinstance(row,list) or len(row)!=2 or
            any(type(v)not in (int,float) or not math.isfinite(v) for v in row) for row in (origin,spacing)) or
        spacing[0]<=0 or spacing[1]>=0 or type(grid.get('nodata'))not in (int,float) or not math.isfinite(grid['nodata'])):
        _refuse('Ground grid dimensions, origin, spacing or NoData are invalid')
    rows=grid.get('samples')
    if (not isinstance(rows,list) or len(rows)!=size[1] or
        any(not isinstance(row,list) or len(row)!=size[0] or
            any(type(v)not in (int,float) or not math.isfinite(v) or v==grid['nodata'] for v in row) for row in rows)):
        _refuse('Ground grid is incomplete, nonfinite or contains unknown samples')
    if canonical_hash(grid)!=evidence['grid_content_sha256']:
        _refuse('Ground grid differs from its declared content identity')


def _outer(point,edges):
    """Only redundant points on unchanged outer lines may be omitted."""
    for a,b in edges:
        dx,dy=b[0]-a[0],b[1]-a[1]
        cross=(point[0]-a[0])*dy-(point[1]-a[1])*dx
        t=((point[0]-a[0])*dx+(point[1]-a[1])*dy)/(dx*dx+dy*dy)
        if abs(cross)<=EPSILON*math.hypot(dx,dy) and -EPSILON<=t<=1+EPSILON:
            if any(abs(point[i]-a[i]-t*(b[i]-a[i]))>EPSILON for i in range(2,5)):
                _refuse('Source Ground changes an unselected outer boundary; neighbor or source policy is unresolved')
            if EPSILON<t<1-EPSILON:return True
    return False


def _layout(points,grid):
    a,b,c,d=points;gx,gy=grid['spacing'];ox,oy=grid['origin']
    x0,y0=round((a[0]-ox)/gx),round((a[1]-oy)/gy)
    nx,ny=round((b[0]-a[0])/gx),round((c[1]-a[1])/gy)
    if (nx<1 or nx!=ny or x0<0 or y0<0 or x0+nx>=grid['size'][0] or y0+ny>=grid['size'][1] or
        any(abs(actual-expected)>EPSILON for actual,expected in
            ((ox+x0*gx,a[0]),(oy+y0*gy,a[1]),(nx*gx,b[0]-a[0]),(ny*gy,c[1]-a[1])))):
        _refuse('Ground grid must cover the quad without extrapolation and share its half-diagonal stations')
    return x0,y0,nx


def _quad(repo,points,grid,edges,budget,stations=None):
    a,b,c,d=points;gx,gy=grid['spacing'];ox,oy=grid['origin']
    if any(any(not math.isfinite(v) or not 0<=v<=1 for v in p[3:]) or
           abs(math.fsum(p[3:])-1)>EPSILON for p in points):
        _refuse('Original affine semantic weights are invalid')
    x0,y0,nx=_layout(points,grid);ny=nx
    # The final ABI is bounded; refuse excessive input before native triangulation.
    if stations is None and (nx-1)*(ny-1)>budget:_refuse('Ground input exceeds the bounded final-quad triangle capacity')
    vertices=[];halves=[[],[]];indices={}
    samples=((x,y) for y in range(ny+1) for x in range(nx+1)) if stations is None else sorted(stations,key=lambda p:(p[1],p[0]))
    for x,y in samples:
        xy=[ox+(x0+x)*gx,oy+(y0+y)*gy]
        triangle=(a,c,b) if x+y<=nx else (b,c,d)
        # Integer source stations give convex weights without subtractive edge cancellation.
        counts=(nx-x-y,y,x) if x+y<=nx else (ny-y,nx-x,x+y-nx)
        point=[*xy,grid['samples'][y0+y][x0+x],
               *[math.fsum(n*p[i] for n,p in zip(counts,triangle))/nx for i in (3,4)]]
        if any(v<0 or v>1 for v in point[3:]) or abs(sum(point[3:])-1)>EPSILON:
            _refuse('Original affine semantic weights are invalid')
        if _outer(point,edges):continue
        index=len(vertices);vertices.append(point);indices[tuple(xy)]=index
        if x+y<=nx:halves[0].append(xy)
        if x+y>=nx:halves[1].append(xy)
    faces=[]
    try:
        for half in halves:
            for face in triangulate_points(repo,half):
                faces.append([indices[tuple(half[i])] for i in face])
        if len(faces)>budget:_refuse('Ground triangulation exceeds the declared final-quad budget')
        if {i for face in faces for i in face}!=set(range(len(vertices))):
            _refuse('Native Ground triangulation did not retain every selected source node')
        validate_triangle_cover(repo,{'type':'Polygon','coordinates':[[a[:2],c[:2],d[:2],b[:2],a[:2]]]},
            [[vertices[i][:2] for i in face] for face in faces])
    except ExecutionEnvironmentError as error:
        raise CompilerError(error.code,str(error),**error.details) from error
    return {'vertices':vertices,'triangles':faces,'water_triangles':[]}


def build_ground_surface(repo_root,cells,grid,evidence,selected,*,maximum_triangles,canonical_crs,resolution=None):
    """Return unpublishable quads; callers authenticate the exact fine product first."""
    _grid(grid,evidence,canonical_crs)
    if type(maximum_triangles)is not int or not 2<=maximum_triangles<=4096:
        _refuse('Ground triangle budget must fit the existing final-quad ABI')
    if (not isinstance(selected,dict) or any(not isinstance(key,str) or not isinstance(quads,(list,tuple)) or
        any(not isinstance(q,(list,tuple)) or len(q)!=2 or any(type(v)is not int for v in q) for q in quads)
        for key,quads in selected.items())):
        _refuse('Ground selection must name explicit integer quad coordinates')
    domain=_domain(cells,selected)
    if not domain:_refuse('At least one explicit Ground quad is required')
    if any(cells[key].get('raster_alignment',{}).get('canonical_crs')!=canonical_crs for key in selected):
        _refuse('Selected canonical Terrain and Ground use different horizontal frames')
    edges=_external_edges(domain);outputs={}
    stations=None if resolution is None else station_sets(domain,
        {bounds:_layout(item[3],grid) for bounds,item in domain.items()},resolution,maximum_triangles)
    for bounds,(identifier,x,y,points) in sorted(domain.items()):
        quad=_quad(repo_root,points,grid,edges,maximum_triangles,
            None if stations is None else stations[bounds]);quad['coordinate']=[x,y]
        outputs.setdefault(identifier,{'quads':[]})['quads'].append(quad)
    _check_seams(outputs,domain)
    return {'construction':'source_ground_node_tin_v1','surfaces':outputs,'consumed':{
        'implementation_sha256':implementation_identity(),
        'geometry_execution_identity':execution_identity(repo_root,True)['identity_sha256'],
        'ground_evidence':dict(evidence),'grid_content_sha256':canonical_hash(grid),
        'resolution_sha256':canonical_hash(None if resolution is None else sorted(resolution,key=lambda r:(r['cell_id'],r['quad']))),
        'canonical_crs':canonical_crs,
        'domain_sha256':canonical_hash({str(bounds):{'cell_id':item[0],'quad':list(item[1:3]),'points':item[3]}
                                      for bounds,item in sorted(domain.items())}),
        'maximum_triangles':maximum_triangles}}
