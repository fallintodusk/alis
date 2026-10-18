"""Admit frozen processed Ground evidence; no decoder or geographic policy is selected."""
from copy import deepcopy
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import tempfile

from jsonschema import Draft202012Validator, FormatChecker
from referencing import Registry, Resource

from ...app.acquisition import verify_source_file
from ...app.contracts import IngestionError, canonical_hash, exclusive_file_lock, file_hash, read_json
from ...retention.api import retain_sources, validate_storage
from .raster_ground import read_grid

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = 'https://alis.world/schemas/world-source/derived-ground-v2.json'
SINGLE_ROLES = ('raw_source', 'source_ground', 'processed_ground', 'extraction_recipe', 'transform_recipe',
                'extraction_receipt', 'processing_receipt', 'qualification', 'runtime')


def _refuse(message, **details):
    raise IngestionError('derived_ground_invalid', message, **details)


def implementation_identity():
    owner = ROOT.parent
    files = [ROOT / 'api.py', *ROOT.glob('app/*.py'), *ROOT.glob('contracts/*.json'),
             owner / 'app/contracts.py', owner / 'app/acquisition.py',
             owner / 'retention/api.py', *owner.glob('retention/app/*.py'),
             *owner.glob('retention/contracts/*.json'), owner / 'contracts/source-profile.schema.json']
    return canonical_hash({p.relative_to(owner).as_posix(): file_hash(p) for p in sorted(files)})


def _document(product, name):
    schemas = [read_json(p) for p in (ROOT / 'contracts').glob('*.json')]
    schemas.append(read_json(ROOT.parent / 'contracts/source-profile.schema.json'))
    schema = next(s for s in schemas if s['$id'] == read_json(ROOT / 'contracts' / name)['$id'])
    registry = Registry().with_resources((s['$id'], Resource.from_contents(s)) for s in schemas)
    error = next(Draft202012Validator(schema, registry=registry, format_checker=FormatChecker()).iter_errors(product), None)
    if error:
        _refuse('Derived-product descriptor violates its contract', pointer=list(error.absolute_path), reason=error.message)


def _validate(product):
    _document(product, 'derived-ground.schema.json')
    names = ('requested_bounds', 'effective_bounds') if product['basis'] == 'observed_las_ground' else ('canonical_bounds',)
    for name in names:
        bounds = product['query'][name]
        if (any(not math.isfinite(v) for v in bounds) or bounds[0] >= bounds[2] or bounds[1] >= bounds[3]):
            _refuse('Query bounds must be finite and ordered')
    if product['query']['selection_rule'] == 'exact_rectangle' and product['query']['requested_bounds'] != product['query']['effective_bounds']:
        _refuse('Exact rectangle selection cannot declare different effective bounds')
    if any(value is not None and not math.isfinite(value)
           for key, value in product['accuracy'].items() if key != 'confidence'):
        _refuse('Unknown accuracy must remain null, not nonfinite')
    roles = [a['role'] for a in product['artifacts']]
    if any(roles.count(role) != 1 for role in SINGLE_ROLES):
        _refuse('Derived product needs exactly one of each required processing artifact')
    ids = [a['id'] for a in product['artifacts']]
    if len(set(ids)) != len(ids):
        _refuse('Artifact identifiers must be unique')


def _path(root, relative):
    path = PurePosixPath(relative)
    if '\\' in relative or ':' in relative or path.is_absolute() or '..' in path.parts or not path.parts:
        _refuse('Artifact path must remain inside its capsule', path=relative)
    target = root.joinpath(*path.parts).resolve()
    if not target.is_relative_to(root.resolve()) or target == root.resolve():
        _refuse('Artifact resolves outside its capsule', path=relative)
    return target


def _source(artifact):
    return {'source_id': artifact['id'], 'adapter': 'qualified_derived_product',
            'file_name': PurePosixPath(artifact['path']).name, 'expected_bytes': artifact['byte_size'],
            'hashes': {'sha256': artifact['sha256']}}


def _frame(document, expected):
    if 'crs' not in document:
        return
    crs = document['crs']
    horizontal, vertical = expected['horizontal'], expected['vertical']
    def urn(value):
        return value.replace('EPSG:', 'crs:EPSG::', 1) if value.startswith('EPSG:') else None
    names = {horizontal, f'urn:ogc:def:crs:{horizontal.replace(":", "::", 1)}',
             horizontal+'+'+vertical.removeprefix('EPSG:')}
    if urn(horizontal) and urn(vertical):
        names.add(f'urn:ogc:def:crs,{urn(horizontal)},{urn(vertical)}')
    if (not isinstance(crs, dict) or crs.get('type') != 'name' or not isinstance(crs.get('properties'), dict) or
            crs['properties'].get('name') not in names):
        _refuse('Explicit Ground CRS contradicts or cannot qualify the descriptor frame')


def _points(product, root, role='processed_ground'):
    artifact = next(a for a in product['artifacts'] if a['role'] == role)
    document = read_json(_path(root, artifact['path']))
    _frame(document, product['source_frame' if role == 'source_ground' else 'target_frame'])
    features = document.get('features')
    if document.get('type') != 'FeatureCollection' or not isinstance(features, list) or len(features) != product['query']['point_count']:
        _refuse('Processed Ground collection has the wrong type or point count')
    points, unique = [], {}
    for feature in features:
        if not isinstance(feature, dict) or not isinstance(feature.get('geometry'), dict) or not isinstance(feature.get('properties'), dict):
            _refuse('Ground feature shape is invalid')
        geometry = feature.get('geometry', {})
        point = geometry.get('coordinates')
        classification = feature.get('properties', {}).get('class')
        if (feature.get('type') != 'Feature' or type(classification) is not int or classification != 2 or
                geometry.get('type') != 'Point' or not isinstance(point, list) or len(point) != 3 or
                any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in point)):
            _refuse('Processed feature is not finite classified Ground XYZ')
        xy = tuple(point[:2])
        if xy in unique and unique[xy] != point[2]:
            _refuse('Ground observations disagree at one XY')
        unique[xy] = point[2]
        points.append(point)
    if role == 'source_ground':
        digits = product['query']['source_coordinate_decimal_places']
        west, south, east, north = [round(v, digits) for v in product['query']['effective_bounds']]
        if any(round(v, digits) != v for point in points for v in point):
            _refuse('Source coordinates differ from declared serialization precision')
        if any(not (west <= x <= east and south <= y <= north) for x, y, _ in points):
            _refuse('Source Ground falls outside the qualified query')
    return points


def _verified_product(product, root):
    _validate(product)
    for artifact in product['artifacts']:
        verify_source_file(_source(artifact), _path(root, artifact['path']))
    if product['basis'] == 'raster_ground_model':
        paths = {a['role']: _path(root, a['path']) for a in product['artifacts'] if a['role'] in ('source_ground', 'processed_ground')}
        grid = read_json(paths['processed_ground'])
        _document(grid, 'ground-grid.schema.json')
        return read_grid(product, read_json(paths['source_ground']), grid)
    _points(product, root, 'source_ground')
    return _points(product, root)


def _relocated(product):
    admitted = deepcopy(product)
    for artifact in admitted['artifacts']:
        artifact['path'] = f"artifacts/{artifact['id']}/{PurePosixPath(artifact['path']).name}"
    return admitted


def read_ground_product(receipt_path: Path, expected_sha256: str):
    """Caller pins the admitted receipt; rehashed payloads do not imply a fresh raw decode."""
    receipt_path = receipt_path.resolve()
    if file_hash(receipt_path) != expected_sha256:
        _refuse('Admission receipt identity differs from the selected immutable input')
    receipt = read_json(receipt_path)
    _document(receipt, 'derived-ground-admission.schema.json')
    if (receipt.get('kind') != 'derived_ground_admission' or receipt.get('schema_version') != 2 or
            receipt.get('fresh_raw_decode') is not False or receipt.get('production_promotion') is not False or
            receipt.get('implementation_sha256') != implementation_identity()):
        _refuse('Receipt is unsupported or its admission implementation changed')
    source_descriptor = receipt_path.parent / 'input-product.json'
    if file_hash(source_descriptor) != receipt['source_descriptor_sha256']:
        _refuse('Frozen input descriptor changed')
    original = read_json(source_descriptor)
    _validate(original)
    if receipt['basis'] != original['basis']:
        _refuse('Receipt basis differs from its selected input')
    if receipt['inputs_hash'] != canonical_hash({'descriptor_sha256': receipt['source_descriptor_sha256'],
                                                'implementation': receipt['implementation_sha256']}):
        _refuse('Admission request does not bind its frozen descriptor and implementation')
    descriptor_path = receipt_path.parent / 'product.json'
    if file_hash(descriptor_path) != receipt.get('product_sha256'):
        _refuse('Admitted product descriptor changed')
    product = read_json(descriptor_path)
    if canonical_hash(product) != canonical_hash(_relocated(original)):
        _refuse('Admitted descriptor is not the exact relocated input')
    payload = _verified_product(product, receipt_path.parent)
    key = 'point_content_sha256' if product['basis'] == 'observed_las_ground' else 'grid_content_sha256'
    if canonical_hash(payload) != receipt.get(key):
        _refuse('Admitted Ground content identity changed')
    raw = next(a for a in product['artifacts'] if a['role'] == 'raw_source')
    evidence = {'basis': product['basis'], 'canonical_crs': product['target_frame']['horizontal'],
        'vertical_datum': product['target_frame']['vertical'], key: canonical_hash(payload),
        'source_sha256': raw['sha256'], 'processing_sha256': receipt['inputs_hash']}
    if product['basis'] == 'observed_las_ground':
        return {'product': product, 'points': payload, 'evidence': {**evidence, 'classification': 2}}
    return {'product': product, 'grid': payload, 'evidence': evidence}


def admit_ground_product(repo_root: Path, descriptor_path: Path, output_root: Path):
    """Verify and retain the complete qualified product before publishing a candidate receipt."""
    repo_root, descriptor_path, output_root = repo_root.resolve(), descriptor_path.resolve(), output_root.resolve()
    scratch = repo_root / 'tmp'
    if (output_root == scratch or not output_root.is_relative_to(scratch) or
            descriptor_path.is_relative_to(output_root) or output_root.is_relative_to(descriptor_path.parent)):
        _refuse('Admission output must be disjoint from input and confined to project tmp')
    descriptor_bytes = descriptor_path.read_bytes()
    descriptor_hash = hashlib.sha256(descriptor_bytes).hexdigest()
    try:
        product = json.loads(descriptor_bytes)
    except (ValueError, UnicodeError) as error:
        raise IngestionError('derived_ground_invalid', 'Cannot parse the frozen product descriptor') from error
    if not isinstance(product, dict):
        _refuse('Product descriptor is not an object')
    implementation = implementation_identity()
    payload = _verified_product(product, descriptor_path.parent)
    source_descriptors = [_source(a) for a in product['artifacts']]
    descriptor_source = {'source_id': 'derived_product_descriptor', 'adapter': 'qualified_derived_product',
        'file_name': descriptor_path.name, 'expected_bytes': len(descriptor_bytes),
        'hashes': {'sha256': descriptor_hash}}
    if descriptor_source['source_id'] in {a['id'] for a in product['artifacts']}:
        _refuse('Reserved descriptor identity is already used')
    profile = {'sources': [*source_descriptors, descriptor_source]}
    validate_storage(repo_root, profile)
    request = canonical_hash({'descriptor_sha256': descriptor_hash, 'implementation': implementation})
    def verify_current_inputs():
        verify_source_file(descriptor_source, descriptor_path)
        if implementation_identity() != implementation:
            _refuse('Admission implementation changed during verification')
    verify_current_inputs()
    output_root.parent.mkdir(parents=True, exist_ok=True)
    with exclusive_file_lock(output_root.with_suffix('.admission.lock')):
        paths = {a['id']: _path(descriptor_path.parent, a['path']) for a in product['artifacts']}
        with tempfile.TemporaryDirectory(dir=output_root.parent, prefix='derived-ground-') as temporary:
            staging = Path(temporary) / 'candidate'
            staging.mkdir()
            frozen_descriptor = staging / 'input-product.json'
            frozen_descriptor.write_bytes(descriptor_bytes)
            paths[descriptor_source['source_id']] = frozen_descriptor
            if output_root.exists():
                receipt_path = output_root / 'admission.json'
                read_ground_product(receipt_path, file_hash(receipt_path))
                previous = read_json(receipt_path)
                if previous['inputs_hash'] != request:
                    _refuse('Existing output belongs to another derived-product request')
                retain_sources(repo_root, profile, paths)
                verify_current_inputs()
                return previous
            retain_sources(repo_root, profile, paths)
            admitted = _relocated(product)
            for artifact in admitted['artifacts']:
                source = paths[artifact['id']]
                target = _path(staging, artifact['path'])
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
            _verified_product(admitted, staging)
            (staging / 'product.json').write_text(json.dumps(admitted, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')
            key = 'point_content_sha256' if product['basis'] == 'observed_las_ground' else 'grid_content_sha256'
            receipt = {'$schema': 'https://alis.world/schemas/world-source/derived-ground-admission-v2.json',
                'kind': 'derived_ground_admission', 'schema_version': 2, 'basis': product['basis'], 'inputs_hash': request,
                'implementation_sha256': implementation, 'source_descriptor_sha256': descriptor_hash,
                'product_sha256': file_hash(staging / 'product.json'),
                key: canonical_hash(payload), 'fresh_raw_decode': False, 'production_promotion': False}
            (staging / 'admission.json').write_text(json.dumps(receipt, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')
            read_ground_product(staging / 'admission.json', file_hash(staging / 'admission.json'))
            verify_current_inputs()
            os.rename(staging, output_root)
        return receipt
