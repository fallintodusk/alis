"""OSM contributors/ODbL: way 1171058926, captured canonical 0.01 m ring.

The seam between neck nodes 11484384440/10882677091 is an identified model
inference. Heights/freeboard below are explicit test models, never survey data.
The complete fixed parent is reused from the existing captured support fixture.
"""
from copy import deepcopy

from World.ExecutionEnvironment.geometry.api import triangulate_polygon, polygon_union
from ..app.contracts import canonical_hash
from .constructed_support_fixture import fixture as fixed_fixture

SOURCE = {'provider_id': 'way/1171058926', 'timestamp': '2024-01-04T03:32:05Z',
          'nodes': [10882677087, 10882677088, 10882677089, 11484384440, 11484384441,
                    10882677092, 11484384442, 10882677091, 10882677090, 10882677087],
          'xy': [[583294.77, 4508136.4], [583293.42, 4508127.23], [583302.42, 4508125.92],
                 [583302.87, 4508128.99], [583322., 4508126.2], [583322.21, 4508127.52],
                 [583322.39, 4508128.86], [583303.56, 4508131.61], [583304.06, 4508135.04],
                 [583294.77, 4508136.4]]}


def assembly(repo, water_level, freeboard, fixed_height):
    ring = SOURCE['xy']
    parent = fixed_fixture()[2]
    if parent['type'] == 'MultiPolygon':
        parent = {'type': 'Polygon', 'coordinates': parent['coordinates'][0]}
    components = {'fixed_pier': parent,
        'floating_landing': {'type': 'Polygon', 'coordinates': [[ring[i] for i in (0, 1, 2, 3, 7, 8, 0)]]},
        'gangway': {'type': 'Polygon', 'coordinates': [[ring[i] for i in (3, 4, 5, 6, 7, 3)]]}}
    floating = water_level+freeboard
    faces = []
    for role, footprint in components.items():
        for face in triangulate_polygon(repo, footprint, 32):
            faces.append([[*p, floating if role == 'floating_landing' or role == 'gangway'
                           and p in (ring[3], ring[7]) else fixed_height] for p in face])
    metadata = {'basis': 'identified_model', 'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
                'source_epoch': {'start': '2024-01-04', 'end': '2024-01-04'},
                'source_sha256': canonical_hash({'source': SOURCE, 'components': components,
                    'seam_basis': 'inferred_neck_nodes', 'fixed_height_m': fixed_height,
                    'freeboard_m': freeboard, 'water_level_m': water_level})}
    whole = {**metadata, 'footprint': polygon_union(repo, list(components.values()),
             components['fixed_pier'])['geometry'], 'triangles': faces}
    body = {**metadata, 'footprint': deepcopy(components['floating_landing']),
            'triangles': [[[ *p, floating] for p in face] for face in
                          triangulate_polygon(repo, components['floating_landing'], 16)]}
    return whole, body
