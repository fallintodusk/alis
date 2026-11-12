"""Captured Pier25 axis/nodes/parents with explicit static model support.

The 2 m width, directed axis, node IDs and footprints are captured source geometry.
Deck levels, freeboard and neck hinge are model inputs, not surveyed Pier25 decks.
The flat reduced Terrain reproduces the retained local 0 m support.
Whole-parent qualification and incident junctions are separate acceptance work.
"""
from pathlib import Path

from ..app.contracts import canonical_hash
from ..app.surface_context import surface_context
from .gangway_assembly_fixture import assembly


def fixture(water_level=0., fixed_height=3.55, freeboard=1.2):
    points = [[583300.93, 4508130.52], [583322.21, 4508127.52]]
    nodes = [11484384439, 10882677092]
    road = {'feature_id': 'alis:osm:way:1171058927', 'feature_class': 'road',
            'geometry': {'type': 'LineString', 'coordinates': points},
            'attributes': {'width_m': 2., 'surface_context': surface_context(
                {'bridge': 'yes', 'layer': '1', '@way_nodes': nodes}, 'road',
                {'type': 'LineString', 'coordinates': points})}}
    cells = {'reduced': {'bounds': [583273.41, 4508092.45, 583333.41, 4508152.45],
                         'sample_spacing': [30., 30.], 'core_samples': [[0.] * 3 for _ in range(3)],
                         'surface_semantics': {'core_samples': {
                             'ground': [[1.] * 3 for _ in range(3)],
                             'hydro_transition': [[0.] * 3 for _ in range(3)]}}}}
    complete, floating_body = assembly(Path(__file__).resolve().parents[4], water_level, freeboard, fixed_height)
    supports = []
    for node, role, surface in zip(nodes, ('floating_landing', 'fixed_pier'), (floating_body, complete)):
        support = {**surface, 'role': role, 'provider_node_id': f'node/{node}'}
        if role == 'floating_landing': support['freeboard_m'] = freeboard
        supports.append(support)
    water = {'model': 'static_representative', 'level_m': water_level,
             'canonical_crs': 'EPSG:32618', 'vertical_datum': 'EPSG:3855',
             'source_epoch': {'start': '1983-01-01', 'end': '2001-12-31'},
             'source_sha256': canonical_hash({'synthetic_water_fixture': water_level})}
    limits = {'maximum_longitudinal_grade': .125, 'maximum_cross_slope': .02,
              'maximum_join_step_m': .001, 'minimum_terrain_clearance_m': .1,
              'minimum_water_clearance_m': .4, 'maximum_triangles': 128}
    return road, cells, supports, water, complete, limits
