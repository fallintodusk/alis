"""Disposable terminal constraint for source-connected, independently fitted channels."""
from copy import deepcopy
from datetime import date, datetime, timezone
import math
from pathlib import Path

from World.SourceIngestion.api import IngestionError, osm_provider_namespace, osm_way_context
from ...app.contracts import CompilerError, canonical_hash
from .contracts import validate_document


def _refuse(message):
    raise CompilerError('water_confluence_unsupported', message)


def _function(feature):
    attributes = feature.get('attributes', {})
    function = attributes.get('surface_function', {})
    knots = function.get('knots')
    if (feature.get('feature_class') != 'water' or attributes.get('surface_geometry') != 'ribbon' or
            attributes.get('surface_behavior') != 'flowing' or attributes.get('surface_role') != 'flow_axis' or
            function.get('function_id') != 'rolling_quantile_l1_isotonic' or
            type(function.get('function_version')) is not int or function['function_version'] != 1 or
            function.get('accuracy') != 'derived_not_surveyed' or function.get('vertical_basis') != 'canonical_terrain_robust_fit' or
            not isinstance(knots, list) or len(knots) < 2 or
            any(not isinstance(p, list) or len(p) != 3 or
                any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in p) for p in knots) or
            any(b[2] > a[2] for a,b in zip(knots, knots[1:]))):
        _refuse('Confluence construction requires a finite monotone original derived channel fit')
    return function


def _source(feature, document, projected):
    references = feature.get('source_refs', [])
    if len(references) != 1:
        _refuse('Confluence construction requires one qualified source way per channel')
    reference = references[0]
    provider_id = reference.get('provider_feature_id')
    try:
        if osm_provider_namespace(reference.get('provider')) != osm_provider_namespace(document.get('provider')):
            _refuse('Supplementary and accepted source namespaces differ')
        way,nodes = osm_way_context(document,provider_id)
    except IngestionError as error:
        raise CompilerError(error.code,str(error),**error.details) from error
    if (way['provider_class'] != 'water' or
            way['properties'].get('waterway') != feature['attributes'].get('water_class') or
            feature['attributes'].get('surface_group_members') != [provider_id] or
            feature['feature_id'] != 'alis:osm:'+provider_id.replace('/',':')):
        _refuse('Source way class, group membership or canonical identity differs')
    try:
        release = date.fromisoformat(reference['release'])
        cutoff = datetime(release.year,release.month,release.day,tzinfo=timezone.utc)
        for item in (way,*nodes):
            stamp = item['properties']['@timestamp']
            timestamp = datetime.fromtimestamp(stamp,tz=timezone.utc) if type(stamp) is int else datetime.fromisoformat(stamp.replace('Z','+00:00'))
            if timestamp > cutoff:
                _refuse('Supplementary primitive version postdates the accepted release')
    except (KeyError,ValueError,TypeError,OverflowError,OSError) as error:
        raise CompilerError('water_confluence_unsupported','Source release or primitive timestamp is unqualified') from error
    geometry = projected.get(provider_id,{})
    points = geometry.get('coordinates')
    identifiers = [node['provider_feature_id'] for node in nodes]
    if (geometry.get('type') != 'LineString' or not isinstance(points,list) or len(points) != len(nodes) or
            len(set(identifiers)) != len(identifiers) or
            any(not isinstance(p,list) or len(p) != 2 or
                any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in p) for p in points) or
            any(a == b for a,b in zip(points,points[1:]))):
        _refuse('Source channel projection is incomplete, closed, repeated or degenerate')
    knots = _function(feature)['knots']
    positions = [[i for i,k in enumerate(knots) if p == k[:2]] for p in points]
    if (points[0] != knots[0][:2] or points[-1] != knots[-1][:2] or
            any(len(indices) != 1 for indices in positions) or
            any(b[0] <= a[0] for a,b in zip(positions,positions[1:]))):
        _refuse('Complete directed source projection differs from the accepted fit axis')
    return way,nodes,points,identifiers


def _controls(properties):
    if 'location' in properties and properties['location'] not in ('surface', 'overground'):
        _refuse('Source channel location is not an admitted open surface waterway')
    if any(key in properties for key in ('ele','height','depth','incline','barrier','lock','lock_name','lock_ref','man_made')):
        _refuse('Mapped elevation or hydraulic control requires its own interpretation')
    for key in ('bridge','tunnel','covered','ford'):
        if key in properties and properties[key] not in ('no','false','0'):
            _refuse('Source crossing mode is not an admitted open-channel confluence')
    if 'layer' in properties and properties['layer'] != '0':
        _refuse('Relative source layer is not an absolute confluence elevation')
    if (properties.get('natural') == 'waterfall' or 'waterway' in properties and
            properties['waterway'] not in ('drain','ditch','stream','river','canal')):
        _refuse('Mapped hydraulic controls or unsupported waterway semantics require their own interpretation')


def build_confluence_constraint(features: list, context: dict, projected: dict) -> dict:
    """Construct changed features only; publication must authenticate base and forward geometry."""
    if len(features) != 2 or features[0].get('feature_id') == features[1].get('feature_id'):
        _refuse('A bounded confluence requires two distinct source channels')
    ordered = sorted(features,key=lambda f:f['feature_id'])
    sources = [_source(feature,context,projected) for feature in ordered]
    shared = set(sources[0][3]) & set(sources[1][3])
    if len(shared) != 1:
        _refuse('Confluence requires one exact shared source node, not a nearby crossing')
    node_id = next(iter(shared))
    eligible = [i for i in range(2) if sources[i][3][-1] == node_id and
                0 < sources[1-i][3].index(node_id) < len(sources[1-i][3])-1]
    if len(eligible) != 1:
        _refuse('Source direction does not identify one tributary endpoint and continuing receiver')
    tributary_index = eligible[0]
    receiver_index = 1-tributary_index
    tributary,receiver = ordered[tributary_index],ordered[receiver_index]
    source_a,source_b = sources[tributary_index],sources[receiver_index]
    receiver_node_index = source_b[3].index(node_id)
    for source in sources:
        _controls(source[0]['properties'])
    for node in (*source_a[1][-2:],*source_b[1][receiver_node_index-1:receiver_node_index+2]):
        _controls(node['properties'])
    xy = source_a[2][-1]
    if xy != source_b[2][receiver_node_index]:
        _refuse('Shared source node has conflicting projected positions')
    baseline,receiving = _function(tributary),_function(receiver)
    receiving_knots = [k for k in receiving['knots'] if k[:2] == xy]
    if len(receiving_knots) != 1:
        _refuse('Receiver has no unique accepted knot at the shared source node')
    target = receiving_knots[0][2]
    original = baseline['knots'][-1][2]
    if target > original:
        _refuse('This bounded constraint cannot raise Water or infer an upstream impoundment')
    if target == original:
        return {}
    changed = deepcopy(tributary)
    knots = deepcopy(baseline['knots'])
    knots[-1][2] = target
    changed['attributes']['surface_function'] = {
        '$schema':'https://alis.world/schemas/world-compiler/water-confluence-v1.json',
        'function_id':'constrained_flowing_knots','function_version':1,
        'vertical_basis':'source_node_constrained_canonical_terrain_fit','accuracy':'derived_not_surveyed',
        'sample_count':len(knots),'knots':knots,'baseline_function':deepcopy(baseline),
        'constraint':{'version':1,'method':'shared_source_node_terminal_lowering',
            'provider_node_id':node_id,'receiver_feature_id':receiver['feature_id'],
            'receiver_function_sha256':canonical_hash(receiving),
            'normalized_context_content_sha256':canonical_hash(context),
            'baseline_terminal_m':original,'target_m':target,'lowering_m':original-target,
            'affected_segment':[knots[-2][:2],knots[-1][:2]],
            'source_interpretation':'derived_free_flow_confluence_not_surveyed'}}
    validate_document(changed['attributes']['surface_function'],Path('water-confluence.json'))
    return {changed['feature_id']:changed}
