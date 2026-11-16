"""Admit an explicitly inferred bank over the qualified Ground operation's output."""
from copy import deepcopy
import json
from pathlib import Path

from World.ExecutionEnvironment.geometry.api import execution_identity
from ...app.contracts import canonical_hash
from ...app.terrain_water_coverage import _intersects
from .authored_terrain import domain_identity, validate_consumers
from .contracts import validate_document
from .geometry import SCHEMA, _domain
from .identity import implementation_identity
from .retained_input import read_retained_input
from .shore_geometry import build, refuse

KIND = 'inferred_shore_transition'
CONSTRUCTION = 'inferred_east_bank_v1'
INPUT_SCHEMA = 'https://alis.world/schemas/world-compiler/shore-transition-input-v1.json'


def admit(repo, operation, features):
    if (not isinstance(operation,dict) or set(operation)!={'kind','feature_ids','input'} or
            operation['kind']!=KIND):
        refuse('A shore operation needs exactly its Water selection and retained input')
    retained = {}
    document,descriptor = read_retained_input(repo,operation['input'],retained,refuse)
    if document.get('$schema')!=INPUT_SCHEMA:
        refuse('The declared bank input has a different role')
    validate_document(document,Path(descriptor['path']))
    if canonical_hash(document['effective_water'])!=document['effective_water_sha256']:
        refuse('The retained effective Water snapshot differs from its semantic pin')
    identifier = document['water_feature_id']
    if (operation['feature_ids']!=[identifier] or identifier not in features or
            canonical_hash(features[identifier])!=document['base_water_sha256']):
        refuse('The bank does not consume the immutable base Water it declares')
    return {'kind':KIND,'feature_ids':[identifier],'base_features':{identifier:document['base_water_sha256']},
        'input':descriptor},retained,document


def construct(repo, operation, document, ground_operation, coverage, cells, features):
    identifier = document['water_feature_id']
    if (document['grid_id']!=coverage['grid_id'] or ground_operation is None or
            document['ground_input_sha256']!=ground_operation['input']['sha256'] or identifier not in features or
            canonical_hash(features[identifier])!=document['effective_water_sha256'] or
            features[identifier]!=document['effective_water']):
        refuse('The bank needs its exact qualified Ground input and post-Ground Water')
    selected = document['selected']; domain = _domain(cells,selected)
    validate_consumers(selected,features)
    for feature in features.values():
        if feature['feature_class']=='water' and feature['feature_id']!=identifier and any(_intersects(feature,b) for b in domain):
            refuse('A second Water influence needs a separately qualified joint relationship')
    water = features[identifier]; function = water.get('attributes',{}).get('surface_function',{})
    if (water.get('feature_class')!='water' or water.get('geometry',{}).get('type') not in ('Polygon','MultiPolygon') or
            'knots' in function or function.get('accuracy')!='derived_not_surveyed' or
            type(function.get('level_m')) not in (int,float)):
        refuse('This inferred bank requires constant derived polygon Water')
    outputs,metrics = build(repo,cells,water,selected,document['parameters'])
    consumed = {'shore_input_sha256':operation['input']['sha256'],
        'ground_input_sha256':document['ground_input_sha256'],
        'water_feature_id':identifier,'base_water_sha256':document['base_water_sha256'],
        'water_sha256':canonical_hash(water),'domain_sha256':domain_identity(coverage,cells,selected),
        'implementation_sha256':implementation_identity(),'execution_identity':execution_identity(repo,True)['identity_sha256']}
    for key,output in outputs.items():
        output['quads'].sort(key=lambda q:q['coordinate'])
        output.update({'$schema':SCHEMA,'schema_version':1,'grid_id':coverage['grid_id'],'cell_id':key,
            'construction':CONSTRUCTION,'coordinate_rule':'canonical_boundary_intersections_float64',
            'consumed':deepcopy(consumed),'policy':deepcopy(document['parameters']),'metrics':deepcopy(metrics)})
    return outputs


def declared_terrain(base, child, request, operation, coverage, entries):
    """Bind a frozen wrapper to its retained input and exact staged regular Ground."""
    document = json.loads(child.read(operation['input']['path']))
    validate_document(document,Path(operation['input']['path']))
    grounds = [op for op in request['operations'] if op['kind']=='qualified_ground_base']
    identifier = document['water_feature_id']
    if (len(grounds)!=1 or document['ground_input_sha256']!=grounds[0]['input']['sha256'] or
            document['grid_id']!=coverage['grid_id'] or operation['feature_ids']!=[identifier] or
            operation['base_features']!={identifier:document['base_water_sha256']}):
        refuse('Frozen shore input lost its declared Ground/Water relationship')
    originals = {}; descriptors = {}
    for key,entry in entries.items():
        manifest = json.loads(base.read(entry['path'])); descriptor = manifest['terrain_artifact']
        originals[key] = json.loads(child.read(descriptor['path'])); descriptors[key] = descriptor
    domain_hash = domain_identity(coverage,originals,document['selected'])
    features = [f for entry in entries.values() for f in json.loads(child.read(
        json.loads(base.read(entry['path']))['feature_artifact']['path']))['features'] if f['feature_id']==identifier]
    original_features = [f for entry in entries.values() for f in json.loads(base.read(
        json.loads(base.read(entry['path']))['feature_artifact']['path']))['features'] if f['feature_id']==identifier]
    if (len(features)!=1 or features[0]!=document['effective_water'] or canonical_hash(features[0])!=document['effective_water_sha256'] or
            len(original_features)!=1 or canonical_hash(original_features[0])!=document['base_water_sha256']):
        refuse('Frozen shore has a stale effective or immutable Water pin')
    result = {}
    for key,coordinates in document['selected'].items():
        descriptor = descriptors[key]; relative = descriptor['path'].replace('canonical/terrain/','canonical/refined_terrain/')
        payload = json.loads(child.read(relative)); validate_document(payload,Path(relative))
        consumed = payload['consumed']; raw = child.read(descriptor['path'])
        import hashlib
        expected_base = {'path':descriptor['path'],'sha256':hashlib.sha256(raw).hexdigest(),'byte_size':len(raw)}
        expected_consumed = {'shore_input_sha256':operation['input']['sha256'],
            'ground_input_sha256':document['ground_input_sha256'],'water_feature_id':identifier,
            'base_water_sha256':document['base_water_sha256'],'water_sha256':document['effective_water_sha256'],
            'domain_sha256':domain_hash,**{k:request[k] for k in ('implementation_sha256','execution_identity')}}
        if (payload.get('construction')!=CONSTRUCTION or payload['grid_id']!=coverage['grid_id'] or payload['cell_id']!=key or
                payload['policy']!=document['parameters'] or consumed!=expected_consumed or payload['base']!=expected_base or
                sorted(q['coordinate'] for q in payload['quads'])!=sorted(coordinates)):
            refuse('Frozen shore wrapper differs from its declared staged Ground and complete selected scope')
        result[relative] = key
    return result
