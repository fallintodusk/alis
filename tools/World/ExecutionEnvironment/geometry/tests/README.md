# Native geometry tests

| Path | Responsibility |
|---|---|
| [test_geometry.py](test_geometry.py) | Translation, collinear cuts, coverage and refusal regressions |
| [test_constraints.py](test_constraints.py) | Closed-cell minimality, open/cut-edge scaffolding, creases/stations, oriented conformity, holes and optional identity locality |
| [constraints_native_fixture.py](constraints_native_fixture.py) | Captured concave parent and interior Road constraint |
| [test_affine.py](test_affine.py) | Exact whole-field error, overlap/hole refusal, conservative bounds and work limits |
| [affine_native_fixture.py](affine_native_fixture.py) | Captured native-float Road triangle and specifying Ground crease |
| [test_boundary_rounding.py](test_boundary_rounding.py) | Captured ring encoding, moved/reordered/missing node refusal, ties and work limits |
| [boundary_rounding_fixture.py](boundary_rounding_fixture.py) | Complete small mapped basin ring and inferred noded boundary |
| [test_triangle_boundary.py](test_triangle_boundary.py) | Exact T-junction topology, holes/components, overlap/manifold refusal and optional identity |
| [triangle_boundary_fixture.py](triangle_boundary_fixture.py) | Captured adjacent inferred pier faces with an existing exact T vertex |
| [test_surfaces.py](test_surfaces.py) | Source indices, shared-edge union, holes and point membership |
| [test_polygon_triangulation.py](test_polygon_triangulation.py) | Captured polygon vertices, holes, disconnected components and refusal |
| [test_mapping.py](test_mapping.py) | Barycentric arithmetic, restoration bounds and invalid-cover refusal |
| [polygon_fixture.py](polygon_fixture.py) | Captured complete fixed-pier boundary |
| [union_roundtrip_fixture.py](union_roundtrip_fixture.py) | Captured native cross-cell Road vertices |
