"""Joint support checks for declared structures on a corrected Ground candidate."""
from World.ExecutionEnvironment.geometry.affine import measure_affine_cover
from World.ExecutionEnvironment.geometry.api import partition_triangles
from ...app.contracts import canonical_hash
from ...app.terrain_water_coverage import _intersects
from .geometry import _point, _interpolate
from .shore_geometry import refuse


def bind_staged(staging, coverage, features):
    """Bind native consumers to exact final artifacts after Terrain staging."""
    from ...app.contracts import read_json
    for feature in features.values():
        surface = feature.get('attributes',{}).get('constructed_surface')
        if surface is None:
            continue
        support = {}
        for key in feature['intersecting_cell_ids']:
            entry = next(e for e in coverage['cells'] if e['cell_id']==key)
            descriptor = read_json(staging/entry['path'])['terrain_artifact']
            support[key] = {'path':descriptor['path'],'sha256':descriptor['content_hash'],'byte_size':descriptor['byte_size']}
        surface['model_provenance']['terrain_artifacts'] = support


def support_identity(cells, payloads, identifiers):
    """Bind consumed final height geometry, excluding rendering and provenance."""
    result = {}
    for key in sorted(identifiers):
        if key not in cells:
            refuse('A structure needs a missing Ground cell')
        cell = cells[key]
        quads = [{'coordinate':q['coordinate'],'vertices':[p[:3] for p in q['vertices']],
            'triangles':q['triangles']} for q in sorted(payloads.get(key,{}).get('quads',[]),key=lambda q:q['coordinate'])]
        result[key] = canonical_hash({'bounds':cell['bounds'],'sample_spacing':cell['sample_spacing'],
            'core_samples':cell['core_samples'],'refined_quads':quads})
    return result


def reference_faces(cells, payloads, identifiers, bounds):
    faces = []
    for key in identifiers:
        cell = cells[key]; replaced = {tuple(q['coordinate']):q for q in payloads.get(key,{}).get('quads',[])}
        for y in range(len(cell['core_samples'])-1):
            for x in range(len(cell['core_samples'][0])-1):
                a,b,c,d = [_point(cell,i,j)[:3] for i,j in ((x,y),(x+1,y),(x,y+1),(x+1,y+1))]
                if b[0]<bounds[0] or a[0]>bounds[2] or a[1]<bounds[1] or c[1]>bounds[3]:
                    continue
                quad = replaced.get((x,y))
                faces.extend([[quad['vertices'][i][:3] for i in face] for face in quad['triangles']] if quad else [[a,c,b],[b,c,d]])
    return faces


def validate_structures(repo, cells, payloads, features):
    for feature in features.values():
        surface = feature.get('attributes',{}).get('constructed_surface')
        if surface is None:
            continue
        identifiers = feature['intersecting_cell_ids']
        if surface['model_provenance'].get('terrain_support') != support_identity(cells,payloads,identifiers):
            refuse('The imported structure does not bind its exact consumed final Ground geometry')
        faces = surface['triangles']
        bounds = [min(p[0] for f in faces for p in f),min(p[1] for f in faces for p in f),
            max(p[0] for f in faces for p in f),max(p[1] for f in faces for p in f)]
        reference = reference_faces(cells,payloads,identifiers,bounds)
        minimum = 1000-measure_affine_cover(reference,faces,offset_m=1000)['maximum_height_error_upper_m']
        if minimum < -1e-8:
            refuse('The complete imported structure penetrates corrected Ground')
        for water in features.values():
            if water['feature_class']!='water' or not _intersects(water,bounds):
                continue
            function = water.get('attributes',{}).get('surface_function',{})
            if 'knots' in function or type(function.get('level_m')) not in (int,float):
                refuse('A structural crossing of nonconstant Water needs a distinct support relationship')
            for face in faces:
                for part in partition_triangles(repo,[p[:2] for p in face],water['geometry'],4096)['wet']:
                    if any(_interpolate([list(p)+[1.,0.] for p in face],xy)[2]<function['level_m']+1e-8 for xy in part):
                        refuse('The complete imported structure lacks positive Water clearance')
