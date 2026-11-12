"""Bounded content-addressed input capture shared by explicit model admissions."""
import hashlib
import re

from ...app.contracts import resolve_owned_path
from .contracts import strict_json_document


def read_retained_input(repo, descriptor, retained, refuse, *, native=False):
    keys = {'path', 'sha256', 'byte_size', *(['format'] if native else [])}
    if (not isinstance(descriptor, dict) or set(descriptor) != keys or
            not isinstance(descriptor['sha256'], str) or not re.fullmatch('[0-9a-f]{64}', descriptor['sha256']) or
            type(descriptor['byte_size']) is not int or not 1 <= descriptor['byte_size'] <= 16_000_000 or
            not isinstance(descriptor['path'], str)):
        refuse('Model input descriptors require exact roles, bounded bytes and content pins')
    path = resolve_owned_path(repo, descriptor['path'])
    if path.stat().st_size != descriptor['byte_size']:
        refuse('A declared model input size differs from its immutable byte pin')
    with path.open('rb') as stream:
        payload = stream.read(descriptor['byte_size']+1)
    if len(payload) != descriptor['byte_size'] or hashlib.sha256(payload).hexdigest() != descriptor['sha256']:
        refuse('A declared model input differs from its immutable byte pin')
    document = strict_json_document(payload, descriptor['path'])
    relative = f"refinement/inputs/{descriptor['sha256']}.json"
    if relative not in retained and sum(map(len, retained.values()))+len(payload) > 64_000_000:
        refuse('Retained model inputs exceed their aggregate byte capacity')
    retained[relative] = payload
    stored = {'path': relative, 'sha256': descriptor['sha256'], 'byte_size': len(payload)}
    if native:
        if descriptor['format'] not in ('osm_json', 'sanitized_osm_full_response'):
            refuse('Unknown native source context format')
        stored['format'] = descriptor['format']
        document = document if descriptor['format'] == 'osm_json' else document.get('document')
        if not isinstance(document, dict) or document.get('version') != '0.6' or not isinstance(document.get('elements'), list):
            refuse('Native source context has no declared primitive document')
    return document, stored
