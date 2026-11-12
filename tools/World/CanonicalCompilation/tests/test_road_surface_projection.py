"""The canonical Road consumes retained UV coverage, even when XY mapping succeeds."""
from pathlib import Path
import unittest

from ..app.contracts import CompilerError
from ..refinement.app.road_surface_projection import project_straight_surface

ROOT = Path(__file__).resolve().parents[4]


class RoadSurfaceProjectionTests(unittest.TestCase):
    def test_canonical_rounding_cannot_drop_consumed_parameter_coverage(self):
        offset = 1e16
        # Finite canonical source edges cross the ribbon at odd local stations;
        # the much coarser canonical lattice can collapse the mapped triangles.
        local = [[[-4., -4.], [-2., -4.], [2., 4.]],
                 [[-4., -4.], [2., 4.], [-4., 4.]],
                 [[-2., -4.], [24., -4.], [24., 4.]],
                 [[-2., -4.], [24., 4.], [2., 4.]]]
        faces = [[[offset+x, offset+y, 3.] for x, y in face] for face in local]
        with self.assertRaises(CompilerError):
            project_straight_surface(ROOT, faces, [[offset, offset], [offset+20., offset]], 4., 128)


if __name__ == '__main__': unittest.main()
