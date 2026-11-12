"""Local bridge support must equal dense geometry without visiting unused quads."""
from copy import deepcopy
import math
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))

from ..app.contracts import CompilerError, canonical_hash
from ..refinement.app import road_elevation
from .road_support_fixture import fixture

def dense_support(cells, points, width):
    """Independent full-grid reference for the original support geometry."""
    domains = [(min(a[0], b[0]) - width/2, min(a[1], b[1]) - width/2,
                max(a[0], b[0]) + width/2, max(a[1], b[1]) + width/2)
               for a, b in zip(points, points[1:])]
    faces, consumed = [], {}
    for identifier, cell in sorted(cells.items()):
        west, _, _, north = cell['bounds']
        sx, sy = cell['sample_spacing']
        rows = cell['core_samples']
        for y in range(len(rows) - 1):
            for x in range(len(rows[0]) - 1):
                def point(i, j):
                    return [west + i*sx, north - j*sy, rows[j][i]]
                a, b, c, d = point(x, y), point(x+1, y), point(x, y+1), point(x+1, y+1)
                if any(not (d[0] < w or a[0] > e or a[1] < s or d[1] > n)
                       for w, s, e, n in domains):
                    faces.extend(((a, b, c), (b, d, c)))
                    consumed[identifier] = canonical_hash(cell)
    return faces, consumed


class RoadSupportTests(unittest.TestCase):
    def support(self, cells, points, width):
        return road_elevation._terrain(cells, points, width)

    def test_captured_bridge_visits_only_intersecting_quads(self):
        cells, road = fixture()
        points, width = road['geometry']['coordinates'], road['attributes']['width_m']
        expected = dense_support(cells, points, width)
        before = deepcopy((cells, road))
        with patch.object(road_elevation, '_point', wraps=road_elevation._point) as point:
            actual = self.support(cells, points, width)
        self.assertEqual(expected, actual)
        self.assertEqual(before, (cells, road))
        self.assertLessEqual(point.call_count, 2*len(expected[0]),
                             'At most four source vertices per intersecting quad should be evaluated')

    def test_grid_boundaries_nextafter_and_reversed_stations_match_dense(self):
        cells, road = fixture()
        cell = cells['captured_cell']
        west, _, _, north = cell['bounds']
        sx, sy = cell['sample_spacing']
        x, y = west + 10*sx, north - 12*sy
        for xedge in (x, math.nextafter(x, -math.inf), math.nextafter(x, math.inf)):
            for yedge in (y, math.nextafter(y, -math.inf), math.nextafter(y, math.inf)):
                for reverse in (False, True):
                    points = [[xedge+1., yedge-1.], [xedge+sx-1., yedge-1.]]
                    if reverse:
                        points.reverse()
                    with self.subTest(x=xedge, y=yedge, reverse=reverse):
                        self.assertEqual(dense_support(cells, points, 2.), self.support(cells, points, 2.))

    def test_multicell_support_and_remote_context_match_dense(self):
        cells, road = fixture()
        cell = cells['captured_cell']
        width = cell['bounds'][2] - cell['bounds'][0]
        for name, offset in (('neighbor', width), ('remote', 20*width)):
            other = deepcopy(cell)
            other['bounds'][0] += offset
            other['bounds'][2] += offset
            cells[name] = other
        x, y = cell['bounds'][2], road['geometry']['coordinates'][0][1]
        points = [[x-20., y], [x+20., y]]
        expected = dense_support(cells, points, 10.)
        self.assertEqual({'captured_cell', 'neighbor'}, set(expected[1]))
        self.assertEqual(expected, self.support(cells, points, 10.))
        local = {key: value for key, value in cells.items() if key != 'remote'}
        self.assertEqual(self.support(local, points, 10.), self.support(cells, points, 10.))

    def test_complete_selected_cell_validation_remains_required(self):
        cells, road = fixture()
        cell = cells['captured_cell']
        cell['core_samples'][0][0] = float('nan')
        with self.assertRaises(CompilerError):
            self.support(cells, road['geometry']['coordinates'], road['attributes']['width_m'])

    def test_captured_native_profile_matches_dense_reference(self):
        cells, road = fixture()
        actual = road_elevation.build_bridge_profile(ROOT, road, cells)
        with patch.object(road_elevation, '_terrain', side_effect=dense_support):
            expected = road_elevation.build_bridge_profile(ROOT, road, cells)
        self.assertEqual(expected, actual)


if __name__ == '__main__':
    unittest.main()
