# Territory Generation

## Routes

Jump straight to the fact you need; each lives in exactly one place.

| Need | Where |
|---|---|
| Real coordinates, controls, and error tolerance | "Geospatial authority" and "Geodetic error budget" below |
| Why regeneration cannot break authored work | "Layered regeneration contract" below |
| Layer add/remove/order and precise dirty regeneration | "Layered regeneration contract" below |
| How authored content stays attached | "Authored anchor semantics" below |
| Manifest ownership, activation, retirement | "Generated-artifact manifest" through "Three operation routes" below |
| Where authored and manifest files live | "World-data roots" and "Manifest authority location" below |
| The Kazan v1 envelope, margins, ceilings | "Territory envelope v1" below; production profile/budget files remain absent until Slice 1 accepts them |
| Legacy world transition decision | [legacy_world_transition.md](legacy_world_transition.md) |
| HOW to run realization, audit, enrollment | [scripts/ue/world/README.md](../../../../scripts/ue/world/README.md) |
| HOW to run the gates and acceptance chain | [tools/World/EndToEndValidation/README.md](../../../../tools/World/EndToEndValidation/README.md) |
| World Partition editor workflow | [world_partition.md](world_partition.md) |
| Verified bugs in this area | [pitfalls.md](pitfalls.md) |

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

When coverage needs more than one admitted raster tile, the compiler never
assigns raw tile IDs directly to terrain cells. Source ingestion first creates
one versioned, deterministic accepted mosaic. Its composite identity hashes the
ordered admitted tile identities and content hashes, common vertical datum,
worst qualified source accuracy/confidence, and frozen mosaic/resampling
contract. The mosaic keeps every component tile in its manifest and rejects
datum disagreement, coverage gaps, unresolved nodata, or overlap differences
outside the accepted budget. Every cell sampled from that mosaic then carries
the same composite terrain authority while retaining the input lineage in the
mosaic manifest. This keeps a valid source-tile boundary from becoming a false
canonical seam failure.

Source basis:
[Copernicus DEM product definition](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM).

## Ownership layers

| Layer | Owns | Must not own |
|---|---|---|
| Source Ingestion | Provider payload admission, provenance, normalized shards | Unreal assets or visual style |
| Canonical Compilation | Metric terrain and typed geographic features per cell | Engine actors or materials |
| ProjectWorld realization | Disposable Landscape, water, roads, foliage, and blockouts | Provider payloads or mutable place boundaries |
| Authored overlay | Protected hero locations and game-specific art | Base geographic truth |

Terrain, river and water geometry, land cover, vegetation areas or points,
road graphs, and building footprints must enter through canonical documents.
Generated meshes and actors remain replaceable consumers.

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

Dependencies form a directed acyclic graph and execute in topological order;
cycles, unknown layers, unknown generators, and artifact-root overlap fail
before mutation. Adding a layer creates only its owned artifacts. Removing a
layer deletes only artifacts proven by its accepted manifest. Replacing a
generator increments its version and dirties that layer plus transitive
dependants; reordering display metadata alone does not regenerate output.

The required dirty units are whole layer, exact canonical cell or source tile,
and stable object ID. Input hashes discover the minimum dirty set. An operator
may manually add dirty units for design work, but may never remove computed
units or bypass transitive dependency expansion. Data Layers may group or
toggle realized actors, but do not establish generation ownership by
themselves.

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
   reference, attachment, ownership, or outer relationship to a disposable
   generated actor or package. It fails closed when the feature disappears,
   changes class or geometry type, or exceeds tolerance.
3. Authored masks and exclusions - protected inputs consumed by
   regeneration (for example foliage exclusion areas), owned in the same
   canonical coordinate model as anchors.

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

### Manifest acceptance lifecycle

The durable accepted manifest is the authority; a manifest regenerated from
the current tree can never "accept" a hand edit it was meant to detect.

Accepted manifests live in ProjectWorldData's tracked,
NON-GENERATED authority root, outside every path deleted by Apply, Delete,
clean reconstruction, or generated-tree rollback. Generic fixture manifests
may live under `ProjectWorld`; production manifests may not. The manifest
document is never one of its own generated artifacts. A manifest stored inside
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

Enrollment is the FINAL mutation of an acceptance cycle. Top-level
validation runs regenerate and deliberately keep the tree they produced,
so they run BEFORE enrollment; nothing may regenerate after it, or the
enrolled pair is invalidated. Enrollment covers every current scope
explicitly: the P0 map scopes, the representative map scopes, and the
shared presentation-profile scope. The frozen acceptance sequence is:

```text
P0 + representative destructive E2E gates
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
current generator fingerprint, because the audit's claim is that the
accepted tree is reproducible by today's generator. For that gate to be
worth obeying, the fingerprint covers only inputs that can change
generated bytes or manifest documents - the realization module, the frozen
schemas, and the lifecycle scripts. The read-only auditor and test sources
are excluded by construction: hashing them would fail the gate on edits
that provably change nothing, which teaches operators to bypass it.

The whole tail is one command:
`bootstrap.py accept --p0-run <id> --representative-run <id>`. It owns the
project-global content lock across audit -> package -> IoStore inspection
-> gate -> audit, so those steps observe ONE coherent tree. Holding the
lock is what makes "exact final tree" evidence rather than assertion: a
before/after comparison alone cannot detect a tree that changed during the
cook and changed back. The package and IoStore inspection are the SAME
helper the E2E gate uses, so the final acceptance can never prove less
than the gate it concludes.

The package `required_map` selects exactly one descriptor-validated
world-data owner. Both durable audits receive that owner explicitly and the
acceptance receipt records it. A ProjectWorldData acceptance can therefore
never borrow ProjectWorld fixture authority.

A run ID on the command line is a label, not provenance. Acceptance
therefore proves the whole path from run to manifest:

- Each supplied run must carry the profile identity it was passed as, so
  the same accepted representative run cannot be handed in as "p0", and
  each `result.json` SHA-256 is recorded.
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

Enrollment after the gates is NOT a plain re-Apply: the gates deliberately
keep the tree they regenerate, and any generator change makes those bytes
differ from the accepted manifests, so drift refuses the mutation. The
contract is that enrollment happens through explicit clean reconstruction
from the gated compile results. The exact operator procedure and flags are
owned by
[scripts/ue/world/README.md](../../../../scripts/ue/world/README.md).

Already-accepted E2E receipts are never mutated afterward; the durable
audit links the evidence chains. Because enrollment is itself a
realization, the pre-enrollment E2E package receipts prove the pipeline,
not the final tree bytes - only the post-enrollment read-only package
proves the shipped tree.

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
commit. An obsolete manifest cannot remain active and create false global
ownership conflicts. Legacy retirement (for example City17) and later
generated-layer retirement follow this route.

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

Production world JSON and every derived Unreal representation live under
`ProjectWorldData`. `ProjectWorld` owns the schemas, definition types,
generation/realization logic, serialization contracts, replication support,
runtime services, and validation. `ProjectWorldData` owns Kazan data and
content only; it must not fork a generator or contain custom world logic.

The accepted root contract is:

- source JSON is the first authority layer under the owning data plugin;
- generated definitions, maps, external actors/objects, HLOD, materials, and
  other serialized Unreal output are derived representations under that same
  plugin mount;
- compiler coverage names one validated content-capable project plugin;
- its existing UE `.uplugin` descriptor is the root authority from which
  generic code derives the mounted package root, `Data/`, `Data/Manifests/`,
  `Generated/`, and `Authored/`; no parallel custom descriptor exists;
- generated output is restricted to `/ProjectWorldData/Generated/`;
- authored output is restricted to `/ProjectWorldData/Authored/`;
- arbitrary package paths and cross-plugin deletion scopes are rejected.
- for a non-fixture owner, validation requires explicit source/compiler
  profile paths physically confined beneath that descriptor-derived `Data/`
  root; source ID/path, compiler ID/owner, and compiler source ID/path must all
  agree before execution;

Existing `/ProjectWorld/Generated/` and `/ProjectWorld/Authored/` content is
limited to generic fixtures and representative adapter evidence. It is not a
valid home for the production Kazan world.

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

- Production durable authority root:
  `Plugins/World/ProjectWorldData/Data/Manifests/`. Generic fixture
  manifests may remain under `ProjectWorld`.
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
- The durable root owns the TRACKED generated tree. Isolated validation
  runs (end-to-end profiles that back up, regenerate, and restore the
  tree) operate against a transient sandbox manifest root under their own
  work root, enroll their sandbox scopes explicitly, and never touch the
  durable root; their sandbox manifests are run evidence, not authority.

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
  constraints, and disposable territory blockouts.
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
repository-relative paths. The superseded 36-cell tool-owned profiles were
removed and must not be restored. Production profiles are created only after
the revised source admission, extraction margin, and dry-run budgets pass
Slice 1.

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
- The admitted Geofabrik Volga snapshot still covers the selection. The
  current Copernicus GLO-30 N55/E049 tile does not cover the technical
  envelope plus source margin west of longitude 49.0. Slice 1 must admit and
  hash the adjacent N55/E048 tile from the same provider before creating the
  production profile.
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
- Extend raster/source manifests before crossing the currently supported
  source set; do not hide added tiles behind a global snapshot list.

### 3. Generated Unreal geography

- Realize Landscape and water first, then roads, scalable foliage, and
  blockout building massing.
- Keep generated content profile-owned and disposable.
- Protect authored overlays during Apply and clean reconstruction.
- Do not migrate legacy buildings, trees, or props into the generated base at
  this stage.

### 4. Measured optimization

- Derive the initial World Partition design from Epic guidance and the
  documented content/travel assumptions in
  [world_partition.md](world_partition.md), then validate it through automated
  cook and deterministic packaged traversal.
- Manual editor observation is diagnostic only. Machine-readable cell state,
  latency, hitch, memory, and frame-time receipts own acceptance.
- Tune cells, loading ranges, streaming sources, Data Layers, HLOD, Nanite,
  instancing, and PCG only when the automated comparison shows a benefit.
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
- bounded foliage and building generation with no duplicate identities;
- protected authored overlays survive unchanged regeneration;
- every point inside the product boundary has generated ground and player
  collision, and territory routes prove load, unload, and reload behavior;
- deterministic packaged traversal stays inside frozen budgets;
- IoStore contains the required map and excludes provider payloads;
- repeatable captures show the territory layers requested by that profile.

The legal boundary for source data and generated assets remains
[`docs/legal/world_data_and_asset_policy.md`](../../../../docs/legal/world_data_and_asset_policy.md).
The executable evidence route remains
[`tools/World/EndToEndValidation/`](../../../../tools/World/EndToEndValidation/README.md).
