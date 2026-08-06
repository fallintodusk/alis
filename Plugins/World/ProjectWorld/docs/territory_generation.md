# Territory Generation

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
2. Feature-anchored - freezes the stable canonical feature ID, resolver
   version, local offset/orientation, and allowed movement tolerance;
   resolved through the documented resolver, NEVER a direct reference,
   attachment, ownership, or outer relationship to a disposable generated
   actor or package. Fails closed when the feature disappears, changes
   class, or exceeds tolerance.
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

Accepted manifests live in a tracked, NON-GENERATED authority root, outside
every path deleted by Apply, Delete, clean reconstruction, or
generated-tree rollback (designated pattern:
`Plugins/World/ProjectWorld/Data/Manifests/<scope>.json`; exact path and
schema version frozen at the generic isolation gate). The manifest document
is never one of its own generated artifacts. A manifest stored inside the
scope it governs would delete its own authority on reconstruction and could
be damaged together with the content it is meant to audit.

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

Enrollment covers every current scope explicitly: the P0 map scopes, the
representative map scopes, and the shared presentation-profile scope. After
enrollment, the P0 and representative top-level gates rerun, and every
participating manifest SHA-256 is recorded in their receipts.

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

### Manual polish layer - physical definition

Before the generic isolation gate can pass, the polish layer must be frozen
here as an implementable layer, not a concept: exact authored content root,
Data Layer or Level Instance policy, external-actor ownership, allowed
dependency set, and anchor representation per the taxonomy above. Until
those are written into this section, the polish layer is unproven by
definition.

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

- Measure representative traversal before changing World Partition settings.
- Tune cells, loading ranges, streaming sources, Data Layers, HLOD, Nanite,
  instancing, and PCG only when evidence shows a benefit.
- Keep optional or Experimental UE 5.8 features behind profile-specific tests
  with a supported fallback.
- Attribute cook and package deltas to the territory rather than total project
  size.

### 5. Content integration decision

Only after the generated territory passes visual, traversal, regeneration,
and packaging gates, audit existing maps by content class:

| Existing content | Decision options |
|---|---|
| Geography-bound roads, trees, and buildings | Regenerate, migrate as an authored overlay, or retire |
| Hero locations and gameplay landmarks | Preserve and align as protected overlays |
| Reusable art kits | Feed a profile-owned procedural polish layer |
| Conflicting or unproven content | Exclude until ownership and placement are resolved |

The choice is evidence-driven per class. There is no blanket migration of an
old map and no requirement to discard useful authored work.

## Acceptance

A territory profile is accepted only when it proves:

- deterministic source-to-Unreal regeneration without manual repair;
- continuous terrain and cross-cell water/road topology;
- bounded foliage and building generation with no duplicate identities;
- protected authored overlays survive unchanged regeneration;
- representative packaged traversal stays inside frozen budgets;
- IoStore contains the required map and excludes provider payloads;
- repeatable captures show the territory layers requested by that profile.

The legal boundary for source data and generated assets remains
[`docs/legal/world_data_and_asset_policy.md`](../../../../docs/legal/world_data_and_asset_policy.md).
The executable evidence route remains
[`tools/World/EndToEndValidation/`](../../../../tools/World/EndToEndValidation/README.md).
