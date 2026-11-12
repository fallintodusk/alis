# Sparse canonical terrain refinement

CanonicalCompilation owns the final surface. The optional
`terrain-refinement-v1` document replaces complete selected grid quads with
explicit canonical triangles and interpolated `ground`/`hydro_transition`
weights. Other quads retain their original grid triangles. A refinement wraps
one authenticated `terrain-cell-v2` base; wrappers cannot be stacked.

See [sparse terrain realization](refinement/docs/architecture/diagrams/terrain_surface.md).

## API and authority

`refinement.api.refine_terrain_cells(repo_root, cells, water, policy, selected)` constructs
payloads without modifying its inputs. `selected` maps cell IDs to explicit
quad coordinates. `refinement.api.prepare_terrain_refinement(repo_root, base_result_path,
output_root, water, policy, selected)` authenticates an accepted base, validates
the requested unchanged Water feature, and publishes a disposable bundle under
repository `tmp/`. The base and candidate directories must be disjoint.

The child owns its schemas and `refinement_result.json`. Its result operation is
`refine`, distinct from the base compiler's `compile_result.json`. Base compiler
promotion accepts only `compile`; neither an accepted disposable result nor its
fixture limits grants generic Terrain activation. Feature-child selection uses
the separately authenticated authority path below. Terrain construction has no
automatic production activation.

The child implementation and consumed parent helpers have their own fingerprint.
Its native geometry capability is provided by ExecutionEnvironment's geometry
child. Refinement implementation edits do not change the unused base compiler or
base execution-environment identities. Changes to consumed base helpers still
invalidate refinement proof. Current child execution never rewrites the frozen
base producing identity or establishes a historical raw-source replay.

The candidate retains original grids and compiler lineage. Selected civil
features gain explicit grade references; other feature content is unchanged. Selected
cell manifests point at wrapper artifacts. Their hashes include the final
surface, unchanged base, Water, explicit policy, implementation, execution
environment and bounded neighboring domain. No global compiler fingerprint is
reinterpreted as incremental reuse. Preparation uses existing atomic staging;
authenticated repeat requests are no-ops, failed promotion removes staging, and
repreparing from the original base with an empty selection removes refinement.

## Correction selection policy

Prefer retaining base Terrain that is adequate for the admitted visual/gameplay
purpose. Where a demonstrated relationship needs adaptation, select an existing
qualified local correction and preserve its bounds, origin and dependencies.
Where supported evidence or design is insufficient, retain an explicit unresolved
disposition or use a qualified authored input instead of manufacturing precise
geometry. This priority is not a generic chain executor: current construction and
publication operations still have their explicit supported contracts and refusals.
An automatic classifier, general residual handoff and procedural influence adapter
are not provided by this policy. The
[territory fidelity contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#fidelity-and-overlapping-physical-roles)
determines the admitted purpose without weakening canonical or native fidelity.

## Geometry and refusal

The implemented construction supports admitted constant polygon Water, including
holes. Pinned GEOS partitions each original affine terrain triangle and performs
constrained triangulation. Wet and shoreline vertices are lowered only when
needed for the requested clearance; dry triangles connect to original boundary
corners. This is a bounded prototype, not a source-derived bank-design law.

Policy explicitly supplies accepted source accuracy, maximum cut/fill, dry-bank
distance, bank gradient, triangle budget and Water clearance. Low-confidence
Water is excluded regardless of the supplied list. Flowing/ribbon functions,
overlapping additional Water influences, invalid topology, unavailable native
geometry and exceeded limits refuse. No automatic relaxation or minimum-knot
substitute is permitted. Production source-quality and numeric limits require
separate acceptance.
Selected cells carrying authored terrain patch IDs also refuse until their
conflict policy is explicit; the prototype does not overwrite authored intent.

Selected neighbors must share complete edge topology, heights and semantic
weights. An outer edge retains its original endpoint geometry and weights;
additional boundary vertices or changed heights require an expanded selected
neighbor domain. Vertex XY coordinates and affine heights remain float64;
independent rounding must not break shared constraints. Validation rejects
missing/overlapping coverage, duplicate vertices, cracks, reversed faces,
invalid indices and conflicting shared attributes.
This is a single-valued height surface: repeated XY positions with different
heights and vertical faces are outside `terrain-refinement-v1`. Such a surface
requires a distinct canonical representation and explicit height-query versus
3D realization/collision contracts; weakening this validator is not an extension.

## Consumers and proof

ProjectWorld loads the authenticated base and final replacement into one typed
surface. Triangle consumers use final faces. Placement queries use those affine
faces throughout a selected quad and fail if coverage is missing. Outside
selected quads the existing clamped bilinear query is retained. Navigation bounds
include final vertices; fixture identities include the new typed fields.

Mesh Terrain realizes these triangles without additional deformation. Source
fidelity, compiled CPU triangle coverage/interior heights, actual Nanite/fallback
pixels and native collision are distinct checks. Matching old grid vertices
cannot certify a changed interior. CPU fallback buffers of a Nanite-enabled mesh
do not prove Nanite cluster rendering or packaged behavior.

Focused tests are `test_terrain_refinement.py`, `test_refinement_artifacts.py`,
`Project.World.Terrain.RefinedQueries`,
`Project.World.Realization.MeshTerrain.RefinedSurface` and
`Project.World.Realization.Runtime.RefinedNavigationBounds`. Native tests create
their own disposable inputs through the public compiler API.

The source-rebased boundary fixture in `test_refinement_artifacts.py` tests
publication refusal independently of construction: schema-valid replacement
geometry with an open height trace or an unsupported boundary station must not
reach promotion. It needs no retained diagnostic files and does not qualify a
geographic model. Run it with `PYTHONPATH=tools`:

```text
python -B -m unittest World.CanonicalCompilation.tests.test_refinement_artifacts.RefinementArtifactTests.test_captured_b_boundary_refuses_publication_before_any_promotion
```

## Authored dry Terrain

`prepare_surface_refinement` accepts the closed operation
`{"kind":"authored_dry_terrain","feature_ids":[],"input":{"path":...,"sha256":...,"byte_size":...}}`.
The pinned input is the existing `authored-overlay-v2` contract with its canonical
schema URI, unique patch/provenance IDs and resolved provenance references. Only
height patches are supported; feature overrides and weight replacement refuse.
This is authored game-world intent, not qualification of a new raster or survey.

The [authored constructor](refinement/app/authored_terrain.py) sums the existing
bounded radial deltas on immutable accepted FINAL grid nodes and quantizes once
with the original height quantum. It selects every incident quad, including
authenticated neighboring cells sharing a changed node. Changed outer edges
without a selected neighbor refuse. Zero effective change retains the base by
refusing a redundant child. Already refined or authored-conflicting cells refuse;
this operation does not stack wrappers or compose arbitrary correction chains.

Complete selected quads must be disjoint from every retained canonical Water
polygon/ribbon. This preserves the accepted Water support rather than rerunning
unrelated Water fitting with newly invented limits. Retained feature documents,
Water levels/functions and original Terrain bytes stay unchanged. Water-touching
edits need a separately admitted joint operation. Cells bound by explicit civil
grade, Road profile or constructed support also refuse; the check runs again
after composition. Ordinary Road/Building consumers still use final Terrain.

The existing publisher stages the wrapper, selected manifests, coverage, retained
input, request, report and result together. Same-cell conflicting operations refuse
without publication. Existing selection preparation authenticates current replay
and frozen input/domain/quad binding; no separate writer or activation exists.
The domain hash includes frame, original corner XYZ and both semantic weights.

Inside selected quads final queries become piecewise affine; original immutable
queries remain bilinear. Even an unchanged node can therefore have a different
interior query. Report that difference separately from node cut/fill. Per-face
slopes and internal/external creases must accompany source agreement; an unchanged
global maximum slope is not a geographic or gameplay quality certificate.

`test_authored_terrain.py` covers cold/no-op equivalence, immutable bytes, shared
nodes, missing neighbors, Water/provenance/conflict refusal, late support failure,
failed promotion and frozen domain/vertex tampering. The permanent
[Madison inputs](../../../Plugins/World/ProjectWorldTestData/Data/Fixtures/madison_dry_ground/README.md)
provide the source-shaped control and per-face observer. Real authority admission
and native saved/collision/rendering evidence remain separate from that mechanical
artifact fixture; see the [native owner](../../../Plugins/World/ProjectWorldMeshTerrain/README.md).

## Immutable civil grade references

Terrain refinement preserves accepted civil grades through the optional
[civil-grade reference](refinement/contracts/civil-grade-reference.schema.json)
in canonical feature attributes. The refinement child selects this model and
authenticates the original grid artifacts already carried by the wrapper.
It is grade preservation, not a claim that an unclassified path is a bridge,
or that a Building has physically verified foundations.

The influence census uses complete selected quads, including their dry portions.
Road support uses source-segment bounds expanded by half the admitted width;
Building influence uses the full-footprint bounds center clamped to its OWNER
grid. A neighboring cell does not change that owner's query. Conflicting
relationships, explicit ford/tunnel evidence, unsupported crossing markers,
unavailable support or a nested
grid reference refuse. Missing source tags remain unknown.

ProjectWorld retains separate authenticated base-grid and final-surface
identities. Named immutable-grid queries reuse the existing grid and native
math: Road consumes affine grid triangles; Building retains clamped bilinear
sampling at its existing bounds center. Ordinary query defaults remain final.
No grid copy, second terrain writer, alternative collision surface or native
ribbon reconstruction is introduced.

Road preserves its native planform, tangents and presentation offset. An explicit
UV profile remains authoritative and qualifies against its original grid support;
its abutments use that same support. Both ordinary preserved grades and profiles
must independently clear the authenticated final Terrain, including stored float
geometry. Planner and builder consume the same grade and final-clearance inputs.
The final Terrain identity therefore remains a Road generation dependency.

A preserved Building consumes its base anchor reference. Its generator does not
evaluate final footprint support, so the final wrapper is not a generation input
for this mode. An owner performing a separate support or civil-relationship
verification must consume that final context itself. This boundary does not add
a new foundation, clearance or structural-design law to Building realization.

See [immutable civil grade consumption](refinement/docs/architecture/diagrams/civil_grade.md).

The public terrain publisher prepares these references inside its existing
tmp-only candidate transaction. Reference identities, owning feature manifests,
coverage and the result advance together. A no-op authenticates all outputs;
preparing an empty selection from the original base removes both refinement and
preservation references. Neither route activates production authority.

Focused consumer regressions are `test_civil_grade.py`,
`Project.World.Terrain.CivilGradeAdmission`,
`Project.World.Realization.Road.ImmutableCivilGrade` and
`Project.World.Realization.Building.ImmutableCivilGrade`. Geometry preservation,
stale-input refusal and dependency locality do not establish a natural bank,
basin operating level, physical support or production visual acceptance.

This preservation policy belongs to the Water-conditioning publisher above.
It is not a default for replacing DSM with qualified Ground. An accepted DSM
remains immutable historical input; its elevations can include objects and are
not automatically a better terrain model. Ordinary Road and Building queries
consume the final surface. Retaining a base grade during source replacement
needs an explicit qualified civil constraint and separate final-clearance proof.

## Raster Ground construction

The optional `refinement.ground` capability constructs unpublished final-quad
geometry from an exact qualified raster Ground grid. Its public entrypoint is
[`build_ground_surface`](refinement/ground.py). The caller authenticates that
specific product through the SourceIngestion reader; a coarse admitted product
cannot substitute for a separately qualified fine grid. Pure construction checks
the content/frame identity but does not establish admission or source quality.

The source grid must cover the explicit selected quads without extrapolation and
share their half-diagonal node stations. Native triangulation operates separately
inside each original affine half, retaining original semantic weights. All source
nodes participate except redundant points on unchanged outer lines.
Source-station semantic interpolation uses nonnegative integer barycentric
numerators over the original half. This preserves the original affine field
without subtractive cancellation at zero/one edges; invalid source weights still
refuse. It does not clamp malformed inputs or change geographic heights.
Changed outer heights/weights refuse; new outer subdivisions are not silently introduced.
Selected neighbors must agree in topology, height and weights. Missing/nonfinite
Ground, wrong basis/frame, incomplete support and excess triangle budget refuse.
The operation neither blends an open edge nor selects a geographic quality limit.

A region-to-region composite is a separate geographic model choice inside this
owner. Keep qualified source-derived and inferred transition geometry distinct;
bind both source fields, selected domain, interpolation and boundary conditions.
Continuity alone does not establish reasonable slopes, civil relationships or
playability. A continuous inferred transition may be valid game-world geometry,
but is not surveyed Ground or a reason to relabel unknown source support.

The optional [Ground composite](refinement/ground_composite.py) implements one
unpublished inner-to-provider model. It preserves a complete identified inner
piecewise affine field and tapers its nearest-boundary residual against the
qualified provider-node bilinear model. The final constrained triangles own the
result; the analytic taper is only their vertex construction rule. Original
quad-half boundaries, inner creases and provider stations participate in one
native arrangement. Semantic weights retain the original affine fields, with
one directed interpolation rule shared by both sides of an original edge.

Its exterior retains the complete provider-model trace and requires neighboring
source continuation. It does not preserve historical Terrain heights or certify
an isolated crop as a valid Terrain replacement. All output is identified model
geometry, including the inferred transition. Inputs, model/frame identities,
selected original support, boundary trace and finite capacities are consumed
explicitly. Unknown provider samples, conflicting inner heights, missing cover,
lost constraints, invalid seams and exceeded capacity refuse without mutation.
Retaining every original inner vertex is insufficient: losing an internal crease
can change the field between those vertices. The constructor compares affine
differences at every overlap vertex and proves complete cover of each original
inner face, establishing preservation over the whole field rather than samples.
This is an exact read-only certificate over the supplied binary coordinates;
native geometry generation is unchanged. The constructor consumes the verifier's
separate identity and applies its existing precision limit to the upward error
bound. Querying the protected inner faces against final output is essential:
reversing those roles would incorrectly require the small inner domain to cover
the surrounding provider model.
The optional constructor and native arrangement have separate identities; callers
that do not consume them retain their existing identities. No existing Ground
outer-boundary guard or production schema is relaxed. Complete civil widths,
source departure, physical roles and geographic quality remain separate admission
requirements. Focused construction, captured-quad and refusal evidence lives in
`test_ground_composite.py`; the result is rejected by production document readers.

A finite bare-earth value establishes neither an elevated civil surface nor a
surface constructed after the source survey. Regional selection must consider
source epoch and surface meaning alongside coverage. Explicit structural
relationships still need metric support; an old DSM is not automatically a
correct deck elevation either. Do not turn missing or temporally unsuitable
evidence into an implicit Ground, bridge or zero-height decision.

Classified observations and a provider-derived raster are distinct source models.
A continuous raster can still misrepresent a paved Ground surface. Source-only
agreement is therefore not sufficient consumer-quality evidence. With classified
observations, retain Water, mixed-class and conflicting-return exclusions when
establishing support; a Ground-only convex hull can interpolate across those
internal exclusions. Never average conflicting observations implicitly. Finite
axis samples do not prove an entire Road width or its endpoint sections.
Return classification is three-dimensional evidence. An excluded return above
valid Ground does not automatically create a Ground hole at the same XY.
[USGS bare-earth surface guidance](https://www.usgs.gov/ngp-standards-and-specifications/lidar-base-specification-digital-elevation-model-surface)
explicitly distinguishes terrain interpolated below a removed bridge from its
deck. Retain other returns with their XYZ, class and flags for role comparison;
an all-class planar triangulation is a sensitivity diagnostic, not proof that
Ground is absent. Interpolation span, real exclusions and conflicting heights
within the claimed Ground layer still require explicit qualification.
An ordinary Ground-supported Road consumes a joint final-Ground correction;
a Road-only lift must not hide an unsupported Ground model or manufacture an
elevated relationship. A separate deck requires its own qualified civil evidence.
Distinguish the recorded pre-surface fitted grid from the accepted final Terrain:
Water conditioning may already have lowered the latter. Neither is interchangeable
with raw DSM observations or later Ground evidence. A classified Ground observation
above a mapped Water polygon does not by itself distinguish reclaimed land, a
constructed platform or an inaccurate Water footprint. Resolve that relationship
before selecting Ground replacement or a separate civil surface; proximity to a
pier and partial point containment are insufficient evidence.

Evaluate source support over the complete consumer domain. Subtracting support
from a land selector can clip a larger unknown region at the shoreline; its
complement is not thereby qualified Ground. Open tagged pier lines establish no
closed exclusion footprint or metric deck height. Distinguish retained historical
terrain, qualified source and modeled transition in evidence and identities.
Only the complete authenticated canonical cell inventory can establish a true
territory exterior. A crop, omitted neighbor, land edge or source frontier cannot
remove an internal seam obligation. The current pure builder conservatively
preserves every selected outer trace; it does not select a regional boundary law.

Whole-quad eligibility and exact source support are different boundaries. When
source continuation justifies extending a candidate, test the actual outside
neighbor: a positive-area supported component must share positive interface
length with the applicable role portion. Membership of the closed interface
alone, a disconnected supported island or a line-only touch proves no outward
continuation. This coverage proof still does not qualify physical Ground.

A Ground-product hole can contain an elevated deck, water column and bed at the
same XY. Qualify those layers separately against the provider's classification
metadata and mapped structural relationships. Neither the hole nor a nearby
classified layer authorizes Ground infill or relabeling deck/water-column returns
as Ground. Source acquisition dates and current mapped geometry remain distinct.

Its implementation identity is independent of the existing feature/Water child
and includes the shared helpers and contracts it consumes. The result separately
binds exact Ground evidence, selected base support and native geometry identity.
Adding this unused capability changes neither base compiler nor existing child
identity. This does not waive normal acceptance of changes to consumed code.

See [pure ground construction](refinement/docs/architecture/diagrams/ground_source.md).

The result is not a loadable `terrain-refinement-v1` document. That Water envelope
does not authenticate Ground by relabelling it as Water. No Ground publication,
World activation, civil grade choice or joint fountain containment follows from
pure construction. `test_ground_surface.py` owns focused two-cell construction,
affine semantics, identity locality and unchanged-input refusal controls.
The test-only `ground_query_fixture.py` carries that construction into
`Project.World.Realization.MeshTerrain.GroundTypedQueries`. It checks typed final
queries and the dynamic source mesh against specifying vertices, semantic weights
and oriented face multiplicities, with stale-grid, missing-coverage, corrupted-weight
and reversed-face controls. This bypasses authority admission deliberately; it
establishes no Ground loader, compiled render geometry, collision or packaged proof.

`ground_consumer_fixture.py` supplies self-contained typed Ground to
`Project.World.Realization.Road.GroundFinalSurface` and
`Project.World.Realization.Building.GroundFinalAnchor`. The actual owners build
source mesh descriptions without saving packages; missing final coverage must
refuse without partial output. Their shared test-only projection does not admit
a replacement authority. An explicit `PROJECT_WORLD_GROUND_SCENE_FIXTURE` can
instead supply a disposable real-base scene; it must not point at production
outputs or be a required input to ordinary test execution.

Road vertex conformity is checked against the affine triangles it consumes.
The oracle includes incident neighbor-cell triangles for the actual Road ribbon:
the ribbon can extend past its axis owner's bounds. An oblique fixture requires
such vertices and rejects owner-cell-only height checks.
Outside selected replacements, the placement sampler's retained bilinear field
is deliberately different and cannot serve as a Road conformity oracle. A
diagnostic Ground crop must contain the complete tested Road footprint and
consumed halo; otherwise its discontinuous edge cannot certify a whole Road.
These probes do not certify continuous saved triangles, collision, navigation,
regional boundary conditions or packaged behavior.

The pure Ground constructor accepts an optional complete `resolution` list of
`cell_id`, `quad` and positive integer source-grid `stride` records. Each stride
must divide its original quad's source stations. Native triangulation retains
the original affine halves and the union of both neighbors' requested edge
stations; a coarse interior does not force an adjacent fine interior. Unknown,
duplicate, incomplete or excessive selections refuse before producing output.
The normalized map and its implementation are consumed identities. This chooses
a declared nodal approximation, not an accuracy guarantee or source-priority
policy. The default still consumes all source stations. Complete source-grid,
frame, convex-semantic and unchanged outer-trace guards remain in force.
This unpublishable constructor establishes no Ground authority or replacement
of the Water-specific refinement provenance contract.

See [finite ground resolution closure](refinement/docs/architecture/diagrams/ground_resolution.md).

## Identified original-width path-floor comparison

The optional [path-floor constructor](refinement/ground_path_floor.py) constructs
one unpublished source-control field over one or two complete adjoining original
quads. It supports the demonstrated finite case: one straight inferred-width path
crossing the region's north/south boundary, with Land explicitly WEST of a separately supplied
south-to-north-spanning Land/reference cut. Reversing the cut endpoints does not
select the other side. Other orientations or physical roles need their own
demonstrated extension; this operation does not infer them from an unoriented edge.
This is a numerical modeling capability, not a general scenario engine or a
physical shore, structure, Ground or source-admission rule.

The caller supplies qualified canonical point controls and their source,
processing and frame identities. Known values may themselves be raster-model
samples; they are not automatically surveyed/current floor observations.
Explicit source UNKNOWN retains its identity and null source height. Missing,
uncaptured, unreadable, malformed or nonfinite replies refuse; they cannot be
converted into source UNKNOWN. Native pixels, datum transformations and source
acquisition remain upstream. Defining axis controls must all be known before
constructing the identified piecewise longitudinal, zero-crossfall floor.

The complete original width is protected through one constrained mesh, including
its defining height creases. Canonical encoding rounds the modeled path boundary
outward and the Land interface inward, with actual exact width/containment checks;
it does not change source geometry or widen an admission gate. All generated
nodes receive an explicit floor, source-control or retained-reference branch.
Whole-field height, continuous directional slopes, C0/nonmanifold conditions,
complete cover and the original non-Land reference are checked before return.
Neighboring quads share one source-axis profile, arrangement and height assignment.
Every original diagonal and shared boundary remains a constraint; all original
reference planes participate in coverage and fidelity proofs. Splitting back into
original owners preserves face XYZ, original semantic interpolation and an exact
shared XYZ/semantic trace, with the triangle capacity enforced per original quad.
Independently constructing adjacent crops can produce different endpoint profiles
and is not evidence of a coherent neighborhood.
Numerical reference preservation is separate from exact equality and geographic
accuracy. Retained side controls are evaluated against the final field only
inside its domain; outside controls do not trigger model extrapolation.

Original semantic weights remain on their original affine halves. Output stays
unpublished model space, with explicit changed outer traces requiring neighbor
continuation. Source/role qualification, accepted departure limits, bank geometry,
clearance, complete-neighborhood acceptance and publication remain separate.
The optional implementation and consumed native/verifier identities are local
to this capability; unused feature/Water operations do not acquire them.
The [captured floor tests](tests/test_ground_path_floor.py) and
[neighbor tests](tests/test_ground_path_neighbors.py) retain the original coordinate
magnitude, source gaps, full inferred width, failing shore join and independent-crop
seam without requiring raw rasters or previous scratch runs.

## Identified strip continuation

The optional [Ground continuation](refinement/ground_continuation.py) joins an
identified incoming field to a caller-authenticated raster-node model in one
east neighbor. Its first variant accepts a contiguous E/W strip of complete
original quads. It keeps the incoming edge's PWA law, including added stations,
and subdivides incident incoming faces so both sides consume the same trace.
Declared incoming wet-face roles follow their parent faces; a dry sample at
zero elevation does not acquire a Water role.

The neighbor's node-based Delaunay field is frozen before further noding. The
original semantic diagonal is a separate constraint: continuous height and equal
shared tuples cannot establish preservation of both complete material-weight
fields. Certification checks the emitted triangles against the incoming field,
fixed neighbor reference and original semantic fields. Unchanged incoming quads
retain their exact payloads. Consumed implementation, native execution, source
and model identities remain separate from geographic acceptance.

The result uses the existing final-quad layout as an unpublished identified
model. Other exterior traces remain OPEN. This operation neither authenticates
source data nor selects coast, bed, structural roles or quality limits, and it
has no publication authority. It does not require the composite constructor's
interior-only support or alter the path-floor constructor's floor/cut contract.
Captured-data, typed-role, locality and output-fault controls live in
[continuation tests](tests/test_ground_continuation.py).

## Identified static basin comparison

The optional [identified basin](refinement/identified_basin.py) constructs a joint
finite Water/supporting-surface model without publishing or admitting it.
`build_identified_basin` consumes the unchanged standing polygon, complete selected
original quads, a separately qualified raster-node reference and explicit model
inputs. Role association, stage, depth, core/rim/influence contours, transition
dimensions, gradient screen and spill margin are identified assumptions, never
measured rim/bed/Water properties. The reference uses its declared grid-node
triangle diagonals, rather than the Ground composite's bilinear taper reference.
Callers authenticate source and role evidence; supplied hashes do not establish
authenticity or current-site accuracy by themselves.

This bounded operation supports a strictly convex single-ring basin, strictly
nested convex model contours and at most 16 complete original quads covering one
rectangle. The basin owner enforces strict convexity independently of the shared
simple-ring encoding certificate. Whole influence must be interior. The support declaration is an
explicit inferred solid-plaza assumption: known canonical under-routes refuse,
and unmapped under-routes remain unknown. It cannot model layered structures or
vertical walls. Pinned native geometry nodes all reference-grid and contour
constraints together. Actual face incidence defines the finite modeled loops;
each loop consumes the optional
[boundary encoding contract](../ExecutionEnvironment/geometry/README.md).
Winding normalization preserves the original source edge set. Ordered rounding
witnesses qualify the representation departure and report conservative nonzero
bounds; they do not establish exact mapped-set equality. A policy requiring exact
footprint preservation cannot be satisfied by silently substituting this model.

The final conforming PWA owns all between-node values. Its wet faces must have
positive interior depth, a complete connected sublevel component and a minimax
escape level above the explicit retention margin. Complete face gradients and
per-quad capacities must pass their supplied screens. Shared heights, original
affine semantic weights and provider exterior trace are retained; that exterior
still requires qualified neighbor continuation. The optional operation consumes
model, source feature, reference evidence, selected support, boundary trace and
implementation/native identities. Ordinary callers do not import this sibling
capability. Source-shaped construction, cold/depth locality, full affine cover,
outside-reference field and refusal controls live in
[basin tests](tests/test_identified_basin.py). Its plain model-space result has no
publication authority. Existing observed contained-Water eligibility and
publication remain separate contracts. The native consumer recognizes the
distinct inferred function at its exact stage, with the same stored-spill
refusal as observed containment. Its closed JSON admission is described below.
This explicit inferred ID/version preserves its supplied
noded canonical XY until float storage; applying the base centimeter quantization
again would change the jointly constructed wet/dry boundary. Ordinary and observed
contained functions retain their coordinate rule. Joint unit storage and
independence requirements belong to the
[World consumer contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md).
The permanent joint fixture exporter is
[identified basin mesh fixture](tests/identified_basin_mesh_fixture.py); its
consumers are `Project.World.Water.IdentifiedBasinTypedConsumer` and the existing
`Project.World.Realization.MeshTerrain.CompositeConsumer`. Exact stored stage
alone does not prove joint shoreline containment after float XY conversion,
rendered materials, physical support or geographic admission.

### Joint basin candidate admission

The optional [basin admission](refinement/basin_admission.py) connects the pure
model to `prepare_surface_refinement` as one closed `identified_basin` operation.
The [retained input](refinement/contracts/identified_basin/input.schema.json)
keeps the complete unchanged source Water separate from the explicitly inferred
model and its retained reference. This does not authenticate a survey, resolve
unknown support or establish current-site accuracy. The shared
[surface contract](contracts/identified_basin/surface.schema.json) owns the wire
projections consumed by Water and supporting Terrain.

One candidate stages both the modeled Water and complete selected Terrain quads
through the existing Terrain serializer. It retains the original grids, feature
identity, source references and feature membership. Unclosed exterior traces,
interacting Water groups, nested refinement, unavailable input or unresolved
Road, physical Building-volume or constructed-surface intersections refuse the
whole candidate. The interaction census includes features created by earlier
operations. It uses conservative physical envelopes rather than Building
placement centers. No partial earlier operation is published on later failure.

Selected authority permits only the explicitly declared Terrain replacement
paths; original Terrain bytes remain immutable. Frozen reads authenticate retained
bytes and declared bindings; current replay recomputes construction against the
base. Basin-specific schemas and construction identities are consumed only by
requests selecting this operation. Unused basin-leaf edits do not invalidate
ordinary bridge or structural-child currency.

Native JSON admission checks the explicit inferred function, retained `basin`
input role, unchanged source associations, frame, stage/spill, numeric policy,
reference evidence, selected quads and exact final Terrain descriptors. Its digest
bindings authenticate producer-derived provenance; they do not rerun the Python
constructor or rounding certificate. Native seam validation independently refuses
an open original-quad trace. Verification identity includes admission metadata.
Depth-only changes can schedule Water evaluation while unchanged Water geometry
semantics reuse saved artifacts. Neither candidate publication nor native loading
authorizes production selection or geographic acceptance.

Public publication, cold replay, locality and no-output refusal controls live in
[admission tests](tests/test_basin_admission.py). The
[native control exporter](tests/basin_admission_native_fixture.py) creates fresh
reduced children for full-loader positive and resealed negative checks.

## Contained standing Water construction

`refinement.api.build_contained_water` is a pure disposable construction operation,
separate from source admission and joint Terrain/Water publication. Its caller
must authenticate the supplementary Ground observations, processing/frame evidence
and matching constructed-basin context. Hashes supplied to this pure operation
do not establish source authenticity by themselves.
Frozen processed Ground products can be authenticated through
[SourceIngestion supplemental admission](../SourceIngestion/supplemental/).
That receipt proves the retained qualified product and its processing scope;
it does not select a terrain approximation or approve production quality.
Provider point samples and their coarse affine terrain representation have
different fidelity. Source-quality evidence must distinguish interpolation across
local breaklines from a civil feature's actual vertical relationship. Inspect
qualified way AND node context: a multi-level elevator or entrance cannot be
classified as ordinary ground-following solely because the way lacks a bridge
or tunnel tag. Finite source support supplies neither a metric connector design
nor permission to smooth away an observed sharp transition.

The operation reuses the configured quantile and quantization primitives on
distinct Ground XY observations inside the mapped polygon. Pinned GEOS triangulates
the observations. The lowest interior observation selects a seed; minimax escape
over the observed TIN establishes its spill. The quantized level must be strictly
above the seed and below spill. The actual connected sublevel footprint must have
positive area and avoid the observed hull. The default `mapped_containment`
model also requires complete containment inside the mapped domain.
Unknown frame/class, conflicting duplicate elevations, open/equality/rounding
cases, point-only connectivity and unsupported footprint models refuse.
No freeboard, wall height, operating Waterline or source accuracy is inferred.

The explicit `observed_ground_component` comparison uses the closed observed
component as its derived footprint and measures departure from the mapped domain.
It retains the original mapped-geometry identity and cannot reinterpret explicit
mapped holes or disconnected source regions. It does not change quantile,
quantization, seed or spill rules to make a failed mapped candidate pass. This
is a separately identified game-model candidate, not a correction to the original
OSM geometry or permission to publish an expanded Water footprint. Source quality,
civil consequences and complete joint Terrain/collar support remain prerequisites
of publication and adoption.

The [contained Water function](refinement/contracts/contained-water.schema.json)
records the derived model and input/geometry identities. It is not the existing
canonical-grid polygon estimator and does not promote the base low-confidence fit.
The existing native Water writer consumes this function at its exact canonical
level, with zero presentation lift. Other function families retain their layer
offset. One Water-owned offset rule is shared by emission, grouping and source
approximation; ordinary input/semantic identities already consume function IDs,
versions, levels and geometry.
ProjectWorld also retains the spill constraint as a consumed typed input. Water
checks the actual float-stored plane back in the canonical height frame against
every contributing contained source before composition or emission. A raw level
below spill does not suffice when storage rounds it to or above spill. The bound
participates in input identity; ordinary function identities remain unchanged.

See [contained standing water construction](refinement/docs/architecture/diagrams/contained_water.md).

The current pure operation does not construct a terrain collar or publish a
canonical child. Exact Water emission alone cannot fix a basin buried by unchanged
Terrain. Continuous Q4 top triangles remain the intended representation for this
joint boundary; no second terrain authority or 3D bank representation is implied.
Focused controls are `test_contained_water.py`,
`Project.World.Water.ContainedFunctionAdmission` and
`Project.World.Water.ExactCanonicalLevel`.

## Observed constructed-surface candidates

The optional [constructed-surface constructor](refinement/constructed_surface.py)
models a straight Road on one source-qualified fixed pier. It associates
classified height observations with the exact parent footprint, including its
holes. A spatial query halo supplies context; observations outside the parent
cannot establish its surface. Ground classifications retain their source meaning:
using them to model a separately qualified platform does not turn underlying
Water into Ground or supply a surveyed current deck.

The requested full width must remain inside the parent and the associated
observation hull. No extrapolation, boundary buffering, implicit conflict
averaging or crossing reinterpretation is permitted. Explicit bridge, tunnel,
ford or unresolved crossing evidence needs its own support relationship. The
constructor checks finite inputs and interpolation outputs, frame/content pins,
query bounds, complete parameter/projected coverage and a bounded face count.
Native point triangulation and polygon partition remain ExecutionEnvironment
capabilities. Its optional identity binds consumed helpers without changing the
unconsumed feature publisher or base compiler.

This is pure construction. The caller must authenticate source admission,
complete parent evidence, source epoch and processing. Its separately identified
observation model leaves Terrain and Water untouched. Constructed endpoints are
on their supporting surface; they are not automatically immutable-Terrain
abutments. The current production Road parser and feature publisher do not admit
this model. Source association and coverage do not establish acceptable slopes,
incident joins, compiled geometry, collision or geographic adoption.
The [captured constructed-surface regression](tests/test_constructed_surface.py)
keeps that distinction executable with source-faithful input and full-field replay.
Typed native diagnostic consumption follows ProjectWorld's
[Road elevation consumption](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#road-elevation-consumption).

## Road elevation candidates

`refinement.api.build_bridge_profile(repo_root, road, cells)` constructs a
disposable source-abutment model. Explicit nonconflicting bridge evidence and a
single finite source way are required. Missing tags, conflicting crossing modes,
repeated/reversing stations and incomplete Terrain refuse. This construction API
does not authenticate supplementary source admission, publish a refinement,
activate a profile in ProjectWorld or authorize production adoption.

Both endpoint sections are sampled from Terrain. This is a model assumption,
not evidence that the physical structure has Terrain-supported abutments or
that its deck shares Terrain's height. A bridge tag, relative layer or footprint
above Water does not establish either endpoint's structural support.

The current model does not represent a fixed-pier-to-floating-dock gangway.
Such a relationship needs separately qualified endpoint roles, footprint,
epochs and metric support before a source-bound model can be admitted. Imagery
can support structural association without supplying deck height; dataset-level
Bridge Deck classification does not prove local returns or their temporal
suitability. Ground and Water observations retain their distinct source roles.
An identified static representative-water model would still need justified dock
freeboard and endpoint connections; Water height alone is not walkable deck height.
These qualification requirements do not prohibit independent source investigation
or authorized disposable comparisons while production quality remains unresolved.

Bridge support retains every terrain quad touching an original source segment's
half-width-expanded bounds, including shared-edge contact. Local quad evaluation
preserves complete selected-cell sample validation, original face order and
consumed-cell identities. It does not make full-cell validation, terrain loading
or candidate publication I/O local. The captured-source and boundary controls in
[bridge support tests](tests/test_road_support.py) compare the complete support
and native profile against a dense reference and bound unused quad evaluations.

`refinement.api.prepare_bridge_refinement` admits a separately byte-pinned
normalized provider document through SourceIngestion's public contract. Shared
OSM namespace qualification preserves distribution-channel provenance. A complete
ordered projection must match the immutable base Road, and qualified way/node
versions must not postdate the accepted source release. Complete node evidence
is mandatory. Explicit source elevations require their own qualified vertical
interpretation rather than silently using Terrain-derived bridge construction.
This is supplementary
context, not a historical raw-source replay. Unknown metadata or unsupported
crossing modes refuse. The base result and every consumed document are bound to
one authenticated byte snapshot; changes during preparation refuse.

Publication uses the same tmp-only staging and distinct refinement result as
Terrain refinement. It updates only the selected feature artifact, its manifest
and derived coverage, retaining original geometry, source references and base
lineage. Terrain bytes remain unchanged. Constructor JSON-content hashes and raw
Terrain artifact hashes are distinct domains; the native consumer authenticates
the latter. Context bytes and child/source-admission/execution identities bind
replay and no-op admission. A changed dependency cannot reuse an old candidate.

See [qualified road candidate preparation](refinement/docs/architecture/diagrams/road_elevation.md).

The [Road elevation schema](refinement/contracts/road-elevation.schema.json)
owns the payload shape. Coordinates are cumulative source arclength and signed
left lateral metres; explicit parameter-space triangles define affine absolute
height, excluding the Road presentation offset. They do not define native XY
mesh ownership. Full abutment sections come from final Terrain triangles, with
breaks at crossed triangle edges. Source-only sections and a nonnegative concave
support envelope construct a candidate; no native segment-length/tangent
tessellation is reproduced in the compiler. Terrain remains unchanged.

The finite support census includes each source-segment bounding box expanded by
half-width. This contains the native ribbon without determining its internal
tessellation. Consumed Terrain and feature content, implementation and native
execution identities are recorded. These hashes identify supplied construction
inputs; they do not establish historical source correspondence or producer
promotion. Existing compiler/source replay requirements remain intact.

Parameter-domain coverage and unchanged input tests are construction proof only.
Native Road must retain its original XY triangles, map their source
station/lateral coordinates, subdivide against the affine elevation field and
add its presentation offset once. Final emitted triangles, abutment/incident
joins, Water clearance, compiled geometry and collision require independent
acceptance. A candidate failing those checks refuses; a source-only support
estimate is not an excuse to relax them. The native Road consumer admits the
optional profile explicitly, including its exact directed-source binding and
complete finite Terrain-support census. Malformed or stale fields refuse rather
than falling back to draping. City adoption uses the authenticated selected-child
and World transaction boundary below. Native source, compiled geometry,
collision and incident joins remain separate acceptance surfaces.

## Static gangway candidates

`refinement.api.build_gangway_profile` is a pure, versioned fixed/floating
construction operation. Its [implementation](refinement/app/gangway_elevation.py)
owns the closed input/output shapes. It accepts a straight directed source
bridge, a complete finite structural assembly, separately identified endpoint support surfaces, a static Water
model and explicit quality limits. Complete source-node bindings, frame/epoch
metadata and observed-versus-identified-model basis remain explicit. The caller
authenticates those inputs; hashes alone are not source qualification.

Both full-width endpoint sections must lie on their supplied support TINs.
Boundary cuts and interval interiors detect gaps, holes and conflicting heights.
The projected assembly preserves body plateaus and modeled ramp hinges; endpoint
interpolation cannot replace that field. All supplied TINs require continuous
heights at shared edges and T junctions. Projection refuses insufficient coverage
or clearance and does not introduce an arch to clear an obstacle. Native parameter coverage,
exact per-face longitudinal/cross gradients, complete endpoint step and affine
Terrain/static-Water clearance must satisfy caller limits. There are no default
geographic quality values. Source/Water/model/policy/native identities bind the
candidate, including the complete assembly, and unchanged remote Terrain does not
enter its consumed support.

Static-Water separation measures the modeled walking top surface against the
identified representative Water level. It does not establish underside clearance,
structural thickness, tidal safety or clearance for vessels. Those properties
need their own geometry and evidence when required by the selected use.

This operation cannot publish a feature, expand a structural parent, qualify
a neighborhood or activate authority. Fixed-pier/floating-body footprints,
all incident junctions, support inference quality and actual selected Water
remain separate qualification inputs. The production parser and closed feature
publisher do not admit this candidate variant. The
[gangway tests](tests/test_gangway_elevation.py) preserve captured source
coordinates and mapped fixed/floating footprints with explicitly inferred model
heights and hinge; they prove the
constructor boundary, not a surveyed or playable complete Pier25 reconstruction.

## Water confluence candidates

`refinement.api.build_confluence_constraint(features, context, projected)`
constructs a disposable terminal constraint for two independently derived
flowing channels. SourceIngestion must qualify complete ordered way/node records.
Their native versions must not postdate the accepted source release; the full
directed projection must occur in the accepted fit's knot order. Forward
projection and the existing polygon-suppression route must separately reproduce
the immutable visible geometry before publication or adoption.

Admission requires exactly one shared source node, one tributary downstream end
and a continuing receiving channel. Existing finite monotone terrain-derived
fits and a unique receiving knot at the shared node are required. Only the
tributary's terminal knot is lowered to that existing value. Raising Water,
unknown source locations, unsupported waterway/control tags, mapped hydraulic
controls or elevations, covered crossings and conflicting layers refuse. This
is a traceable free-flow game-world model, not surveyed hydraulic equality or
proof that no unmapped control exists. No feature ID selects the rule.

The [confluence schema](refinement/contracts/water-confluence.schema.json) owns
`constrained_flowing_knots:v1`. The complete original estimator and diagnostics
remain under `baseline_function`; the new model records the node, receiver
function/context identities, affected interval and lowering. Original geometry,
source references, other knots, receiving function and Terrain remain unchanged.
Keeping the original estimator name after applying a constraint would misstate
the model. The child construction API is not a production publisher or an
authentication substitute.

See [source-node water confluence construction](refinement/docs/architecture/diagrams/water_confluence.md).

Native Water evaluates the admitted knots with the existing height quantization.
Equal quantized regions may reuse its source-approximation and horizontal-union
operations; unrounded functions need not be identical. Retessellation can change
incident receiving triangles even when that function is unchanged. Verify actual
stored coverage, exterior sections, Terrain clearance, cell seams, saved/compiled
geometry and loaded materials independently. Attribute an improvement to an old
interpolation error explicitly; do not report byte-identical receiving geometry.
Existing Water has no collision surface, so Terrain/gameplay collision remains
a separate check. No terrain conditioning, surveyed-level snapping or global
network solver follows from this operation.

## Combined feature preparation

`refinement.api.prepare_surface_refinement(repo_root, base_result_path,
output_root, operations)` prepares closed feature operations from one authenticated
compile base. `bridge_elevation` and `water_confluence` supply existing feature
IDs and an explicitly byte-pinned normalized source document. The separate
`constructed_surface_import` variant creates source-bound identified records
from declared field, policy and original-source snapshots. The `identified_basin`
variant jointly constructs one admitted Water feature and its selected Terrain
replacement under the [basin admission contract](#joint-basin-candidate-admission).
Construction-only Ground, continuation, path-floor and structural helpers do not
become publishable operations merely by producing valid geometry. This is not a
rule language. Complete primitive context is qualified before construction.
Water source projection and the admitted native ribbon/polygon suppression
contract must reproduce the base visible geometry exactly.

Operations with interacting feature ownership refuse. Independent corrections
to one owned shard are merged before writing that shard, manifest and coverage
once, preserving both results. Operations use deterministic kind/feature order;
that order is not a prerequisite graph or a dependent-correction protocol. Joint
basin and authored-dry operations also require disjoint original Terrain-cell ownership. A basin
evaluates the effective feature set including earlier imported records, and
refuses unresolved physical interaction rather than delegating it to a later
correction. Distinct contexts have content-qualified evidence
paths; shared identical context is stored once. The
[surface request](refinement/contracts/surface-refinement-request.schema.json)
binds base/feature hashes, normalized context, source admission, current child
implementation and native execution identities. Replay authenticates every
output; changed dependencies cannot reuse the old result. Base artifact
descriptors must agree with the authenticated payloads, not only parse as JSON.

The bridge-specific public entrypoint delegates to this same publisher. There
is one feature-publication authority and no sequential application of sibling
full trees, which could otherwise discard an earlier correction. Candidate
staging remains tmp-only and retains original source lineage, Terrain and
unrelated artifacts. A genuine `refinement_result.json` is the output; production
selection and World transactions are separate acceptance boundaries.

The [candidate-preparation view](refinement/docs/architecture/diagrams/candidate_preparation.md)
shows this lifecycle and its refusal boundary. A later construction failure
publishes neither the failed operation nor an earlier successful operation.
`test_surface_refinement.py` owns independent-order/no-op, interacting or unknown
operation, and second-operation failure controls; the basin and structural-import
tests own their operation-specific qualification and mixed-owner cases.

Fresh preparation reads the complete base cell manifests, feature shards and Terrain
artifacts, then copies all base outputs into the isolated candidate before
replacing changed artifacts. A sparse changed-cell report or byte-identical
unaffected output does not establish sparse read/copy cost. Reuse authenticates
the complete candidate for the same request; individual operation results have
no separate persisted cache. The feature publisher's implementation fingerprint
conservatively includes shared compiler helpers and schemas, while selected
basin/import constructors add their consumed identities. Proof currency is
distinct from a consumer's actual geometry or placement dependency; downstream
generation follows the [component ownership contract](../../../docs/architecture/principles.md#component-ownership).

## Declared source-bound structural imports

The [constructed import](refinement/constructed_import.py) validates and imports
an explicitly identified finite PWA model through the same feature publisher.
The pinned field is a declared input. Deterministic replay reconstructs the child
from that field and its retained sources; it does not claim to rerun a historical
fit or turn inferred geometry into surveyed evidence. The
[model contract](refinement/contracts/constructed-import-model.schema.json) and
[policy contract](refinement/contracts/constructed-import-policy.schema.json)
own its input shapes and comparison limits.

Creation-only IDs must be globally absent from the authenticated base and match
the declared primary source IDs through the existing canonical identity adapter
and the base's profile namespace. Complete way/node or
multipolygon/member/node evidence is qualified through SourceIngestion. Conflicting
primitive versions, unknown metric interpretation, unsupported roles, missing
source context and incomplete canonical-cell membership refuse. Face roles cover
every triangle exactly once; Land cannot enter structural ownership. Full model
coverage, continuous field/role gradients and shared-height agreement are checked
per imported record before the staged child can be published. Cross-record joins,
vertical relationships and whole-neighborhood continuity require separate proof.
Coverage uses the native geometry kernel's area budget; exact rational decisions
apply to shared heights and gradients, not to a zero-hole area certificate.

A complete multipolygon's fence-member height can be qualified as the barrier's
object dimension, separately from structural surface elevation. This follows
OSM's [object-height semantics](https://wiki.openstreetmap.org/wiki/Key:height)
and [fence tagging](https://wiki.openstreetmap.org/wiki/Tag:barrier%3Dfence).
Only an unambiguous member fence with a positive finite bare-meter dimension
uses this interpretation. The literal primitive is retained; its support
elevation and physical realization remain unresolved. Primary objects, standalone
ways, nodes, mixed structural roles and other unqualified metric meanings still
refuse. This interpretation neither changes deck XYZ nor creates an obstacle.
Actual route/barrier crossings and mapped passages require separate geographic
and gameplay qualification.

`constructed_surface` feature geometry is explicitly the MODELED domain. The
[shared wire contract](contracts/constructed-surface.schema.json) retains original
source geometry, primitives, versions and membership separately, through bound
qualified records and the exact original snapshots. A partial inferred apron
does not claim a full parent's geometry, measured width, deck height or ownership.
Imported triangles are retained at their declared canonical heights; no fitting
or consumer-side position correction occurs here.

The operation appends owner records and updates reference-only manifests,
effective counts and artifact descriptors together. Base Terrain and inherited
compile reports remain historical and unchanged. The refinement report carries
the effective additions, ownership and current field checks. An unchanged
reference-only feature shard is reused.

The [captured import fixture](tests/structural_import_fixture.py) regenerates
disposable complete bundles for the native handoff test. With the locked World
interpreter and `PYTHONPATH=tools`, run
`python -B -m World.CanonicalCompilation.tests.structural_import_fixture --output-root tmp/world/canonical_compilation/import_fixture`.
The optional `--supporting-member` case retains a qualified source binding that
does not own an extra face role. The command requires a fresh disposable target
and reports the receipt path/hash for `Constructed.CanonicalImportBundle`.

Every model, policy and source/native snapshot has a declared content-addressed
archive descriptor. Native OSM JSON is validated under its exact retained-input
role rather than misrepresented as a schema-bearing compiler document. Unknown
archive extras refuse; no wildcard evidence namespace is accepted. Shared schemas
resolve from the local registry, and unknown references refuse offline.

Fresh replay and first activation share the same selected-operation identity
checks, including the actual import and source qualification variants. Frozen
reading and recovery authenticate retained bytes without invoking a current
importer. Native parsing, a registered physical producer, explicit Road delegation,
complete-neighborhood quality and gameplay remain separate acceptance boundaries.

## Effective-authority candidates

`refinement.api.prepare_surface_authority(repo_root, active_path, profile_path,
result_path, output_root)` prepares a reversible authority candidate under
repository `tmp/`. It authenticates the existing base authority and exactly
replays the closed surface operations against its immutable compile result.
Admission requires current child, source-context and native execution identities;
every output and the result bytes must equal that replay. Terrain refinements
with unresolved quality policy cannot enter this feature-only route.

The existing authority schema has one optional `selected_refinement` descriptor.
All base identity, external inputs, admission and compile-result fields remain
intact. The original immutable ZIP remains independently addressable. The child
has its own deterministic ZIP, genuine refinement receipt, frozen producer
identities and effective identity derived from both base and child. Normalized
qualified source context is retained. Structural imports also retain the explicitly
declared native primitive snapshots required for complete relation qualification;
this does not claim a historical full-provider snapshot or source reacquisition.

See [effective base and child materialization](refinement/docs/architecture/diagrams/effective_authority.md).

The existing materializer locks and keys the tree by effective identity for a
selected child, and returns `refinement_result.json`. Base-only selection retains
its original cache key and result name. A cached receipt with altered bytes or
an altered output refuses. Frozen child validation authenticates historical
producer identities without calling current source admission or native producers;
new candidate preparation always requires those current identities.

Fresh selection also replays producer origin. Matching archive hashes and producer
labels alone establish retained integrity, not that the claimed original feature
or modeled consumed feature actually produced those bytes. The public
`validate_current_surface_selection` authenticates the complete base and child,
extracts their declared results in disposable storage, and replays the existing
closed publisher. Exact result/output bytes must match. Full authority validation
and producing identities are checked again after replay. Base ZIP admission
records remain owned by base authority validation; the child alone has a closed
result-only ZIP inventory. Neither operation reacquires provider data or runs the
base compiler.

Candidate preparation never changes the accepted pointer. It publishes the
isolated candidate only after full archive validation and a final unchanged-base
check. The production compile promoter refuses to erase or carry a selected
child implicitly. Explicit child acceptance, selected-owner compatibility and
coordinated recovery remain separate requirements. Existing immutable-input and
shared-map guards remain strict; copying a candidate pointer is not a valid
adoption route.

## Qualified selection publication

The refinement owner supplies narrow primitives for a coordinating World
transaction. `prepare_selection_transition` derives the active target from its
compiler profile, authenticates an unchanged-base candidate and current producing
contracts, and installs the immutable child ZIP without selecting it. A conflicting
existing payload refuses. Its [transition contract](refinement/contracts/canonical-selection-transition.schema.json)
retains the exact old/new pointer bytes and identities; no historical source
acquisition or base compilation is executed.

`publish_selection_transition` serializes with ordinary base promotion through the
same profile lock and compares the expected prior pointer before atomic replacement.
Transition preparation, direct first activation and E2E candidate admission use
the same fresh-origin check before accepting new bytes. Frozen reading, repeated
selection and recovery deliberately retain their historical contracts; they
authenticate original bytes without asserting current producer equivalence.
First activation requires current producers. Repeated selection is a no-op;
qualified reverse publication restores the exact prior bytes. Frozen validation,
recovery and an already-selected no-op do not rerun unused current producers.
Wrong derived targets, changed profiles, damaged payloads, forged recovery bytes
and pointers matching neither known state refuse without overwriting the pointer.

The public compiler CLI exposes `prepare-selection-transition` and
`selection-transition --action validate|publish|restore`. Recovery input files
belong under repository `tmp/`; the coordinating journal owns their retained
metadata. These primitives change one canonical selection only. They neither
accept World output nor make canonical and World publication jointly atomic.
The World coordinator must retain its journal and snapshots until both accepted
states validate. Production World integration and its required acceptance remain
separate from these isolated primitives.

### Durable schema reference repair

New candidates carry the relative owned-schema reference for their final durable
location, even while their payloads are staged under `tmp/`. Both fresh transition
preparation and first publication reject URL annotations. Historical read and
reverse publication still authenticate their original exact bytes.

The closed `prepare-schema-reference-transition` operation can normalize the
demonstrated URL annotation on an already selected child. It changes only
`$schema`, retains the complete base/child identities and requires the exact prior
pointer. It uses the same CAS/restore primitives. It cannot admit a new source,
compiler, child, elevation or output. Its historical handoff reader binds the
original selected publication to the exact metadata successor.

World must update its corresponding accepted pointer binding through the existing
outer recovery and authority publication. This does not invoke a realization
writer. Retained old/new pointer bytes qualify historical geometry receipts;
they never relabel historical producer fingerprints as current execution.
