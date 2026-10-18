"""Native point triangulation and polygon union; no geographic classification or policy."""
import ctypes as C
import json
import math

from .kernel import _Geometry, _local_polygon
from ...app.contracts import ExecutionEnvironmentError


def _error(message):
    raise ExecutionEnvironmentError('native_geometry_input', message)


def triangulate_points(repo_root, points):
    if (not isinstance(points, list) or len(points)<3 or
            any(not isinstance(p, list) or len(p)!=2 or
                any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in p) for p in points) or
            len({tuple(p) for p in points})!=len(points)):
        _error('Triangulation needs distinct finite XY points')
    origin = min(points)
    local = [[p[i]-origin[i] for i in (0,1)] for p in points]
    indices = {tuple(p):i for i,p in enumerate(local)}
    if len(indices)!=len(points):
        _error('Local translation collapsed distinct input points')
    native = _Geometry(repo_root)
    try:
        delaunay = native.bind('GEOSDelaunayTriangulation_r',[C.c_void_p,C.c_void_p,C.c_double,C.c_int],C.c_void_p)
        cloud = native.read({'type':'MultiPoint','coordinates':local})
        mesh = native.own(delaunay(native.context,cloud,0.,0))
        count = native.count(native.context,mesh)
        if count<1:
            _error('Point cloud does not span a triangulated domain')
        faces = []
        for i in range(count):
            sequence = native.sequence(native.context,native.ring(native.context,native.part(native.context,mesh,i)))
            size = C.c_uint()
            if not sequence or native.size(native.context,sequence,C.byref(size))!=1 or size.value!=4:
                _error('Native point triangulation did not return triangles')
            face = []
            for corner in range(3):
                x,y = C.c_double(),C.c_double()
                if native.x(native.context,sequence,corner,C.byref(x))!=1 or native.y(native.context,sequence,corner,C.byref(y))!=1:
                    _error('Native point coordinate read failed')
                index = indices.get((x.value,y.value))
                if index is None:
                    _error('Native triangulation introduced an unsupported point')
                face.append(index)
            a,b,c = [local[index] for index in face]
            if (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])<0:
                face.reverse()
            offset = face.index(min(face))
            faces.append(face[offset:]+face[:offset])
        return sorted(faces)
    finally:
        native.close()


def polygon_union(repo_root, polygons, domain):
    """Return a native union and measured exterior area against an explicit domain."""
    if not polygons:
        _error('Union requires nonempty polygon pieces')
    origin = min(p for polygon in polygons for ring in polygon['coordinates'] for p in ring)
    native = _Geometry(repo_root)
    try:
        shapes = [native.read(_local_polygon(p,origin)) for p in polygons]
        region = native.read(_local_polygon(domain,origin))
        clone = native.bind('GEOSGeom_clone_r',[C.c_void_p,C.c_void_p],C.c_void_p)
        collection_fn = native.bind('GEOSGeom_createCollection_r',[C.c_void_p,C.c_int,C.POINTER(C.c_void_p),C.c_uint],C.c_void_p)
        duplicates = [clone(native.context,p) for p in shapes]
        if not all(duplicates):
            for pointer in duplicates:
                if pointer:
                    native.destroy(native.context,pointer)
            _error('Native union could not clone its input geometries')
        copies = (C.c_void_p*len(shapes))(*duplicates)
        pointer = collection_fn(native.context,7,copies,len(shapes))
        if not pointer:
            for pointer in duplicates:
                native.destroy(native.context,pointer)
            _error('Native union collection creation failed')
        collection = native.own(pointer)
        union = native.bind('GEOSUnaryUnion_r',[C.c_void_p,C.c_void_p],C.c_void_p)
        result = native.own(union(native.context,collection))
        if native.valid(native.context,result)!=1:
            _error('Native union returned invalid topology')
        exterior = native.own(native.difference(native.context,result,region))
        writer_new = native.bind('GEOSGeoJSONWriter_create_r',[C.c_void_p],C.c_void_p)
        writer_write = native.bind('GEOSGeoJSONWriter_writeGeometry_r',[C.c_void_p,C.c_void_p,C.c_void_p,C.c_int],C.c_void_p)
        writer_free = native.bind('GEOSGeoJSONWriter_destroy_r',[C.c_void_p,C.c_void_p],None)
        free = native.bind('GEOSFree_r',[C.c_void_p,C.c_void_p],None)
        writer = writer_new(native.context)
        if not writer:
            _error('Native union writer creation failed')
        output = None
        try:
            output = writer_write(native.context,writer,result,-1)
            if not output:
                _error('Native union serialization failed')
            geometry = json.loads(C.string_at(output))
        finally:
            if output:
                free(native.context,output)
            writer_free(native.context,writer)
        def restore(value):
            if isinstance(value,list) and len(value)>=2 and all(isinstance(v,(int,float)) for v in value):
                return [value[i]+origin[i] for i in (0,1)]
            return [restore(v) for v in value]
        if geometry['type'] not in ('Polygon','MultiPolygon'):
            _error('Native union is not a positive-area polygon')
        geometry['coordinates'] = restore(geometry['coordinates'])
        return {'geometry':geometry, 'area_m2':native.measure(result), 'outside_area_m2':native.measure(exterior)}
    finally:
        native.close()


def covered_points(repo_root, points, domain):
    if (not isinstance(points,list) or not points or any(not isinstance(p,list) or len(p)!=2 or
            any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in p) for p in points)):
        _error('Point membership requires finite XY positions')
    origin = min(points)
    native = _Geometry(repo_root)
    try:
        region = native.read(_local_polygon(domain,origin))
        covers = native.bind('GEOSCovers_r',[C.c_void_p,C.c_void_p,C.c_void_p],C.c_byte)
        result = []
        for point in points:
            shape = native.read({'type':'Point','coordinates':[point[i]-origin[i] for i in (0,1)]})
            value = covers(native.context,region,shape)
            if value not in (0,1):
                _error('Native point membership is unknown')
            result.append(bool(value))
        return result
    finally:
        native.close()
