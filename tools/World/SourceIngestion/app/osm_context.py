"""Qualification of supplementary normalized OSM context, not historical source replay."""
from pathlib import Path
from datetime import datetime, timezone
import math
from .contracts import IngestionError, canonical_hash, file_hash, validate_document


def osm_provider_namespace(provider: str) -> str:
    # Distribution channels differ; their primitive IDs belong to the same provider namespace.
    if provider not in ('OpenStreetMap', 'OpenStreetMap via Geofabrik'):
        raise IngestionError('source_namespace_unknown', 'Supplementary context has no qualified OSM namespace')
    return 'osm'


def osm_context_identity() -> str:
    root=Path(__file__).resolve().parents[1]
    paths=[root/'api.py', root/'app/contracts.py', Path(__file__).resolve(),
           root/'contracts/provider-feature.schema.json']
    return canonical_hash({path.relative_to(root).as_posix():file_hash(path) for path in paths})


def _refuse(message):
    raise IngestionError('source_context_invalid', message)


def _records(document):
    if not isinstance(document, dict) or document.get('$schema') != 'https://alis.world/schemas/world-source/provider-feature-v1.json':
        _refuse('OSM context must use the normalized provider contract')
    validate_document(document, Path('supplementary-context.json'))
    osm_provider_namespace(document['provider'])
    if not all(isinstance(document.get(key), str) and document[key] for key in ('snapshot_id', 'release')) or not document['license'].get('id'):
        _refuse('OSM context requires source and license provenance')
    records = {}
    for feature in document['features']:
        key = feature['provider_feature_id']
        if key in records:
            _refuse('OSM context contains duplicate primitive records')
        records[key] = feature
    return records


def _timestamp(stamp):
    try:
        if isinstance(stamp, int) and not isinstance(stamp, bool):
            date = datetime.fromtimestamp(stamp, tz=timezone.utc)
        elif isinstance(stamp, str):
            date = datetime.fromisoformat(stamp.replace('Z', '+00:00'))
        else:
            _refuse('OSM context primitive timestamp is not qualified')
        if date.tzinfo is None:
            _refuse('OSM context primitive timestamp has no timezone')
    except (ValueError, OverflowError, OSError) as error:
        raise IngestionError('source_context_invalid', 'OSM context primitive timestamp is invalid') from error
    return date


def _primitive(records, key, kind):
    feature = records.get(key) if isinstance(key, str) else None
    if not feature or feature['precision'] != {'source_crs': 'EPSG:4326', 'coordinate_decimals': 7}:
        _refuse('OSM context lacks a complete native primitive at its declared precision')
    properties = feature['properties']
    identifier, version = properties.get('@id'), properties.get('@version')
    if (properties.get('@type') != kind or isinstance(identifier, bool) or not isinstance(identifier, int) or
            identifier <= 0 or key != f'{kind}/{identifier}' or isinstance(version, bool) or
            not isinstance(version, int) or version <= 0):
        _refuse('OSM context primitive identity or version is invalid')
    _timestamp(properties.get('@timestamp'))
    return feature


def _point(value):
    try:
        return (isinstance(value, list) and len(value) == 2 and
                all(not isinstance(v, bool) and isinstance(v, (int, float)) and math.isfinite(v) for v in value) and
                -180 <= value[0] <= 180 and -90 <= value[1] <= 90)
    except OverflowError:
        return False


def osm_way_context(document: dict, provider_feature_id: str) -> tuple[dict, tuple]:
    """Qualify a native way and all ordered node records; geographic policy stays downstream."""
    records = _records(document)
    way = _primitive(records, provider_feature_id, 'way')
    references = way['properties'].get('@way_nodes')
    geometry = way['geometry']
    coordinates = geometry.get('coordinates')
    if (geometry.get('type') != 'LineString' or not isinstance(references, list) or len(references) < 2 or
            any(isinstance(v, bool) or not isinstance(v, int) or v <= 0 for v in references) or
            not isinstance(coordinates, list) or len(coordinates) != len(references) or not all(_point(p) for p in coordinates)):
        _refuse('OSM way context needs its complete ordered native line and node references')
    nodes = tuple(_primitive(records, f'node/{identifier}', 'node') for identifier in references)
    for node, xy in zip(nodes, coordinates, strict=True):
        if node['geometry'].get('type') != 'Point' or not _point(node['geometry'].get('coordinates')) or node['geometry']['coordinates'] != xy:
            _refuse('OSM node context differs from the complete way geometry')
    return way, nodes
