"""Pin an existing saved regional scene for the exact render-only PIE observer."""
import argparse
import hashlib
import json
import math
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def prepare(repo, scene, original, presentation, plan, output):
    repo = repo.resolve()
    scene, original, presentation, plan, output = [Path(p).resolve()
        for p in (scene, original, presentation, plan, output)]
    scratch = repo/'tmp/world'
    if (not scene.is_relative_to(scratch) or not original.is_relative_to(scratch)
            or not output.is_relative_to(scratch) or output.exists()
            or output.is_relative_to(scene) or scene.is_relative_to(output)):
        raise ValueError('Saved scene, original fixture and fresh separate output must use repository tmp/world')
    for path in (presentation, plan):
        if not path.is_relative_to(repo) or not path.is_file():
            raise ValueError('Presentation and camera plan must be readable repository inputs')
    sections = scene/'sections.json'
    expected = read(sections)
    if digest(original) != expected['fixture_sha256']:
        raise ValueError('Original canonical fixture differs from the saved scene receipt')
    original_document = read(original)
    result = (repo/original_document['canonical_result']['path']).resolve()
    if (not result.is_relative_to(scratch)
            or digest(result) != original_document['canonical_result']['sha256']):
        raise ValueError('Original fixture no longer binds its exact canonical result')
    plugins = list((scene/'runtime_plugin').glob('*.uplugin'))
    if len(plugins) != 1:
        raise ValueError('Saved scene must declare exactly one native runtime plugin')
    plugin = plugins[0]
    files = [plugin, sections, original]
    for path in sorted((plugin.parent/'Content').rglob('*')):
        if not path.is_file():
            continue
        if path.suffix not in ('.umap', '.uasset', '.ubulk', '.uexp', '.uptnl'):
            raise ValueError('Saved scene contains an unsupported content artifact')
        files.append(path)
    if len(files) > 1024:
        raise ValueError('Saved regional inventory exceeds the native observer bound')
    map_file = plugin.parent/'Content/SectionMap.umap'
    if map_file not in files or not any(p.suffix == '.uasset' for p in files):
        raise ValueError('Saved regional scene is missing its map or native actor packages')
    saved = {p.relative_to(repo).as_posix(): digest(p) for p in files}
    camera = read(plan)['vantages'][0]
    start = [camera[k] for k in ('x', 'y', 'z')]
    if not all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in start):
        raise ValueError('The observer needs a finite first camera position')
    document = {'schema_version': 1, 'map': f'/{plugin.stem}/SectionMap',
        'saved_files': saved, 'sections': sections.relative_to(repo).as_posix(),
        'original_fixture': original.relative_to(repo).as_posix(),
        'presentation_profile': presentation.relative_to(repo).as_posix(),
        'presentation_sha256': digest(presentation), 'vantage_plan': plan.relative_to(repo).as_posix(),
        'vantage_plan_sha256': digest(plan), 'start': start,
        'scope': 'Read-only saved regional map; compiled Terrain/Water PIE appearance. No Hero, streaming or geographic admission.'}
    output.mkdir(parents=True)
    path = output/'input.json'
    path.write_text(json.dumps(document, indent=2)+'\n', encoding='ascii')
    launch = {'input': path.relative_to(repo).as_posix(), 'input_sha256': digest(path),
              'plugin': plugin.relative_to(repo).as_posix(), 'map': document['map']}
    (output/'launch.json').write_text(json.dumps(launch, indent=2)+'\n', encoding='ascii')
    return launch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--scene', type=Path, required=True)
    parser.add_argument('--original-fixture', type=Path, required=True)
    parser.add_argument('--presentation-profile', type=Path, required=True)
    parser.add_argument('--vantage-plan', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(REPO, args.scene, args.original_fixture,
        args.presentation_profile, args.vantage_plan, args.output), indent=2))


if __name__ == '__main__':
    main()
