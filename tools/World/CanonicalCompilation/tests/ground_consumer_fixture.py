"""Self-contained typed Ground consumer fixture; no source authority admission."""
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.CanonicalCompilation.tests.ground_query_fixture import main as ground_fixture
from World.CanonicalCompilation.tests.refinement_fixture import make_base


def main(destination):
    ground_fixture(destination)
    make_base(destination/'base')
    source=destination/'ground-typed-fixture.json'
    payload=json.loads(source.read_text())
    payload.update(road_id='fixture:ground-road',road_class='footway',road_width_m=2.,
        road_points=[[10.,12.],[50.,18.]],selected_cells=[cell['cell_id'] for cell in payload['cells']],
        base_result=str(destination/'base/compile_result.json'),
        output_path=str(destination/'native-road.json'),building_output_path=str(destination/'native-building.json'))
    for index,cell in enumerate(payload['cells']):
        cell['cell_x']=index;cell['cell_y']=0;cell['samples_x']=2;cell['samples_y']=2
    (destination/'road-ground-fixture.json').write_text(json.dumps(payload)+'\n',encoding='ascii')


if __name__=='__main__':
    main(Path(sys.argv[1]).resolve())
