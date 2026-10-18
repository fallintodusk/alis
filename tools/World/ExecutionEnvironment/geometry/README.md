# Native geometry capability

Owns pinned GEOS partition/coverage execution and its consuming identity.
It uses the existing ExecutionEnvironment tool installation and lock.

| Path | Responsibility |
|---|---|
| [api.py](api.py) | Public geometry operations and capability identity |
| [app/](app/) | Native geometry implementation |
| [tests/](tests/) | Native partition, numerical precision and refusal controls |

Base acquisition and compilation do not import this child capability. Its
current implementation is authenticated by operations that consume it.

Polygon partition and coverage calculations translate to a deterministic local
origin before native GEOS overlay, then return canonical coordinates. This
avoids losing thin boundary intersections at large projected coordinates;
vertices are not snapped and source-local coverage checks remain strict. Translation-equivalence and
hole/refusal regressions live in `tests/test_geometry.py`.

Wet and dry output jointly cover the original triangle, as well as each native
partition. Returned canonical coordinates are translated back and checked again.
That round trip admits only its measured float64 displacement area bound (the
triangle perimeter times displacement buffer bound), in addition to the existing
native coverage tolerance. It is not a configurable geographic allowance.

If that round trip makes a native triangle exactly collinear, the pinned GEOS
orientation predicate removes it from the returned mesh. The displacement bound
still includes the collapsed face. Separate partition and joint domain coverage
checks must accept the remaining mesh before return; a real missing region or
failed orientation query refuses. No precision grid, source snapping or wider
coverage tolerance is introduced. Arbitrary `validate_triangle_cover` calls have
no round-trip provenance and retain their stricter independent allowance.

For a known nonempty triangle parent, native point-to-parent distances provide
an independent convex-containment certificate when polygon overlay reports a
contradictory exterior. Its maximum possible exterior area is bounded by the
parent perimeter times the maximum vertex distance plus the circular expansion
area. The bound must fit the same existing absolute exterior limit; no tolerance
is widened. Missing/overlapping coverage remains separately checked. This
certificate is excluded for arbitrary polygons, holes and nonconvex regions;
failed or unknown native distances refuse.
