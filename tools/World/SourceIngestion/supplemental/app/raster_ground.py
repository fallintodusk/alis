"""Validate a qualified raster Ground model without relabelling it as LAS returns."""
import math

from ...app.contracts import IngestionError


def _refuse(message):
    raise IngestionError('derived_ground_invalid', message)


def _crs_id(component):
    identifier = component.get('id')
    if (not isinstance(identifier, dict) or identifier.get('authority') != 'EPSG' or
            type(identifier.get('code')) is not int):
        _refuse('Qualified raster CRS component has no explicit EPSG identifier')
    return f"EPSG:{identifier['code']}"


def read_grid(product, source, grid):
    """Qualification owns native processing; admission binds its frozen header and grid."""
    query, frame = product['query'], product['source_frame']
    size = source.get('size')
    transform = source.get('geoTransform')
    bands = source.get('bands')
    if any(not isinstance(source.get(name), dict) for name in ('stac', 'coordinateSystem', 'metadata')):
        _refuse('Qualified source raster metadata is malformed')
    if not isinstance(source['metadata'].get(''), dict):
        _refuse('Qualified source raster pixel metadata is malformed')
    if (not isinstance(size, list) or len(size) != 2 or any(type(v) is not int or v < 1 for v in size) or
            not isinstance(transform, list) or len(transform) != 6 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in transform) or
            transform[1] <= 0 or transform[5] >= 0 or transform[2] != 0 or transform[4] != 0 or
            not isinstance(bands, list) or len(bands) != 1 or not isinstance(bands[0], dict)):
        _refuse('Source raster header is missing, rotated or not a single north-up grid')
    native_crs = source['stac'].get('proj:projjson')
    if not isinstance(native_crs, dict):
        _refuse('Qualified raster header needs its native structured CRS')
    components = native_crs.get('components') if native_crs.get('type') == 'CompoundCRS' else [native_crs]
    if (not isinstance(components, list) or len(components) not in (1, 2) or
            (native_crs.get('type') == 'CompoundCRS' and len(components) != 2) or
            any(not isinstance(c, dict) for c in components) or
            [c.get('type') for c in components] not in (['ProjectedCRS'], ['ProjectedCRS', 'VerticalCRS'])):
        _refuse('Qualified source raster CRS components are unsupported')
    authority = source['stac'].get('proj:epsg')
    wkt = source['coordinateSystem'].get('wkt')
    if (_crs_id(components[0]) != frame['horizontal'] or type(authority) is not int or
            f'EPSG:{authority}' != frame['horizontal'] or not isinstance(wkt, str)):
        _refuse('Qualified source raster CRS contradicts the descriptor')
    vertical = frame['vertical'].removeprefix('EPSG:')
    if not frame['vertical'].startswith('EPSG:') or not vertical.isdecimal():
        _refuse('Qualified source raster has no named vertical frame')
    if ('VERTCRS[' in wkt) != (len(components) == 2):
        _refuse('Qualified raster WKT and structured vertical components disagree')
    if len(components) == 2 and _crs_id(components[1]) != frame['vertical']:
        _refuse('Explicit source raster vertical CRS contradicts the descriptor')
    unit = bands[0].get('unit')
    units = {'m': {'m', 'metre', 'meter'}, 'us_ft': {'US survey foot'}}
    if not isinstance(unit, str) or unit not in units[frame['unit']] or source['metadata'][''].get('AREA_OR_POINT') != 'Area':
        _refuse('Qualified raster units or pixel interpretation contradict the descriptor')
    source_nodata = bands[0].get('noDataValue')
    if (source_nodata is not None and (type(source_nodata) not in (int, float) or not math.isfinite(source_nodata)) or
            source_nodata != query['source_nodata']):
        _refuse('Source raster NoData does not match its processing contract')
    if grid['frame'] != product['target_frame'] or grid['frame']['unit'] != 'm':
        _refuse('Processed Ground grid contradicts its canonical frame')
    nx, ny = grid['size']
    ox, oy = grid['origin']
    dx, dy = grid['spacing']
    nodata = grid['nodata']
    if (any(type(v) is not int for v in (nx, ny, *query['grid_size'])) or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in (ox, oy, dx, dy, nodata)) or
            dx <= 0 or dy >= 0 or nodata != query['target_nodata'] or
            grid['size'] != query['grid_size'] or len(grid['samples']) != ny or
            any(len(row) != nx for row in grid['samples'])):
        _refuse('Processed Ground grid has inconsistent size, spacing or NoData')
    if any(type(v) not in (int, float) or not math.isfinite(v) or v == nodata
           for row in grid['samples'] for v in row):
        _refuse('Processed Ground grid contains unknown or nonfinite samples')
    bounds = [ox, oy+(ny-1)*dy, ox+(nx-1)*dx, oy]
    if any(not math.isfinite(v) for v in bounds) or bounds != query['canonical_bounds']:
        _refuse('Processed Ground nodes do not cover the qualified bounds')
    return grid
