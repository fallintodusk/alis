"""Bind qualified native geographic rasters to their retained provider FGDC evidence."""
from pathlib import PurePosixPath
import re
from urllib.parse import urlparse
import xml.etree.ElementTree as ET

from ...app.contracts import IngestionError
from .raster_ground import validate_grid, validate_header


def _refuse(message):
    raise IngestionError('derived_ground_invalid', message)


def read_collection(product, source, grid, root, artifact_path):
    """Native decoding and header-to-raw agreement remain preceding qualification."""
    if product['source_frame'] != {'horizontal': 'EPSG:4269', 'vertical': 'EPSG:5703', 'unit': 'm'}:
        _refuse('Native geographic collection requires qualified NAD83/NAVD88 metre evidence')
    artifacts = {a['id']: a for a in product['artifacts']}
    required = {a['id'] for a in product['artifacts'] if a['role'] == 'raw_source'}
    seen, metadata = set(), set()
    for member in source['members']:
        identity, evidence_id = member['raw_artifact_id'], member['metadata_artifact_id']
        raw, evidence = artifacts.get(identity), artifacts.get(evidence_id)
        if (identity not in required or identity in seen or not evidence or
                evidence['role'] != 'supporting_evidence' or evidence_id in metadata):
            _refuse('Native raster members need distinct complete raw and provider-metadata artifact bindings')
        seen.add(identity); metadata.add(evidence_id)
        url = member['source_url']
        if PurePosixPath(raw['path']).name != PurePosixPath(urlparse(url).path).name:
            _refuse('Native raw artifact filename disagrees with its provider download identity')
        payload = artifact_path(root, evidence['path']).read_bytes()
        fgdc = b'<!DOCTYPE metadata SYSTEM "https://www.fgdc.gov/schemas/metadata/fgdc-std-001-1998.dtd">'
        declarations = re.findall(br'<!DOCTYPE\b[^>]*>', re.sub(br'<!--.*?-->', b'', payload, flags=re.S), re.I)
        if (b'\0' in payload or b'<!ENTITY' in payload.upper() or
                (declarations and declarations != [fgdc])):
            _refuse('Provider FGDC evidence cannot declare document entities')
        try:
            document = ET.fromstring(payload)
        except ET.ParseError as error:
            raise IngestionError('derived_ground_invalid', 'Cannot parse retained provider FGDC evidence') from error
        expected = {
            'spref/horizsys/geodetic/horizdn': 'North American Datum of 1983',
            'spref/vertdef/altsys/altdatum': 'North American Vertical Datum of 1988',
            'spref/vertdef/altsys/altunits': 'meters',
        }
        if document.tag != 'metadata' or any(len(document.findall(k)) != 1 or document.findtext(k) != v
                                             for k, v in expected.items()):
            _refuse('Provider FGDC horizontal datum, vertical datum or elevation units contradict the descriptor')
        urls = document.findall('distinfo/stdorder/digform/digtopt/onlinopt/computer/networka/networkr')
        if len(urls) != 1 or urls[0].text != url:
            _refuse('Provider FGDC download identity differs from the selected native raster')
        validate_header(member['header'], product['source_frame'], product['query'],
                        geographic=True, qualified_unit=True)
    if seen != required:
        _refuse('Native raster collection omits a selected raw object')
    return validate_grid(product, grid)
