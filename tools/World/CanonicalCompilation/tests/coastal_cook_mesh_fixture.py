"""Export the self-contained captured native cook-support fixture."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT/'tools'))
from World.CanonicalCompilation.tests.coastal_cook_support_fixture import fixture


def main(destination, *, original=False):
    destination = destination.resolve()
    if destination.exists() or destination == ROOT/'tmp' or not destination.is_relative_to(ROOT/'tmp'):
        raise ValueError('Cooked-support consumer fixture requires fresh project scratch')
    payload = fixture(original=original)
    destination.mkdir(parents=True)
    path = destination/'composite-terrain-fixture.json'
    path.write_text(json.dumps(payload, separators=(',', ':'), allow_nan=False)+'\n', encoding='ascii', newline='\n')
    print(json.dumps({'path':path.relative_to(ROOT).as_posix(), 'quads':len(payload['quads']),
                      'faces':sum(len(q['triangles']) for q in payload['quads']),
                      'required_physics_points':len(payload['required_physics_points'])}))


if __name__ == '__main__':
    main(Path(sys.argv[1]), original='--original' in sys.argv[2:])
