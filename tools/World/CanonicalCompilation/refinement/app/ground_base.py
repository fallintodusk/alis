"""Pure whole-domain Ground-base replacement before existing Water fitting and conditioning."""
from copy import deepcopy
import math
from types import SimpleNamespace

from ...app.contracts import CompilerError, canonical_hash
from ...app.spatial import cell_bounds, cell_id, quantize
from ...app.terrain_surface import finalize_terrain_cells
from ...app.water import fit_flow_stations, linear_quantile
from ...app.water_geometry import TerrainSampler, polygon_sampling_dependencies
from ..ground import _grid
from .geometry import EPSILON


def _refuse(message):
    raise CompilerError('ground_base_invalid', message)


def _finite(value):
    try:return type(value)in (int,float) and math.isfinite(value)
    except OverflowError:return False


def _domain(document, coverage, cells, features):
    required={'grid','evidence','compiler_profile','source_metadata'}
    if not isinstance(document,dict) or not required.issubset(document) or set(document)-required-{'water_conditioning_limits'}:
        _refuse('Ground base requires its closed qualified input and compiler contract')
    profile=document['compiler_profile'];grid=profile['grid']
    if (coverage['grid']!=grid or profile['profile_id']!=coverage['profile_id'] or
            grid['vertical_datum']!='EPSG:3855'):
        _refuse('Ground base and immutable canonical profile/grid disagree')
    declared=[item['cell_id'] for item in coverage['cells']]
    if not cells or len(set(declared))!=len(declared) or set(declared)!=set(cells):
        _refuse('Ground replacement must consume every original canonical cell exactly once')
    targets=profile.get('target_cells')
    if targets is None:
        r=profile['target_cell_range']
        targets=[{'x':x,'y':y} for y in range(r['min_y'],r['max_y']+1) for x in range(r['min_x'],r['max_x']+1)]
    identifiers=[cell_id(coverage['grid_id'],p['x'],p['y']) for p in targets]
    if len(set(identifiers))!=len(identifiers) or set(identifiers)!=set(cells):
        _refuse('Compiler target domain differs from the complete immutable domain')
    for target in targets:
        key=cell_id(coverage['grid_id'],target['x'],target['y']);cell=cells[key]
        nx,ny=grid['cell_quads']
        if (cell.get('$schema')!='https://alis.world/schemas/world-compiler/terrain-cell-v2.json' or
                cell.get('cell_id')!=key or cell.get('grid_id')!=coverage['grid_id'] or
                cell['bounds']!=list(cell_bounds(grid,target['x'],target['y'])) or
                cell['sample_spacing']!=grid['sample_spacing'] or cell['height_quantization']!=grid['height_quantization'] or
                len(cell['core_samples'])!=ny+1 or any(len(row)!=nx+1 for row in cell['core_samples'])):
            _refuse('Ground base needs unchanged complete regular-grid identities, shape and XY')
        if cell.get('authored_patch_ids'):
            _refuse('Ground replacement cannot silently discard applied authored Terrain intent')
    for key,feature in features.items():
        if feature['feature_id']!=key or feature['owner_cell_id'] not in cells:
            _refuse('A canonical feature identity or owner is outside the complete domain')
        attrs=feature.get('attributes',{})
        bound=set(attrs.get('civil_grade_reference',{}).get('terrain_artifacts',{}))
        bound.update(attrs.get('road_elevation_profile',{}).get('support_terrain_artifacts',{}))
        if (bound.intersection(cells) or 'constructed_surface' in attrs or
                attrs.get('surface_function',{}).get('function_id')=='identified_static_lined_basin'):
            _refuse('Ground replacement cannot retain stale Terrain-bound civil or constructed support')
    return profile


def _regular_cells(document, coverage, cells):
    grid,evidence,meta=document['grid'],document['evidence'],document['source_metadata']
    if (not isinstance(meta,dict) or set(meta)!={'source_crs','source_accuracy_m','confidence'} or
            not isinstance(meta['source_crs'],str) or not meta['source_crs'].startswith('EPSG:') or
            not isinstance(meta['confidence'],str) or not meta['confidence']):
        _refuse('Qualified source frame, accuracy and confidence must be explicit')
    accuracy=meta['source_accuracy_m']
    if accuracy is not None and (not _finite(accuracy) or accuracy<0):
        _refuse('Source accuracy must be unknown or a finite nonnegative source statistic')
    profile=document['compiler_profile'];gx,gy=grid['spacing'];ox,oy=grid['origin']
    if [gx,-gy]!=profile['grid']['sample_spacing']:
        _refuse('Qualified Ground nodes must share the existing canonical spacing')
    quantum=profile['grid']['height_quantization'];halo=profile['grid']['halo_samples']
    if type(halo)is not int or halo<0 or type(quantum)not in (int,float) or not math.isfinite(quantum) or quantum<=0:
        _refuse('Canonical halo and height quantum are invalid')
    output={};component=canonical_hash({'source':evidence['source_sha256'],'processing':evidence['processing_sha256']})
    for key,original in sorted(cells.items()):
        cell=deepcopy(original);west,_,_,north=cell['bounds'];rows,cols=len(cell['core_samples']),len(cell['core_samples'][0])
        values=[]
        for y in range(-halo,rows+halo):
            row=[]
            for x in range(-halo,cols+halo):
                px,py=west+x*gx,north+y*gy;i,j=round((px-ox)/gx),round((py-oy)/gy)
                if (not 0<=i<grid['size'][0] or not 0<=j<grid['size'][1] or
                        abs(ox+i*gx-px)>EPSILON or abs(oy+j*gy-py)>EPSILON):
                    _refuse('Qualified Ground does not cover an exact original core/halo node')
                row.append(quantize(grid['samples'][j][i],quantum))
            values.append(row)
        core=[r[halo:halo+cols] for r in values[halo:halo+rows]]
        window={**cell['halo_window'],'samples':values}
        cell.update(core_samples=deepcopy(core),halo_window=deepcopy(window),
            water_fit={'core_samples':core,'halo_window':window},authored_patch_ids=[],
            vertical_provenance={'source_ref':'qualified_ground:'+evidence['processing_sha256'],
                'vertical_datum':'EPSG:3855','source_accuracy_m':accuracy,'confidence':meta['confidence'],
                'sampling_quantization_residual_m':quantum*.5},
            raster_alignment={'authority':'qualified_ground_grid_nodes','source_crs':meta['source_crs'],
                'canonical_crs':coverage['grid']['canonical_crs'],'resampling_method':'bilinear'},
            raster_component_dependencies=[{'component_id':component,'sample_semantic_sha256':canonical_hash({
                'qualified_grid':evidence['grid_content_sha256'],'cell_id':key,'quantized_halo':values})}])
        output[key]=cell
    return output


def _flow(function, sampler, configuration, height_step, coordinate_step):
    knots=function.get('knots')
    if (not isinstance(knots,list) or len(knots)<2 or type(function.get('sample_count'))is not int or
            function['sample_count']!=len(knots) or any(not isinstance(p,list) or len(p)!=3 or
            any(not _finite(v) for v in p) for p in knots)):
        _refuse('Flowing Water must retain a complete ordered knot/sample census')
    points=[p[:2] for p in knots]
    return {**function,**fit_flow_stations(points,sampler,configuration,height_step,coordinate_step)}


def construct(repo, document, coverage, cells, all_features, scratch):
    """Return detached regular cells, changed Water records and proof; never publish."""
    try:
        profile=deepcopy(_domain(document,coverage,cells,all_features))
        _grid(document['grid'],document['evidence'],coverage['grid']['canonical_crs'])
        terrain=_regular_cells(document,coverage,cells)
    except (KeyError,TypeError,ValueError,OverflowError) as error:
        raise CompilerError('ground_base_invalid','Ground input, domain or source metadata is malformed') from error
    if 'water_conditioning_limits' in document:
        profile['terrain_surface']['water_conditioning_limits']=deepcopy(document['water_conditioning_limits'])
    sampler=TerrainSampler.from_cells(terrain);waters=[f for _,f in sorted(all_features.items()) if f['feature_class']=='water']
    polygons={f['feature_id']:f['geometry'] for f in waters if f['attributes'].get('surface_behavior')=='standing'}
    points,fallbacks=polygon_sampling_dependencies(repo,scratch/'water',polygons,sampler.core_view(),coverage['grid']['canonical_crs']) if polygons else ({},{})
    changed={};groups={};step=profile['grid']['height_quantization'];config=profile['water_semantics']['elevation']
    for feature in waters:
        attrs=feature['attributes'];old=attrs['surface_function'];behavior=attrs['surface_behavior']
        cfg=config.get(behavior)
        basis={'standing':'canonical_terrain_polygon_quantile','flowing':'canonical_terrain_robust_fit'}.get(behavior)
        if (not cfg or old.get('function_id')!=cfg['function_id'] or old.get('function_version')!=cfg['function_version'] or
                old.get('vertical_basis')!=basis or
                old['function_id'] not in ('standing_polygon_quantile','rolling_quantile_l1_isotonic')):
            _refuse('Water function family/version is outside qualified Ground refitting')
        if behavior=='standing':
            selected=points[feature['feature_id']];raw=[sampler.values[p] for p in selected]
            if not raw:raw=[sampler.core_view().sample(fallbacks[feature['feature_id']])]
            current={**old,'sample_count':len(raw),'level_m':quantize(linear_quantile(raw,cfg['quantile']),step),
                'accuracy':'derived_not_surveyed' if selected else 'low_confidence_point_fallback'}
        elif behavior=='flowing':current=_flow(old,sampler,cfg,step,profile['grid']['coordinate_quantization'])
        else:_refuse('Water behavior is outside qualified Ground refitting')
        group=attrs.get('surface_group_id',feature['feature_id'])
        if not isinstance(group,str) or not group:_refuse('Water group identity is invalid')
        if group in groups and groups[group]!=(old,current):_refuse('Retained Water group members have contradictory old or refitted functions')
        groups[group]=(old,current)
        if current!=old:
            updated=deepcopy(feature);updated['attributes']['surface_function']=current;changed[feature['feature_id']]=updated
    compiled=SimpleNamespace(by_owner={key:[] for key in cells})
    for feature in all_features.values():
        if feature['owner_cell_id'] not in compiled.by_owner:_refuse('A canonical feature owner is outside the complete domain')
        compiled.by_owner[feature['owner_cell_id']].append(changed.get(feature['feature_id'],feature))
    finalize_terrain_cells(terrain,compiled,profile,coverage['grid_id'],
        {'grid_id':coverage['grid_id'],'provenance':[],'terrain_patches':[]})
    proof={'construction':'qualified_ground_base_v1','ground_evidence':deepcopy(document['evidence']),
        'source_metadata':deepcopy(document['source_metadata']),'cell_count':len(terrain),'water_count':len(waters),
        'changed_water_features':sorted(changed),'compiler_profile_sha256':canonical_hash(document['compiler_profile']),
        'water_conditioning_limits':deepcopy(document.get('water_conditioning_limits')),
        'effective_water_conditioning_limits':deepcopy(profile['terrain_surface'].get('water_conditioning_limits')),
        'domain_sha256':canonical_hash({'grid_id':coverage['grid_id'],'grid':coverage['grid'],
            'cells':{key:{'bounds':c['bounds'],'shape':[len(c['core_samples'][0]),len(c['core_samples'])]} for key,c in sorted(cells.items())}})}
    return terrain,changed,proof
