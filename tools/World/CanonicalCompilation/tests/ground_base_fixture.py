"""Captured D39 USGS nodes and fountain geometry over explicitly reduced mechanical cells."""
from copy import deepcopy
from ..app.contracts import canonical_hash
from ..app.spatial import cell_bounds,cell_id,grid_id

CAPTURED_GRID = {'$schema': 'https://alis.world/schemas/world-source/ground-grid-v1.json',
 'schema_version': 1,
 'basis': 'raster_ground_model',
 'frame': {'horizontal': 'EPSG:32618', 'vertical': 'EPSG:3855', 'unit': 'm'},
 'sample_rule': 'grid_nodes',
 'origin': [586153.41, 4513002.45],
 'spacing': [30.0, -30.0],
 'size': [5, 6],
 'samples': [[19.713083267211914,
              19.08769989013672,
              18.376188278198242,
              17.78413200378418,
              17.408720016479492],
             [19.606922149658203,
              18.923734664916992,
              18.207006454467773,
              17.631351470947266,
              17.502418518066406],
             [19.542133331298828,
              19.180795669555664,
              18.601795196533203,
              18.016908645629883,
              17.746410369873047],
             [19.43414878845215,
              19.168493270874023,
              18.813947677612305,
              18.341848373413086,
              18.034015655517578],
             [19.29397964477539,
              19.138185501098633,
              18.905174255371094,
              18.614025115966797,
              18.252599716186523],
             [19.171628952026367,
              19.09177589416504,
              18.921829223632812,
              18.741107940673828,
              18.537036895751953]],
 'nodata': -9999.0}
OLD_NODES = [[177.7, 160.6, 120.6], [181.8, 167.3, 131.3], [175.2, 162.5, 131.2], [165.0, 153.1, 123.8]]
FOUNTAIN_GEOMETRY = {'type': 'MultiPolygon',
 'coordinates': [[[[586193.44, 4512911.5],
                   [586193.49, 4512909.98],
                   [586194.03, 4512908.56],
                   [586194.99, 4512907.38],
                   [586196.28, 4512906.56],
                   [586197.77, 4512906.2],
                   [586199.29, 4512906.34],
                   [586200.74, 4512906.98],
                   [586201.9, 4512908.06],
                   [586202.62, 4512909.47],
                   [586202.85, 4512911.02],
                   [586202.54, 4512912.57],
                   [586201.74, 4512913.93],
                   [586200.52, 4512914.95],
                   [586199.04, 4512915.51],
                   [586197.46, 4512915.56],
                   [586196.0, 4512915.1],
                   [586194.77, 4512914.2],
                   [586193.88, 4512912.96],
                   [586193.44, 4512911.5]]]]}


def fixture():
    grid={'canonical_crs':'EPSG:32618','vertical_datum':'EPSG:3855',
        'origin':[586183.41,4512882.45],'sample_spacing':[30.,30.],
        'cell_quads':[1,1],'halo_samples':1,'height_quantization':.1,
        'coordinate_quantization':.01,'resampling_method':'bilinear'}
    identity=grid_id(grid)
    profile={'profile_id':'captured_ground_base','grid':deepcopy(grid),
        'target_cells':[{'x':x,'y':y} for y in range(3) for x in range(2)],
        'terrain_surface':{'contract_id':'terrain_surface_semantics','contract_version':1,
            'roles':['ground','hydro_transition'],'hydro_transition_radius_m':30.,'water_clearance_m':0.},
        'water_semantics':{'elevation':{
            'standing':{'function_id':'standing_polygon_quantile','function_version':1,'quantile':.5},
            'flowing':{'function_id':'rolling_quantile_l1_isotonic','function_version':1,'quantile':.5,'rolling_radius':1}}}}
    cells={}
    for y in range(3):
        for x in range(2):
            key=cell_id(identity,x,y);rows=deepcopy([r[x:x+2] for r in OLD_NODES[2-y:4-y]])
            halo=[[rows[max(0,min(1,r-1))][max(0,min(1,c-1))] for c in range(4)] for r in range(4)]
            cells[key]={'$schema':'https://alis.world/schemas/world-compiler/terrain-cell-v2.json',
                'schema_version':2,'grid_id':identity,'cell_id':key,'bounds':list(cell_bounds(grid,x,y)),
                'sample_spacing':[30.,30.],'height_quantization':.1,'core_samples':rows,
                'halo_window':{'index_bounds':[x-1,y-1,x+2,y+2],'samples':halo},
                'water_fit':{'core_samples':deepcopy(rows),'halo_window':{'samples':deepcopy(halo)}},
                'vertical_provenance':{'source_ref':'captured_dsm','vertical_datum':'EPSG:3855',
                    'source_accuracy_m':4.,'confidence':'provider_model','sampling_quantization_residual_m':.05},
                'raster_alignment':{'authority':'pinned_gdalwarp_vrt','source_crs':'EPSG:4326',
                    'canonical_crs':'EPSG:32618','resampling_method':'bilinear'},
                'raster_component_dependencies':[],'surface_semantics':{},'height_edges':{},
                'height_border_bands':{},'weight_mask':{},'authored_patch_ids':[]}
    ids=sorted(cells)
    water={'feature_id':'alis:osm:way:394395237','feature_class':'water','geometry':deepcopy(FOUNTAIN_GEOMETRY),
        'owner_cell_id':ids[0],'intersecting_cell_ids':ids,'representations':[],
        'attributes':{'surface_geometry':'polygon','surface_behavior':'standing','surface_group_id':'captured_fountain',
            'surface_function':{'function_id':'standing_polygon_quantile','function_version':1,
                'vertical_basis':'canonical_terrain_polygon_quantile','accuracy':'low_confidence_point_fallback',
                'sample_count':1,'level_m':168.3}}}
    flow={'feature_id':'fixture:flow','feature_class':'water',
        'geometry':{'type':'LineString','coordinates':[[586188.41,4512930.],[586238.41,4512930.]]},
        'owner_cell_id':ids[0],'intersecting_cell_ids':ids,'representations':[],
        'attributes':{'surface_geometry':'ribbon','surface_behavior':'flowing','width_m':2.,'surface_group_id':'mechanical_flow',
            'surface_function':{'function_id':'rolling_quantile_l1_isotonic','function_version':1,
                'vertical_basis':'canonical_terrain_robust_fit','accuracy':'derived_not_surveyed','sample_count':3,
                'knots':[[586188.41,4512930.,160.],[586213.41,4512930.,159.],[586238.41,4512930.,158.]]}}}
    other={'feature_id':'fixture:building','feature_class':'building','geometry':{'type':'Point','coordinates':[586240.,4512960.]},
        'owner_cell_id':ids[-1],'intersecting_cell_ids':[ids[-1]],'attributes':{'height_m':10.}}
    source=deepcopy(CAPTURED_GRID)
    evidence={'basis':'raster_ground_model','canonical_crs':'EPSG:32618','vertical_datum':'EPSG:3855',
        'grid_content_sha256':canonical_hash(source),'source_sha256':'e29b816df45679a779458dc02e8699dd44f09a9b28d8434c61a08c50132b4bc1',
        'processing_sha256':canonical_hash({'captured_source_crop':source,'basis':'D39_qualified_USGS_nodes'})}
    document={'grid':source,'evidence':evidence,'compiler_profile':profile,
        'source_metadata':{'source_crs':'EPSG:4269','source_accuracy_m':None,'confidence':'qualified_model_no_local_hard_bound'},
        'water_conditioning_limits':{'maximum_cut_m':20.,'maximum_support_distance_m':60.,
            'accepted_accuracy':['derived_not_surveyed','low_confidence_point_fallback']}}
    coverage={'profile_id':profile['profile_id'],'grid_id':identity,'grid':deepcopy(grid),
        'cells':[{'cell_id':key} for key in ids]}
    return document,coverage,cells,{f['feature_id']:f for f in [water,flow,other]}
