# Native geometry capability

Owns pinned GEOS polygon triangulation, partition/coverage and consuming identity.
It uses the existing ExecutionEnvironment tool installation and lock.

| Path | Responsibility |
|---|---|
| [api.py](api.py) | Public geometry operations and capability identity |
| [constrained.py](constrained.py) | Optional finite constraint arrangement and its separate identity |
| [affine.py](affine.py) | Optional exact read-only affine-cover certificate and its separate identity |
| [boundary_rounding.py](boundary_rounding.py) | Optional exact binary64 ring-subdivision certificate and its separate identity |
| [triangle_boundary.py](triangle_boundary.py) | Optional exact existing-vertex field boundary and its separate identity |
| [app/](app/) | Native geometry implementation |
| [tests/](tests/) | Native partition, numerical precision and refusal controls |

Base acquisition and compilation do not import this child capability. Its
current implementation is authenticated by operations that consume it.

The optional constrained entrypoint nodes finite XY segments with polygon
boundaries and mandatory stations using pinned native union, polygonization and
constrained Delaunay triangulation. Native point Delaunay supplies incident
linework for isolated stations and known open or cut edges; it does not replace
the requested constraints. With no isolated station input, native polygonization
first checks whether the noded source graph already defines complete cells.
Only a complete cell-edge census retaining every mandatory endpoint omits the
unnecessary scaffold. Native cut edges or dangles retain the original source
linework and scaffold route; invalid rings, unknown native results or missing closed-cell
edges refuse. [Native polygonization diagnostics](https://libgeos.org/doxygen/classgeos_1_1operation_1_1polygonize_1_1Polygonizer.html)
distinguish these cases. Nonempty station inputs retain their existing route.
Original
boundaries, constraints and complete scaffold enter one native noding operation;
classification follows noding. Every noded edge must belong to a polygonized cell,
including scaffold outside the selected domain. Inputs
outside the domain or inside holes refuse. Every mandatory node and selected cell edge
must survive, interior edges must have opposing incidence, and both local and
restored canonical meshes must cover the domain. Collapsed or reversed restoration refuses;
no snapping, topology repair or caller-supplied coverage allowance is added.
Heights, geographic roles and admissible exterior conditions belong to the caller.
Each native polygonized cell is selected by a verified interior point. Its complete
outer/hole ring incidence specifies the triangulated mesh boundary exactly; missing
shared faces refuse even when their area is below the coverage budget. Generated
intersection coordinates need not satisfy an exact predicate on their original
implicit lines ([native arithmetic semantics](https://libgeos.org/usage/faq/#robustness)).
Cell-ring provenance supplies topology; local domain coverage stays strict.
Only the owned canonical-restoration check consumes the existing
measured float64 round-trip area bound, with corresponding vertices captured before
face normalization. It certifies the local cover and that restoration, rather than
promising acceptance by an arbitrary strict cover check without that lineage. This
introduces no proximity allowance or source snapping.
The optional implementation is outside the parent API's identity sweep, so unused
callers are unaffected. Its own identity includes that parent and its implementation.

The optional affine certificate uses exact rational arithmetic over supplied binary
coordinates. For each independent query triangle it requires complete reference
coverage and rejects positive-area overlap between clipped reference pieces;
matching an area sum alone cannot hide an overlap balanced by a hole.
Exact clipped-piece bounds skip polygon intersections only for provably disjoint
positive interiors; every piece-pair comparison still consumes the same work
budget and counter. Boundary height/contact evaluation remains unchanged. Differences
between affine fields are measured at every overlap vertex, including boundary
contacts. The reported maximum rounds upward, so conversion cannot hide a caller's
threshold violation. Input size and comparison work are bounded; unknown or
incomplete evidence refuses. Callers own query union/topology, source qualification
and height policy. This verifier generates no geometry and has its own consumed
identity; native generation and legacy geometry identity remain unchanged.

The optional boundary-rounding certificate checks a simple nonzero-area mapped ring
against a simple, equally oriented encoded subdivision. Original vertices must
survive in cyclic order. Each added node needs an ordered, strict-interior exact
source-edge witness inside its binary64 rounding cell; midpoint-only ties refuse.
It modifies no coordinates and accepts no caller-supplied distance allowance.
Its upward-rounded Hausdorff and symmetric-difference area bounds describe
encoding departure, not exact set equality or source accuracy. Witnesses establish
that an ideal subdivision exists, rather than recovering a producer's latent
intersection parameters. Callers must bind the certificate to actual output
boundary incidence, source identities and an explicit representation policy.
It cannot waive an exact-footprint requirement or qualify a geographic role.
Ring sizes and exact-arithmetic work are bounded. This optional module has its
own consumed identity and does not change the parent geometry identity.

The optional triangle-boundary extractor operates on the actual finite XY field.
It rejects exact positive overlap, subdivides edges only at existing exact
collinear vertices, and requires opposing internal incidence and manifold closed
boundary loops. Exact outer/hole containment and area cancellation preserve
components and genuine holes without snapping or inventing coordinates. A removed
interior face can produce a legitimate new hole; the caller's independently
declared expected topology and interfaces must reject that altered model.
Recovering the boundary does not establish coverage of a separate source domain
or source accuracy. Work is bounded and its separate identity leaves unused
parent consumers unaffected.

Direct polygon triangulation uses the pinned constrained Delaunay operation on
the complete supplied polygon, preserving holes and disconnected components.
Returned canonical faces must pass the unchanged arbitrary-domain coverage
validator. It introduces no clipping boundary or coverage allowance. Captured
projected-coordinate and refusal controls live in
[polygon triangulation tests](tests/test_polygon_triangulation.py).

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

`map_triangle_cover` owns finite barycentric mapping between two nonempty
triangles. It checks the complete source cover and the target-local cover before
canonical addition, then checks restored target coverage using only its measured
float64 displacement bound. Its addition order agrees with native Road's
triangle mapping. Output identifies retained source faces and reports numeric
displacement/area bounds; it does not claim the output passed the stricter
arbitrary-domain validator. Missing, duplicated or exterior source faces must
refuse before restoration can be accepted. The mapping has no caller-supplied
coverage allowance and cannot infer geographic support.

Polygon union validates both its native local result and the restored canonical
ring topology. A restoration that cannot represent the valid local union refuses;
it does not return an invalid footprint or apply a topology repair. Captured
cross-cell float32 Road vertices exercise this refusal in the surface tests.

For a known nonempty triangle parent, native point-to-parent distances provide
an independent convex-containment certificate when polygon overlay reports a
contradictory exterior. Its maximum possible exterior area is bounded by the
parent perimeter times the maximum vertex distance plus the circular expansion
area. The bound must fit the same existing absolute exterior limit; no tolerance
is widened. Missing/overlapping coverage remains separately checked. This
certificate is excluded for arbitrary polygons, holes and nonconvex regions;
failed or unknown native distances refuse.
