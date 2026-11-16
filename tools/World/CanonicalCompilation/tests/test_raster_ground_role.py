"""Raster Ground evidence is not classified point evidence for basin construction."""
import hashlib
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tools'))
from World.SourceIngestion.tests import test_derived_ground as source_fixture
from World.CanonicalCompilation.app.contracts import CompilerError,canonical_hash
from World.CanonicalCompilation.refinement.api import build_contained_water


class RasterGroundRoleTests(unittest.TestCase):
    def test_raster_model_cannot_enter_classified_contained_water_constructor(self):
        fixture=source_fixture.DerivedGroundTests();fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        fixture.raster_product()
        api=fixture.api();api.admit_ground_product(fixture.repo,fixture.profile,fixture.output)
        digest=hashlib.sha256((fixture.output/'admission.json').read_bytes()).hexdigest()
        ground=api.read_ground_product(fixture.output/'admission.json',digest)
        water={'feature_class':'water','geometry':{'type':'Polygon','coordinates':[[[0,0],[1,0],[0,1],[0,0]]]},
               'attributes':{'surface_geometry':'polygon','surface_behavior':'standing'}}
        context={'amenity':'fountain','barrier':'kerb','geometry_sha256':canonical_hash(water['geometry'])}
        with self.assertRaises(CompilerError) as error:
            build_contained_water(ROOT,water,[[0.,0.,1.],[1.,0.,1.],[0.,1.,1.]],context,
                ground['evidence'],quantile=.5,height_step=.1,canonical_crs='EPSG:32618')
        self.assertEqual('contained_water_unsupported',error.exception.code)
        self.assertIn('Ground class',str(error.exception))
