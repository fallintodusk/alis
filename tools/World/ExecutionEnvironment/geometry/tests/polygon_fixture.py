"""Captured OSM relation/20784500 canonical footprint, OpenStreetMap contributors (ODbL-1.0).

The projected coordinates preserve the long slanted outline and its short gate
edges. Direct native CDT must cover this polygon without artificial split edges.
"""
from copy import deepcopy

SOURCE_PARENT_SHA256 = '146842fead07308a3da91f4ba16df6796d0f44ece53359e640a96e2ef0ebf347'

_PARENT = {'coordinates': [[[[583073.2, 4508183.62],
                   [583115.27, 4508177.47],
                   [583129.56, 4508175.37],
                   [583264.85, 4508155.76],
                   [583324.97, 4508146.77],
                   [583322.39, 4508128.86],
                   [583322.21, 4508127.52],
                   [583322.0, 4508126.2],
                   [583321.54, 4508123.39],
                   [583366.48, 4508116.95],
                   [583375.54, 4508178.23],
                   [583372.32, 4508178.71],
                   [583358.77, 4508180.69],
                   [583282.32, 4508191.9],
                   [583078.83, 4508221.73],
                   [583073.2, 4508183.62]]]],
 'type': 'MultiPolygon'}


def fixture():
    return deepcopy(_PARENT)
