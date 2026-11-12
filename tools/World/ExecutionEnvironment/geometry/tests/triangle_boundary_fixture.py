"""Unchanged inferred P25 field faces 1932/2135 with an exact existing T vertex.

This is a reduced retained comparison-model case, not surveyed elevation data.
The coordinates preserve the original binary64 values; only XY enters topology.
"""
from copy import deepcopy

XYZ = [
    [[583356.0,4508181.096170046,3.1680085418446144],
     [583354.7154782338,4508181.284521766,3.1839053351784483],
     [583356.0,4508180.0,3.1759280938147687]],
    [[583358.7380338861,4508180.694687249,3.138490552814204],
     [583356.0,4508181.096170046,3.1680085418446144],
     [583356.0,4508180.948595212,3.1690747327840456]],
]


def triangles():
    return [[point[:2] for point in face] for face in deepcopy(XYZ)]
