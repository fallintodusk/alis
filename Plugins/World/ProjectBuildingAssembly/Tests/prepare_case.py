"""Materialize admitted inputs and prepare an isolated native prototype case."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(repo / 'tools'))
from World.CanonicalCompilation.api import canonical_active_path, materialize_canonical

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
args = parser.parse_args()
output = args.output.resolve()
if not output.is_relative_to(repo / 'tmp') or output.exists():
    raise ValueError('A fresh native case must be under repository tmp')
profile = repo / 'Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/manhattan_showcase_v1.compile.json'
base = materialize_canonical(repo, canonical_active_path(repo, profile), profile)
output.mkdir(parents=True)
case = {'$schema': os.path.relpath(Path(__file__).parent / 'case.schema.json', output).replace('\\', '/'),
        'canonical_result': str(base), 'canonical_sha256': hashlib.sha256(base.read_bytes()).hexdigest(),
        'feature_id': 'alis:osm:way:250440683', 'output': str(output / 'native'),
        'kit': str(Path(__file__).resolve().parents[1] / 'Data/Kits/original_test_v1.json')}
(output / 'case.json').write_text(json.dumps(case, indent=2)+'\n', encoding='ascii')
print(output / 'case.json')
