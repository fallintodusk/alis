"""Captured outgoing B edge over the actual zero-height hydro-proxy law.

Source coordinates are rebased from [583333.41, 4508112.45] into the existing
30 m fixture. The corner height and interior-station XY offset are captured.
The added station uses baseline height to isolate missing neighbor topology;
the wrapper is mechanical, not source/geographic admission.
Captured packed-field SHA256:
e41b33608f3f3e3b1c457eca66d8b70a3a8aa2b3ee84f318a7f3483e1f515718.
"""
from .test_terrain_refinement import fixture as original_fixture

BAD_CORNER = [0., 0., -2.108511542257687, 0., 1.]
INTERIOR_STATION = [0., 4508114.164132799-4508112.45, 0., 0., 1.]


def fixture():
    cells, water, policy, selected = original_fixture()
    for cell in cells.values():
        cell['core_samples'] = [[0., 0.], [0., 0.]]
        cell['surface_semantics']['core_samples'] = {
            'ground': [[0., 0.], [0., 0.]], 'hydro_transition': [[1., 1.], [1., 1.]]}
    water['attributes']['surface_function']['level_m'] = 0.
    return cells, water, policy, selected
