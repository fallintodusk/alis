"""Compile the tiny permanent building fixture through normal source/canonical APIs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(repo / 'tools'))
from World.SourceIngestion.app.cli import main as source_main
from World.CanonicalCompilation.app.pipeline import compile_world
from World.CanonicalCompilation.app.spatial import grid_id

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
args = parser.parse_args()
out = args.output.resolve()
if not out.is_relative_to(repo / 'tmp/world') or out.exists():
    raise ValueError('Use a fresh output under repository tmp/world')
out.mkdir(parents=True)
data = repo / 'Plugins/World/ProjectWorldTestData/Data'
provider = data / 'Fixtures/building_neighborhood/provider.json'
profile = json.loads((data / 'Profiles/SourceIngestion/synthetic_territory_twin.source.json').read_text())
profile['$schema'] = str(repo / 'tools/World/SourceIngestion/contracts/source-profile.schema.json')
profile['profile_id'] = 'building_neighborhood'
source = profile['sources'][0]
source['local_path'] = os.path.relpath(provider, out).replace('\\', '/')
source['expected_bytes'] = provider.stat().st_size
source['hashes']['sha256'] = hashlib.sha256(provider.read_bytes()).hexdigest()
source_path = out / 'source-profile.json'
source_path.write_text(json.dumps(profile, indent=2) + '\n')
if source_main(['run', '--profile', str(source_path), '--output-root', str(out / 'source')]) != 0:
    raise RuntimeError('Building fixture source ingestion refused')
compiler = json.loads((data / 'Profiles/CanonicalCompilation/synthetic_territory_twin.compile.json').read_text())
compiler['$schema'] = 'https://alis.world/schemas/world-compiler/compiler-profile-v1.json'
compiler['profile_id'] = compiler['source_profile_id'] = 'building_neighborhood'
compiler['source_profile'] = source_path.relative_to(repo).as_posix()
compiler['identity_namespace'] = 'building_fixture'
compiler['grid']['sample_spacing'] = [3.0, 3.0]
compiler['grid']['cell_quads'] = [21, 21]
compiler['grid']['height_quantization'] = 0.001
compiler['water_semantics']['surface_groups'] = []
overlay = json.loads((data / 'Fixtures/CanonicalCompilation/synthetic_territory_twin/authored_overlay.json').read_text())
overlay['$schema'] = str(repo / 'tools/World/CanonicalCompilation/contracts/authored-overlay.schema.json')
overlay['grid_id'] = grid_id(compiler['grid'])
overlay_path = out / 'overlay.json'
overlay_path.write_text(json.dumps(overlay, indent=2) + '\n')
compiler['authored_overlay'] = overlay_path.relative_to(repo).as_posix()
compiler_path = out / 'compiler-profile.json'
compiler_path.write_text(json.dumps(compiler, indent=2) + '\n')
result, _ = compile_world(str(compiler_path), source_result=out / 'source/run_result.json', output_root_value=out / 'canonical')
if result['status'] != 'accepted':
    raise RuntimeError('Building fixture canonical compilation refused')
def write(name, value, directory=out):
    path = directory / name
    path.write_text(json.dumps(value, indent=2) + '\n')
    return path

def schema(path, directory=out):
    return os.path.relpath(path, directory).replace('\\', '/')

kit = json.loads((repo / 'Plugins/World/ProjectBuildingAssembly/Data/Kits/original_test_v1.json').read_text())
input_dir = repo / 'tmp/world/inputs/building_assembly' / hashlib.sha256(out.relative_to(repo).as_posix().encode()).hexdigest()[:16]
input_dir.mkdir(parents=True)
kit['$schema'] = schema(repo / 'Plugins/World/ProjectBuildingAssembly/Data/Schemas/modular-kit.schema.json', input_dir)
kit_path = write('kit.json', kit, input_dir)
selection = {'$schema': schema(repo / 'Plugins/World/ProjectWorld/Data/Schemas/project_world_building_selection.schema.json', input_dir),
             'schema_version': 1, 'assemblies': [{'feature_id': 'alis:building_fixture:way:2001', 'kit_source': kit_path.relative_to(repo).as_posix()}]}
selection_path = write('selection.json', selection, input_dir)
write('case-inputs.json', {'kit': kit_path.relative_to(repo).as_posix(), 'selection': selection_path.relative_to(repo).as_posix()})
realization = json.loads((data / 'Profiles/Realization/synthetic_territory_twin.realization.json').read_text())
token = out.name.replace('-', '_')
realization['$schema'] = schema(repo / 'Plugins/World/ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json')
realization['profile_id'] = 'building_d36_' + token
realization['canonical_profile_id'] = 'building_neighborhood'
realization['runtime_profile_id'] = 'none'
root = '/ProjectWorldTestData/Generated/BuildingAssemblyD36/' + token + '/'
realization['map_package'] = root + 'L_Neighborhood'
realization['layers'] = [l for l in realization['layers'] if l['layer_id'] in ('terrain', 'buildings')]
for layer in realization['layers']:
    layer['artifact_root'] = root + layer['layer_id'].title() + '/'
    if layer['layer_id'] == 'buildings':
        layer['settings']['assembly_source'] = selection_path.relative_to(repo).as_posix()
write('realization.json', realization)
authored = json.loads((data / 'Authored/building_neighborhood.json').read_text())
if authored['grid_id'] != overlay['grid_id']:
    raise ValueError('Fixture grid changed; requalify the permanent authored overlay')
print(out / 'canonical/compile_result.json')
