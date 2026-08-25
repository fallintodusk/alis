# Territory Generation Contract

Start at [territory_generation.md](territory_generation.md). This file is the
deep contract SOT; import only the section routed for the current task.

This document owns CONTRACTS - what must be true. Commands and flags are
owned by the script and tool READMEs above; execution steps live in the
active milestone todo. A fact stated here is not restated there.

## Purpose

ProjectWorld expands trustworthy generated geography before it imports or
polishes legacy map content. The next territory milestone must establish the
large-scale world skeleton first: terrain, water, land cover, vegetation,
roads, and blockout buildings.

The order is fixed:

```text
accepted source coverage
  -> canonical territory cells
  -> generated Unreal geography
  -> measured World Partition optimization
  -> legacy-content or regenerative-polish decision
```

This prevents existing props and hand-authored scenes from becoming hidden
inputs to the geographic compiler.

## Stable identity

Coordinates and generated contract IDs own territory identity. City, region,
and country names are human-facing aliases only; renamed places or changed
administrative borders do not rename canonical cells or generated packages.

| Boundary | Stable identity |
|---|---|
| Source coverage | Pinned coordinate envelope and source snapshot hashes |
| Compiler | `grid_id` plus integer cell coordinates |
| Generated Unreal content | Grid/profile-derived package and actor identity |
| Human navigation | Profile label or place alias |

`kazan_territory_v1` is one generated and traversable territory, not a broad
visual envelope around a smaller playable island. Every point inside its
geodesic product boundary requires generated ground, player collision, and
World Partition load, unload, and reload acceptance. Technical source cells
outside the circle may exist as generation margin and do not enlarge the
product boundary.

`kazan_scenario_v1` selects the first route, gameplay anchors, AI/navigation
coverage, and content-density focus inside that territory. It never redefines
which geography is playable and never duplicates geographic authority.

## Geospatial authority

The accepted horizontal authority is WGS84 (`EPSG:4326`). Canonical
compilation uses UTM zone 39N (`EPSG:32639`), and Unreal derives local
centimetres from accepted projected coordinates. Raw Unreal transforms are
never geographic authority.

The named Kazan v1 control and territory centre is Spasskaya Tower:

- latitude: `55.796504` degrees north;
- longitude: `49.108322` degrees east;
- projected coordinate: approximately `E 381408.925`, `N 6185050.761` metres;
- horizontal source: operator-supplied landmark coordinate;
- vertical coordinate: not supplied by this control point.

The compiler lattice origin remains independently frozen at
`E 379760`, `N 6184170`. The integer Unreal GeoReferencing level origin is
`E 381409`, `N 6185051`; Epic recommends integer projected level-origin
values to limit floating-point rounding. The exact Spasskaya coordinate stays
the named control and product centre. Separating these three roles preserves
existing cell IDs while placing the Unreal world against a named real-world
point. Changing only the engine origin must not change `grid_id` or any cell
identity.

Official basis: [Epic GeoReferencing a Level](https://dev.epicgames.com/documentation/en-us/unreal-engine/georeferencing-a-level-in-unreal-engine).

Every accepted territory profile records its CRS pair, transform identity,
georeference origin, lattice origin, vertical datum, source provenance, and
declared accuracy. At least three non-collinear, named control points across
the territory must pass forward and inverse placement checks. Important
authored landmarks store both WGS84 and projected coordinates; generated
geometry remains reversible through the recorded transform instead of
duplicating latitude/longitude at every vertex.

Coordinate precision and source accuracy are separate. The current
quantization and placement gate is `0.01 m`, but it does not claim centimetre
survey accuracy. At Spasskaya Tower the UTM scale factor is approximately
`0.99977251`, or `-0.02275 percent`: treating raw projected distance as ground
distance is about `1.27 m` short over the `5.58 km` radius and `2.54 m` over
the diameter. The territory boundary is therefore defined geodesically in
WGS84; UTM remains the interoperable compiler grid. Provider geometry and
elevation errors are recorded separately and are never hidden by the smaller
numeric transform tolerance.

### Geodetic error budget

One tolerance must never hide several different errors. Every accepted build
and anchor-resolution receipt reports these terms separately:

| Term | Kazan v1 gate |
|---|---|
| Numeric forward/inverse and Unreal realization error | `<= 0.01 m` |
| UTM-versus-geodesic scale delta across the product radius | `<= 1.50 m` |
| Control-network residual | `<= source accuracy + 0.10 m` per control |
| Horizontal anchor error | `XY source accuracy + feature drift + coordinate quantization <= horizontal tolerance` |
| Surface-snap Z error | `sampled terrain source accuracy + sampling/height quantization residual <= vertical tolerance` |
| Absolute Z error | `vertical source accuracy + numeric resolver error <= vertical tolerance` |

Placement classes keep the rule simple and prevent a large source error from
being accepted as an implementation tolerance:

- `precision`: `2.0 m` maximum total horizontal error; required for hero
  landmarks, gameplay-critical anchors, and comparison/reference cameras;
- `standard`: `5.0 m` maximum total horizontal error; allowed for ordinary
  generated geography and non-critical placement;
- `context`: source-reported accuracy, permitted only for background
  geography and never as authority for a gameplay or authored anchor.

Every control and anchor records its class, horizontal tolerance, vertical
mode, vertical tolerance, and stable horizontal provenance reference. XY and
Z evidence are axis-specific. `surface_snap` derives Z provenance from the
accepted canonical terrain cell actually sampled. `absolute` declares a
separate vertical provenance reference; it may equal the horizontal reference
only when one qualified record genuinely supports both axes. Resolver receipts
expose both identities plus every summed term without copying source accuracy
into each anchor. Unknown accuracy is reported as unknown; it is not converted
into a convenient number. Such a point may
retain a name and coordinate as provisional metadata, but cannot accept the
control network or a precision placement until qualified by a better source.
The operator-supplied Spasskaya Tower coordinate is the named origin now; its
source accuracy must be qualified in the Slice 1 control-network receipt.

The accepted Slice 1 control profile qualifies the operator-supplied
Spasskaya coordinate with a conservative `5.0 m` project-policy bound and
`context` class. It is valid as the named product centre and network metadata,
but it does NOT authorize a precision hero-anchor placement; that requires a
later surveyed or otherwise precision-qualified coordinate. Two non-collinear
technical-corner controls span the territory and qualify the EPSG transform at
the numeric gate. Their unknown Z is explicit, so the network accepts XY while
authorizing no absolute height.

At least three non-collinear controls are checked individually and as a
network. The receipt includes maximum and RMS residual, pairwise distance
error, boundary/corner error, and the worst-case total error for each placement
class. Any exceeded or unknown required term fails closed. Expansion beyond
the accepted projection budget requires a new geospatial profile or CRS
topology version; it never accumulates silent drift over distance.

Projection scale delta is reported separately because a WGS84-to-UTM
transformation does not itself misplace a coordinate; it slightly changes map
distance relative to ground distance. It therefore gates approximately 1:1
scale fidelity but is not added again to an anchor's absolute-position budget.

Vertical error is independent. A source without a qualified vertical datum
cannot authorize absolute Z. Terrain-relative placements use `surface_snap`
against the accepted canonical terrain samples in the compilation bundle,
never a generated Landscape actor lookup. Each canonical terrain cell freezes
its raster snapshot identity, vertical datum, source accuracy/confidence, and
the height sampling/quantization residual. A seam point must resolve to equal
terrain provenance on every participating cell or fail closed. The current
Copernicus GLO-30 source is a DSM in EGM2008
(`EPSG:3855`), not survey-grade bare-earth terrain. Copernicus publishes
GLO-30 absolute vertical accuracy below `4 m` and absolute horizontal accuracy
below `6 m` at its stated 90 percent confidence; the admitted product receipt
owns the exact values used by ALIS. Absolute-height anchors
require qualified vertical provenance and their own tolerance. This preserves
approximately 1:1 placement without claiming accuracy the current 30 m terrain
source cannot provide.

When coverage needs more than one admitted raster tile, source ingestion first
creates one versioned, deterministic accepted mosaic. Its global authority
hashes the ordered admitted component identities and semantic sample hashes
plus the frozen mosaic contract. Exact COG bytes have a separate artifact
hash. The mosaic rejects datum disagreement, geometric coverage gaps,
unresolved nodata, or unequal positive-area overlap. Components must have zero
rotation, equal signed pixel sizes, and origins separated by integer pixel
offsets within the frozen tolerance. Equal absolute resolution does not prove
alignment, and admission never silently resamples a shifted component.

Global provenance carries the full mosaic authority. Each terrain cell carries
the stable mosaic contract ID plus only the ordered component semantic pairs
touched by its core and halo bilinear kernels. This preserves exact source-tile
lineage without making an E048-only content change invalidate E049-only cells.
For `kazan_territory_v1`, the raster-sampling contract pins unit GDAL warp
scales outside the spatial grid identity, which makes the four-pixel bilinear
dependency derivation exact while preserving the shared Kazan `grid_id`.

Feature cell lineage carries stable source and generator contract identity,
not the hash of the complete current feature population. The full feature-set
semantic hash belongs to the compile run contract. This permits precise local
feature rebuilding while keeping the resulting accepted tree reusable as the
next base. Linear road and water fragments prove exact common endpoints.
Polygonal water remains single-owner authority with per-cell references; both
adjacent cells must derive the same quantized segment on their common edge.

Geometry and hydrologic behavior are independent canonical properties. A wide
flowing polygon and its exact ordered centerline IDs form one surface group
before Z fitting or clipping: the polygon owns visible area, its centerline
owns flow direction, and only uncovered tails remain ribbons. Width resolves
before suppression. The canonical line is trimmed against polygon authority
expanded by half its resolved width plus the profile clearance; the final
round-buffered footprint must record zero overlap. Missing, disconnected, or
conflicting group authority rejects; names and proximity never repair it.

Water Z is a canonical surface function, never terrain Z copied per output
vertex. Kazan v1 uses a canonical-terrain median for standing
water and a rolling median plus downstream non-increasing L1 fit for flowing
water. Operator approval freezes that visual geographic v1 policy; a changed
estimator or width/buffer contract creates new canonical authority.
The compiler persists quantized knots and fit diagnostics. Realization must
evaluate that function into level cross-sections and identical shared-edge Z.
Widths are source metric values when valid, otherwise profile heuristics marked
`heuristic_visual_not_surveyed`. Hidden, temporal, non-surface, deferred, and
unknown semantics never become permanent visible water by fallback.

`width_m` is the total visible ribbon width, not a radius. Canonical GIS
buffering therefore uses `width_m / 2`. For the admitted UE 5.8 round open-path
route, an exact production-builder regression pins the native offset input to
`width_m`: it produces `width_m / 2` on each side and `width_m` in total. Do not
add a second half conversion from generic offset wording.

Water surface functions are versioned executable contracts. Unreal currently
accepts only `standing_polygon_quantile:v1` and
`rolling_quantile_l1_isotonic:v1`, preserves the integer version in semantic
identity, and rejects missing or unknown ID/version pairs. A new compiler
function version requires a matching realization implementation and proof
before its canonical output can be consumed.

Ribbon cell membership derives from the final quantized buffered surface
footprint, not only its centerline. A footprint-only cell stores an explicit
reference to the same feature-level centerline, width, and surface function;
it does not create a duplicate Z authority. Width changes dirty the union of
old and new surface-touched cells, while realization clips the derived surface
to exact cell bounds and evaluates one shared function on both sides.

The compiler classifies and resolves the complete admitted water population
before target-cell pruning. Polygon geometry and final ribbon footprints then
establish target relevance, including a ribbon whose centerline is outside the
territory edge. Such a footprint-only axis may fit Z from the authenticated
canonical terrain halo; axes that already intersect a target cell retain the
core-terrain fit. A surface reaching beyond available halo context rejects
rather than inventing elevation. Fully polygon-suppressed axes retain their
suppression state and never fall back to raw centerline output.

Incremental feature compilation always evaluates the complete target-relevant
water population, even for an unrelated local vector change. It replaces those
water outputs in memory, semantic-diffs complete cell documents against the
accepted base, and writes only changed cells. This deliberately small prepass
captures polygon/axis, standing-polygon/ribbon, width, removal, and surface
function dependencies without a second spatial dependency index.

Source basis:
[Copernicus DEM product definition](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM).

## Ownership layers

| Layer | Owns | Must not own |
|---|---|---|
| Source Ingestion | Provider payload admission, provenance, normalized shards | Unreal assets or visual style |
| Canonical Compilation | Metric terrain and typed geographic features per cell | Engine actors or materials |
| ProjectWorld realization | Generic Unreal generation, serialization, manifest, and runtime logic | Kazan data or place-specific assets |
| ProjectWorldTestData | Synthetic profiles, inputs, and authored fixtures; ignored generated packages/manifests during test transactions | Reusable logic, shipping content, or durable generated authority |
| ProjectWorldData | Kazan profiles, canonical JSON, authored assets, and realized Unreal packages | Reusable generator logic |
| Authored overlay | Protected hero locations and game-specific art | Base geographic truth |

Terrain, river and water geometry, land cover, vegetation areas or points,
road graphs, and building footprints must enter through canonical documents.
Generated meshes and actors remain transactionally replaceable consumers.

### Staged authority and persistence

Authority is scoped by pipeline stage; persistence does not make a downstream
representation an editable upstream SOT.

| Stage | Owner and home | Contract |
|---|---|---|
| Kazan intent | `ProjectWorldData/Data/` | Human-edited source, profiles, controls, overlays, and provenance |
| Accepted canonical world | `ProjectWorldData/Data/Canonical/` | Immutable generated JSON bundles and indexes; sole input to production realization |
| Realized Unreal world | `/ProjectWorldData/Generated/` plus external actor/object mirrors | Saved, cooked, manifest-owned runtime representation |
| Authored Unreal overlay | `/ProjectWorldData/Authored/` | Protected manual SOT; never generated or deleted by regeneration |
| Synthetic test world | `ProjectWorldTestData/Data/` and `/ProjectWorldTestData/` | Editor-only deterministic fixture authority; never shipped |
| Candidate and scratch output | `tmp/world/`, `Saved/`, or an isolated validation root | Disposable; never accepted authority |

The production game never regenerates the territory at startup. Editor build
realizes the accepted canonical bundle, cook consumes the saved Unreal
packages, and runtime World Partition streams the cooked representation.
Unreal serialization and replication operate on realized types and mutable
runtime state; the presence of JSON alone does not provide replication, and
runtime state is never written back into canonical data.

Accepted canonical output is promoted only after its complete receipt and
hashes pass. Territory promotion first admits the same-owner source, compiler,
control, and budget set as one contract and enforces hard compile ceilings. It
is never manually edited and remains the precise regeneration base. An
`accepted` candidate under ignored `tmp/` is not production authority.

Logical cell identity does not require one version-control file per cell.
Promotion verifies every accepted output, then stores deterministic compiler
outputs and a normalized receipt in one content-addressed ZIP plus a small
active index under `Data/Canonical/<profile_id>/`. Observational timing metrics
remain outside D1 and the bundle; raw provider payloads are forbidden there.
Validation authenticates every path, byte, schema, stored admission receipt,
and current human input hash without rerunning old evidence through newer tools.
Materialization needs no recompile. Azure Git LFS stores the internal ZIP while
the public source branch excludes it. The matching public data release must
provide the exact authenticated ZIP, attribution, and licence notices so a
public checkout can fetch and materialize the same authority. Packing preserves
cell IDs, lineage, and dirty selection.

## Layered regeneration contract (scale-out precondition)

This contract removes the one failure that cannot be afforded at territory
scale: a large generated region is rebuilt and the rebuild silently destroys
or shifts content in layers above it, including manual polish applied late in
production. No territory-scale generation starts before this contract is
proven with full layer coverage.

Layer stack, bottom to top:

| Layer | Ownership | Regeneration behavior |
|---|---|---|
| Terrain (Landscape) | Generated, profile-owned | Fully replaceable |
| Water | Generated, profile-owned | Fully replaceable |
| Roads | Generated, profile-owned | Fully replaceable |
| Vegetation and foliage | Generated, profile-owned | Fully replaceable |
| Building massing (territory blockouts) | Generated, profile-owned | Fully replaceable |
| Gameplay placement | Generated, profile-owned | Fully replaceable; mutable runtime state is external |
| Modular building assembly (selected features; later generator) | Generated, profile-owned | Fully replaceable; passes admission before first territory/playable use |
| Presentation material | Generated, profile-owned | Fully replaceable |
| Authored overlay (hero locations, spatial anchors) | Authored, protected | Byte-identical across any regeneration |
| Manual polish layer (late art passes above everything) | Authored, protected | Byte-identical across any regeneration |

The accepted design is an extensible data-defined dependency graph rather
than a closed C++ enum. The production layer catalog supports adding,
removing, ordering, and replacing generators without changing the map
contract. It distinguishes:

- geographic layers generated from provider/canonical data;
- gameplay-placement layers generated from designer-authored JSON;
- protected binary overlays that are genuinely hand-authored and are never
  regenerated;
- runtime state, which is saved and replicated but never written back into
  generation SOT.

Each catalog entry declares `layer_id`, `layer_kind`, `generator_id`,
`generator_version`, `depends_on`, source selectors, coverage, artifact root,
spatial ownership, dirty granularity, dependency halo, and runtime mapping.
`display_order` is optional presentation metadata and never substitutes for
`depends_on`. The supported kinds are generated geography, generated gameplay
placement, protected authored overlay, and runtime-state exclusion.

The reusable profile schema is
`ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json`.
Concrete profiles live under the owning data plugin's
`Data/Profiles/Realization/` directory. The loader verifies that the profile,
map package, protected roots, runtime exclusions, and every layer artifact root
belong to that same data plugin. Generator settings are typed by the registered
ID/version pair. The v1 terrain contract requires one component per proxy; the
v1 water contract requires non-Nanite Single Layer Water. Unknown pairs,
versions, or settings fail before mutation.

The executable registry is deliberately smaller than the schema vocabulary.
It currently admits only these complete tuples:

| Generator | Kind | Selector | Spatial ownership | Dirty unit | Runtime mapping |
|---|---|---|---|---|---|
| `project_landscape:v1` | generated geography | terrain | logical Landscape with cell proxies | canonical cell | World Partition owner |
| `project_water_mesh:v1` | generated geography | water | cell local | canonical cell | World Partition spatial |
| `project_road_mesh:v1` | generated geography | roads | cell local | canonical cell | World Partition spatial |
| `project_vegetation_instances:v1` | generated geography | vegetation | cell-local HISM actors | canonical cell | World Partition spatial |
| `project_building_massing:v1` | generated geography | buildings | cell-local StaticMesh actors | canonical cell | World Partition spatial |
| `project_gameplay_placement:v1` | generated gameplay placement | gameplay placements | object-local ObjectDefinition actors | stable object ID | World Partition spatial |

A layer artifact root must be a strict descendant of the owner's `Generated/`
root; no layer may claim that complete root.

Dependencies form a directed acyclic graph and execute in topological order;
cycles, unknown layers, unknown generators, and artifact-root overlap fail
before mutation. Adding a layer creates only its owned artifacts. Removing a
layer deletes only artifacts proven by its accepted manifest. Replacing a
generator increments its version and dirties that layer plus transitive
dependants; reordering display metadata alone does not regenerate output.

Every normalized layer contract includes a semantic profile-execution hash.
That hash covers the owner, canonical profile, map package, runtime profile,
logical Landscape identity, partition setting, protected roots, and excluded
runtime roots. The layer identity additionally covers dirty granularity and
the complete registered tuple. Changing any generator-consumed behavior
therefore produces a contract mismatch and a full dirty layer; JSON whitespace
alone does not.

The dirty vocabulary supports whole layer, exact canonical cell, source tile,
and stable object ID. Each executable pair supplies its typed unit domain.
Cross-granularity dependencies require an explicit source-unit to target-unit
mapping; terrain-cell changes therefore dirty only gameplay objects assigned
to that cell. Computed removal units use the union of current and accepted-prior
units. Operator additions may target only current units or explicit `*`.
Canonical-cell halo expansion is clipped to the target layer's real domain, so
it cannot invent off-territory cells. Input hashes discover the minimum dirty
set. An operator may manually add work for design, but may never remove
computed units or bypass transitive dependency expansion. Data Layers may group
or toggle realized actors, but do not establish generation ownership by
themselves.

Terrain cell input identity is the authenticated canonical terrain artifact.
Water cell input identity is a deterministic hash of only water semantics
consumed by that cell: membership, geometry, width, surface function/version,
surface Z, cell bounds, and relevant coordinate quantization. The enclosing
all-feature artifact hash is not water identity; a road or building edit in the
same cell cannot dirty unchanged water.
Road cell input identity likewise covers only the terrain dependency and road
semantics consumed by that cell. Adding another profile layer cannot invalidate
an accepted layer merely because the aggregate profile document hash changed;
the normalized layer contract remains that layer's behavior authority.
Building cell input identity covers only intersecting canonical building
geometry and admitted height, the owning terrain artifact, intersecting authored
building masks, quantization, and the normalized building-layer contract. A
building that crosses a cell boundary participates in each affected cell; an
unrelated road, water, vegetation, or authored overlay does not dirty it.
Gameplay-object input identity covers its typed placement record, referenced
ObjectDefinition identity, owning terrain-cell input, provider contract, and
normalized gameplay-layer contract. ProjectWorld consumes the ProjectCore
spawn interface; ProjectObject remains the sole ObjectDefinition and capability
realization owner. The generation source owns stable placement and definition
identity only. Quantity, container contents, condition, interaction progress,
and other mutable runtime state remain in runtime persistence and replication.

Dirty selectors are transient operation input, not profile state. The wrapper
writes one schema-validated dirty-input document from the accepted per-layer
unit hashes plus operator additions. The commandlet returns the computed union
and dependency closure. A first Apply dirties every generated layer. An exact
layer no-op retains its accepted layer manifest; map or presentation scopes
remain byte-authoritative and may republish if UE rewrites their packages.
Generators that explicitly save their own persistent external actors do not
trigger a broad dirty-package save when they are the only mutation. New maps,
Landscape changes, partition-policy changes, and mixed-owner mutations still
use the broad save path. A cell-owned producer that transitions to no output
must destroy the actor and explicitly delete its external package file before
claiming a self-saved mutation; in-memory destruction alone is not persistent.

Each active `layer_<realization_profile_id>_<layer_id>` manifest owns an exact
artifact list plus semantic output identity. The profile namespace prevents two
maps that both use `terrain` or `water` from colliding. Normal layer assets must
exactly populate the declared artifact root.
World Partition external actor/object files may also belong to the layer only
when the commandlet names each exact file under the target map's confined
external-package roots. Filename prefixes never establish ownership. Files not
claimed by a layer remain map-owned, and prospective authority rejects overlap.
Profile-driven layer removal retires only that layer's accepted artifacts and
manifest; shared presentation scopes retain their stricter active-consumer
guard. Map, presentation, and all touched layer roots use the same snapshot,
journal, recovery, and final active-set commit.

Invariants:

1. Authored layers never live inside generated-owned roots. Every generated
   package may be deleted and rebuilt at any time; nothing authored is lost
   when that happens.
2. Any Apply - full, incremental, or clean rebuild - leaves every authored
   package byte-identical. A rejected Apply restores generated files exactly,
   including restoring an initially absent target to absence.
3. Generated content is owned through a generated-artifact manifest recorded
   at acceptance, not through embedded tags alone. Actors additionally carry
   runtime ownership tags where supported; artifacts that cannot
   self-identify (engine-generated HLOD, external objects) are owned only
   through the manifest.
4. A bounded canonical-input change regenerates only its proven impact
   scope; unchanged layers produce semantically identical output
   (fingerprint equality), so polish placed relative to unchanged geography
   cannot drift.
5. Hand edits inside generated roots are lost by design and forbidden by
   process. Enforcement is the drift validator below, which inspects the
   actual generated tree against the accepted manifest; it never depends on
   Git status.

Every mutating operator route previews the exact map and scopes it will
replace, the Authored root and Landscape correction layer it protects, and
the scopes it will not touch. Interactive mutation requires an exact `yes`;
automation must opt in explicitly. The generation-history view is derived
from immutable active and archived manifests and is read-only - it never
becomes another authority store.

### Authored anchor semantics (byte equality is not enough)

An authored package can stay byte-identical while becoming semantically
broken: a referenced generated actor is deleted, its outer package identity
changes, or the geography beneath it moves while its transform does not.
Authored content therefore declares one of three relations to generated
geography. Canonical coordinates are the single durable authority; raw
Unreal world position is never the durable geographic SOT - the Unreal
transform is always derived through the accepted coordinate resolver.

1. Coordinate-anchored - stores canonical CRS, horizontal coordinate,
   vertical datum/origin and height, optional stable orientation/offset,
   and the compatible grid or territory-profile identity. Independent of
   any generated artifact.
2. Feature-anchored - freezes the stable canonical feature ID, expected
   feature class and geometry type, expected canonical point, resolver
   version, local offset/orientation, and allowed movement tolerance. The
   resolver projects the expected point onto the current geometry of that
   same feature, so additive geometry growth cannot move the anchor while
   real displacement remains measurable. Resolution is NEVER a direct
   reference, attachment, ownership, or outer relationship to a generated
   actor or package. It fails closed when the feature disappears,
   changes class or geometry type, or exceeds tolerance.
3. Authored masks and exclusions - protected inputs consumed by
   regeneration (for example foliage exclusion areas), owned in the same
   canonical coordinate model as anchors.

All overlay records resolve before any map mutation. Each placement creates
one manifest-owned generated Level Instance anchor with a soft reference to
the protected authored map. Receipts freeze the overlay identity and hash,
each resolution binding and transform, resolved/refused/placed/mask counts,
and maximum drift. Any unresolved or over-tolerance anchor rejects the whole
operation before generated or authored packages change.

The generic isolation gate selects the exact assets, components, and
storage roots that implement these anchor classes.

Every regeneration proof must show BOTH: authored package hashes unchanged,
AND authored actors, anchor bindings, expected world transforms, and
required references still resolve. Feature-anchored content fails closed
when its canonical feature disappears or moves beyond its declared
tolerance; the failure is reported, never silently re-anchored.

### Generated-artifact manifest and drift validation

Each accepted realization records, per artifact: map/profile/input identity,
artifact path, artifact kind, owning generated layer, and a stable semantic
digest (byte digest where output is byte-deterministic). The validator
rejects, as "unowned or drifted generated content":

- an artifact under a generated root absent from the manifest;
- a manifest artifact missing on disk;
- an owned artifact whose current digest differs from the accepted digest;
- ambiguous ownership: two owners claiming one artifact path.

Ownership is distinct from consumption. Scopes:

- map-owned artifacts: the map, its external actors/objects, its map HLOD;
- presentation-profile-owned artifacts: shared generated material consumed
  by several generated maps;
- generator-layer-owned artifacts: water, foliage, or assembly resources;
- consumer references are recorded separately from the single owner.

Two legitimate consumers of one owned artifact are never an ownership
conflict; two owners for one path always are.

The one always-loaded Recast navigation authority stays inside its map
package. It is map infrastructure, not a streamable actor, so a random UE
actor GUID must never create a generated external-package path.

### Manifest acceptance lifecycle

The durable accepted manifest is the authority; a manifest regenerated from
the current tree can never "accept" a hand edit it was meant to detect.

Accepted manifests live in the owning data plugin's tracked, NON-GENERATED
authority root, outside every path deleted by Apply, Delete, clean
reconstruction, or generated-tree rollback. Synthetic authority lives in
`ProjectWorldTestData`; production authority lives in `ProjectWorldData`.
`ProjectWorld` owns only the manifest contract and lifecycle logic. The
manifest document is never one of its own generated artifacts. A manifest stored inside
the scope it governs would delete its own authority on reconstruction and
could be damaged together with the content it is meant to audit.

A realization operation can mutate several ownership scopes at once
(existing proof: applying a representative map touches both map-owned
assets and the shared presentation-profile material). The lifecycle is
therefore built around an explicit MUTATION SCOPE SET:

```text
operation declares every scope it may mutate
-> preflight validates every participating accepted manifest
-> preflight performs global ownership-conflict validation
-> snapshot artifacts and manifests for the complete scope set
-> generator emits candidate artifacts and one IMMUTABLE candidate
   manifest per touched scope
-> validate all candidates together
-> replace artifacts, then atomically replace ONE active-manifest-set
   record LAST
-> rejection restores the entire scope set
```

Manifest documents are immutable and versioned; activation is owned by a
single active-manifest-set record, replaced atomically as one small file.
Re-enrolling a previously retired scope continues after the highest immutable
generation in active, historical, or archived manifests; numbering never
restarts and an old manifest is never overwritten.
The record contains: transaction/generation ID; the exact active scope
set; manifest path and SHA-256 for every scope; the prior active-set
SHA-256; and the acceptance operation ID. Only manifests referenced by the
active-set record are authoritative - a candidate manifest merely existing
under the authority hierarchy activates nothing. Ownership transfers and
multi-scope retirement share this one unambiguous commit point.

Rebuilding one map never deletes or re-enrolls a shared artifact unless
that shared scope is explicitly in the operation's scope set. Ownership
transfer between scopes updates all affected manifests in one transaction.
A consumer reference never grants the consumer permission to mutate the
owner's artifact.

One world-data owner has exactly one physical generated presentation root and
therefore at most one active presentation-profile scope. Apply fails closed if
a different presentation scope is active. A profile-ID transition explicitly
deletes its consuming maps, atomically retires the old scope with the last
consumer, then enrolls the new profile; two scopes never share that root.

### Recoverable transactional replacement and interruption

Replacing a World Partition map, many external actor files, generated
materials, and several manifests cannot be one literally atomic filesystem
operation. The guarantee is RECOVERABLE TRANSACTIONAL REPLACEMENT:

```text
validated candidate prepared
-> recovery snapshot/journal made durable
-> generated artifacts replaced
-> immutable candidate manifests written
-> ONE active-manifest-set record atomically replaced LAST
-> recovery state removed only after complete success
```

Interruption semantics:

- a crash before the active-set record is replaced leaves the previous
  active set authoritative; the partially changed tree fails preflight;
- the recovery snapshot and transaction journal remain available;
- the next normal Apply refuses mutation;
- only the EXPLICIT TRANSACTION RECOVERY operation (below) resolves the
  interrupted transaction;
- no command ever generates a fresh manifest from the partial tree.

Schema (frozen fields): ownership-scope ID; normalized repository-relative
artifact paths; digest kind and digest-algorithm/version; generator/schema
version; accepted input/profile identity; consumer references. The manifest
SHA-256 is recorded in the acceptance receipt. Ignored `Saved/Validation`
receipts are evidence copies, never the only manifest. A clean clone
validates tracked generated content without access to another machine's
ignored receipts. Ambiguous ownership is checked GLOBALLY across all active
accepted manifests, not only within one manifest.

### Initial manifest enrollment (one-time, non-circular)

No accepted manifest exists before the generic isolation gate. The
enrollment authority is accepted inputs plus the pinned generator revision -
NEVER the current working tree. Hashing and adopting the tracked tree,
even after human review, is forbidden: it would legitimize exactly the
hand edits the validator exists to detect.

```text
accepted inputs + pinned generator revision
-> isolated clean reconstruction through the supported route
-> existing D0-D3 and generated-output validation
-> candidate artifacts and manifests
-> transactionally publish manifests to the working tree
```

The tracked tree is only a comparison target: explained differences are
recorded as evidence; the validated candidate replaces the tracked
generated artifacts; unexplained differences BLOCK enrollment until their
cause is understood. Repository commits remain operator-owned.

Enrollment is the FINAL mutation of an acceptance cycle. Top-level Matrix
runs are destructive only inside their locked transaction and ALWAYS restore
the exact pre-run generated tree before returning accepted or rejected
evidence. They run before enrollment because their immutable evidence
authorizes the later mutation; nothing may regenerate after enrollment, or
the enrolled pair is invalidated. Enrollment covers every current scope
explicitly: the P0 map scopes, the representative map scopes, and the
shared presentation-profile scope. The frozen acceptance sequence is:

The test-level selection SOT is
[World Pipeline Test Layers](../../../../docs/testing/world_pipeline_layers.md).
A normal slice stops at L2 and does not package. The complete sequence below
is the L4 route for a milestone/release or a slice that changed cook, IoStore,
shipping inclusion, packaged runtime, or rendered-performance boundaries.

```text
one common Check receipt
-> P0 + representative internally destructive, externally observational
   Matrix gates using that receipt
-> final enrollment (the last generated-tree mutation)
-> durable read-only authority audit (references the E2E run IDs;
   records final active-set SHA-256, every active manifest
   path/SHA/generation, generator fingerprint, exact artifact
   verification)
-> read-only package + packaged Presentation Gate against that exact
   final tree (their operation IDs recorded in the audit)
```

The durable audit step is executable, not procedural. What it checks and
how to run it are owned by
[scripts/ue/world/README.md](../../../../scripts/ue/world/README.md); the
contract here is only that a rejected audit receipt BLOCKS the read-only
package step.

Authority documents are written LF-only, deliberately. Their ON-DISK bytes
are hashed and those hashes ARE the activation authority, while the
repository declares `eol: lf` for these paths - so persisting Windows CRLF
would make git rewrite them on checkout and every `manifest_sha256` would
mismatch on a clean clone. Any future document whose stored hash is
authoritative must follow the same rule; verify with
`git check-attr text eol` rather than assuming.

Fingerprint currency FAILS CLOSED: every active manifest must carry the
current fingerprint of its owning producer, because the audit's claim is that
the accepted scope is reproducible by today's producer. Producer-local source
sets are explicit, not inferred transitively. True shared byte-producing or
manifest-producing primitives participate in each affected producer; terrain,
water, road, vegetation, building, presentation, and map implementation files participate
only in their owners. Direct shared placement/lifecycle dependencies participate
in every producer that calls them. A road-only implementation edit therefore
cannot stale terrain, water, vegetation, presentation, or map scopes. The
normalized layer contract remains the data-defined behavior authority and is
not duplicated into the source set. Pure tuple catalog, profile-schema,
validation, and dispatch additions are not byte-producing inputs and therefore
do not move existing producer fingerprints. The read-only auditor and tests
are also excluded because they cannot change generated bytes or manifest
documents.

The enrollment pre-audit has one narrow transition exception: generator
fingerprint drift may exist only on map/layer scopes for the exact Matrix-
accepted target map that enrollment will reconstruct. Every artifact must
still be byte-intact and owned, every manifest and consumer reference must be
valid, and every unrelated producer must remain current. The post-enrollment
audit is always strict. Never refresh stale fingerprint metadata onto old
bytes when the current producer changes their output. The realization wrapper
converts each stale layer fingerprint into whole-layer dirty work, and the
generator must bypass content-semantic reuse for that work. A producer
fingerprint cannot advance unless its existing artifacts were actually
reconstructed by the current producer.

The whole tail is one command:
`bootstrap.py accept --p0-run <id> --representative-run <id>`. It owns the
project-global content lock across audit -> package -> IoStore inspection
-> gate -> audit, so those steps observe ONE coherent tree. Holding the
lock is what makes "exact final tree" evidence rather than assertion: a
before/after comparison alone cannot detect a tree that changed during the
cook and changed back. Check and Matrix never package; the final acceptance
is the one packaging and rendered-performance authority.

The cook discovers the current content tree without reading prior Asset
Registry caches. Generated World Partition external-actor package paths must
remain identical across unchanged Apply, incremental Apply, and clean
reconstruction; Matrix rejects path churn even when semantic actor GUIDs are
stable. After a legitimate artifact-set change, a stale descriptor can still
select a deleted package and omit the current always-loaded actor. Missing
generated external packages or duplicate actor descriptors reject acceptance.

For layered territory profiles, Matrix includes a road-locality
characterization using one canonical cell that contains road data. Roads alone
must originate the selected dirty unit. Its declared downstream dependency
closure is also dirty; for the Kazan profile that means vegetation. Unrelated
siblings stay clean, and content-identical reevaluation writes no geometry or
instances. The common Check proves the matching source boundary: a road
implementation edit moves only the road producer fingerprint, while canonical
road input still invalidates consumers declared by the realization profile.

The package `required_map` selects exactly one descriptor-validated
world-data owner. Both durable audits receive that owner explicitly and the
acceptance receipt records it. A ProjectWorldData acceptance can therefore
never borrow ProjectWorld fixture authority.

A run ID on the command line is a label, not provenance. Acceptance
therefore proves the whole path from run to manifest:

- Each supplied run must carry the profile identity it was passed as, so
  the same accepted representative run cannot be handed in as "p0", and
  each `result.json` SHA-256 is recorded.
- Both Matrix runs must pin the same accepted common Check receipt. Its
  SHA-256 is verified again during acceptance, so common suites run once
  without becoming an unauthenticated skip flag.
- Each run FREEZES the SHA-256 of its own realization receipts, and
  acceptance verifies those bytes before reading a single provenance
  field. A path recorded in `result.json` is not evidence on its own: a
  child receipt mutated after acceptance would otherwise launder itself
  into provenance while the parent hash still validated.
- Identity is compared as a TUPLE per map package, selected by the
  manifest's own `map_package`, never as a union of hashes seen anywhere.
  A union would accept a P0 map carrying the perfectly legitimate compile
  hash of a different leg - that proves the hash occurred, not that this
  map came from this input. One map package produced from two different
  tuples fails closed as ambiguous.
- Fields carrying the literal sentinel `none` are absences, not claims -
  P0 legs declare no runtime profile and the presentation scope is
  profile-owned with no compile result - and must compare equal to an
  absent field on the evidence side. The shared presentation scope owns no
  map, so its provenance is derived from the presentation input its
  consumer map scopes were actually gated with, which must be unanimous.

The emitted `acceptance_chain.json` is schema-validated
(`contracts/acceptance-chain.schema.json`), written atomically, and
written on every HANDLED failure: a refusal produces a `status: rejected`
receipt carrying the partial evidence and the failure, so a rejection
leaves an artifact rather than a console message. That includes unexpected
programming and environment faults, which are converted into a structured
`acceptance_internal_error` rather than escaping silently; only an
interrupt or a failure of the write itself can defeat it. It links both E2E runs and their result
hashes, both audit receipts with their active-set SHAs, the package with
its IoStore receipt hash and required-map result, the Shipping binary and
package-summary hashes, the Presentation Gate operation, and the pinned
bootstrap-preflight identity. Acceptance requires both audits to report
the SAME active-set SHA.

Enrollment after the gates is NOT a plain re-Apply: Matrix has restored the
durable accepted tree, while the newly gated bytes belong to a different
input or generator identity. The contract is that enrollment happens through
explicit clean reconstruction from the gated compile results. The exact
operator procedure and flags are owned by
[scripts/ue/world/README.md](../../../../scripts/ue/world/README.md).

Already-accepted Matrix receipts are never mutated afterward; the durable
audit links the evidence chains. Because enrollment is itself a realization,
the pre-enrollment Matrix receipts prove deterministic generation, not the
final tree bytes. Only the post-enrollment read-only package proves the
shipped tree.

### Active manifests and scope retirement

Activation is defined ONLY by the active-manifest-set record; it is never
derived from directory enumeration. Every manifest referenced by the
active set must exist, validate against its schema, and match its recorded
hash - any missing, malformed, unsupported-version, or hash-mismatched
manifest fails CLOSED as an activation error, never silently drops its
ownership claim. Unreferenced candidate manifests are inert staging or
historical evidence. Unknown files in the authority location are rejected
or explicitly ignored by a frozen naming/layout rule, never interpreted
opportunistically.

Scope retirement is an explicit transaction that first proves no active
consumers remain, removes the owned artifacts, and removes the scope from
the active-set record (archiving its manifest as evidence) in the same
commit. Deleting a map updates a shared presentation scope's consumer set;
deleting its last map consumer also retires that presentation scope and its
generated files in the same recoverable transaction. An obsolete manifest
cannot remain active and create false global ownership conflicts. Legacy
retirement (for example City17) and later generated-layer retirement follow
this route.

Delete means complete package-scope absence. After the commandlet removes the
generated actors, the transactional wrapper removes any remaining map, HLOD,
external-actor, and external-object paths before committing retirement. Those
paths remain covered by the rollback snapshot until the active-set commit.
Direct package ownership is exact: `<Map>.umap`, optional
`<Map>_BuiltData.uasset`, and reserved `<Map>_HLODLayer_*.uasset` companions.
A shared filename prefix grants no ownership, so deleting `L_City` cannot
touch an independent `L_CityNight` package.

### Three operation routes

Operator intent must be distinguishable from corruption; absence of
generated content is NEVER, by itself, authorization to regenerate. The
supported operations are exactly: normal Apply, explicit clean
reconstruction, and explicit transaction recovery.

Normal Apply:

- the accepted manifest must exist;
- every manifest artifact must exist and match its accepted digest;
- missing or drifted content rejects BEFORE any mutation.

Explicit clean reconstruction:

- requested through a named mode/flag, never inferred;
- the accepted manifests remain available outside the generated roots;
- the requested SCOPE SET is identified exactly: a map-only reconstruction
  preserves compatible shared scopes; a map-plus-presentation
  reconstruction names both explicitly;
- partial absence or a mixture of old and unknown artifacts rejects;
- the transaction intentionally removes the complete owned scope set and
  rebuilds it;
- rejection restores the exact prior present/absent state and the prior
  manifest;
- acceptance replaces the manifest only after full validation;
- available only from a coherent accepted state: an interrupted
  transaction must first be resolved by explicit transaction recovery,
  unless a separately named destructive operator procedure deliberately
  abandons it.

Explicit transaction recovery:

- reads the durable journal; without a valid journal, partial generated
  state fails closed;
- either rolls the entire mutation scope set back to the prior active set
  and artifacts, or completes activation ONLY when all candidate artifacts
  and manifests were already fully validated and match the journal;
- when the active-set record was committed but the process stopped before
  removing recovery state, recovery recognizes the completed transaction
  and removes the stale recovery state instead of rolling it back.

### World-data roots and manual polish layer

World JSON and every derived Unreal representation live under their declared
data owner: `ProjectWorldTestData` for synthetic tests and `ProjectWorldData`
for Kazan. `ProjectWorld` owns the schemas, definition types,
generation/realization logic, serialization contracts, replication support,
runtime services, and validation. Data plugins own data and content only;
they must not fork a generator or contain custom world logic.

The accepted root contract is:

- source, profile, control, and overlay JSON is the human-edited authority
  under the owning data plugin;
- accepted canonical generated JSON is confined to that plugin's
  `Data/Canonical/` promotion root;
- generated definitions, maps, external actors/objects, HLOD, materials, and
  other serialized Unreal output are derived representations under that same
  plugin mount;
- compiler coverage names one validated content-capable project plugin;
- its existing UE `.uplugin` descriptor is the root authority from which
  generic code derives the mounted package root, `Data/`, `Data/Manifests/`,
  `Generated/`, and `Authored/`; no parallel custom descriptor exists;
- generated output is restricted to the descriptor-derived
  `/<Owner>/Generated/` root;
- authored output is restricted to the descriptor-derived
  `/<Owner>/Authored/` root;
- arbitrary package paths and cross-plugin deletion scopes are rejected.
- for every data owner, validation requires explicit source/compiler
  profile paths physically confined beneath that descriptor-derived `Data/`
  root; source ID/path, compiler ID/owner, and compiler source ID/path must all
  agree before execution;

`ProjectWorld` has `CanContainContent=false`. `/ProjectWorld/Generated/` and
`/ProjectWorld/Authored/` are invalid. Generic fixtures and representative
adapter evidence live under `ProjectWorldTestData`.

- Nothing authored ever lives under the owning plugin's generated root or
  its external-actor mirrors.
- Overlay and polish representation: each authored overlay is a Level
  Instance (or standalone streamed level) whose packages and external
  actors live entirely under the authored subtree
  (`__ExternalActors__/Authored/...`, `__ExternalObjects__/Authored/...`).
- Placement: the generated map hosts one GENERATED-OWNED anchor actor per
  overlay; the anchor stores the canonical anchor record (per the anchor
  taxonomy) and references the authored Level Instance asset by SOFT
  reference only. The anchor actor is manifest-owned and regenerated; the
  authored packages it points at are never touched by regeneration.
- Allowed authored dependencies: engine content, art assets, canonical
  feature IDs as data, and other authored packages. Forbidden: any hard or
  soft reference from an authored package to a generated package or actor.
- In-asset authored channels: the Landscape "Authored Corrections" edit
  layer is the single sanctioned authored channel inside a generated
  asset; it is identity-tracked (GUID + hash) and must survive
  regeneration unchanged. No new in-asset authored channels are added
  without extending this contract.

### Manifest authority location

- Concrete durable authority roots belong to data plugins:
  `Plugins/World/ProjectWorldData/Data/Manifests/` for Kazan and
  `Plugins/World/ProjectWorldTestData/Data/Manifests/` for synthetic tests.
  `ProjectWorld` owns the manifest schemas and lifecycle logic, never concrete
  manifest instances.
- Layout (unknown entries are rejected):
  - `active_set.json` - the single active-manifest-set record;
  - `scopes/<scope_id>.<generation>.json` - immutable per-scope manifests;
  - `archive/` - retired manifests (evidence only);
  - `journal.json` - present ONLY while a transaction is in flight or
    interrupted; its presence blocks normal Apply.
- Human documentation lives above this root. Even a `README.md` inside
  `Data/Manifests/` is an unknown authority entry and fails closed.
- Manifest `$schema` values remain repository-relative, per the project-wide
  data contract, but are computed from the actual owner root back to the
  ProjectWorld-owned schemas. Schemas are never duplicated into a data plugin.
- Every movable ProjectWorld data loader applies the same rule: resolve the
  declared forward-slash relative `$schema` against the actual document
  parent, then require the normalized result to equal the expected file under
  `ProjectWorld/Data/Schemas/`. Literal reference-string comparisons, URLs,
  absolute paths, backslashes, and different resolved targets are rejected.
  JSON-Schema path patterns are a secondary shape gate for sanctioned fixture
  and production-owner layouts; manifest schemas additionally admit the
  validation-sandbox layout. Exact resolved-target comparison remains the
  authority.
- Scope IDs are lowercase tokens: `map_<package-path-with-underscores>`
  for map scopes and `presentation_<profile_id>` for the shared
  presentation-profile scope.
- The durable root owns the TRACKED generated tree. Isolated validation runs
  regenerate the real generated tree under the global content lock and enroll
  only a transient sandbox manifest root under their own work root. Before a
  Matrix returns on success or failure, its outer transaction restores the
  exact prior generated file path/hash set, including prior absence. Sandbox
  manifests and realization receipts remain immutable run evidence; only L3
  may intentionally leave durable generated-world mutation behind.

### Proof split (execution order)

Proven today (representative scale): the authored Landscape correction
layer, presentation and hero overlays, rejected-Apply restoration, and
unchanged-import fingerprint equality.

- Generic isolation gate (scale-out precondition): authored storage roots
  and the polish-layer physical definition frozen; ownership manifest and
  drift validator implemented; invariants 1-5 plus anchor-resolution proofs
  passed on the layers that exist today (terrain, roads, building preview,
  presentation) across full, incremental, rejected-apply, and clean-machine
  rebuild.
- Per-layer admission (inside geography realization): every new OR
  MATERIALLY CHANGED generator, artifact layout, anchoring contract, or
  ownership scope passes, before territory-scale use: minimal synthetic
  implementation -> authored overlay and polish fixtures above it -> the
  full/incremental/rejected/clean regeneration matrix -> acceptance. This
  applies equally when an existing layer substantially changes its
  implementation (roads gaining intersections or bridges, terrain changing
  its package strategy, presentation changing its sharing model) and to
  later generators such as modular building assembly before its first
  territory or playable use.
- The coverage matrix completes per stage, never ahead of its generators:
  the geography-realization exit gate requires the matrix complete for
  every generated layer ENABLED in the territory profile, with
  later-slice generators (such as modular assembly) recorded as
  "not enabled", never as accepted; the legacy/assembly integration stage
  extends the matrix with each newly admitted layer before its first use;
  the release gate requires the matrix complete for every generated layer
  PRESENT in the released package. Within any stage: every enabled layer
  has at least one applicable protected fixture; coordinate, feature, and
  mask anchor classes each have direct coverage somewhere in the matrix;
  an explicitly inapplicable layer/anchor pair is recorded as N/A with a
  reason; a generator planned for a later slice is PENDING, never N/A.
  The matrix never requires every anchor class against every layer.

### Building geometry authority split

- ProjectWorld owns canonical footprints, height evidence, massing
  constraints, and transactionally replaceable territory blockouts.
- ProjectBuildingAssembly owns optional modular assemblies for selected
  stable building feature IDs, consumed through a stable public
  footprint/massing contract - never through ProjectWorld internals.
- For an assembled feature, the assembly explicitly replaces or suppresses
  its ProjectWorld blockout; the two never render or collide as competing
  building geometry.
- Buildings not selected for assembly keep their ProjectWorld massing.
- Intentionally authored hero buildings remain protected overlays and are
  never silently forced through the assembly generator.
- Assembly output is a generated layer under manifest ownership and
  per-layer admission like any other.

## Delivery stages

### 1. Territory contract

- Select one coordinate-owned extent and resolution profile.
- Pin every admitted source snapshot and license/provenance receipt.
- Estimate cell, feature, terrain-sample, disk, cook, and runtime scale before
  Unreal realization.

#### Territory envelope v1 (operator decision 2026-08-10)

Profile identity: `kazan_territory_v1`. Its accepted source, compiler,
runtime, presentation, validation, control, and budget JSON belongs under
`Plugins/World/ProjectWorldData/Data/`. Generic tools consume explicit
repository-relative paths. Reusable realization commands and helpers require
explicit owner-relative map and profile inputs; they never default to a
concrete data or test-data plugin. The superseded 36-cell tool-owned profiles
were removed and must not be restored. The accepted Slice 1 executable
contracts are
[`kazan_territory_v1.source.json`](../../ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json),
[`kazan_territory_v1.compile.json`](../../ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_territory_v1.compile.json),
[`kazan_territory_v1.control.json`](../../ProjectWorldData/Data/Controls/kazan_territory_v1.control.json),
and
[`kazan_territory_v1.budget.json`](../../ProjectWorldData/Data/Profiles/Budgets/kazan_territory_v1.budget.json).
The validation profile remains absent until the first accepted compile can
provide non-invented per-leg counts and its synthetic twin.

Grid invariant (the deepest contract): every Kazan profile shares ONE
byte-identical grid block - `EPSG:32639`, origin `[379760, 6184170]`, 30 m
samples, 31x31-quad cells (930 m), alignment bounds `[-256..255]`. The
`grid_id` hashes the grid block, and the runtime profile pin refuses a
mismatched grid, so the lattice is frozen once for all envelopes. Envelopes
are integer cell selections over that lattice; growth (district -> city core
-> greater Kazan) is ADDITIVE - new cells extend the selection and existing
cell identities, manifests, and anchors never re-key.

Selection:

- Product boundary: geodesic circle centred on the named Spasskaya Tower
  coordinate, radius `5.58 km`, diameter `11.16 km`, area approximately
  `97.8 km^2`.
- Full-cell generation cover: x in `[-6..8]`, y in `[-6..7]` - 15x14 =
  210 compiler cells. Its technical envelope spans approximately
  `E 374180..388130`, `N 6178590..6191610`, or 13.95 x 13.02 km. The minimum
  spare ground from the product circle is approximately 1650 m west, 1142 m
  east, 882 m south, and 981 m north. This clears the baseline 768 m loading
  range on every side; it does not enlarge the product boundary.
- The admitted Geofabrik Volga snapshot still covers the selection.
  Copernicus GLO-30 N55/E048 and N55/E049 jointly cover the accepted source
  bbox `[48.9838, 55.73314, 49.22434, 55.86052]`. Planning inverse-projects
  the technical bounds expanded by the 30 m terrain halo and 350 m source
  margin, checks 1,902 edge samples, and proves the raster union; one
  intersecting tile is never accepted as complete coverage.
- All geography inside the product circle is generated and walking-traversal
  capable. Scenario content density and AI navigation may remain bounded.

Clip margin remains measured, never inferred from a map screenshot. Grid
north and true north diverge, so the revised geographic bbox must be projected
and checked at every edge and corner against the full technical envelope. The
accepted bbox requires at least `350 m` worst-case horizontal margin beyond
the terrain-resampling halo. The prior bbox and margin table are obsolete.

The prior 36-cell estimates, 40-cell maximum, and matching territory budget
are invalidated by this decision. The 210-cell technical envelope is about
5.83 times the prior square area. Slice 1 performs a planning dry run and
freezes revised source, canonical, compile, Unreal, cook, package, memory,
and traversal ceilings before accepted territory compilation. Measurements
may tighten a provisional ceiling; they never silently redefine success.

The old Option A/B/C comparison is retired. The accepted v1 product scope is
the radius above. Greater Kazan remains later additive growth over the same
lattice and coordinate authority, not a competing v1 option.

### 2. Canonical coverage

- Compile seamless terrain and typed water, vegetation, road, and building
  records across the selected cells.
- Prove stable boundaries, unique provider identities, and incremental rebuild
  scope.
- Run `osmium tags-filter` without `-R` over the full pinned provider snapshot
  before OGR membership selection, preserving matching objects and their
  references. Derive membership from complete point/line/multipolygon
  geometries, then close exact selected IDs recursively and validate
  references. Do not read individual OGR layers directly from a large
  unfiltered PBF: layer accumulation can yield incomplete output despite a
  successful process exit. Do not require unrelated outer boundaries of a
  provider regional extract to be globally reference-complete.
- Keep the dense urban, riverbank, suburban, sparse edge, and cross-cell
  quality scopes in the production compiler profile so acceptance measures the
  same named cells on every run.
- Extend raster/source manifests before crossing the currently supported
  source set; do not hide added tiles behind a global snapshot list.

### 3. Generated Unreal geography

- Realize Landscape and water first, then roads, scalable foliage, and
  blockout building massing.
- The vegetation v1 producer consumes accepted `vegetation_area` and
  `foliage_point` records. It retains explicit points and uses one globally
  aligned, seed-stable lattice inside area polygons and holes. Output is one
  spatial HISM actor per occupied canonical cell, never one actor per tree.
  Mesh paths, density, jitter, scale, seed, and cell population ceiling remain
  typed realization-profile inputs. Its exact dependencies are terrain, water,
  and roads. Candidate points inside selected road footprints, canonical water
  polygon/ribbon footprints matching water v1, or intersecting authored masks
  whose `excludes` contains `vegetation` are removed before placement. The cell
  input hash includes only the road fragments, canonical water polygon/ribbon
  footprint inputs, and authored masks intersecting that cell, so relevant
  exclusion changes dirty vegetation and unrelated overlays remain clean.
- Partitioned PCG was evaluated for v1. The reusable PCG module and forest pack
  contain no admitted graph or biome content, so routing v1 through PCG would
  create a second unproven placement authority. A future PCG adapter may replace
  the producer only through a new registered tuple and the same layer gates.
- The building-massing v1 producer consumes accepted canonical `building`
  Polygon or MultiPolygon geometry and its compiler-admitted `height_m`. It unions parts,
  preserves holes, clips output to canonical cells, and creates one persistent
  StaticMesh plus one spatial OFPA actor per occupied cell. Flat massing is
  anchored to the owning terrain at the clamped footprint-bounds center.
  Geometrically equal footprints keep the lowest stable feature ID; strictly
  contained footprints associate with their container; both participants in
  any remaining positive-area overlap are rejected with their stable IDs.
  Malformed geometry, non-positive or over-profile height, and fragments that
  intersect authored masks excluding `buildings` are rejected before mesh
  output. Cell cuts do not emit internal seam walls, so adjacent fragments do
  not duplicate visible or collision surfaces.
- Building assets are Nanite-enabled opaque StaticMeshes with
  complex-as-simple query-and-physics collision, no navigation contribution,
  no distance field, no authored LOD chain, and no HLOD participation. They are
  territory blockouts only: no interiors, doors, gameplay placement, or modular
  assembly is implied. Later selected-feature assembly must replace only the
  selected massing through its own admitted generator tuple.
- Do not generate HLOD layers, proxy meshes, merged meshes, simplified
  meshes, or HLOD companion packages for `kazan_territory_v1`. HLOD adds a
  separate distant-representation build, storage, cook, and regeneration
  lifecycle that is rejected at territory scale.
- Nanite is the only geometry-detail reduction path for compatible generated
  static meshes. UE 5.8 rejects Single Layer Water as a Nanite shading model,
  so primary water is a persistent cell-local non-Nanite StaticMesh with no
  authored LOD chain. Landscape keeps its component/proxy streaming contract
  and builds its Nanite representation; foliage remains instance-owned and
  uses Nanite-enabled static meshes. None of these paths authorizes HLOD.
- Keep generated content profile-owned, persistent, and transactionally
  replaceable.
- Protect authored overlays during Apply and clean reconstruction.
- Do not migrate legacy buildings, trees, or props into the generated base at
  this stage.

Active P0 and representative maps contain no HLOD companions or serialized
HLOD layer references. Older immutable manifest generations retain their
historical HLOD inventory for provenance and recovery only. Generic lifecycle
code must continue to recognize that history, but no active or future
generation may recreate it.

### 4. Measured optimization

- Derive the initial World Partition design from Epic guidance and the
  documented content/travel assumptions in
  [world_partition.md](world_partition.md), then validate it through automated
  cook and deterministic packaged traversal.
- Manual editor observation is diagnostic only. Machine-readable cell state,
  latency, hitch, memory, and frame-time receipts own acceptance.
- Tune cells, loading ranges, streaming sources, Data Layers, Nanite,
  instancing, and PCG only when the automated comparison shows a benefit.
- Treat zero territory HLOD layers, actors, and companion packages as a hard
  design invariant, not as an optimization candidate.
- Keep optional or Experimental UE 5.8 features behind profile-specific tests
  with a supported fallback.
- Attribute cook and package deltas to the territory rather than total project
  size.

### 5. Content integration decision

Only after the generated territory passes visual, traversal, regeneration,
and packaging gates are retained legacy references moved by content class.
The accepted coexistence, per-class disposition, and retirement conditions
are owned by [legacy_world_transition.md](legacy_world_transition.md). There
is no blanket migration route.

## Acceptance

A territory profile is accepted only when it proves:

- deterministic source-to-Unreal regeneration without manual repair;
- continuous terrain and cross-cell water/road topology;
- bounded foliage and building generation with no duplicate identities or
  cross-cell render/collision seam walls;
- protected authored overlays survive unchanged regeneration;
- every point inside the product boundary has generated ground and player
  collision, and territory routes prove load, unload, and reload behavior;
- deterministic packaged traversal stays inside frozen budgets;
- generated manifests and cooked-package audits contain zero territory HLOD
  layers, actors, or companion packages;
- IoStore contains the required map and excludes provider payloads;
- repeatable captures show the territory layers requested by that profile.

The legal boundary for source data and generated assets remains
[`docs/legal/world_data_and_asset_policy.md`](../../../../docs/legal/world_data_and_asset_policy.md).
The executable evidence route remains
[`tools/World/EndToEndValidation/`](../../../../tools/World/EndToEndValidation/README.md).
