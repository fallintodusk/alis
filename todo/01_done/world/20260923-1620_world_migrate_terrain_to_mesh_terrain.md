# Migrate Generated Terrain To UE 5.8 Mesh Terrain

**Status:** DONE - production Kazan and Manhattan use Mesh Terrain; schema-v2 authority provenance and permanent migration cleanup are corrected; closure is proved by the packaged fixed-view receipts, exact-source package, and successful Editor build
**Scope:** ProjectWorld generated-terrain realization - canonical final-surface ownership, the neutral producer seam, Mesh adapter, fingerprints, manifests, generated authority, release integration, and representation-specific consumer cleanup
**Stable documentation owner:** [World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md) (native reuse gate, topology, representation policy) and [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md) (layer tuples, locality, authored channel, terrain acceptance), both routed from [ProjectWorld docs](../../../Plugins/World/ProjectWorld/docs/README.md); commands in [Canonical World Realization](../../../scripts/ue/world/README.md)

Agent route: read this file, then only the owner sections it cites. Choose proof depth with
[World pipeline layers](../../../docs/testing/world_pipeline_layers.md), including its R1 and R2
independent reviews. Phase 4 starts only after Phase 3 passes, possibly after workarounds (D6).
Reviewers start with `## Reviewer brief`.

## Current state and next steps

State on 2026-09-26: production Kazan and Manhattan select `project_mesh_terrain:v1`. Both use one
shared MPD, canonical schema-v2 final terrain, truthful `ground` and `hydro_transition` semantics,
and the accepted `MaxSectionComplexity=2048` topology. ProjectWorld has no generated Landscape
producer and no dependency on ProjectMaterialEditor. The old MeshDevelopment route and the old P0
and representative generated maps are retired; City17's separately authored Landscape remains
outside this generated-terrain migration.

Closure review removed stale generated-Landscape topology and performance language from the
durable World documentation, recorded the producer-neutral ProjectWorld -> ProjectWorldMeshTerrain
boundary, and fixed two file-local helper-name collisions exposed by Unreal's unity build. The
project Editor target builds successfully through the supported wrapper.

The final source builds. Focused profile, producer, architecture, shared-definition, inventory,
navigation, and product-policy tests pass. Kazan and Manhattan each reconstruct from promoted
canonical authority and then accept an unchanged apply with zero dirty units. Their static World
Partition audits pass. The durable authority audit accepts 14 active scopes and verifies 3,097
artifacts, 200,361,968 bytes, current producer fingerprints, unique ownership, resolved consumers,
no unowned generated file, and no pending transaction.

The migration-only compiler route is removed from the permanent architecture. Profiles now
declare terrain-surface policy explicitly, authored overlays accept only schema v2, and the
schema-v1 terrain contract is gone. Corrected Kazan and Manhattan authorities preserve the
materialized parent `source_inputs_hash` and real `source_result_sha256`, record
`source_overlap_verified=false`, and retain their immutable parent migration receipts. Applying
both corrected authorities produced zero dirty units in every layer and zero actor rewrites; the
durable audit again accepts all 14 scopes and 3,097 artifacts.

The first final Shipping cook exposed one real cleanup miss: `DefaultGame.ini` still listed the
retired P0 map. Unreal only warned and continued, so the obsolete cook entry was removed rather
than accepted as harmless drift. That diagnostic package otherwise built Shipping, cooked both
production Mesh territories, produced IoStore, and passed archive/content policy. The source-engine
release wrapper remains independently blocked before UAT by the repository's pre-existing engine
manifest/documentation governance mismatch; it is not evidence against Mesh Terrain.

| Area | Current state | Remaining action |
|---|---|---|
| Canonical authority | Closed: Kazan and Manhattan promoted on schema v2; Water, hydro, and authored overlay end in one immutable terrain projection | Preserve the migration receipts and promoted bundles |
| Production realization | Closed: both production profiles and public variants select Mesh Terrain; one shared MPD; 210 canonical terrain cells per territory | Keep representation changes inside ProjectWorldMeshTerrain |
| Legacy generated terrain | Closed: generated Landscape code/tests, MeshDevelopment, and obsolete P0/representative generated maps are retired | Do not remove unrelated authored City17 Landscape support |
| Runtime and partition | Closed in editor/commandlet evidence: collision, neutral nav ownership, product policy, partition audit, and no-op locality pass | Keep the packaged product acceptance contract as the regression gate |
| Generated authority | Closed: 14 scopes and 3,097 artifacts pass the complete durable audit | Rerun after any fingerprinted producer change |
| Shipping | Focused Kazan and Manhattan packaged fixed views are accepted; the final closure package is `Saved/PackageRelease/MeshTerrainCutoverClosureR2` | Require accepted frames plus a successful build/cook/archive/IoStore summary whose final source identity equals its initial identity |
| Material appearance | The Mesh substrate and generated material ownership are ready; richer ProjectMaterial v2 appearance remains a separate presentation task | Do not reopen terrain topology for material look work |

City17 remains separately authored Landscape content. Its registered packaged experience also
passes the known indoor fixed-view route, but that frame is not an outdoor Landscape-material
proof and does not make City17 part of the generated Mesh Terrain pipeline.

### Superseded 2026-09-25 Phase-3 state

State on 2026-09-25: `ProjectWorldMeshTerrain` is first-class ALIS source behind the neutral
`project_mesh_terrain:v1` producer tuple. Daily development now uses one normal repository and the
non-production `kazan_territory_mesh_development_v1` profile/map. Production Kazan and Manhattan
profiles still select Landscape. Release source isolation remains available for its independent
reproducibility owner, but it is not the Mesh development environment.

The ordinary packaged product route now loads the MeshDevelopment map through ProjectLoading and
accepts the production game mode, possessed DefinitionCharacter, grounded movement, terrain/road/
building collision, gameplay interaction, center -> edge -> center unload/reload, and restored
PreviewFlight. The accepted v11 receipt observes terrain collision on a compiled Mesh section. This
supersedes the older isolated runtime timeout and short-ascent records below; they remain only as
investigation history.

Authenticated current packages now exist at revision
`050af7a9679cc99dd3d6772c1e253cd5923ac392` and source-state
`bbc12c12b7266e1c62ee455702e82b07a8cef4a6f3b274a14caaa0ff1b0fb0e2`.
Development passes build, cook, archive integrity, IoStore policy, and post-package source equality;
it contains 53 files totaling 2,335,879,434 bytes. Shipping passes the same source-identity and
package-integrity gates; it contains 38 files totaling 2,116,918,557 bytes. Both ordinary packaged
product routes accept the exact MeshDevelopment map under D3D12, including production pawn/game
mode, terrain/road/building collision, gameplay interaction, and center -> edge -> center streaming.
The packages include both production Landscape maps and the non-production Mesh map, so whole-
package totals are not representation-only terrain comparisons. The earlier drift-rejected
packages remain diagnostic history only; their concurrent public-mirror process has exited. The
release script retains the initial/final revision and state on any future source-drift refusal.

The short-ascent defect was in the route harness, not Mesh collision. A one-shot upward capsule
sweep using the possessed character's collision profile hit the
`ProjectWorld.BuildingMassing.v2` actor
`ProjectWorld_Buildings_grid_413718bc833994e5_x1_y0` at Z 8966.07 above canonical terrain Z 8566;
the blocker was a `StaticMeshComponent`, face 442, with no initial penetration. The terrain-only
ground loop had deliberately ignored that building and placed the pawn below it. Ground placement
now stops at the first Pawn-channel blocker, while the independent terrain probe still proves the
neutral terrain collision contract. Because first-blocking placement put this route on a roof, the
grounded-movement check also selects a local direction whose endpoint remains within step height.
The focused regression and the packaged v11 route both pass.

The complete v11 playable tour reaches all four waypoints, rises 7,544.22 cm, descends 12,225.14 cm,
travels 1,602,777.11 cm horizontally, proves blocked descent and collision slide, exercises pause
open/close and every input phase, unloads and reloads 23 center cells, and reports zero streaming
failures across 11,493 performance samples. Its 20.483 ms frame p95 is retained as an earlier
route-harness result, not the topology decision gate. The later fixed three-child comparison uses
one revision, one source-state identity, one Development package, the same runtime profile and raw
collector, and pooled nearest-rank percentiles: Mesh is 14.016 ms frame p95 across 46,152 samples;
Landscape is 14.841 ms across 44,752 samples; both have zero streaming failures against the fixed
16.670 ms budget. Per the accepted decision rule, `MaxSectionComplexity=2048` is not changed and no
Insights/topology experiment is justified. World Partition section splitting remains excluded by
the locality invariant.

The strengthened authenticated packaged runtime diagnostic explains the 212 actor count. Windows retains 106
collision sections and 106 Nanite sections. The Nanite sections contain 106 main-pass Nanite mesh
components plus 106 Epic-owned non-main-pass virtual-texture fallback helpers. They are not 212
duplicate visible terrain surfaces. Every inspected static-mesh component in the 106 Nanite
sections uses Epic's per-section MIC whose effective parent chain is exactly
`MI_ProjectTerrain_Default -> M_ProjectTerrain`. Center collision returns canonical Z 8566 cm
exactly, and center -> edge -> center unload/reload passes. The compiled-asset audit also compares
202,710 expected samples for each of the Nanite and fallback surfaces with zero mismatches and
effectively zero maximum error. Therefore the blue fixed-view image is
the intentionally minimal generated material's current appearance, not absent terrain or the
default-material fallback. The current Development product route renders 30 frames after center
reload and its screenshot is stable against the earlier accepted route (SSIM 0.999867, PSNR
57.456), so the positive VSM reload sequence passes. A deliberately stale-shadow negative control
is still absent, so the stronger VSM-discrimination claim remains open. This closes the realization
ambiguity but does not claim production material parity.

IoStore inspection identifies 24 MeshDevelopment World Partition packages containing all 212
compiled-section exports. Their 36 container files occupy exactly 18,490,057 compressed stored
bytes. This is an exact section-owner package-set measurement, not an export-level additive size.
The analogous 168 Landscape packages occupy 14,386,071 bytes but also contain 404 generated static-
mesh actors/components, so that number is not an exact Landscape-terrain byte total and must not be
used as a representation-only comparison. Isolating Landscape physical bytes would require a
separate cook/chunk or an export-aware accounting method. Product dynamic navigation is now
implemented at the accepted boundary. ProjectWorld product realization owns one deterministic,
always-loaded territory `NavMeshBoundsVolume` and map-owned Recast authority; it persists no
territory-sized invoker. The packaged acceptance path temporarily registers the possessed moving
character as the invoker, proves a cross-cell path before and after unload/reload, and records
active tiles. Mesh acceptance separately requires navigation-relevant collision and a navigation-
irrelevant non-main-pass helper. It checks every loaded collision/helper component and both traced
route endpoints, rather than accepting one existential example. The same bounded acceptance
contract derives six authenticated runtime-height probes and supports a normal-cache versus
forced-fresh VSM streaming control. The forced-fresh arm now rejects an already-disabled cache and
records the before/during/after values. Focused C++ tests, the editor build, and the 2/2 PowerShell
derivation tests pass; the known-bad component-policy and tampered canonical-cell controls reject.
The first packaged RED correctly found that the saved maps predate the new territory navigation
actor; no packaged forced-fresh arm has passed yet.

Final packaging cannot be repaired honestly by rerunning realization yet. Production Landscape is
manifest-authenticated to compile-result SHA-256
`b2fc42dbe6260a1dd129db1184cb4448a3cc68a7aca34ca970c1f16f81854c0b` and promoted authority
inputs `11cf1c13c1792afa88a0013c5b0950ced0e3678c8bb653ffad81d2a15433823e`; that authority contains
schema-v1 terrain cells. MeshDevelopment needs the approved schema-v2 surface ABI and was realized
from a later September candidate. The current compiler profile hash no longer matches the promoted
active authority, so authenticated materialization rejects with `canonical_profile_changed`. A
fresh compile from the currently declared August source also fails closed: the exact pinned
`volga-fed-district-260802.osm.pbf` is absent locally and its provider URL returns 404. The remaining
provider file `260801` has different byte size and MD5, so substituting it would change source
meaning. The earlier September source is likewise a different provider snapshot. Final same-
authority packaging therefore requires an operator choice: recover the exact August payload and
promote its schema-v2 result, authorize a reviewed migration of the accepted schema-v1 authority to
the approved surface ABI, or authorize a new provider-source authority. None is an ordinary Phase 3
harness fix.

The architecture direction remains accepted: CanonicalCompilation owns the final terrain surface;
ProjectWorld owns neutral producer selection and terrain runtime identity; ProjectWorldMeshTerrain
alone owns MeshPartition, one shared MPD, private channel indices, and section realization;
ProjectMaterial owns appearance. `ProjectWorld -> ProjectMaterialEditor` remains forbidden and is
absent. Canonical surface names remain provenance-honest (`ground`, `hydro_transition`); slope alone
does not become factual `rock`.

| Remaining gap | Current state | Next executable step |
|---|---|---|
| Ground placement and ascent | Closed: the first blocker was a valid building skipped by the terrain-only loop; first-blocking placement plus a grounded local direction pass the complete tour | Keep the focused regression and independent terrain collision probe; do not reopen Mesh collision without new producer evidence |
| Accepted Mesh performance | Closed for topology selection: fixed paired three-child Mesh p95 is 14.016 ms versus Landscape 14.841 ms, both under 16.670 ms with zero streaming failures | Keep 2048 and do not run Insights or topology tuning unless a same-envelope regression fails reproducibly |
| Render variant/material presence | Closed diagnostically: 106 main-pass Nanite components, 106 non-main-pass VT helpers, 106 collision sections, one effective generated-material chain | Preserve the strengthened packaged gate; do not interpret the minimal blue appearance as missing geometry |
| Height fidelity | Compiled Nanite and fallback surfaces match all 202,710 canonical samples; the new authenticated contract derives six center/perimeter/boundary/high/low/hydro probes and its hash-tamper control rejects | Run the packaged six-point receipt after both maps are regenerated from one accepted authority |
| Product dynamic navigation | Implemented locally: neutral ProjectWorld owns territory bounds plus map-owned Recast; the packaged harness temporarily registers the moving possessed pawn; no permanent world-sized invoker or Mesh nav owner was added | Regenerate both maps from one accepted authority, then require pre/post-reload cross-cell paths in the same package |
| VSM unload/reload | Positive path passes; the new forced-fresh control disables cache only across unload/reload, restores it after streaming completion, and preserves the fixed post-reload view | From the same final Development package, compare normal and forced-fresh terrain-shadow ROI before considering any workaround |
| Representation-only bytes | Mesh section-owner package set is exact at 18,490,057 compressed stored bytes; Landscape's 14,386,071-byte candidate set is co-packed with 404 non-terrain static-mesh actors/components | Either isolate Landscape through cook/chunk ownership or report the physical Landscape terrain delta as UNVERIFIED; never subtract co-packed package totals |
| Current Development/Shipping | Closed at revision `050af7a9` and source-state `bbc12c12`: both packages and ordinary D3D12 product-route smokes pass | Preserve the authenticated summaries and receipts; no further package rerun is needed unless source affecting the candidate changes |
| Canonical authority identity | BLOCKER: production Landscape uses promoted August schema-v1 authority; MeshDevelopment uses a later schema-v2 September candidate; the exact August PBF is unavailable and the current profile cannot materialize the promoted bundle | Operator selects exact-source recovery, reviewed schema-only authority migration, or a new provider-source authority; do not relabel either arm as same-authority |
| Final comparison/review | Topology gate passes on an earlier paired source-state identity, but final same-canonical-authority packaging is blocked as above; memory has no additional accepted owner beyond existing product performance metrics | After the authority decision, regenerate both maps, package once, run navigation/height/VSM plus the fixed three-child pair and Shipping smoke, then obtain fresh independent review |
| Phase 4 | Blocked and unauthorized; production profiles are unchanged | Request cutover approval only after every Phase 3 acceptance item is green |

## Superseded execution state (retained for audit)

State on 2026-09-25: the authenticated complete-Kazan run invalidated two earlier Phase 1/R2
assumptions and fixed both before comparison continued. A producer change under the same scope had
left 210 Landscape external actors on disk, and running MeshPartition after the wrapper transaction
had left 318 compiled sections unowned. The wrapper now removes only superseded prior artifacts and
runs the builder plus structural audit before manifest publication. Current focused verification
passes 23/23 PowerShell tests and 38/38 targeted Python tests. The final Phase 3 evidence directory
does not retain an authenticated before/failure/after rollback receipt or the earlier exact no-op
receipt, so those stronger final-source claims remain unverified rather than inferred from older
scratch evidence.

CanonicalCompilation now publishes one staged final terrain surface after Water,
hydro, and authored corrections; ProjectWorld selects terrain producers by generator ID; and the
`ProjectWorldMeshTerrain` adapter owns one shared MPD, private channel indices, three platform
variants, editor-only build pipelines, compiled-section runtime tags, and a plain-data layout
receipt. ProjectMaterial independently validates that receipt without a World-to-material-editor
dependency.

The two-cell fixture builds 6 sections: collision, Nanite, and full-resolution non-Nanite fallback
for each cell. The final audit reports 0 cm fallback-surface error against a 5 cm limit. An unchanged
builder run reuses 6/6 sections. A one-cell edit rewrites one base actor and the corresponding three
sections; rollback and Delete restore exact bytes. A 128/384 -> 256/768 runtime-profile switch and
restore change no generated-layer manifest or artifact bytes. Same-path material content changes
reuse sections and preserve World package hashes, while the cook rewrites the shared MPD to
identical bytes; the path-change control rebuilds. The
Development package loads the exact fixture map without transformer-serialization or Nanite
material-fallback warnings. A full WindowsServer asset cook succeeds and strips the two render
cells, leaving the collision route. `AlisClient` and `AlisServer` executable builds remain unavailable
because Epic's launcher distribution rejects those target types before project compilation; this is
an environment limitation, not a project compile result.

The final isolated Mesh transition removes all 210 prior Landscape terrain paths and owns all 318
compiled sections in the terrain scope. The builder and census observe one partition and 106
sections per variant, but the transaction audit ran with `ExpectedSectionsPerVariant=0`; it did not
enforce that expected count and has no wrong-count control. The complete authority audit accepts 16
scopes and verifies 2,817 artifacts totaling
166,363,302 bytes. The latest Development Mesh package is 2,249,885,511 bytes, 3,846,580 bytes
smaller than the retained 2,253,732,091-byte Landscape control. The earlier clean Shipping pair
(Landscape 2,036,009,742 bytes, Mesh 2,032,161,626 bytes) establishes the same size direction, but it
predates the final runtime-gate source identity and is not the final candidate's exact Shipping
measurement. Earlier contaminated comparisons that reported a `LandscapeStreamingProxy` as the
Mesh collision owner are invalid and must not be cited.

The current packaged Landscape control is accepted: three center-edge-return runs produced 45,674
samples with pooled frame p95 13.747 ms against the fixed 16.670 ms budget and zero streaming
failures. It loads `MI_ProjectTerrain_Default`; retained SM5 sampler-overflow errors belong only to
legacy City17 `M_Landscape` and are not Kazan control evidence. Visual Verification authenticated
nine current-map captures and exposed flat blockout shading,
30 m stair-stepped shorelines, and visible territory edges. Epic's 5.8.3 MeshPartition control is
also red (32/66 pass): all 34 failures are engine-plugin golden-mesh mismatches or preview-build
timeouts, so ALIS proves only its exact supported adapter route.

The final isolated candidate is based on commit
`14cd7aead3a84c0371fcedc42a3ae7143802eeff` with overlay identity
`cc9d6f5fc03ff40b84fe5b890c2af404f0c60d9c88a3315af2cb46c2e13b18b0`. Its editor build,
complete-Kazan realization, authority audit, Development package, archive integrity, and IoStore
policy checks pass. The packaged Mesh runtime gate rejects after 36.926 seconds with
`runtime_gate_phase_timeout`: World Partition is enabled and reports one explicit 153,600 cm source
at the center, but no tagged Mesh Terrain center section loads and no center collision is proved.
The receipt does not distinguish a trace miss from an untagged hit, so root cause remains
unverified. Because the gate fails before the product route, Mesh performance, memory, dynamic
navigation, VSM unload/reload, height fidelity, and visual parity are unverified. The Landscape and
final Mesh arms also do not share one source identity after the runtime-harness corrections, so a
completed comparison cannot be claimed. Material T1 is complete as a design packet but remains
PATCH on exact texture revision/hash/license/lineage and machine-readable ProjectTexture sidecar
authority. No production authority changed; Phase 4 is blocked.

The runtime evidence instrument is now patched but not yet proved through a newly packaged candidate.
The gate asks the owned `UWorldPartitionStreamingSourceComponent` for completion before it inspects
tagged sections or collision. It emits separate fail-closed outcomes for a disabled source, native
streaming not completed, no tagged section around the probe, no trace hit, an untagged hit, failure
to unload the center, and failure to reload it. The v2 receipt retains the source identity, probe
derivation hash, current proof point, loaded tagged-section count, trace classification, and the
first relevant hit's class, package, component, tags, and impact Z. The integration wrapper copies a
child rejection receipt before it propagates a non-zero process exit, so a failed packaged run no
longer loses its most useful evidence.

The production runtime wrapper no longer accepts unauthenticated center, edge, probe-Z, or radius
values. It authenticates every canonical terrain document against the accepted compile result,
requires the compiler and runtime profiles to agree on the grid, derives center from the runtime
`engine_georeference_origin` spawn anchor, selects the farthest authenticated terrain-cell center as
the edge, derives probe Z from the two cells' maximum heights plus the runtime spawn clearance, and
derives source radius from `runtime_partition.loading_range_m`. For the retained complete-Kazan
inputs this maps center to cell `x1:y0`, edge to cell `x-6:y7` at
`(-676400, -609400, 30480)` cm, and radius to `153600` cm. Their `910431.392` cm separation exceeds
two source radii. The focused derivation tests pass 2/2, including a tampered terrain-document hash
control. An intentional run against the retained old package preserved both the derivation and its
old v1 configuration rejection, then left no owned process alive; it is a wrapper negative control,
not runtime proof of the new v2 gate.

One reviewer statement is narrower than the installed engine reality. Epic's public description
says the component reports completion for its source, but UE 5.8.3 implements that call through
`UWorldPartitionSubsystem::IsStreamingCompleted(this)`, and the streaming policy first tests all
non-spatial cells before it performs the provided source's spatial query. Therefore the component
result is the correct native prerequisite and is now recorded directly, but it is not proof that a
false result belongs specifically to a Mesh Terrain spatial cell. The tagged-section census and
collision classification remain separate evidence after the native result becomes true.

| Remaining gap | Current state | Next executable step |
|---|---|---|
| New v2 packaged runtime proof | C++ build and exact contract test pass; no new package exists | Package the frozen candidate and run center -> edge -> center once |
| Run-level section count | Generic producer still has no hard-coded `106`; the next-run controller freezes the preflight count and runs an expected-minus-one rejection | Execute both audits under the next source identity and retain both receipts |
| Same-source comparison | Mesh-only resume was removed from the next-run controller; one workspace must create both arms | Abort unless every arm-level summary carries the one workspace identity |
| Final-source no-op and rollback | Older receipts exist, but the final comparison identity still lacks mandatory retained receipts | Produce and retain final-source unchanged-rerun plus before/failure/after evidence, otherwise decision stays `UNVERIFIED` |
| Phase 4 | Blocked and unauthorized | Stop after the complete Phase 3 decision packet and request cutover approval |

| Step | Runs | Verified by | Starts when |
|---|---|---|---|
| R1 review | fresh `architect` subagent, plus a web-research subagent for every `## Reviewer brief` item | findings applied here, one short review-record entry | now |
| Phase 0 census and Landscape baseline | implementer with `world-engineering` | R1 reviewer checks the baseline evidence | operator approval and engine 5.8.3 |
| Phase 1 synthetic seam proof | `implement-approved-change` with `world-engineering` | exact L0/L1 tests (R-1 to R-17); R2 independent `architect` review | Phase 0 done |
| Phases 2-3 complete-Kazan comparison | implementer | independent `architect` review of the packet; failures loop to workarounds (D8) | Phase 1 green |
| Phase 4 cutover | implementer | L2 gates, L3 only with operator authorization, L4, R2 review, operator visual acceptance | Phase 3 passed |

## Contents

- Goal - Authority register - Non-goals
- Verified evidence (repository, installed engine, Epic, reliability history, existing guards,
  inferences, unverified, refuted)
- Current architecture and source of truth - Capability gap
- Decision (target shape, premise / KISS gate, comparison host, naming, alternatives)
- Required invariants - Implementation tasks (Phases 0-5)
- Test-first and verification plan - Documentation plan - Rollout and rollback
- Reviewer brief - Completion criteria - Review record

## Goal

Move ALIS generated terrain to Epic's UE 5.8 Mesh Terrain (the MeshPartition plugin family, D6):
prove it on a same-canonical-input comparison of the complete Kazan territory, fix what the
comparison finds (workarounds allowed, D8), then realize every generated territory's terrain
through one Mesh Terrain producer with the mandatory hand layer (D7), and remove the superseded
Landscape realization without legacy mentions (D5).

The task ends in exactly one of two states: one accepted Mesh Terrain terrain owner with Kazan and
Manhattan regenerated through it, or - only if an unresolved blocker survives every workaround - an
evidence record of that blocker with production authority untouched (Q6).

## Authority register

Binding `D<n>` entries outrank `## Decision`, the KISS gate, reviewer findings, and agent
preference within their scope. Cite IDs elsewhere; never restate or renumber them.

### Operator decisions

- **D1** Make the largest architecture moves now, while they are cheap, when a new Epic
  technology is genuinely future-proof and positioned as the next generation with strong
  performance and quality - pick it. Operator's words: "we should do hugestest architecture all
  moves exact now while we can if it really future proove, so if some tech arrived and epic claims
  it's will be next with great performance quality and etc , so we should pick".
  - Effect: Mesh Terrain is the primary 3.0 substrate candidate. An accepted migration is finished
    now instead of kept as an optional backend. "Genuinely future-proof" is what Phases 0-3 prove.
  - Reason: "while we can" - an early foundational move costs less than a late one.
  - Date/source: 2026-09-23, operator conversation supplied in this session.
- **D2** Mesh Terrain migration plan: build a temporary Kazan Mesh Terrain twin from the exact same
  canonical data; keep current Kazan untouched; compare geometry fidelity, seams, collision,
  streaming, GPU/CPU, generation time, memory, and packaged bytes; verify the weight-channel/data
  path; if it wins, migrate the production realization and remove the superseded classic-Landscape
  path in the same coherent cleanup. The temporary map and comparison harness belong inside this
  task, not in separate tasks.
  - Effect: Phases 0-5 below; no separate comparison task.
  - Reason: not stated by the operator.
  - Date/source: 2026-09-23; the operator accepted the proposed 3.0.0 plan ("ok so prepare these
    todos ...").
- **D3** Mesh Terrain must not leak above the realization boundary: no MeshPartition types in
  provider ingestion, canonical compilation, or ProjectWorldData source semantics. Use Epic's
  native API and extension points first; do not fork the plugin; patch or copy only when
  investigation proves an actual missing requirement.
  - Effect: invariants 1-3 and 16; see Q3 for what "patch or copy" can still mean in this
    repository.
  - Reason: stated in the accepted proposal - if Epic changes Mesh Terrain APIs between 5.8, 5.9,
    and 6.x, one black box is replaced instead of ALIS architecture.
  - Date/source: 2026-09-23; accepted 3.0.0 plan.
- **D4** Do not couple to existing stubs; keep the right separation-of-concerns naming hierarchy
  and dependency direction; build future-proof, data-driven, fully modular, black-boxed,
  decoupled, and component-driven. Operator's words: "we don't have fully architecture for such
  solutions so no need couple for existing stubs, we need keep the right SOC names hierarchy and
  vector and future proof oriented build with our data driven design fully modular blackboxed
  decoupled component driven".
  - Effect: the terrain producer is selected by profile data, not by grid compatibility;
    Mesh Terrain sits behind one adapter black box; representation-specific names are not carried
    into representation-neutral contracts; unused stubs such as the never-written
    "Generated Roads" edit layer are not re-created. The reliability guard still applies to real
    mechanisms (transactions, locks, rollback, verification).
  - Reason: not stated beyond the words above.
  - Date/source: 2026-09-23, this session.

- **D5** Even though Mesh Terrain is Experimental, it is the long-term direction: after the test
  passes, the architecture wraps fully around it, with no legacy support and no legacy mentions.
  Operator's words: "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions".
  - Effect: Experimental maturity alone is not a rejection reason, which closes Q1 clause (a).
    After an accepted test, Phase 4 leaves no Landscape compatibility path, alias, flag, or
    mention in stable docs, code, comments, tests, or configuration; history stays in Git and in
    this task once done. D6 later closes Q1: Phase 3 evidence now starts workarounds on failure and
    gates Phase 4 when every required invariant passes.
  - Reason: "future long going goal".
  - Date/source: 2026-09-23, this session.
- **D6** Switch to Mesh Terrain for real. If the test shows problems, keep working on them and give
  up only when something unresolved blocks it completely. Operator's words: "yes point will
  definetly switch to mesh terrain , if test will some bad we will try until give up completely if
  something unresolved hit, but goal is this".
  - Effect: closes Q1, including its clause (b) about the plugins that ship in Game, Client, and
    Server builds. Phase 3 reports evidence instead of asking for a yes or no; a failed invariant
    starts the workaround loop (D8), not a rejection. L3 enrollment still needs the operator's
    explicit authorization under the repository rules.
  - Reason: "but goal is this".
  - Date/source: 2026-09-23, this session.
- **D7** A hand-adjustment layer is mandatory in every generation: first generate from raw data,
  then apply a flexible hand layer on top. Operator's words: "we need to understand how to keep
  extra layer that will apply hand adjusting - it's our mandatory in all generations - first
  generations by raw data then applying hand layer flexible".
  - Effect: closes Q5. Retiring the channel is not an option; this task designs and proves how the
    hand layer works on Mesh Terrain (Decision, "Hand layer") for heights and for surface-channel
    weights. Other generated layers keep the territory contract's authored overlay and manual
    polish layers.
  - Reason: not stated beyond the words above.
  - Date/source: 2026-09-23, this session.
- **D8** When Mesh Terrain lacks something, try workarounds, including a custom C++ wrapper that
  overrides behaviour properly. Only if the audit shows it is too raw and incompatible may we give
  up. Operator's words: "if we falied we will try workaround even for custom wrapper cpp to
  override properly , if it will be very raw from audit and not compatible we could give up and
  keep some plugin with it".
  - Effect: closes Q3. A missing capability first gets an ALIS-owned C++ wrapper in a project
    plugin that subclasses or overrides Epic behaviour through its extension points (for example
    instanced `FTransformer` structs, modifier subclasses, builder options). Copying Epic source
    stays forbidden by the native reuse gate. What "keep some plugin with it" means on the give-up
    path is Q6.
  - Reason: not stated beyond the words above.
  - Date/source: 2026-09-23, this session.
- **D9** Update the source engine (`UE_SOURCE_PATH`) to 5.8.3 now, using the proper tag; the
  operator is updating the launcher binaries to 5.8.3. Operator's words: "definetly need update
  souce code now to the 5.8.3 (do it with proper tag [UE_SOURCE_PATH]) , I'm already updating
  binaries launcher version to 5.8.3".
  - Effect: closes Q2. Phases 0-3 run on 5.8.3. E-facts read from the 5.8.1 plugin source are
    rechecked on 5.8.3 in Phase 0. The project adopts 5.8.3 through
    `scripts/ue/update/update_engine.ps1` (`-Apply` for the launcher, then `-CompleteSource` for
    the source engine), per [Engine Version Update](../../../docs/ue_engine/version_update.md).
  - Reason: not stated.
  - Date/source: 2026-09-23, this session.
- **D10** A bigger package is fine if it buys more freedom, quality, and optimization, and the
  future path is proven robust. Operator's words: "if it bigger but we gain more freedom and
  quality and optimization and will be ensured that it future path robust - we fine".
  - Effect: closes Q4. Size is still measured and reported (invariant 19), but a size increase
    alone does not stop the migration.
  - Date/source: 2026-09-23, this session.
- **D11** `ProjectWorldMeshTerrain` is normal first-class ALIS code. Develop it in one repository
  through a non-production Mesh Kazan profile/map; keep production Kazan and Manhattan on their
  accepted producer until the cutover gate. Source-projection/worktree isolation belongs only to
  release reproducibility, not daily Mesh development. If an engine limitation remains, freeze the
  unselected adapter instead of deleting the seam.
  - Effect: replaces the isolated-worktree Phase 2/3 execution model below. Daily comparison needs
    the same current revision, canonical geography, settings, hardware, and route. Exact source
    identity remains mandatory for final acceptance receipts. Production-profile changes still
    require explicit operator approval.
  - Date/source: 2026-09-25, operator correction supplied in this session.

### Operator gates

- **Q1 [CLOSED by D6; clause (a) earlier by D5]:** After the same-Kazan comparison, does the operator approve a
  production contract that adopts Experimental MeshPartition for generated terrain? This is the
  "later operator-approved contract" that the
  [native reuse gate](../../../Plugins/World/ProjectWorld/docs/world_partition.md#unreal-native-reuse)
  requires before MeshPartition may be reconsidered. Approving it also (a) waives that section's
  "when Epic changes its maturity" precondition, because Epic still labels Mesh Terrain
  Experimental in 5.8 (evidence E28), and (b) changes the same gate's GeometryProcessing row:
  MeshPartition enables GeometryScripting, the Beta GeometryProcessing plugin, and HairStrands for
  every target, so they would ship in the Game, Client, and Server builds (E16, E18). - gates:
  Phase 4 and every change to production generated authority.
- **Q2 [CLOSED by D9]:** Should the comparison, and any cutover, run on UE 5.8.3 instead of
  the configured 5.8.1? Epic's 5.8.1 and 5.8.2 hotfixes each fixed two Mesh Terrain crashes
  (E28). - default while open: A4.
- **Q3 [CLOSED by D8]:** If a required
  deterministic operation is reachable only through non-exported or private MeshPartition code, a
  project fork is not available: the native reuse gate says "Never copy or vendor Epic Engine
  source into the public repository". Does the operator then reject Mesh Terrain for 3.0, or
  accept a narrower contract without the missing capability? - gates: Phases 2-4.
- **Q4 [CLOSED by D10]:** If Mesh Terrain wins on
  architecture, quality, and performance but increases packaged bytes, which size trade is
  acceptable? - default while open: A5; report exact deltas and invent no threshold.
- **Q5 [CLOSED by D7]:** The territory contract's
  single authored in-asset terrain channel has no contract-compliant home on Mesh Terrain (see A6).
  Should the contract gain a new protected authored terrain channel - a path outside generated
  coverage whose edits the builder's reuse identity sees - or should the channel be retired under
  D4 if Phase 0 finds no real corrections in production? - gates: invariant 11 and its Phase 1
  proof.
- **Q6 [OPEN - NON-BLOCKING until the give-up path of D8]:** In D8, what does "keep some plugin
  with it" mean if we give up: keep an ALIS plugin that wraps Mesh Terrain for later, or keep the
  classic Landscape path? - default while open: stop and ask before any give-up.

### Working assumptions

- **A1 [ACTIVE]:** Heights enter Mesh Terrain as exact canonical samples in ALIS-built
  `FDynamicMesh3` base meshes (one base per canonical cell) through the exported base-modifier API,
  not through Epic's heightmap importer, so no resampling occurs. Phase 1 proves it.
- **A2 [REVISED by D7 and E40]:** ALIS keeps owning terrain-water conformance, but the target order
  is exact: sample provider terrain; quantize the canonical base used for Water fitting; solve
  canonical Water; apply hydro conformance; apply the hand overlay; then quantize and validate the
  final canonical terrain surface. The Water solve uses the provider-derived base before hand
  correction, so the hand layer cannot feed back into Water elevation. A hand
  edit that violates final Water clearance fails closed; ProjectWorld must not silently clamp it a
  second time. Epic's Water and MeshPartitionWater plugins stay unused. Phase 0 proves the staged
  compiler dependency and dirty-locality design before implementation.
- **A3 [ACTIVE]:** The comparison baseline is the current non-Nanite classic Landscape at the
  task's starting commit, not the Nanite-Landscape configuration the World docs describe (see
  Refuted).
- **A4 [RESOLVED by D9]:** Phases 0-3 run on 5.8.3. The engine changes only through
  `scripts/ue/update/update_engine.ps1`; the 2026-09-24 source update and smoke completed.
- **A5 [RESOLVED by D10]:** The operator's hope that packaged size shrinks is an expectation, not a
  requirement. Mesh Terrain adds compiled meshes, trimesh collision, and channel texture arrays;
  heightfields are compact; Epic states no size comparison (E28).
- **A6 [REJECTED by evidence]:** The territory contract's single protected authored terrain channel
  stays required. On Mesh Terrain it becomes standalone authored modifier actors in protected
  authored packages that sort above the generated base through `ModifierTypePriorities`. -
  Refuted by the architect review: a standalone actor placed in the generated map gets its package
  under `__ExternalActors__/Generated/<map>/`, which the contract forbids for authored content and
  which `Get-ProjectWorldGeneratedPaths` claims wholesale (`generated_content_transaction.ps1:78-82`,
  [World-data roots](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#world-data-roots-and-manual-polish-layer));
  the compliant home, an authored Level Instance, is where the builder's reuse hash cannot see
  modifier changes (E27), and forcing full rebuilds would break invariant 8. The decision moved to
  Q5.
- **A7 [REJECTED by interruption census]:** The comparison can be hosted by extending the existing in-place tournament
  pattern (global content lock, generated snapshot, candidate realization at the same map
  package, packaging, measurement, exact restore) without a second Kazan data authority or a
  product menu entry. Phase 0 verifies this, including interruption recovery. - Rejected because
  the tournament has a lock and `finally` restore but no durable outer journal; process death can
  leave the live generated tree replaced. Replaced by A8.
- **A8 [ACTIVE]:** The comparison runs in a detached project-local worktree from one exact base
  commit plus one authenticated candidate overlay, using the isolation lifecycle owned by the
  linked 3.0.0 projection task. The overlay receipt records the base `HEAD`, binary patch bytes for
  tracked changes, every untracked candidate path and byte hash, the resulting worktree inventory,
  and one aggregate source identity. Materialization must reproduce that identity before Unreal
  runs; omitted, extra, or changed files fail closed. This is comparison evidence for uncommitted
  Phase 1-3 work, not release authority. The candidate may mutate only that disposable checkout;
  interruption recovery discards it and never restores over the operator's live generated tree.

## Non-goals

- Changing provider acquisition, CRS, cell or feature IDs, provenance, or raw provider terrain and
  Water semantics. CanonicalCompilation does change in scope to own the derived terrain-surface
  contract, hydro/hand ordering, lineage, and dirty planning required by D7.
- Epic Water or MeshPartitionWater, PCG write or adapter paths, 3D terrain features (overhangs,
  tunnels), and road deformation of the terrain. Each is a later concern with its own proof.
- Production terrain material work. The comparison binds the smallest comparable material; the
  production material is owned by the current Terrain Material v2 task.
- Any runtime toggle, menu entry, or permanent second terrain backend.
- Editing installed Engine files or copying Epic source into the repository.
- City17's authored Landscape, `M_Landscape`, and the engine's Landscape module; they are not
  generated terrain.
- HLOD; the zero-HLOD invariant stays.
- Runtime terrain generation (Epic: Mesh Terrain generation is editor-only).

## Verified evidence

Repository evidence was rechecked at HEAD `77ee583acfbaa22798d316929ee68361f27110d1` with a clean
working tree. Engine evidence was rechecked against the launcher engine (`UE_PATH`)
(Build.version 5.8.3, CL 58210709) and the source engine (`UE_SOURCE_PATH`) at exact tag
`5.8.3-release`; engine paths below are relative to `Engine/Plugins/Experimental/`.

### Verified facts - repository

- **E1** The terrain layer is `layer_id: terrain`, `generator_id: project_landscape`,
  `generator_version: 1`, selectors `terrain` and `water`, spatial ownership
  `logical_landscape_with_cell_proxies`, dirty unit `canonical_cell`
  ([kazan_territory_v1.realization.json](../../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)).
  Every profile also carries a top-level `landscape` block
  (`logical_landscape_id`, `components_per_proxy`). The same tuple is used by
  `kazan_territory_public_v1`, `manhattan_showcase_v1`, `manhattan_showcase_public_v1`, and
  `ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json`.
- **E2** Water, roads, vegetation, buildings, and gameplay all declare `depends_on: terrain` in
  the Kazan profile. The territory contract says "Replacing a generator increments its version and
  dirties that layer plus transitive dependants", and every layer's semantic profile-execution hash
  covers "logical Landscape identity, partition setting"
  ([Layered regeneration contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#layered-regeneration-contract-scale-out-precondition)).
  Dependants consume canonical terrain, not the Landscape actor: gameplay uses
  `surface_policy: canonical_terrain_snap`, and buildings use
  `terrain_anchor_policy: owner_cell_clamped_bounds_center`.
- **E3** The registry only admits tuples. It does not dispatch.
  `ProjectWorldRealizationGeneratorRegistry::IsRegistered` (`:251-261`) and
  `ValidateSettings`/`ValidateLandscape` (`:95-106`, `:263-288`) validate the tuple;
  `FProjectWorldRealizationService::Run` then runs the Landscape producer when `Layout.bCompatible`
  (`ProjectWorldRealizationService.cpp:541-555`) and otherwise falls back to ProceduralMesh
  terrain for grids without a layer plan (`ProjectWorldGeneratedGeometry::CreateOwnedActors`,
  service `:556-570`). Water, roads, vegetation, buildings, and gameplay run afterwards in fixed
  order (`:571-610`) and each finds its own layer by generator-ID string (for example
  `ProjectWorldWaterRealization.cpp:101`); `project_landscape` is otherwise used only for inventory
  lookups (`ProjectWorldLandscapeRealization.cpp:400`, `:839`; `ProjectWorldLayerInventory.cpp:461`).
  The profile shape is owned by
  `Plugins/World/ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json`
  (loader constant `ExpectedSchemaFilename`, `ProjectWorldRealizationProfile.cpp:22`).
  `ProjectWorldRealizationService.cpp` has 1132 lines, above the 1000-line guardrail in
  [canonical.md](../../../docs/agents/canonical.md#10-mega-file-baseline--file-size-guardrail), and is
  not in its baseline table.
- **E4** Landscape-specific constraints paid today:
  - `SelectLandscapeLayout` admits only exact section/subsection divisors
    (`ProjectWorldCoordinateMapping.cpp:40-103`); incompatible dimensions "fail rather than
    resample" (scripts/ue/world/README.md).
  - 16-bit encoding limits heights to about +/-256 m at 1/128 m and rejects anything beyond with
    "16-bit height encoding exceeds canonical height tolerance"
    (`ProjectWorldLandscapeRealization.cpp:636-639`).
  - Edit layers "Generated Base", "Generated Roads", "Authored Corrections" (`:29-31`,
    `:319-348`). "Generated Roads" is created but never written; it is only cleared on Delete
    (`:921`).
  - `ForceUpdateLayersContent()` needs rendering in the commandlet or the surface stays flat
    (`:510-529`).
  - `FLandscapeConfigHelper::PartitionLandscape(..., 1)` yields 210 components and 210 proxies
    for Kazan (`:209-216`). `ProjectWorldLandscapeRealization.cpp` has 943 lines.
  - Coordinates and heights: `RelativeHeight = canonical - HeightOriginMeters` is encoded with
    `LandscapeDataAccess::GetTexHeight` (`:632-634`); actor scale is
    `(SampleSpacing.X*100, SampleSpacing.Y*100, 100)` (`:771-774`); `HeightOriginMeters` is 0 for
    Kazan and Manhattan; the single mapping owner is `ProjectWorldCoordinateMapping.cpp:6-14`
    (X = (E - origin) * 100, Y = -(N - origin) * 100). Any new producer reuses
    `CanonicalToUnreal`, never a second transform.
  - Layout: 465 x 434 quads, 466 x 435 vertices, 31 quads per section and one section per
    component; final-height verification covers 215,040 per-cell samples
    (`ProjectWorldRealizationService.cpp:997-998`).
  - Each proxy is an external, spatially loaded actor tagged `ProjectWorld.TerrainCell=<cell>`,
    `ProjectWorld.Generated.v1`, and `ProjectWorld.Landscape.v1` (`:274-284`, `:294-298`); the root
    Landscape is not spatially loaded (`:311-315`, `:781`). HLOD is disabled per proxy (`:299-304`)
    and world-wide (`ProjectWorldPartitionPolicy.cpp:67`, `:133`).
- **E5** Kazan is 15 x 14 = 210 canonical cells of 930 m (31 quads at 30 m), E 374180..388130,
  N 6178590..6191610
  ([Territory envelope v1](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#territory-envelope-v1-operator-decision-2026-08-10)).
  Manhattan uses the same cell range. The active terrain manifests
  (`layer_kazan_territory_v1_terrain.21.json`, `layer_manhattan_showcase_v1_terrain.2.json`) each
  list 210 proxy packages whose SHA-256 values match disk.
- **E6** Uncooked terrain proxy bytes: Kazan 6,297,082; Manhattan 6,466,886 (`du -sb` over the
  manifest-listed packages). All external-actor packages: Kazan 11,674,511 bytes in 851 files,
  Manhattan 15,027,803 in 736; generated content folders: Kazan 26,140,281, Manhattan 78,938,683.
  The recorded static audit is 850 generated actors, 210 Landscape proxies, and 11,755,861
  external-package bytes ([World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md)).
  The declared terrain `artifact_root` folders (`.../Territory/Terrain/`,
  `.../Showcase/Manhattan/Terrain/`) do not exist on disk: every terrain artifact is a map
  `__ExternalActors__` package. No tracked evidence records cooked or packaged per-layer bytes.
- **E7** Nanite Landscape is off: no code sets `bEnableNanite` (engine default `false`,
  `Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:491`), and saved proxies contain no
  Nanite component.
- **E8** Collision uses the engine-default `LandscapeHeightfieldCollisionComponent` in every
  proxy. ProjectWorld configures no terrain collision.
- **E9** The terrain material is hard-coded:
  `ProjectWorldPresentationMaterialBinding.cpp:22-25` fixes
  `/ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default`. A material identity change
  dirties the root and every proxy (`ProjectWorldLandscapeRealization.cpp:858-872`, pitfall 25).
- **E10** Hydro-conditioning lowers terrain to `min(canonical_terrain_z, canonical_water_surface_z)`
  inside Water footprints (`ProjectWorldTerrainWaterConformance.cpp`, `BuildCellHeights`
  `:197-358`). Verification compares against that projection with tolerance
  `max(height_quantization, 1/128)` (`ProjectWorldTerrainVerification.cpp:87-107`). Terrain cell
  identity includes the Water cell hash (`HashTerrainWaterCellInput`,
  `ProjectWorldLayerInventory.cpp:69-138`).
- **E11** Fingerprints: the `project_landscape:v1` producer set includes
  `ProjectWorldLandscapeRealization.*`, `ProjectWorldTerrainVerification.*`,
  `ProjectWorldTerrainWaterConformance.*`, and `ProjectWorldWaterMeshBuilder.*`
  (`scripts/ue/world/generator_fingerprint.ps1:96-105`); `map:v1` also includes the Landscape
  realization (`:70-71`). Text inputs are LF-normalized, `PROJECTWORLD_PRODUCER_BEGIN/END` regions
  scope shared files per producer, the envelope is `project_world_producer_fingerprint_v3`, and a
  stale fingerprint forces whole-layer dirty work (`realization_layer_operation.ps1:241-256`);
  fingerprint currency fails closed. Recomputed `project_landscape:v1` (`3f3c2900...`) and
  `map:v1` (`bf7d7da7...`) match the active manifests. Per-cell input identity is
  `project_world_composite_canonical_input_v1` over the terrain artifact hash plus
  `project_water_cell_input_v1`; proxy semantic output identity is
  `project_landscape_proxy_v2|<logical id>|<cell>|<input>|<SectionBase>`
  (`ProjectWorldLayerInventory.cpp:522-530`). A new producer needs its own source set and its own
  semantic output identity.
- **E12** Consumers of Landscape-specific identity outside the producer:
  - `-RequireLandscape` / `require_landscape`: `scripts/ue/world/realize_canonical_world.ps1:38,273,610`;
    `ProjectWorldRealizeCommandlet.cpp:98-100` sets `bRequireLandscapeCompatible`
    (`ProjectWorldRealizationService.cpp:390`); EndToEndValidation
    `contracts/validation-profile.schema.json:29,43`, `app/execution.py:664`,
    `app/validation.py:434-447`, `app/enrollment.py:321`; validation profiles
    `kazan_territory_v1`, `p0`, `representative_v1`; `test/integration/realization_layer_lifecycle.ps1:250`,
    `runtime_profile_locality.ps1:164`, `authored_overlay_persistence.ps1:148`;
    `test/performance/run_kazan_runtime_profile_tournament.ps1:106`;
    `scripts/ue/package/public_world_projection.ps1:158` (release path).
  - Shipping runtime: `Plugins/World/ProjectWorld/Source/ProjectWorld/Private/Presentation/ProjectWorldProductRouteGate.cpp:53,645`
    finds ground by the actor tag `ProjectWorld.Landscape.v1`.
  - Evidence and audit: `terrain_final_height_*` reads `ULandscapeComponent::GetHeightmap(FGuid())`
    ([Terrain elevation acceptance](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-elevation-acceptance));
    `ProjectWorldTerrainVerification.*`, `ProjectWorldStaticPartitionAudit.cpp`,
    `ProjectWorldSemanticEvidence.cpp`; `tools/World/EndToEndValidation/app/layered_validation.py`
    (`updated_landscape_components`); `tools/World/VisualVerification/app/census.py` and
    `plan_vantages.py`; `scripts/ue/world/execution_envelope.ps1`, `operator_controls.ps1`.
  - Release: `scripts/ue/package/release.ps1:311-336` builds and requires the Kazan playable-tour
    Candidate (`scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`); the Shipping Water
    proof classifies Water against Landscape pixels
    ([Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)).
  - Cinematics: `scripts/ue/cinematic/release_capture_editor.py:181` and
    `shot_capture_editor.py:186` pin the Landscape scalability group `sg.LandscapeQuality`, which
    has no effect on Mesh Terrain.
  - Unrelated stub, left untouched here: `EProjectWorldLayerType::Landscape`
    (`Plugins/World/ProjectWorld/Source/ProjectWorld/Public/ProjectManifestTypes.h:15`, `:50`) is a
    world-manifest region classification, not the terrain representation (D4: not carried into new
    contracts). Its removal, with the unused `UProjectWorldManifest`, remains a separate cleanup
    scope.
- **E13** The native reuse gate excludes "Experimental MeshPartition/MegaMesh", which "may be
  reconsidered only through a later operator-approved contract when Epic changes its maturity and
  a focused proof shows a simpler compliant path", and states "Never copy or vendor Epic Engine
  source into the public repository"
  ([native reuse gate](../../../Plugins/World/ProjectWorld/docs/world_partition.md#unreal-native-reuse)).
- **E14** The territory contract names "the Landscape 'Authored Corrections' edit layer" as "the
  single sanctioned authored channel inside a generated asset", identity-tracked and required to
  survive regeneration unchanged
  ([World-data roots](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#world-data-roots-and-manual-polish-layer)).
  Test: `Project.World.Realization.AuthoredLandscapeLayerSurvives`
  (`ProjectWorldRealizationTests.cpp:451`).
- **E15** Runtime contracts: one 2D runtime grid of 512 m cells and 1536 m loading range; zero
  HLOD; primary performance gate on physical RTX 4070, High, 1440p, 60 FPS; RTX 3060 Medium 1080p
  is a separate shipping qualification
  ([World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md#generated-terrain-representation)).
- **E16** Engines: launcher 5.8.3 CL 58210709; source engine (`UE_SOURCE_PATH`) at exact tag
  `5.8.3-release`; both contain MeshPartition. The source updater reached `DONE` and its editor
  start-window smoke passed. The project cooks `PCD3D_SM5` and `PCD3D_SM6` for
  DX12 and `PCD3D_SM5` for DX11 (`Config/DefaultEngine.ini`,
  `[/Script/WindowsTargetPlatform.WindowsTargetSettings]`). Targets include AlisServer. The
  realization commandlet already requires rendering: `scripts/ue/world/execution_envelope.ps1`
  forbids `-NullRHI`, and `realize_canonical_world.ps1:589-615` runs with
  `-AllowCommandletRendering`. The native reuse gate keeps the Beta `GeometryProcessing` plugin
  Editor-only, and `ProjectWorld.uplugin` enables it for Editor targets only; MeshPartition declares
  GeometryScripting, GeometryProcessing, and HairStrands for all targets (E18), so adopting it
  brings them into game targets - a packaging and size input for Phase 3.
- **E17** The runtime-profile tournament already compares in-place candidates:
  `run_kazan_runtime_profile_tournament.ps1` (464 lines) takes `Enter-ProjectWorldContentLock`,
  snapshots generated paths, realizes each candidate profile at the same map package, packages
  through `package_release.ps1`, measures, restores with `Restore-ProjectWorldGeneratedSnapshot`,
  and asserts runtime-only candidates dirty no generated layer (`:117`). Its building blocks:
  `Get-ProjectWorldGeneratedPaths` (`generated_content_transaction.ps1:37-91`) covers exactly the
  map `.umap`, `_BuiltData`, `<Map>_HLODLayer_*`, `__ExternalActors__` and `__ExternalObjects__`
  under the map path, and `Generated/Presentation`; `Assert-ProjectWorldSnapshotCoverage`
  (`:93-120`) enforces that coverage; `Complete-ProjectWorldGeneratedTransaction` (`:262-328`)
  commits only on exit 0 with an accepted result. The global content lock is
  `tmp/world/world_realization/content_mutation.lock` (`Enter-ProjectWorldContentLock`,
  `generated_manifest.ps1:59-67`), shared with ProjectMaterial generation; the authority lock is
  `<ManifestRoot>/authority.lock` (`:69-83`); `Invoke-ProjectWorldTransactionRecovery`
  (`:743-875`, also `recover_generated_transaction.ps1`) finishes or rolls back an interrupted
  journal. A definition asset, data-layer asset, or other new package kind must fall inside that
  coverage or extend it.

### Verified facts - installed/source MeshPartition 5.8.3

- **E18** All five plugins (MeshPartition, MeshPartitionWater, MeshTerrainMode,
  PCGMeshPartitionInterop, PCGPrimitives_MeshPartitionInterop) are `IsExperimentalVersion: true`
  and ship full C++ source. MeshPartition modules: `MeshPartition` (Runtime),
  `MeshPartitionCompute` (Runtime, PostConfigInit), `MeshPartitionEditor`,
  `MeshPartitionModelingToolset`, `MeshPartitionEditorUI` (Editor). Plugin dependencies for all
  targets: GeometryScripting, GeometryProcessing, HairStrands.
- **E19** Runtime model: `AMeshPartition` is always loaded (`MeshPartition.cpp:19-20`) and holds a
  `UMeshPartitionDefinition` data asset: `Material`, `ModifierTypePriorities`,
  `CompiledSectionBuildVariants` (`MaxSectionComplexity` 262144 vertices, `TransformerPipeline`,
  `bSplitSectionsToMatchWorldPartitionRuntimeGrid`), `PerPlatformRuntimeSettings`, `ChannelMap`,
  `ChannelTexelSize` 100 cm, `PhysicalMaterialChannels`. Runtime `ACompiledSection` actors
  (`IsRuntimeOnly`) each live in their own external package with static meshes, a trimesh
  collision component, a far-field mesh, a per-section `UTexture2DArray` of channels, and a
  per-section material instance. Base modifiers are editor-only external actors
  (`MeshPartitionEditorComponent.cpp:689-747`). More structure an implementer must know:
  - `AMeshPartitionDataLayerContainer` creates one runtime data layer `MPDL_<Variant>` per build
    variant, initially Unloaded (`MeshPartitionDataLayerContainer.cpp:216`).
  - `FWPActorPropertiesTransformer` sets `bIsSpatiallyLoaded`, `RuntimeGrid`, `DataLayerAssets`,
    `HLODLayer`, and `bIncludeActorInHLOD`, so the zero-HLOD invariant depends on the pipeline.
  - Sections: `BuildModifierGroups` (`MeshPartitionModifierDescriptors.cpp:987`) greedily merges
    neighbouring bases while total vertex count stays within `MaxSectionComplexity`
    (`MeshPartitionDefinition.h:43`, default 262144), and never subdivides a base that is already
    over it. `bSplitSectionsToMatchWorldPartitionRuntimeGrid` instead emits one section per runtime
    grid cell, using the grid of the last WP transformer
    (`MeshPartitionWorldPartitionHelpers.cpp:33-117`), and hashes that grid into every section's
    build-variant hash (`WorldPartitionMeshPartitionBuilder.cpp:416-425`). `FSubsectionTransformer`
    (12800 cm) can split further on a 3D grid.
  - The default definition `/MeshPartition/DataAssets/MPD_Default` has three variants (a
    gameplay collision variant, a high-end Nanite render variant, a low-end LOD variant; values
    from a name scan, unverified). Trap: the class-default build variant has no
    `TransformerPipeline`, so a definition without pipelines produces no meshes and no collision.
  - Compiled static meshes set `bGenerateMeshDistanceField = false`,
    `bGenerateLightmapUVs = false`, and `bSupportRayTracing = true`.
  - Vertices stay in partition-local space; no maximum world extent constant exists.
- **E20** Only `UWorldPartitionMeshPartitionBuilder` produces compiled sections
  (`-run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionMeshPartitionBuilder`; `ReuseMode`
  `ForceRebuild`, `ByModifierHash`, `ByPackageHash` default, `ValidateHashes`). Cook does not build
  them (`MeshPartitionWorldUpdater.cpp:152-166`). Channel rendering returns early without a
  renderer (`MeshPartitionChannelCollection.cpp:612-616`) while the builder's
  `RequiresCommandletRendering()` returns false: a headless build silently loses channel textures.
  Epic's own editor menu launches the builder out of process with `-AllowCommandletRendering`
  (`WorldPartitionMeshPartitionBuilder.cpp:568-575`). The builder also writes map-owned state: it
  installs `UPlatformCellTransformer` into the map's World Partition and saves that package
  (`:313-320`), and the data-layer container creates data layer instances in the world's
  `WorldDataLayers` (`MeshPartitionDataLayerContainer.cpp:135-143`). The builder loads modifiers
  from saved actor
  descriptors ("Ensure the asset is saved before compiling or cooking"), so bases are saved before
  the build. Preview sections build through `ForceRebuildAllSections`,
  `BuildMegaMeshPreviewSections`, `SetForceSynchronousPreviewSectionBuild(true)`, and
  `IsAnyPreviewSectionBuildActive()`; they are editor-only and never acceptance evidence.
  `URuntimeCellTransformer` is installed only at PIE start (`MeshPartitionEditorSubsystem.cpp:385-392`);
  the builder installs `UPlatformCellTransformer` (`WorldPartitionMeshPartitionBuilder.cpp:316-319`).
- **E21** Each build writes fresh GUIDs (`BuildKey`, `WorldPartitionMeshPartitionBuilder.cpp:138`;
  `BodySetupGuid`, `MeshPartitionCollisionComponent.cpp:229`), so compiled-section packages are not
  byte-stable across rebuilds. Mesh commits use hash-as-GUID and deterministic normals, and the
  debug cvar `MegaMesh.Cache.ValidateModifierDeterminism` checks modifier determinism. No seed
  property exists.
- **E22** `FHeightmapImporter` accepts PNG only, samples bilinearly at pixel centres (a half-pixel
  resample), and is not script-exposed. Exported C++ creation path:
  `UMeshPartitionEditorComponent::SpawnBaseModifier(FDynamicMesh3&&, ...)`,
  `UMeshProviderModifier::SetMesh(FDynamicMesh3&&, bool)`, and
  `FRectangleGeneratorUtils::GenerateSectionMesh`; named `FDynamicMeshWeightAttribute` layers on a
  base mesh become channels by name (`MeshPartitionMeshData.cpp:438-449`). Blueprint and Python
  cannot create, import, or build. `MeshPartitionTestUtils.h` (`CreateTestMesh`,
  `CreateTestMeshSectioned`) shows Epic's complete creation sequence. Modifiers apply bases first,
  then other modifiers ordered by the definition's `ModifierTypePriorities` layers and a
  `double Priority` within each layer; sculpt layers are `UProjectMeshLayersModifier`. Linkage
  caveat by MinimalAPI convention (not test-built): `UTexturePatchModifier::SetHeightChannel` lacks
  `UE_API` and `UTexturePatchEntry` has no API macro. Hard limits: at most 4096 import sections and
  256 location volumes, channel textures up to 4096 px, at most 24 channels.
- **E23** Channels bake to 8-bit (`PF_G8`) slices per section at about section size /
  `ChannelTexelSize`, capped at 4096 px, with at most 24 channels by packing. The channel material
  expressions are Editor-module classes
  (`MeshPartition/Source/MeshPartitionEditor/Public/MeshPartitionMaterialExpressionUtils.h`):
  `...ChannelSample` resolves a name through a `UMeshPartitionDefinition` reference;
  `...ChannelSampleIndex` takes a plain index.
- **E24** Collision is `FCollisionTransformer` trimesh: complex-as-simple, double sided, BlockAll,
  affects navigation, per-triangle physical materials from channel weights. There is no heightfield
  path. `FCollisionSimplificationSettings` defaults to no simplification (QEM or Cluster available,
  10 cm error tolerance); render meshes default to `NoCollision`; collision is cooked into the body
  setup, and the complex mesh is not available at runtime.
- **E25** Nanite is optional per build variant. Variants are chosen per cook target platform, not
  per RHI at runtime (`MeshPartitionDefinition.cpp:263-295`, `MeshPartitionCompiledSection.cpp:91-108`).
  One Windows cook therefore serves both `PCD3D_SM6` and `PCD3D_SM5` (E16), and on SM5 a Nanite
  section draws its fallback mesh, which defaults to 20 percent of the triangles
  (`MeshPartitionStaticMeshTransformer.h:215-225`: `NaniteFallbackMode = Enabled`,
  `NaniteFallbackTarget = PercentTriangles`, `NaniteFallbackPercentTriangles = 0.2f`).
- **E26** Server cooks carry an open TODO: "How to handle cooks with
  -TargetPlatform=WindowsServer+Linux+MacOS?" (`MeshPartitionPlatformCellTransformer.cpp:22`).
- **E27** MeshPartitionWater is an Editor module that depends on Epic's Water plugin and carves
  through `AWaterBody` components. Base modifiers inside level instances are ignored ("base
  modifiers are not supported inside level instances", `MeshPartitionSeparateWorldBuilder.cpp:498`);
  a partition actor that is spatially loaded or inside a level instance is unsupported
  (`MeshPartitionDescriptorCache.cpp:475`); and the builder's reuse hash misses modifiers inside
  level instances ("this DOES NOT include GUIDs from modifiers inside level instances",
  `WorldPartitionMeshPartitionBuilder.cpp:730`). The territory contract represents authored
  overlays as Level Instances, so an authored terrain modifier there would be invisible to reuse
  (Q5). No tool
  converts an `ALandscape` to Mesh Terrain; ALIS regenerates from canonical data instead. The
  plugin ships 70 CQTest methods in 9 classes (categories `MeshPartition`,
  `MeshPartition.Modifier`, `MeshPartition.Modifier.Splines`), usable as an engine sanity control.
  Its code carries about 106 TODO/FIXME/HACK lines; the virtual-texture fallback is marked "a
  stop-gap solution, will be removed later", and `RecomputeTangents` is "not yet supported".

### Verified facts - Epic documentation (fetched 2026-09-23)

- **E28** Every Mesh Terrain page: "Learn to use this Experimental feature, but use caution when
  shipping with it." 5.8 release notes: the aim is "to replace and remove the typical constraints
  of the current heightfield-only landscape solution". Epic staff on the forums: "quite a bit of
  work before it can be considered production-ready and on-par"; Landscape stays "mostly in
  support mode" and both will "co-exist for a certain time (and quite possibly forever)"; "no major
  new landscape feature ... will be implemented"; production readiness version: "Too soon to
  tell"; runtime generation: "editor only at the moment"; performance "comparable to Landscape" is
  the aim, and no size claim exists. UE 5.8 is "the last planned major Unreal Engine 5 release".
  Hotfixes 5.8.1 and 5.8.2 each fixed two Mesh Terrain crashes; 5.8.3 fixed none. 5.8.2 added a
  default cap of 100 million new elements per tessellating modifier after a crash on extreme
  tessellation, so 5.8.1 lacks that guard. Further documented constraints: "You must use Mesh
  Partition in a World Partition level"; compiled sections must be built for PIE and cooked games;
  the builder "recalculates the entire section even if it is a small change"; only one UV channel
  (auto-unwrapped) exists; channels not declared in the definition stay editor-only; Epic staff
  report a known virtual-shadow-map issue while terrain streams in; and "for anything you need to
  ship, Runtime Virtual Texture is the supported option today". Sources:
  `dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine` and sibling pages,
  the 5.8 release notes, the UE 5.8 launch post, and Epic staff posts on forums.unrealengine.com.
- **E29** No MeshPartition or Mesh Terrain code, profile, or asset exists in the repository: the
  only hits are `world_partition.md` (the exclusion) and engine descriptor paths in
  `Build/*/FileOpenOrder/EditorOpenOrder.log`.
- **E30** Epic's current crafting guide says large sections amplify small modifier changes because
  the whole affected section is recalculated. It exposes automatic max-triangle partitioning and
  explicit section layout, but gives no universal section-size recommendation. Therefore Phase 1,
  not documentation guesswork, must choose ALIS's fixed producer-owned section bound.
  [Crafting Mesh Terrain](https://dev.epicgames.com/documentation/unreal-engine/crafting-mesh-terrain-in-unreal-engine).
- **E31** Epic's current Mesh Partition Definition guide recommends error-derived screen size for
  terrain/blockout LODs and exposes triangle-fraction guardrails, subsection target vertex count,
  skirts, collision simplification, and per-platform variants. These are candidate controls, not
  accepted ALIS values; Phase 1 must prove cracks, achieved error, SM5 fallback, collision, and
  package cost before freezing the definition.
  [Mesh Partition Definition](https://dev.epicgames.com/documentation/unreal-engine/mesh-partition-definition-in-unreal-engine).
- **E32** Epic staff confirm a known streaming/VSM cache failure where the cached shadow can use a
  lower-resolution mesh than the displayed terrain. `r.Shadow.Virtual.Cache=0` is a valid temporary
  workaround; `r.Nanite.VSMInvalidateOnLODDelta=1` is experimental and was not tested by the staff
  responder on Mesh Terrain. Neither becomes configuration by assumption: Phase 3 must reproduce
  or clear the defect on the accepted streaming route and measure any workaround hitch/cost.
  [Epic staff response](https://forums.unrealengine.com/t/shadows-on-mesh-terrain/2744029).
- **E33** The public UE 5.8 API documents `FTransformer` and `UTransformerPipeline` in the runtime
  MeshPartition module, with pipelines storing instanced transformer structs. It also declares
  `AMeshPartition` and the pipeline `MinimalAPI`. This makes an extension seam plausible, not
  proven: export/link/subclass/serialization feasibility must be compiled against the completed
  5.8.3 source engine before D8 selects a wrapper.
  [FTransformer](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartition/FTransformer),
  [UTransformerPipeline](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartition/UTransformerPipeline),
  [AMeshPartition](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartition/AMeshPartition).
- **E34** Epic's public 5.8.3 hotfix notes list no Mesh Terrain-specific fix. The completed local
  `5.8.1-release..5.8.3-release` diff proves that public-note silence did not mean a byte-identical
  plugin: four implementation files changed (null safety, preview cleanup, and tessellation failure
  handling; 50 insertions and 21 deletions). No definition, channel, material-expression, builder
  reuse-hash, or public extension header changed, so E18-E27's reviewed ownership and API facts hold
  on 5.8.3. Export/link viability still requires the planned ALIS target compile probe.
  [UE 5.8.3 hotfix notes](https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315).
- **E35** `run_kazan_runtime_profile_tournament.ps1` takes the global generated-content lock and
  restores a snapshot in `finally`, but owns no durable outer interruption journal. The release
  projection has the same failure shape. Locking prevents interleaving; it does not run `finally`
  after process or host death. Therefore the candidate comparison must not mutate the live tree.
- **E36** Epic's public workflow documents compiled-section generation through the Editor Build
  menu and states it is mandatory for PIE/cooked games; it does not document a Mesh Terrain
  commandlet or unchanged-build/no-write guarantee. ALIS automation must therefore use the exported
  builder path verified in 5.8.3 source and prove idempotence itself rather than infer it from the UI
  workflow.
  [Crafting Mesh Terrain](https://dev.epicgames.com/documentation/unreal-engine/crafting-mesh-terrain-in-unreal-engine#build-compiled-sections).
- **E37** Epic documents Sculpt modifiers with local mesh layers, sculpt/paint tooling, and weight
  channels that can be painted or injected by modifiers. It does not document export of those edits
  to an engine-neutral delta file or Level Instance participation in the reuse hash. The selected
  hand-layer design therefore keeps ALIS-authored deltas authoritative and treats Epic sculpt/paint
  as an editor capture front end only after a round-trip proof.
  [Mesh Terrain overview](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine),
  [Sculpting and painting](https://dev.epicgames.com/documentation/unreal-engine/crafting-mesh-terrain-in-unreal-engine#sculpting-and-painting).
- **E38** ALIS already has a representation-neutral authored terrain overlay authority. The
  CanonicalCompilation `authored-overlay.schema.json` owns `terrain_patches`; production controls
  select an overlay, and `terrain.py:249-252` adds each patch's `delta_m` to raw terrain before
  quantization. `lineage.py:179-193` derives dirty bounds from changed patch identity and footprint,
  while synthetic tests prove the edit changes the intended terrain cells. The current schema lacks
  grid binding, per-edit provenance, surface-weight overrides, and explicit overlap semantics. The
  hand layer must extend this owner rather than create a realization-only terrain-edit database.
- **E39** Epic 5.8.3 keeps the channel ABI in `UMeshPartitionDefinition`: material, priority layers,
  build variants, platform selection, `FChannelMap`, channel texel policy, and physical-material
  channels. `GatherDependencies` hashes the channel map and texel/layout policy into section build
  identity. Epic explicitly says the same MPD can be used by multiple Mesh Partitions, and name- and
  index-based material samples both remain available. This supports one shared adapter-owned MPD
  with semantic names outside and numeric indices private to the adapter.
  [Mesh Partition Definition](https://dev.epicgames.com/documentation/unreal-engine/mesh-partition-definition-in-unreal-engine),
  [Mesh Terrain Material](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-material-in-unreal-engine).
- **E40** The current hand/hydro order is not the draft's former "raw/hydro-conditioned" ambiguity.
  `CanonicalCompilation/app/terrain.py:245-252` applies each authored `delta_m` to provider terrain
  and then quantizes it. Later, ProjectWorld realization copies those canonical heights and clamps
  covered samples to the canonical Water surface
  (`ProjectWorldTerrainWaterConformance.cpp:205-218,339-355`), called while building the Landscape
  heightfield (`ProjectWorldLandscapeRealization.cpp:567-590`). The current order is therefore
  `raw -> hand delta -> quantization -> ProjectWorld hydro clamp`. That can erase a positive hand
  correction under Water and does not satisfy D7's hand layer "on top" target.
- **E41** UE 5.8.3 `UMeshPartitionDefinition::GatherDependencies` explicitly states that material
  contents are not used to build sections and records only the material path
  (`MeshPartitionDefinition.cpp:93-97`). It does hash the build variant, modifier priorities,
  channel map/layout policy, transformer dependencies, and physical-material channels. Epic's API
  says the gathered dependencies participate in section-staleness decisions, but source inspection
  alone does not prove the real builder/cook no-write behavior after a same-path material content
  edit. Phase 1 measures it with a known-rebuilding object-path-change control.
  [UMeshPartitionDefinition API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartition/UMeshPartitionDefinition).
- **E42** The current ProjectWorld plugin has `CanContainContent=false`; its runtime and Editor
  Build.cs files name no ProjectMaterial module, and repository search finds no
  ProjectMaterialEditor or material-generation host call under ProjectWorld, ProjectWorldData,
  CanonicalCompilation, or World realization scripts. Therefore the shared MPD needs a
  content-capable adapter owner, while the existing zero World-to-ProjectMaterialEditor dependency
  is a contract to preserve rather than a missing integration.

### Reliability history the current mechanisms answer

Read these [ProjectWorld pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md) before
replacing the mechanism each one produced; the new representation must keep the guarantee:

- 10 - global identity on the logical Landscape broke cell locality (per-cell identity);
- 11, 13, 14 - territory scale, relief, and terracing (surface-quality gates);
- 17 - an empty recovery journal (transaction recovery);
- 22, 26, 33 - stale producer fingerprints (fail-closed fingerprint currency);
- 25 - a material migration must dirty every proxy (material identity versus same-path tuning);
- 28 - Water z-fighting, fixed by hydro-conditioning (A2).

The surface-quality gate `tools/World/VisualVerification/app/surface.py` (`MAX_TERRACE_RATIO`
0.45, `MIN_SUPPORTED_LEVEL_RATIO` 0.50) judges canonical terrain itself, so it is substrate
independent ([canonical.md](../../../docs/agents/canonical.md#gate-scope-and-missing-acceptance-dimensions)).

### Existing guards on the current representation

- C++ (`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/`):
  `Project.World.Realization.NativeTwin.LandscapePartitionAndEditLayers`, `.LandscapeProxySemanticIdentity`,
  `.SingleLayerWaterNaniteCompatibility`, `.GeneratedBaseHeightAuthority`, `.TerrainVerifierRejectsFlat`,
  `.FinalHeightmapAuthority`, `.PlannerDirtyOverridesCacheTag`, `.TerrainWaterLayerOrder`,
  `.TerrainWaterLayerComposition`; `Project.World.Realization.SavePolicy`, `.LandscapeLayoutFailClosed`,
  `.LandscapeHeightfield`, `.GeneratedCellPlacementMatchesLandscape`, `.AuthoredLandscapeLayerSurvives`,
  `.OwnedDeletionPreservesAuthoredActor`, `.CommandletBoundary`;
  `Project.World.Realization.Layers.ProfileContract`, `.DirtyClosure`, `.IncrementalInventory`;
  `Project.World.Realization.Runtime.NoHLODPartitionPolicy`.
- Pester (`scripts/ue/world/test/`): `generated_content_transaction.Tests.ps1`,
  `generator_fingerprint.Tests.ps1`, `generated_layer_manifest.Tests.ps1`,
  `generated_manifest.Tests.ps1`.
- Integration: `test/integration/realization_layer_lifecycle.ps1` (unchanged Apply updates zero
  terrain components; one-cell dependency closure; `-ProvePackageLocality`),
  `runtime_profile_locality.ps1`.
- EndToEndValidation: `tests/test_territory_matrix.py`
  (`test_layered_realization_rejects_flat_final_surface`,
  `test_incremental_reuse_scales_to_the_complete_territory`), `tests/test_end_to_end_validation.py`.

Each guard either stays green, is rewritten against the representation-neutral invariant it
protects, or is deleted with the Landscape code it pins - never silently dropped.

### Inferences

- Mesh Terrain removes three Landscape constraints ALIS pays for: exact-divisor layout admission
  (and the ProceduralMesh fallback it forces), the 16-bit height range, and edit-layer composition
  through `ForceUpdateLayersContent`. Whether that yields a simpler compliant path is what
  Phases 1-3 decide.
- Locality changes shape. The dirty unit stays the canonical cell (one base per cell), but compiled
  sections group bases, so a one-cell change rebuilds every section that intersects it. Neither
  engine default fits ALIS: complexity alone keeps all of Kazan in one section (210 x about 1024 =
  215,040 vertices, under 262,144), which defeats spatial streaming; splitting by runtime grid
  hashes the runtime grid into every section, so switching the runtime profile would rebuild all
  terrain, contradicting "switching a runtime candidate must preserve all terrain ... manifest
  entries and artifact bytes" ([native reuse gate](../../../Plugins/World/ProjectWorld/docs/world_partition.md#unreal-native-reuse)).
  The partition must be fixed by the producer's own typed settings.
- Replacing the terrain generator regenerates every dependent layer by contract (E2). The cutover
  is therefore a full regeneration of both territories, and the comparison twin must include all
  layers to be comparable.

### Assumptions and unverified areas

- SM5 rendering of compiled sections; the dedicated-server cook; builder reuse producing zero
  writes on unchanged input; bitwise-identical channel textures across GPUs and drivers; cook
  behaviour when `URuntimeCellTransformer` is only installed at PIE start; whether compiled
  sections can carry ALIS actor tags through a project transformer.
- The staged CanonicalCompilation implementation needed for A2: canonical base terrain currently
  feeds Water compilation only after the hand delta and quantization. Phase 0 must prove a cycle-free
  provider-terrain -> base-quantization -> Water -> hydro -> hand -> final-surface pipeline,
  incremental reuse, and exact dirty closure.
- The exact content-capable adapter plugin and package path for the one shared MPD. The current
  ProjectWorld plugin cannot contain it (`CanContainContent=false`).
- Same-path material content invalidation through the actual builder/cook route (E41).

### Refuted

- "Landscape keeps ... and builds its Nanite representation"
  (`world_partition.md`, Geometry representation policy) and the matching statement in
  `territory_contract.md` - Nanite Landscape is not enabled (E7).
- "a water-only input change dirties no Landscape package" (`world_partition.md`, Production
  realization invariants) - contradicted by `territory_contract.md` and `HashTerrainWaterCellInput`
  (E10).
- MeshPartitionWater as a ready ALIS Water integration - it requires Epic's Water plugin, which the
  native reuse gate rejects as realization authority (E13, E27).
- Epic's heightmap importer as the ingestion path - PNG-only and resampling (E22).
- `architecture_overview.md` as a ProjectWorld owner - it does not exist; the architecture owner is
  [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md).
- `tools/World/EndToEndValidation/README.md:136` says the realization commandlet runs with
  `-NullRHI` - `execution_envelope.ps1` forbids it and the realizer requires rendering (E16).
- The "Generated Roads" edit layer as a road deformation pass (`world_partition.md`, Landscape
  topology) - it is created but never written (E4).
- "ProjectWorldData supplies only real-place facts" - too narrow. The owner map says it also owns
  concrete profiles, authored inputs, canonical bundles, realized packages, and manifests. The
  correct boundary is that reusable terrain-surface meaning and derivation do not belong there.
- Hand-authored cell IDs as a second terrain authority - rejected. Canonical coordinates and the
  accepted grid are authoritative; affected cell IDs are derived and recorded in compile output and
  dirty receipts. Storing an independently editable per-cell index would duplicate identity.
- "ProjectWorld is only an Unreal-side disposable adapter" - too broad. The owner map gives
  ProjectWorld reusable contracts, schemas, generation logic, runtime services, and validation.
  The narrower reviewer conclusion is valid: engine-independent canonical surface names, meanings,
  derivation, and authored-override semantics belong to CanonicalCompilation, while ProjectWorld
  consumes them for Unreal realization.
- "`UMeshPartitionDefinition::GatherDependencies` hashes material contents" - false in UE 5.8.3;
  the implementation records only the material object path (E41). That does not prove package reuse,
  so the builder/cook probe remains required.

## Current architecture and source of truth

```text
provider snapshots -> SourceIngestion -> CanonicalCompilation -> canonical bundle
                                      ^
                                      | existing authored-overlay authority
        |
        v
ProjectWorld realization (profile layer "terrain" = project_landscape:v1)       [this task]
        |   hydro-conditioned cell heights, identity, dirty scope, transaction
        v
ALandscape root + 210 LandscapeStreamingProxy packages per territory
        |
        v
World Partition 512/1536 runtime grid -> product route gate (tag ProjectWorld.Landscape.v1)
```

Ownership: [Territory generation owner map](../../../Plugins/World/ProjectWorld/docs/territory_generation.md);
layer tuples, locality, and authored channel in the [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md);
representation policy in [World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md);
transactions, locks, manifests, and enrollment in [Canonical World Realization](../../../scripts/ue/world/README.md).

Boundary declaration:

- **Owning black boxes:** CanonicalCompilation owns the engine-independent final terrain-surface
  contract and hydro/hand derivation; ProjectWorld realization owns the terrain producer lifecycle;
  the Mesh Terrain adapter owns representation ABI and its shared MPD.
- **Public contracts:** the canonical terrain-surface bundle/lineage; the realization-profile layer
  tuple (`generator_id:generator_version` plus typed settings); the adapter layout receipt and shared
  MPD identity; the owner's generated roots and manifests.
- **Expected CHANGED (Phases 1-3):** the CanonicalCompilation terrain-surface and authored-overlay
  schemas/logic, staged provider/base-quantization/Water/hydro/hand pipeline, lineage, tests, and
  synthetic fixtures; the narrow SourceIngestion boundary-selection correction required because a
  reference-complete OSM export can contain member ways beside the selected relation, with its
  direct unit regression and no provider-contract change; a
  terrain producer seam selected by generator ID; one new content-capable World representation
  adapter on the current MeshPartition allowlist; one shared MPD and layout receipt; a new registry
  tuple and profile-schema settings; its fingerprint source set; a synthetic test profile; a sibling
  comparison harness; host-level integration with the sibling material task.
- **Expected CHANGED (Phase 4 only):** the four production profiles and the synthetic twin
  profile; every E12 consumer; removal of the Landscape producer.
- **Expected UNTOUCHED:** provider payloads and SourceIngestion's provider-neutral output contract;
  CRS, grid, cell, feature, raw provider-terrain, and Water source semantics; the other layer
  producers' logic; ProjectMaterial internals (the sibling material task owns its adapter); the
  guarantees of transaction and lock semantics; and production generated authority before Phase 4.

## Capability gap

Nothing is broken. The gap is strategic: the generated terrain is a heightfield Landscape, which
Epic now keeps "mostly in support mode", while Epic's stated next-generation large-world terrain is
Mesh Terrain (E28). ALIS pays for Landscape constraints (E4) and has not proved whether Mesh
Terrain can meet ALIS's invariants - determinism, locality, transaction ownership, collision,
streaming, Shipping cook, performance, and size - while it is still Experimental.

## Decision

### Target shape

```text
provider terrain + canonical Water inputs
        |
        v
CanonicalCompilation terrain-surface contract               (engine-independent semantic authority)
        |   base quantization -> Water solve -> hydro conformance -> hand overlay
        |   final quantization/validation, versioned semantic weights, lineage
        |
        v
ProjectWorld terrain layer (role "terrain")                 (owns identity, dirty scope,
        |   final canonical heights + semantic weights       transaction, verification)
        |   producer chosen by generator_id from the profile
        v
Mesh Terrain adapter black box                              (reviewed representation adapter)
        |   one shared adapter-owned MPD
        |   semantic name -> private MPD channel/index
        |   schema-validated channel-layout receipt
        |   AMeshPartition,
        |   one FDynamicMesh3 base per canonical cell,
        |   WorldPartitionMeshPartitionBuilder with rendering
        v
compiled sections: static meshes + trimesh collision + channel arrays
        |
        v
World Partition runtime; runtime consumers identify terrain by a representation-neutral role
```

The terrain layer's role stays `terrain`. The representation lives only in the generator tuple
`project_mesh_terrain:v1` and its typed settings; the top-level `landscape` profile block does not
survive the cutover (D4). The content-capable adapter is `ProjectWorldMeshTerrain`, split into the
runtime `ProjectWorldMeshTerrain` module and editor-only `ProjectWorldMeshTerrainEditor` module.
Compiled terrain carries the representation-neutral tag `ProjectWorld.Terrain.v1`, spatial
ownership is `compiled_sections_from_canonical_cells`, and the shared definition object path is
`/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1`.

MeshPartition dependencies are allowed only in explicitly declared representation-adapter modules.
The strict current allowlist contains this World Mesh Terrain adapter and the ProjectMaterial Mesh
Terrain sampling adapter. ProjectWorld core/runtime, ProjectWorldData, SourceIngestion,
CanonicalCompilation, and canonical schemas have zero MeshPartition dependency. A future Water,
physics, or other native adapter requires architecture review and an explicit allowlist update; the
count itself is not the invariant. The channel
material expressions live in `MeshPartitionEditor` and the Resource expression hard-references
`/MeshPartition/Textures/Void2DArray` (`MeshPartitionMaterialExpressionUtils.cpp:27`), so the
material side cannot avoid the dependency without copying Epic's channel-table packing, which is
vendoring.

The World adapter publishes and validates a schema-versioned plain-data channel-layout receipt. The
ProjectMaterial host consumes that receipt and invokes ProjectMaterialEditor independently. No
ProjectWorld module calls, links, loads, or gains a Build.cs dependency on ProjectMaterialEditor; a
host-level integration gate proves the canonical contract, adapter receipt, shared MPD, and generated
material agree before promotion.

### Canonical surface and hand-layer order (D7)

The selected authority is the existing CanonicalCompilation authored overlay (E38), extended in one
schema revision rather than a second terrain-edit database. ProjectWorldData owns concrete overlay
records; CanonicalCompilation owns schema validation, application, lineage, and dirty derivation;
its engine-independent terrain-surface contract owns the reusable names and meanings that an override
may name. ProjectWorld consumes the result. Epic sculpt and paint tools may later be a capture front
end, but their actor state is never accepted authority.

The target order is exact and has one writer:

```text
provider-derived terrain samples
    -> base canonical quantization for Water fitting
    -> canonical Water solve before hand correction
    -> canonical hydro conformance
    -> authored height and surface overrides
    -> final quantization and Water-clearance validation
    -> immutable canonical terrain-surface projection consumed by ProjectWorld
```

CanonicalCompilation must expose the provider-derived base stage internally so Water fitting does
not depend on hand edits, then publish only the final derived terrain surface as realization input. A positive
hand edit under Water is not silently erased by a later ProjectWorld clamp: final validation rejects
it when it violates the declared Water clearance. ProjectWorld removes its second hydro writer at
cutover. Phase 0 must prove this staging can preserve deterministic incremental compilation and
locality; inability to do so is an R1 blocker because reverting to hand-before-hydro would contradict
D7 rather than merely change an implementation detail.

The first complete schema has explicit regeneration semantics:

1. The overlay binds to the accepted `grid_id`; each patch has a stable `patch_id`, canonical-metre
   footprint, and `provenance_ref`. Canonical coordinates are authoritative. The compiler derives
   and receipts affected cell IDs; authors do not maintain a second cell index.
2. Height edits are additive metres over the hydro-conditioned canonical base and are quantized only
   after application. If provider terrain or Water changes, the same delta remains the same relative
   correction. Overlapping height patches sum deterministically. Final Water-clearance validation
   rejects an incompatible edit rather than clamping it away.
3. A surface edit is a complete mapping from canonical terrain-surface semantic name to weight. Unknown,
   duplicate, missing, negative, or non-finite weights fail closed. Values must sum to one within a
   fixed schema tolerance and are canonicalized deterministically. The override replaces procedural
   weights inside its footprint; overlapping surface overrides use explicit integer priority, and
   an equal-priority overlap is rejected rather than ordered accidentally by JSON position.
4. Overlay identity and every consumed patch hash enter canonical terrain lineage, terrain producer
   identity, and dirty planning. Editing or removing a patch dirties the union of its previous and
   current derived cell footprints plus the declared dependency halo.
5. Absolute-height edits are not inferred from an additive record. They require a later explicit
   schema capability with its own rebase semantics.

The territory contract's Landscape-only "Authored Corrections" exception is replaced at cutover.
Existing correction data, if Phase 0 finds any, migrates into this overlay authority and is proved
against the same regeneration semantics.

### Section partition

The producer's typed settings fix the section partition independently of the runtime profile: an
explicit section-complexity bound, or a subsection grid aligned to canonical cells. R1 reviews the
selection rule; Phase 1 measurements choose and receipt the value.
`bSplitSectionsToMatchWorldPartitionRuntimeGrid` is not used, because it couples
terrain bytes to the runtime profile (see Inferences). Choosing it anyway is an operator contract
change, not an implementation detail.

### Premise / KISS gate

- **Owner:** CanonicalCompilation already owns engine-independent canonical terrain, features,
  authored-overlay application, lineage, and deterministic bundles. It gains the derived final
  terrain-surface projection and exact hydro/hand order. ProjectWorld keeps realization identity,
  dirty execution, manifests, transactions, and verification, and swaps only the Unreal
  representation behind that boundary. ALIS owns policy and identity; Unreal owns geometry,
  serialization, and streaming.
- **Added:** one content-capable adapter owner and its Experimental plugin dependency; one producer
  seam; a canonical terrain-surface contract; one shared adapter-owned MPD and transformer policy;
  a plain-data channel-layout receipt; the existing authored overlay's complete terrain
  edit schema; a GPU-rendered builder step inside the existing transaction; compiled sections that
  are not byte-stable.
- **Removed after an accepted cutover:** the Landscape producer and layout admission, the 16-bit
  encoding path, edit-layer composition and the never-written "Generated Roads" layer, the
  ProceduralMesh terrain fallback once no fixture needs it, the `project_landscape:v1` tuple and
  fingerprint set, and the representation-specific names in E12.
- **Capability knowingly given up:** Landscape-native features - heightfield collision, Landscape
  grass, splines, and edit layers. ALIS uses only edit layers (replaced per A6) and heightfield
  collision (replaced by trimesh; measured in Phase 3).
- **Why an adapter owner instead of code in ProjectWorldEditor:** it structurally enforces D3
  (nothing else in World can include MeshPartition headers), keeps the Experimental dependency out
  of the World core, makes the Q6 give-up cleanup bounded, and localizes future Epic API changes.
  It must be its own content-capable World plugin because the existing ProjectWorld descriptor has
  `CanContainContent=false` and one shared MPD cannot belong to a concrete territory data plugin.
  The selected owner, module split, package path, and representation-neutral runtime identity are
  fixed above; Phase 1 proves their game-target loading contract.
  The Shipping game needs the MeshPartition runtime module either way, because compiled sections are
  MeshPartition actors.
- **Why a producer seam:** a second terrain producer cannot be selected while dispatch keys on
  `Layout.bCompatible` (E3). The seam lands outside `ProjectWorldRealizationService.cpp`, which is
  already above the guardrail.

### Comparison host

Create a sibling substrate harness that reuses the tournament's measurement helpers without using
its in-place snapshot lifecycle (E35). From one exact base commit plus the authenticated A8 candidate
overlay, the isolation helper creates a detached worktree under `tmp/`. The candidate realizes complete Kazan there (all layers,
E2 inference) at the same map package and runtime profile, so the normal product route reaches it
without a menu entry or second canonical authority. Packaging and measurement happen inside that
checkout. Success, failure, or interruption can leave only release-owned disposable files; the
operator's live generated tree and index remain unchanged.

The harness records source commit, engine identity, profile identities, and isolated root. Cleanup
uses the projection task's exact path/HEAD/receipt checks. It does not extend the World transaction
journal to own an outer release/test projection, and it does not weaken the runtime-only
tournament's existing "no generated layer dirtied" assertion.

### Alternatives considered

- **Keep Landscape and improve only the material** - no longer a planned fallback after D6. It is
  considered only if an unresolved blocker survives D8 workarounds and Q6 returns to the operator.
- **Enable Nanite on the existing Landscape** - a cheap performance experiment, but it does not
  decide the substrate question, and Epic documents that Nanite Landscape streams on top of
  ordinary Landscape data.
- **ALIS-owned per-cell Nanite StaticMesh terrain** on the already-admitted
  `FMeshDescription` / `UStaticMesh::BuildFromMeshDescriptions` route (the native reuse gate's
  "Persistent static mesh" row, already used for buildings and water) - mature API, one package per
  cell, no Experimental dependency. Not the default because D1 targets Epic's next-generation
  substrate, and it would forgo Epic's tessellation, channel baking, far field, and future engine
  investment. It is not built as a third candidate, but the Phase 3 packet states which measured
  Mesh Terrain benefits this route would also deliver, so Q1 separates "mesh terrain" from "Epic
  Mesh Terrain".
- **Keep both substrates permanently** - rejected: two realizations of one authority, doubled
  fingerprints, transactions, tests, and incidents (D4, KISS).
- **Epic heightmap import** - rejected: PNG-only and resampling (E22).
- **Epic Water plus MeshPartitionWater for hydro** - rejected: Epic Water is excluded as
  realization authority (E13); ALIS computes the canonical conditioned surface (A2).
- **One identical MPD generated under every territory owner** - rejected as the default. Epic
  supports sharing one definition across partitions, and territory copies would duplicate channel
  ABI and build-policy authority. Phase 1 may admit per-territory projections only after proving a
  concrete engine or transaction constraint that the shared adapter-owned asset cannot satisfy.
- **PCG as an authoring path** - rejected as terrain authority; its adapter component serializes
  the full mesh into cooked data.
- **Fork or vendor the plugin** - forbidden in this public repository (E13); see Q3.

## Required invariants

1. Provider authority is unchanged: provider snapshots, CRS, grid, cell and feature IDs, provenance,
   and real-place terrain/water facts do not change as a side effect of representation migration.
   The canonical contract changes only for the engine-independent terrain-surface projection,
   hydro/hand order, and existing authored overlay described in E38-E40; those changes are
   schema-versioned, deterministic, lineage-complete, and independently tested. Every canonical
   semantic name claims only what its derivation and provenance prove; slope alone cannot become a
   factual geology or land-cover class.
2. No MeshPartition type, module dependency, or Mesh Terrain vocabulary appears in SourceIngestion,
   CanonicalCompilation, canonical schemas, the terrain-surface semantic contract, authored overlay,
   or ProjectWorldData source semantics (D3). Realization profiles may name the generator tuple and
   its typed settings.
3. MeshPartition headers, module dependencies, and plugin dependencies exist only in modules on the
   reviewed representation-adapter allowlist. The current allowlist is the World Mesh Terrain
   adapter and ProjectMaterial Mesh Terrain sampling adapter. ProjectWorld core/runtime,
   ProjectWorldData, SourceIngestion, CanonicalCompilation, and canonical tools remain dependency
   free. Any future adapter requires architecture review and an explicit allowlist update (D3, D4).
   No ProjectWorld module depends on or invokes ProjectMaterialEditor; coordination is plain-data at
   the host and integration-gate boundary.
4. Production generated authority for Kazan and Manhattan is byte-identical after Phases 0-3 and
   after any rejected or interrupted run. Comparison runs hold the global content lock.
5. At every canonical sample the realized surface reproduces the final canonical terrain-surface
   projection - provider terrain, base quantization, canonical Water solve, hydro conformance, hand
   overlay, then final quantization/validation - within a tolerance derived from canonical
   quantization and the new
   representation's precision. R1 fixes the derivation rule; Phase 1 records the achieved precision
   and resulting tolerance before the acceptance run, and it is never loosened to pass. ProjectWorld performs no
   second hydro clamp. There is no resampling, and shared cell edges match exactly.
6. Final-surface evidence reads the compiled runtime representation that cooks, not preview
   sections or base-modifier input, and stays independent of source-layer evidence. A flat or
   offset surface fails it.
7. Triangulation between samples is deterministic and recorded. Phase 0 identifies the
   interpolation behind `canonical_terrain_snap`, building anchors, and road draping, and the
   realized surface stays within tolerance of it, so dependent geometry neither floats nor sinks.
8. Identical canonical input and generator identity produce identical semantic output identity
   (heights, triangulation, channels, section partition, bounds). No-op is decided before
   serialization, and an unchanged Apply writes zero packages, compiled sections included. The
   producer identity includes the engine build identity (changelist and MeshPartition plugin
   version), because compiled sections are engine output and `Get-ProjectWorldGeneratorFingerprint`
   hashes only first-party files (`generator_fingerprint.ps1:226-245`); ProjectMaterial already
   records `engine_identity`. The definition's channel map and the channel weight sources are
   terrain producer inputs, because the channel map is a build dependency of every section
   (`MeshPartitionDefinition.cpp:104`). One shared adapter-owned MPD owns that ABI and build policy;
   every territory references it. Semantic names come from the canonical contract, while numeric
   indices remain private to the adapter. Per-territory MPDs require a recorded Phase 1 engine
   constraint.
9. A one-cell terrain change rewrites only that cell's base package plus the compiled sections
   intersecting it within a declared halo; a water-only change rewrites only cells whose canonical
   hydro projection changes; a hand edit rewrites its exact previous/current footprint plus halo.
   The real builder/cook probe must prove same-path material content tuning rewrites no compiled
   section or World package before that locality contract is accepted. Each rewrite set is bounded
   and reported.
10. The shared MPD is manifest-owned, snapshot-covered, and rolled back by the adapter owner; its
    version and bytes hash enter every consuming territory identity. Territory-specific packages -
    partition actor, base modifiers, compiled sections, data-layer container and assets - are
    manifest-owned under the territory's generated root, snapshot-covered, and rolled back exactly.
    A coordinated rollout keeps the prior MPD bytes/version until every consuming territory can
    roll back safely; a shared-MPD change cannot leave mixed ABI consumers. The builder runs inside
    the territory transaction and shared lock. Every package the builder and data-layer container
    write, including the map `.umap`
    and its World Data Layers (E20), belongs to exactly one scope: the map producer owns that
    one-time setup, or Phase 1 proves it is stable across clean reconstruction; terrain-only work
    never re-saves the map ([Production realization invariants](../../../Plugins/World/ProjectWorld/docs/world_partition.md#generated-authority-and-transactions)).
11. The hand layer exists (D7) by extending the existing CanonicalCompilation authored overlay, not
    by adding another store. It binds to the accepted grid and records canonical-coordinate
    footprints plus provenance. CanonicalCompilation orders provider terrain -> base quantization ->
    Water solve -> hydro conformance -> hand overlay -> final quantization/validation. Heights are
    additive metres over
    the hydro-conditioned base; incompatible Water-clearance edits fail closed rather than being
    clamped later;
    surface-weight overrides are complete normalized semantic-name maps with explicit priority and
    fail-closed overlap rules. The overlay survives Apply, reconstruction, rollback, and Delete
    byte-identically, shapes final heights and channels, and dirty planning derives the exact union
    of previous/current affected cells and halo.
12. Terrain streams spatially under the accepted runtime grid; no terrain geometry is always loaded
    beyond the engine's non-spatial `AMeshPartition` root; zero HLOD stays true. The section
    partition is independent of the runtime profile: switching runtime profiles rewrites no terrain
    package.
13. Terrain collision supports spawn, traversal, and centre-edge-centre streaming in Development and
    Shipping. The dedicated-server cook keeps terrain collision, or its absence is reported as a
    blocker (E26).
14. Both cooked shader platforms (`PCD3D_SM6` with Nanite, `PCD3D_SM5` without) render terrain with
    no default-material fallback, and channel textures and far-field meshes exist in the cooked
    build. The geometry SM5 actually draws - the Nanite fallback mesh (E25) - also meets invariants
    5 and 7, so roads and buildings neither float nor sink on SM5. The fallback target is set
    explicitly for terrain, and the 20 percent default is the known-bad control.
15. Section builds run in the accepted rendering envelope. A headless or NullRHI build is
    rejected and is never accepted with missing channel textures (E20).
16. No installed Engine file is modified and no Epic source is copied into the repository.
17. There is no runtime toggle, menu entry, or permanent second substrate. After an accepted
    cutover exactly one terrain producer exists, and no stable doc, code, comment, test, or
    configuration mentions the superseded Landscape realization (D5); after a rejection nothing of
    the candidate remains.
18. Both candidates are measured on the same accepted envelope (E15) with the same route, warmup,
    and samples. A candidate that misses the fixed frame budget cannot be accepted, and the budget
    is never relaxed. Streaming from unloaded to final detail also produces correct shadows; a
    stale VSM cache image fails. Any workaround from E32 is accepted only after its hitch and frame
    costs are measured on the same route and recorded as an owned temporary compatibility measure.
19. Size is measured from cooked and packaged artifacts, per owner and for the whole package, never
    from source folders.
20. Representation-specific names do not enter representation-neutral contracts: the layer role,
    the runtime terrain identity, evidence field names, and validation flags (D4).

## Implementation tasks

### Phase 0 - live census and baseline (read-only plus L0)

- [x] Record `git status` and leave unrelated work untouched. The only starting changes were this
      task and the linked material task; the Git index was not changed.
- [x] Re-read the cited owner sections, confirm each E-fact, and update this task where the tree
      moved.
- [x] Confirm the source update ended `DONE`, diff MeshPartition from `5.8.1-release` to
      `5.8.3-release`, and recheck E18-E27 (E34). The target-version source facts are current; the
      public extension seams in E33 still require an ALIS target compile probe.
- [ ] Classify every E12 consumer: a representation-neutral contract to rename, Landscape-only code
      to delete at cutover, or unrelated (City17, authored). This remains a Phase 4 preflight gate;
      it was not required to admit the isolated comparison.
- [x] Identify the interpolation used by dependent layers (invariant 7):
      `ProjectWorldGeneratedGeometry::SampleTerrain` performs bilinear canonical-cell sampling.
- [x] Measure whether the production "Authored Corrections" layers of Kazan and Manhattan contain
      any non-zero data; any existing correction migrates into the new hand layer (D7).
      Both current canonical overlay documents are empty.
- [x] Choose the hand-layer authority: extend the existing CanonicalCompilation authored overlay
      with the explicit semantics in Decision, "Hand layer" (E38). R1 must review the contract.
- [x] Design and review the cycle-free CanonicalCompilation stages required by A2: preserve raw
      provider terrain for Water fitting, derive hydro conformance, apply hand height/surface
      overrides, quantize and validate the final surface, then derive lineage and dirty closure from
      every consumed provider-base, Water, contract, and overlay input. Prove there is one published
      final-surface authority and no ProjectWorld post-clamp.
- [x] Verify the isolated comparison host (A8) against the projection task's path, source-identity,
      candidate-overlay, LFS, cleanup, concurrency, and forced-interruption proofs. Prove a tracked
      binary change and an untracked candidate file survive materialization with the recorded byte
      identities, and prove an omitted or extra file is rejected. Reject any host that mutates the
      live generated tree, needs a second Kazan authority, or adds a menu entry. The helper passes
      tracked-binary, untracked-file, omission, extra-file, LFS, concurrency, interruption,
      long-path, and live-tree/index isolation controls. R2 authenticated overlay v3 and the
      materialized workspace byte-for-byte at source identity
      `f77687bf12b88ec076c2eab98eb8fb35435069df702fdeb6d7859cfa7ecb406c`.
- [x] Capture the Landscape baseline from one source identity: generation time; package inventory
      by owner; cooked IoStore bytes for terrain and the whole package; the frame percentiles the
      [performance evidence owner](../../../scripts/ue/world/test/performance/project_world_performance_evidence.ps1)
      records on the accepted route; residency and streaming; collision traversal; same-vantage
      captures (player height, oblique, aerial, shoreline, long traverse) through
      [Visual Verification](../../../tools/World/VisualVerification/README.md).
      The retained accepted evidence uses base revision
      `14cd7aead3a84c0371fcedc42a3ae7143802eeff` and source-state SHA-256
      `052aa85156f7ebde503b6af828ccb7acba3ae0e23ca591cf8569e33ac0da7327`.
      Development payload is 2,253,732,091 bytes. Three accepted packaged center-edge-return runs
      contain 45,674 samples with frame p95/p99 13.747/17.060 ms, game p95 5.088 ms, render p95
      13.747 ms, GPU p95 9.469 ms, peak process physical 3,394,748,416 bytes, peak GPU local
      3,676,110,848 bytes, accepted Landscape collision traversal, and zero streaming failures.
      Census and surface checks passed for all 210 terrain cells and 145 Water actors; nine
      1920x1080 captures were authenticated and inspected. An earlier 38,063-sample run at 17.333
      ms p95 was a superseded control and is not the current baseline.
- [x] Verify the fixed tuple, spatial-ownership value, adapter plugin/module split, runtime terrain
      tag, and shared-MPD path/version contract, then define the typed settings schema. Phase 1
      records the empirical section, tolerance, and build-policy values. The adapter is a
      content-capable plugin; ProjectWorld itself and concrete territory plugins are not valid
      shared-asset owners.
- [x] R1 independent review of the complete lifecycle before production code (PASS on 2026-09-24).

### Phase 1 - synthetic seam proof (L0 and L1, ProjectWorldTestData only)

- [x] Run Epic's `MeshPartition` CQTest categories once on the configured engine as an engine
      control before attributing any failure to ALIS code. The 5.8.3 installed plugin discovered
      and completed 66 tests: 32 passed and 34 failed. Failures were 31 golden-mesh vertex-count
      mismatches across Boolean, Remesh, Spline, and TexturePatch modifiers plus three preview-build
      timeouts. Treat this as a red engine baseline; ALIS acceptance depends on its exact supported
      base-modifier/builder/cook path and does not claim the entire experimental plugin is green.
- [ ] Before implementing the adapter, compile and link a minimal test-owned 5.8.3 seam probe that
      ODR-uses the exact supported runtime/editor APIs the adapter needs: `AMeshPartition`,
      `UMeshPartitionDefinition`, an instanced `FTransformer`/`UTransformerPipeline`, and
      `UMeshPartitionEditorComponent::SpawnBaseModifier`. Exercise compiled-section generation
      through the supported `WorldPartitionBuilderCommandlet` plus
      `WorldPartitionMeshPartitionBuilder` name, not by directly linking the builder's unexported
      callable methods. Prove the shared MPD can be configured through supported reflection/editor
      property APIs, saved, reloaded, and read back with the exact material, channel, priority,
      build-variant, transformer, and platform values. Build `AlisEditor` for editor-only seams and
      build `Alis`, `AlisClient`, and `AlisServer` for the runtime module/plugin dependency surface.
      The probe must call or instantiate across module boundaries so header-only visibility cannot
      pass a missing export/link seam. Record a known-bad non-exported/private direct-link control
      that the probe rejects, then remove it; that expected rejection does not trigger D8. Only an
      inaccessible operation required by the supported adapter/commandlet route enters D8 before
      adapter implementation. Do not infer viability from source inspection. `AlisEditor` and
      `Alis` compile/link and the commandlet route passes. `AlisClient` and `AlisServer` stop before
      project compilation because this launcher engine rejects those target types, so the four-target
      wording remains open even though the WindowsServer asset cook passes. This remains a Phase 4
      preflight gate and prevents an unqualified Phase 0/1 PASS.
- [x] Producer seam: the terrain producer is selected by generator ID.
- [x] Extend the existing authored-overlay schema and compiler before realization: red/green tests
      prove provider terrain -> base quantization -> Water solve -> hydro -> hand -> final
      quantization/validation order; additive height
      survives changed terrain and Water bases; an edit that breaks Water clearance fails closed;
      normalized canonical semantic surface overrides replace procedural weights;
      grid/provenance/overlap violations fail closed; and an overlay edit dirties only the derived
      previous/current cell union plus halo. Do not add a second overlay file family.
- [x] Adapter owner with the MeshPartition dependency; an Architecture-kind test proves every
      dependency is confined to the explicit reviewed adapter allowlist and rejects an injected
      include from ProjectWorld core, ProjectWorldData, or CanonicalCompilation.
- [x] From the synthetic terrain-and-water fixture, generate one shared adapter-owned MPD (material
      slot, one
      synthetic channel, a collision variant, a Nanite render variant, and a non-Nanite LOD
      variant, with section size bounded by an explicit producer-owned partition setting independent
      of runtime-grid selection), the
      schema-validated channel-layout receipt, `AMeshPartition`, and one base per canonical cell from
      exact final canonical surface samples with a named weight layer. Two fixture territories must
      reference the same MPD. A per-territory result fails unless the engine emits a recorded
      requirement that cannot be repaired at the shared adapter boundary.
- [x] Select fixed producer-owned section sizing from measured one-cell rewrite amplification,
      streaming, generation time, and package counts (E30). Select the initial LOD/subsection/skirt
      settings from achieved geometric error, cracks, fallback quality, and package cost (E31), not
      from an undocumented default.
- [x] Run the builder inside the transaction with rendering. Prove compiled sections, collision,
      and channel arrays exist, and prove a headless build is rejected.
- [x] Material invalidation probe: preserve the material object path, change only its package
      contents through the ProjectMaterial host, then run the real builder and cook reuse route and
      prove compiled-section and World package bytes are untouched. Change the material object path
      as a known-rebuilding control. If same-path contents rebuild, repair the adapter/identity split
      before accepting invariant 9; do not weaken locality in documentation.
- [x] Compiler-boundary probe: the World adapter emits its layout receipt; the ProjectMaterial host
      independently compiles against it; the integration gate rejects a hash/layout mismatch before
      promotion. An injected ProjectWorld -> ProjectMaterialEditor Build.cs dependency or API call
      must fail the architecture guard.
- [x] Prove invariants 5, 6, 8, 9, 10, and 12 on the fixture, including exact rollback and Delete,
      an engine-identity change forcing a rebuild, and a runtime-profile switch rewriting no
      terrain package. Prove invariant 11 with the chosen hand-layer mechanism.
- [x] Inventory every package the builder and data-layer container write, and assign each to one
      scope (invariant 10). Inventory the shared MPD separately under the adapter transaction and
      prove a forced mid-rollout failure restores compatible MPD and territory versions. The
      retained forced-failure receipt records exit 8 after the injected post-self-save fault and
      byte-identical before/after inventories for all 25 packages, including shared MPD SHA-256
      `711afd257508fec80583399de416c9cd89a52f1713c265ca9b84ebd9ff99c529`.
- [x] Prove save, reload, cook, and packaged load of the fixture map in Development; SM5 rendering
      of the fallback geometry within tolerance (invariant 14); and terrain collision in a
      WindowsServer cook. Record every runtime module and plugin MeshPartition adds to the Game,
      Client, and Server targets (Q1). The full WindowsServer cook completed 4,187 packages and
      stripped both render cells through `PlatformCellTransformer`, retaining the collision route.
      Development packaged load reached the exact fixture map without material-fallback or unknown
      transformer-struct warnings.
- [x] Prove how runtime consumers identify terrain on compiled sections without a MeshPartition
      dependency in the runtime ProjectWorld module (for example a project transformer that tags
      sections, if `FTransformer` is an exported extension point). The adapter transformer writes
      the stable `ProjectWorld.Terrain.v1` tag to every compiled section.
- [x] If a required operation is unreachable through exported API, record the exact operation and
      build the D8 workaround: an ALIS-owned C++ wrapper in a project plugin that overrides through
      Epic's extension points, never a copy of Epic source. Only an unresolved blocker after that
      returns to the operator (Q6). No required operation was unreachable; the adapter uses public
      extension points and the supported builder commandlet route.

### Phase 2 - complete Kazan non-production route (same repository)

- [x] Candidate profile: the same canonical inputs, runtime profile, and other layers; only the
      terrain producer differs. It now lives as the normal non-production
      `kazan_territory_mesh_development_v1` profile and MeshDevelopment map in ProjectWorldData.
      It is not menu-exposed and no production profile selects it.
- [x] Bind the smallest comparable material so the structural/package comparison isolates the
      substrate. Production texture selection remains owned by the material task.
- [x] Realize complete Kazan, build sections, package Development, and run it through the ordinary
      same-repository product route. Release-isolation evidence below remains historical; it is not
      the development workflow after D11. Final-source Shipping and the complete paired comparison
      remain Phase 3 gaps.

### Phase 3 - comparison and decision packet

- [ ] With the same source identity, hardware, settings, and route, compare: extents and sampled
      heights; seams, cracks, and LOD transitions; collision traversal; streaming load, unload,
      and failures; CPU, GPU, and frame percentiles; memory only through an existing trustworthy
      native metric (otherwise UNVERIFIED); generation and build time; section, package, and actor
      counts; terrain-owner cooked bytes and whole-package bytes; the size of the plugins and
      modules MeshPartition adds to game targets; dynamic navmesh generation over trimesh collision
      (the project generates navigation dynamically around invokers, `Config/DefaultEngine.ini`
      `[/Script/NavigationSystem.RecastNavMesh]`); channel storage (texel size, slices, bytes) as
      input for the material task; the unloaded-to-loaded VSM shadow sequence and any E32 workaround
      cost; and the Experimental API call sites ALIS depends on. The Landscape arm and the ordinary
      packaged Mesh correctness route are accepted. The fixed paired three-child performance gate
      accepts Mesh at 14.016 ms and Landscape at 14.841 ms pooled frame p95, so topology tuning is
      not admitted. Authenticated current Development and Shipping packages and ordinary D3D12
      product-route smokes pass. The strengthened runtime diagnostic proves 106 main-pass Nanite
      components, 106 non-main-pass VT helpers, collision, exact center height, and center-edge-
      center residency. The compiled audit matches 202,710 samples per render variant. Positive
      post-reload VSM rendering passes, and the Mesh section-owner package set occupies 18,490,057
      compressed stored bytes. Product dynamic navigation is not configured by the current
      `territory_product` realization path. A packaged multi-point runtime-height receipt, stale-
      shadow control, exact Landscape terrain-byte attribution, trustworthy memory evidence if an
      existing metric exists, one final exact-source paired performance/fixed-view packet, and
      fresh independent review remain open.
- [x] State which measured structural/package benefits the ALIS-owned per-cell Nanite StaticMesh
      route would also give (Alternatives considered). Runtime benefits are not claimed.
- [x] Present evidence and consequences without an invented weighted score, with an independent
      architect review of the packet. A failed invariant starts the workaround loop (D6, D8) and the
      affected Phase 1-3 steps repeat; it is not a rejection. The earlier ascent blocker was an
      authored building correctly hit by Pawn collision, not a Mesh Terrain defect. The packet
      remains PATCH and blocks Phase 4 until the remaining explicit Phase 3 gates and fresh
      independent review pass.

### Phase 4 - production cutover (after Phase 3 passes, possibly after workarounds; D6)

- [x] Freeze the pre-cutover generated authority and keep its exact rollback bytes until the
      operator accepts the result.
- [x] Stage the new shared adapter MPD/version and material authority without deleting the prior
      accepted assets. Regenerate all consuming territories against one layout receipt, promote only
      after the integration gate proves canonical contract, MPD, material, and territory identities
      agree, and retain prior bytes until the coordinated rollback drill passes.
- [x] Switch the four production profiles and the synthetic twin profile to the accepted tuple.
      Replace the top-level `landscape` block with the producer's typed settings and accept the
      contract's one-time regeneration of dependent layers.
- [x] Rename the representation-specific contracts in E12 in one pass, with no aliases, per the
      [public repo migration policy](../../../docs/agents/canonical.md#public-repo-migration-policy).
      Generated-actor tag names are hand-copied in about 13 files across both ProjectWorld modules;
      give them one owner, a tag header in the runtime module, inside this cutover, which
      regenerates everything anyway. Do not move the Landscape terrain check into a fingerprinted
      file as a stand-alone cleanup before the cutover: that alone would force terrain
      regeneration.
- [x] After a repository-wide consumer audit, remove `ProjectWorldLandscapeRealization.*`, layout
      admission, the 16-bit encoding path, edit-layer machinery, the `project_landscape:v1` tuple and
      fingerprint set, Landscape-internal tests (replaced by representation-neutral ones), and the
      ProceduralMesh terrain fallback if no consumer remains.
- [x] Update the release projection, playable-tour and Water proofs, the performance tournament,
      the static partition audit, semantic evidence, and the Visual Verification census for the new
      representation.
- [ ] Run the L2 gates, L3 enrollment only with explicit operator authorization, the L4 packaged
      proof for both territories, and R2 review.
- [x] Update stable documentation to current truth.

### Phase 5 - give-up path (only if an unresolved blocker survives the workarounds; D6, D8)

- [ ] Ask the operator (Q6) before giving up. Then remove every candidate addition that has no
      remaining purpose, leave production untouched, and record the blocker as current truth in the
      native reuse gate.

## Test-first and verification plan

### Red evidence

Each item is observed failing, or recorded, before production code.

| Case | Kind | Wrong behaviour captured today |
|---|---|---|
| R-1 tuple admission | Acceptance | A synthetic profile with `project_mesh_terrain:v1` is rejected as an unknown tuple before mutation. |
| R-2 producer selection | Permanent guard | A profile naming a non-Landscape terrain generator still runs the Landscape producer, because dispatch keys on layout compatibility. |
| R-3 final surface | Permanent guard | No verifier reads a compiled Mesh Terrain surface; flat and offset known-bad controls must fail the new verifier. |
| R-4 unchanged rerun | Permanent guard | A forced rebuild rewrites every section; the zero-write check must detect that churn before its pass is trusted. |
| R-5 locality | Permanent guard | One-cell and water-only changes need a bounded, reported rewrite set on the new representation. |
| R-6 hand layer | Permanent guard | The existing overlay lacks complete semantics. Tests must prove additive height after a changed base, normalized named surface replacement, grid/provenance/priority refusal, persistence, and exact derived dirty cells. |
| R-7 headless build | Permanent guard | A NullRHI build must be rejected for missing channel textures. |
| R-8 dependency boundary | Permanent guard (Architecture) | Fails if any first-party file outside the reviewed representation-adapter allowlist includes MeshPartition headers or names MeshPartition modules or plugins. A known-bad include in ProjectWorld core must fail it. |
| R-9 same-Kazan packet | Acceptance | The Landscape baseline is recorded before the candidate runs. |
| R-10 naming | Reviewer-checked | Representation-neutral contracts carry no representation-specific names. |
| R-11 runtime-profile independence | Permanent guard | A runtime-profile switch must rewrite no terrain package; a grid-split variant is the known-bad control. |
| R-12 SM5 surface | Permanent guard | The SM5-drawn fallback geometry must meet tolerance; the default 20 percent fallback is the known-bad control. |
| R-13 engine identity | Permanent guard | A changed engine build identity must force a section rebuild instead of a no-op. |
| R-14 canonical order | Permanent guard | Current code applies hand delta before ProjectWorld hydro. CanonicalCompilation tests must prove provider terrain -> base quantization -> Water solve -> hydro -> hand -> final quantization/validation and reject a clearance-breaking edit; the UE consumer test must fail if realization clamps again. |
| R-15 shared MPD | Permanent guard | Two fixture territories currently have no Mesh Terrain definition. The test requires one identical object identity, version, layout hash, and bytes; independently generated territory definitions are the known-bad control. |
| R-16 material invalidation | Acceptance | A same-path material content edit has no proven section-reuse result. The real builder/cook probe must record zero compiled-section/World writes; an object-path change must rebuild as the control. |
| R-17 compiler boundary | Permanent guard (Architecture) | Fails if ProjectWorld links or calls ProjectMaterialEditor, or if independently generated material/adapter receipts disagree. Injected Build.cs/API and layout-hash mismatches must fail it. |

Proposed exact UE automation names, fixed in R1 before code so the dev loop never uses a broad
filter:

- `Project.World.Realization.MeshTerrain.TupleAdmission`
- `Project.World.Realization.MeshTerrain.ProducerSelection`
- `Project.World.Realization.MeshTerrain.FinalSurface`
- `Project.World.Realization.MeshTerrain.IdempotentNoWrite`
- `Project.World.Realization.MeshTerrain.PackageLocality`
- `Project.World.Realization.MeshTerrain.HandLayerPersistence`
- `Project.World.Realization.MeshTerrain.HeadlessBuildRefusal`
- `Project.World.Realization.MeshTerrain.ArchitectureDependencyBoundary`
- `Project.World.Realization.MeshTerrain.RuntimeProfileIndependence`
- `Project.World.Realization.MeshTerrain.SM5FallbackSurface`
- `Project.World.Realization.MeshTerrain.EngineIdentity`
- `Project.World.Realization.MeshTerrain.NoSecondHydroWriter`
- `Project.World.Realization.MeshTerrain.SharedDefinition`
- `Project.World.Realization.MeshTerrain.MaterialInvalidation`
- `Project.World.Realization.MeshTerrain.MaterialCompilerBoundary`

R-9 remains a comparison receipt and R-10 remains reviewer-checked; they are not automation tests.

### Green evidence

- Each new test by exact full name through `scripts/ue/test/unit/iterate.ps1 -TestFilter <name>`;
  Pester files under `scripts/ue/world/test/` for harness and transaction changes; EndToEndValidation
  Python tests for validation changes; and CanonicalCompilation Python tests for the exact staged
  surface order, Water-clearance refusal, lineage, and dirty closure.
- Every guard listed under "Existing guards on the current representation" stays green, is
  rewritten against the representation-neutral invariant it protects, or is deleted together with
  the Landscape code it pins.
- L2 through `python -S tools/World/EndToEndValidation/bootstrap.py plan --base <base>` plus the
  selected Check and Matrix; L4 for both territories at cutover.

### Proof traceability

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1 | provider and canonical semantic authority | repository inputs | provider pre/post hashes, canonical schema diff, and slope-to-rock known-bad refusal | both territory receipts share source identity and a provenance-honest versioned canonical surface contract | provider drift, overclaimed semantics, duplicate vocabulary, or representation type in canonical contract |
| 2 | owner dependency graph | build/module graph | forbidden include/dependency census | clean Editor/Game/Client/Server builds | reverse or leaked dependency |
| 3 | adapter and compiler boundary | architecture/integration tests | R-8 known-bad core include plus R-17 Build.cs/API/layout mismatch controls | independent diff review, matching receipts, and target builds | MeshPartition outside allowlist or World -> ProjectMaterialEditor dependency |
| 4 | tuple/dispatch lifecycle | synthetic profile | R-1/R-2 refusal and selection | production Apply/Delete uses one tuple | layout-based dispatch remains |
| 5 | final canonical surface samples/edges | synthetic terrain-and-Water pair | R-14 exact order, clearance refusal, edges, and quantization tolerance | Kazan/Manhattan sampled final-surface proof | wrong order, second clamp, resample, or seam outside tolerance |
| 6 | cooked final-surface accessor | compiled fixture | R-3 flat/offset controls | packaged verifier on both territories | preview/input-only evidence |
| 7 | interpolation/dependent placement | synthetic anchors | recorded triangulation checks | road/building anchors on SM5 and SM6 | float/sink outside tolerance |
| 8 | identity/no-op and shared MPD ABI | repeated Apply plus adapter-definition audit | R-4/R-13/R-15 and name/index mismatch refusal | unchanged complete territory writes zero packages and both territories reference one shared MPD | churn, stale engine output, or territory-specific definition without proof |
| 9 | dirty and material locality | one-cell/water/hand/material fixtures | R-5 bounded rewrite set plus R-16 builder/cook controls | complete-territory locality and same-path tuning receipts | undeclared rewrite or same-path material churn |
| 10 | package ownership/rollback | interrupted territory and shared-adapter fixture | inventory plus mixed-version failure injection | coordinated MPD/territory production recovery drill before enrollment | foreign package or mixed MPD ABI after rollback |
| 11 | canonical authored overlay | compiler and authored fixture | R-6/R-14 base-change, order, normalization, clearance refusal, apply/delete/rollback/edit | complete-territory edit survives terrain/Water regeneration with exact derived dirty set | ambiguous order, lost/clamped edit, or unbounded dirtying |
| 12 | streaming/profile independence | centre-edge-centre route | R-11 known-bad grid split | packaged traversal and profile switch | always-loaded geometry or rewrite |
| 13 | collision/server | Development and Server | synthetic traversal/cook | packaged traversal plus server asset census | absent/incorrect collision |
| 14 | SM6/SM5 render variants | cooked fixture | R-12 20 percent fallback control | both territories render without fallback material | geometry/material mismatch |
| 15 | build envelope | commandlet | R-7 NullRHI refusal | rendering build creates channel textures | headless acceptance |
| 16 | engine integrity | source tree | engine status/diff | installed/source engine trees unchanged | vendored/edited Epic code |
| 17 | single substrate/no legacy | repository and product route | candidate-removal drill | post-cutover grep, asset audit, menu traversal | toggle, second path, legacy mention |
| 18 | fair comparison/performance/shadows | fixed E15 route | Landscape baseline and stale-shadow control | Phase 3 packet plus operator review | relaxed budget or stale VSM image |
| 19 | size | cooked IoStore/package | per-owner inventory tool | Phase 3 and final 2.0.0 comparison | source-folder estimate |
| 20 | neutral contracts | schemas/evidence/API | R-10 review census | repository grep and schema review | representation name above adapter |

## Documentation plan

- **Authoritative owners:** the CanonicalCompilation contracts and owner documentation (canonical
  terrain-surface vocabulary, hydro/hand order, overlay semantics, lineage, and dirty derivation);
  `world_partition.md` (native reuse gate decision and reason, topology, production realization
  invariants, representation policy); and `territory_contract.md` (tuple table, final canonical
  surface acceptance accessor, ProjectWorld consumption, and profile-execution hash contents).
- **Also:** `Plugins/World/ProjectWorld/docs/architecture/structure.md` (Composition section, routed
  from [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md))
  for the adapter black box and reviewed dependency allowlist; the adapter owner documents the
  one shared MPD, private channel ABI, layout receipt, build policy, and coordinated rollback; the
  native reuse gate's GeometryProcessing row at Phase 4 cutover;
  [Canonical World Realization](../../../scripts/ue/world/README.md) for
  renamed flags and the builder step; the adapter owner's README and its parent router if a new
  owner is created; `pitfalls.md` only for traps that actually cost diagnosis time (for example the
  silent headless channel loss).
- **Refuted statements:** the false World doc statements were corrected before closure; the
  durable owners now state the current generated-terrain architecture without a compatibility
  path (D5).
- **Duplication avoided:** docs route to these owners instead of restating them. Stable docs, code,
  tests, and configuration never link to this task.

## Rollout and rollback

- Phases 0-3 leave production bytes unchanged under lock, snapshot, and exact restore.
- The operator approved the switch (D6), but the native reuse gate text changes only at cutover.
  Until Phase 3 passes, Phase 1-3 code stays uncommitted local work or on a branch the operator
  chooses, the operator owns every commit, and nothing enables MeshPartition for game targets on the
  shared line.
- Phase 4 snapshots the accepted authority, stages the shared adapter MPD/material authority, then
  generates every territory under the new producer identity. The integration gate validates one ABI
  before coordinated promotion. Prior shared and territory bytes remain until all consumers pass the
  rollback drill and the operator accepts L4. Rollback restores through the owning transactions,
  never through destructive Git operations.

## Reviewer brief

For the R1 review before production code, and again for the Phase 3 packet. Review with the
`architect` skill and the review contract. D1-D10 bind: a finding that contradicts one names it and
argues with evidence. Judge against ALIS goals and standards: [VISION.md](../../../VISION.md)
(checkable real geography), the
[native reuse gate](../../../Plugins/World/ProjectWorld/docs/world_partition.md#unreal-native-reuse)
and representation policy, the [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md),
[World pipeline layers](../../../docs/testing/world_pipeline_layers.md) (proof layers, R1/R2),
[canonical.md](../../../docs/agents/canonical.md) (checkpoint scope, public repo migration policy,
file-size guardrail), and `AGENTS.md`.

Investigate carefully, on the web wherever the fact is Epic's. Cite primary sources: the
dev.epicgames.com docs, release and hotfix notes, Epic staff posts, and the installed engine source.

- **Maturity and direction:** current 5.8.x hotfixes, later primary UE6 statements if any, the
  roadmap card, any stated production-readiness target, UE-391544, and the VSM streaming issue.
  Absence from release notes is not proof of unchanged internals (E34).
- **Automation:** Epic guidance or samples for creating Mesh Terrain from C++ or commandlets; the
  builder's supported arguments and reuse modes; whether an unchanged rebuild writes zero packages;
  whether channel textures are identical across GPUs.
- **Section partition:** Epic's recommended section sizing for large worlds without coupling to the
  runtime grid, whether the subsection transformer can align to canonical cells, and whether the
  E30-E31 empirical selection is sufficient given Epic publishes no universal size.
- **SM5 and non-Nanite:** Epic's recommended fallback settings for terrain meshes, and whether one
  Windows cook can carry a non-Nanite variant for SM5.
- **Server cooks:** WindowsServer handling given the open TODO (E26), and whether dedicated servers
  need the render variants at all.
- **Hand layer (D7):** review the selected extension of the existing CanonicalCompilation authored
  overlay: additive-height behavior after base changes, normalized semantic surface replacement,
  provenance/grid binding, overlap priority/refusal, previous/current dirty-footprint derivation,
  and round-trip capture from Epic sculpt/paint without making Epic actor state authoritative. Prove
  the staged provider terrain -> base quantization -> Water solve -> hydro -> hand -> final
  quantization/validation order is cycle-free,
  locality-preserving, and leaves no second ProjectWorld hydro writer.
- **Canonical ownership and compiler handshake:** verify CanonicalCompilation owns semantic names,
  meanings, derivation, and overlay semantics; ProjectWorld only maps final canonical weights to
  Mesh Terrain; the adapter emits a plain-data layout receipt; and ProjectMaterial generation is an
  independent host operation with zero ProjectWorld -> ProjectMaterialEditor module/API dependency.
- **Semantic honesty:** verify every canonical surface name is no stronger than its derivation and
  provenance. A slope-only fixture must not produce factual `rock`; slope remains a continuous input
  and ProjectMaterial may independently choose a rock-like appearance. Apply the same rule to future
  physics, PCG, vegetation, and gameplay consumers.
- **Shared MPD and material invalidation:** verify one content-capable adapter plugin owns one MPD
  reused by both fixture territories; review its versioning and coordinated rollback. Run R-16 on
  the real builder/cook route. Do not infer material-content hashing from `GatherDependencies`: E41
  proves UE 5.8.3 records only the material path.
- **Custom wrapper feasibility (D8):** which MeshPartition classes and structs are exported and
  subclassable (`MinimalAPI`, `UE_API`), whether instanced `FTransformer` structs, modifier
  subclasses, and builder options are real extension points, and what a project plugin can override
  without copying Epic source.
- **5.8.3 delta:** E34 records the completed source diff and E18-E27 recheck. Review the remaining
  target-build export/link probe; do not repeat the completed source census without contrary
  evidence.
- **Comparison isolation:** verify A8 reuses the release projection's frozen-commit worktree owner
  without creating a second cleanup lifecycle, and proves forced interruption cannot touch live
  generated content.
- **Collision and navigation:** trimesh against heightfield cost at territory scale; dynamic navmesh
  over trimesh collision.
- **Better paths to weigh, not assume:** the ALIS-owned per-cell Nanite StaticMesh route
  (Alternatives considered); any Epic converter or heightfield import that preserves exact samples.

Return `PASS`, `PATCH`, `BLOCKER`, or `UNVERIFIED`; state each finding as evidence, consequence,
and smallest fix, with sources linked.

## Completion criteria

This task reaches `PASS` in one of two ways.

**Migration accepted:**

- Phase 3 passed (after any workarounds), and invariants 1-20 hold with exact evidence.
- ProjectWorld owns one Mesh Terrain producer; Kazan and Manhattan (private and public profiles)
  regenerate through it, consume one canonical terrain-surface contract, and reference one shared
  adapter-owned MPD.
- The comparison harness additions, candidate artifacts, and superseded Landscape code are removed.
- L4 packaged proof and operator visual acceptance pass; stable docs state the current
  architecture; the final architect review is `PASS`.

**Given up (only if an unresolved blocker survives every workaround, after Q6):**

- The evidence records the blocker and the workarounds tried.
- Every candidate addition is removed, production authority is unchanged, and no stable doc claims
  adoption.
- The rejection is returned to the 3.0.0 release router for the operator's scope decision.

## Review record

### 2026-09-23 - initial investigation

- **Trigger:** the operator selected Mesh Terrain as the serious 3.0 substrate candidate and asked
  for a same-Kazan comparison before migration, with no coupling to existing stubs.
- **Root cause:** a strategic capability gap, not a defect (see Capability gap).
- **Fix:** this plan - an evidence-first same-canonical comparison, then one clean producer cutover
  only after Q1.
- **Verification:** read-only census of the repository, the installed 5.8.1 MeshPartition source,
  and Epic documentation. Commands included `git status`, `git ls-files --eol`, `du -sb`,
  and grep and read passes. No build, test, or generation ran.
- **Authority:** D1-D4 recorded; Q1-Q4 opened; A1-A7 active.

### 2026-09-23 - independent architect review (PATCH)

- **Trigger:** the operator asked for `/architect` review; a fresh non-author reviewer returned
  `PATCH` and spot-checked the E-facts against the tree and the installed engine.
- **Root cause:** the first draft assumed one MeshPartition dependent, left the section partition
  and the builder's map writes unassigned, missed SM5's fallback geometry and engine identity, and
  gave the authored channel a home the contract forbids.
- **Fix:** two allowed dependents (invariant 3, R-8); a runtime-profile-independent section
  partition (Decision, invariant 12, R-11); builder map writes assigned to one scope (invariant 10);
  engine identity and channel inputs in producer identity (invariant 8, R-13); SM5 fallback geometry
  held to tolerance (invariant 14, R-12); a fuller Q1; the per-cell Nanite StaticMesh alternative;
  navmesh measurement; branch policy before Q1; corrected citations in E20 and E27. The reviewer's
  factual claims were rechecked in the engine source before being applied.
- **Authority:** Q5 added; A6 rejected by evidence (decision moved to Q5).

### 2026-09-23 - operator direction and reviewer brief

- **Trigger:** "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions", and "ensure that all
  finding are in todos and if some has questions or maybe better pathes - propose reviewer to
  investigate it carefully via web and our goals and standarts".
- **Fix:** invariant 17 now forbids legacy mentions after cutover; `## Reviewer brief` added; the
  refuted World doc statements and the unused manifest stub moved to the backlog task that owns them.
- **Authority:** D5 added; Q1 clause (a) closed by D5.

### 2026-09-23 - operator answers to the open gates

- **Trigger:** the operator answered every open question in plain words (switch definitely, the
  hand layer is mandatory, workarounds before giving up, update to 5.8.3 now, bigger is fine).
- **Fix:** the goal now states the switch; a "Hand layer" design section with three candidate
  mechanisms; invariant 11 and R-6 require the hand layer; Phase 3 failures start the workaround
  loop; Phase 5 became the give-up path; the reviewer brief gained the hand-layer, wrapper
  feasibility, and 5.8.3 delta investigations. The source engine update to `5.8.3-release` was
  started on the operator's instruction (logs under `tmp/engine/source_update_5.8.3/`).
- **Authority:** D6-D10 added; Q1 closed by D6, Q2 by D9, Q3 by D8, Q4 by D10, Q5 by D7; Q6 opened;
  A4 resolved by D9; A5 resolved by D10.

### 2026-09-23 - investigation closure after R1

- **Trigger:** the operator requested a complete review picture and clean implementation steps,
  without implementation.
- **Finding:** Epic publishes no universal section size; it exposes error-driven LOD and subsection
  controls that must be measured. The VSM streaming defect needs an explicit acceptance case. Public
  API docs make custom transformers plausible but do not prove 5.8.3 export/link viability. The
  existing in-place tournament has the same lock-plus-`finally` interruption gap as release
  projection, and the task lacked the required one-to-one proof table.
- **Fix:** E30-E37, the 5.8.3 hard gate, empirical section/LOD tasks, shadow-streaming proof,
  exported-wrapper compile probe, A7 rejection/A8 isolated host, and the 20-row proof matrix.
- **Verification:** repository census and Epic primary documentation only. The source-engine status
  did not end `DONE`, so the source engine was not read or modified and E18-E27 remain pending
  target-version recheck. No build, generation, package, index, commit, or external write occurred.
- **Authority:** A8 added; A7 rejected by evidence; D1-D10 unchanged. This authoring pass is not the
  required independent R1.

### 2026-09-24 - reviewer boundary corrections (PATCH applied)

- **Trigger:** the operator asked to evaluate the reviewer's four architecture findings against the
  current repository and apply the valid corrections.
- **Finding:** all four outcomes were valid, but two suggested phrasings needed repository-specific
  correction. ProjectWorldData owns more than real-place facts, and canonical coordinates - not a
  second hand-authored per-cell index - remain terrain-edit authority. The task also encoded the
  current two MeshPartition adapters as a permanent count and left MPD policy territory-specific.
- **Fix:** ProjectWorld owns one terrain-surface semantic contract; the World adapter owns one
  reusable MPD/channel ABI/build recipe with private indices; MeshPartition use is enforced by a
  reviewed adapter allowlist; the hand layer extends the existing CanonicalCompilation authored
  overlay with explicit additive-height, normalized-surface, provenance, overlap, and rebase
  semantics.
- **Verification:** the repository owner map, territory contract, authored-overlay schema,
  `terrain.py`, `lineage.py`, current fixtures/tests, launcher 5.8.3, exact source tag
  `5.8.3-release`, the full 5.8.1-to-5.8.3 MeshPartition diff, and Epic's current Mesh Terrain,
  MPD, material, and PCG documentation were read. The source delta leaves the reviewed definition
  and channel APIs unchanged. No build, generation, package, or production mutation ran.
- **Authority:** D1-D10 are unchanged; the corrections implement D3, D4, D5, and D7. This authoring
  pass is not the required independent R1.

### 2026-09-24 - reviewer canonical-boundary and ordering corrections (PATCH applied)

- **Trigger:** the operator supplied the next reviewer PATCH and requested code-backed evaluation,
  plan fixes, and explicit refutation of unsupported claims.
- **Finding:** canonical authored overrides named a downstream ProjectWorld vocabulary; World was
  tasked with calling ProjectMaterialEditor; territory-local MPDs remained allowed by default;
  same-path material locality lacked a builder/cook proof; and "raw/hydro-conditioned" hid the
  actual hand-before-hydro implementation order.
- **Fix:** CanonicalCompilation now owns the engine-independent terrain-surface contract and target
  order `provider -> base quantization -> Water solve -> hydro -> hand -> final quantization/validation`;
  ProjectWorld consumes the final
  projection and publishes a plain-data adapter layout; the ProjectMaterial host compiles
  independently; one content-capable adapter plugin owns one shared MPD; R-14 to R-17 prove order,
  MPD reuse, material invalidation, and the compiler boundary; coordinated rollback prevents mixed
  MPD ABI consumers.
- **Verification:** the canonical pipeline and overlay application, current ProjectWorld hydro
  implementation and caller, owner documentation, ProjectWorld descriptor/Build.cs files, UE 5.8.3
  MeshPartition source, and Epic's MPD/material documentation were read. Epic supports MPD reuse.
  UE 5.8.3 hashes the material path rather than its contents in `GatherDependencies`, so that
  reviewer implication was refuted while its requested builder/cook probe was retained. No build,
  generation, package, or production mutation ran.
- **Authority:** D1-D10 are unchanged. The exact order implements D7; failure to preserve its
  semantics is an R1 blocker rather than permission to move the hand layer back below hydro.

### 2026-09-24 - planning revision accepted for R1 (PASS)

- **Trigger:** reviewer recheck passed the architecture packet and directed the next work to fresh
  independent R1 rather than another speculative design cycle.
- **Finding:** the boundary and proof plan are coherent. The remaining semantic risk is naming a
  canonical class more strongly than its derivation proves, especially treating slope as geological
  evidence for `rock`.
- **Fix:** invariant 1, its proof row, and the R1 brief now require provenance-honest canonical names
  and a slope-to-`rock` known-bad refusal. The material task owns the concrete Q1/T1 vocabulary work;
  no new subsystem, task, or architecture branch was added.
- **Verification:** the correction was checked against canonical ownership, the adapter boundary,
  and the existing proof matrix. No build, generation, package, or production mutation ran.
- **Authority:** D1-D10 are unchanged. The packet is ready for fresh independent R1; no further
  architecture redesign precedes that review.

### 2026-09-24 - fresh independent R1 (PATCH applied; recheck pending)

- **Finding:** an exact-commit detached worktree would omit the uncommitted Phase 1 candidate, while
  the operator has forbidden staging and commits. The linked release-projection contract also
  intentionally refuses uncommitted inputs.
- **Fix:** A8 and Phase 0 now require an authenticated base-HEAD plus candidate-overlay receipt,
  including tracked binary patch bytes, untracked path/byte inventory, aggregate identity, exact
  materialization, and omitted/extra-file refusal. It remains disposable comparison evidence and
  does not weaken the release route's exact-commit contract.
- **Finding:** E33 correctly called the 5.8.3 extension seams plausible rather than proven, but no
  explicit implementation step forced the needed runtime/editor symbols through the ALIS target
  linker before adapter work.
- **Fix:** Phase 1 now starts with an ODR-using compile/link probe across `AlisEditor`, `Alis`,
  `AlisClient`, and `AlisServer`, plus a known-bad inaccessible-symbol control. A failed required
  seam enters D8 before adapter implementation.
- **Authority:** D1-D10 and the ownership architecture are unchanged. The same independent reviewer
  must recheck these bounded corrections before production code.

### 2026-09-24 - fresh independent R1 first recheck (PATCH applied; final recheck pending)

- **Finding:** several promises still assigned empirically measured names, partition values, and
  precision to R1, and the seam probe incorrectly treated direct linking to the unexported builder
  methods as a required extension seam.
- **Fix:** the plan now fixes the concern owner and stable identities directly:
  `ProjectWorldMeshTerrain`, its runtime/editor module split, `project_mesh_terrain:v1`,
  `compiled_sections_from_canonical_cells`, `ProjectWorld.Terrain.v1`, and the shared MPD path.
  R1 reviews section/tolerance selection rules; Phase 1 measurements fix their values. The seam
  probe invokes the supported builder commandlet route, treats rejected direct builder linkage as
  the expected control, and adds supported MPD configure/save/reload/readback proof.
- **Authority:** D1-D10 remain unchanged. No direct call to an unexported Epic builder method is a
  requirement, and no such expected failure authorizes a wrapper.

### 2026-09-24 - fresh independent R1 final recheck (PASS)

- **Verdict:** PASS. The independent reviewer found no remaining required planning defect after the
  bounded corrections.
- **Verified:** the canonical stage graph is acyclic by design; semantic names cannot overclaim
  slope as rock; one shared adapter-owned MPD is the default; material locality remains a measured
  builder/cook claim; ProjectWorld has zero ProjectMaterialEditor dependency; the seam probe uses
  supported commandlet generation and MPD round-trip proof; and the candidate overlay authenticates
  uncommitted comparison source.
- **Deferred evidence:** shared-MPD reuse, same-path no-write behavior, compiler locality, and the
  5.8.3 link/export seams remain Phase 0/1 execution gates, not facts inferred by R1.
- **Authority:** D1-D10 are unchanged. Phase 0/1 implementation is authorized; Phase 4 remains
  outside the current run.

### 2026-09-24 - Phase 0/1 execution candidate ready for R2

- **Canonical contract:** the compiler now stages provider terrain -> Water -> hydro conformance ->
  authored height/surface corrections -> final quantization and validation. The 84-test
  CanonicalCompilation suite passes under the required CPython 3.14 host. Slope stays continuous;
  the only canonical surface roles are `ground` and `hydro_transition`.
- **Adapter:** `ProjectWorldMeshTerrain` owns the runtime/editor split, one shared MPD, the
  `project_mesh_terrain:v1` producer, two private named-weight channels, one base per canonical cell,
  collision/Nanite/fallback variants, and the stable `ProjectWorld.Terrain.v1` compiled-section tag.
  The build pipelines are editor-only, so cooked runtime packages contain no editor transformer
  structs.
- **Measured result:** 2 canonical cells produce 6 compiled sections. The final audit at
  `Saved/Validation/WorldRealization/mesh-terrain-phase1/final-editoronly-audit.json` accepts all
  sections and reports 0 cm fallback error against 5 cm. The final unchanged builder log at
  `Saved/Validation/WorldRealization/mesh-terrain-phase1/builder-final-complete-reuse.log` records
  Built 0, Reused 6, Failed 0, Removed 0.
- **Locality and recovery:** one-cell terrain mutation rebuilds only its base and three variants;
  unchanged Apply/build is a semantic no-op; engine/build-policy changes rebuild; forced failure and
  Delete restore exact bytes. The final runtime-profile locality receipt is
  `Saved/Validation/WorldRealization/runtime-profile-locality/f83eb1156e6a4ec8878511e3751680b3/summary.json`.
- **Material boundary:** the adapter emits a schema-validated layout receipt and ProjectMaterial
  independently compiles it. The architecture test rejects any ProjectWorld MeshPartition or
  ProjectMaterialEditor dependency. Same-path material content change performs zero World writes;
  iterative cook rewrites the shared MPD to identical bytes, so this is not claimed as zero MPD
  writes. The path-change control rebuilds.
- **Cook/runtime:** Development package v4 completed in 115-120 seconds and the exact fixture map
  loaded from `tmp/world/mesh_terrain_phase1/package_development_v4` without the earlier unknown
  transformer-struct or Nanite material-usage warnings. The full WindowsServer cook completed 4,187
  packages and selected collision-only cells. Client/Server executable builds are still unavailable
  in the launcher engine and fail before project compilation.
- **Verification:** final exact C++ guards passed for architecture, export seam, shared definition,
  producer selection, layout receipt, canonical fail-closed loading, presentation profile, and the
  substrate-neutral bounded-static-route profile. Data/schema validation, the 7-test generator
  fingerprint suite, AlisEditor build, Alis Development build/package, packaged load, SM5 fallback
  audit, and WindowsServer cook pass. No Git index, commit, remote, durable production manifest, or
  Phase 4 cutover was changed.
- **Open for R2:** the four-target seam checklist cannot be fully executed on this engine
  distribution, and the separate Phase 0 complete-comparison-host proof plus production Landscape
  Shipping baseline remain open. R2 must decide whether those are prerequisites for Phase 2/3 or
  documented environment/baseline limitations.

### 2026-09-24 - independent R2 final recheck (PASS)

- **Verdict:** PASS. Phase 2 may run only in the authenticated isolated comparison workspace;
  Phase 4 remains outside this run.
- **Verified:** the shared MPD guard and retained build-policy digest cover material and physical
  material defaults, channel and UV policy, collision policy, explicit Nanite/fallback mapping,
  platform variants, and runtime-grid independence. The retained receipt has build-policy SHA-256
  `54da3e54d755fc8842b858abf8109a7813a12f4b009bf1a199e7253e29d6d9e0`, receipt SHA-256
  `7f8a0e644d6b47c4c2f85b418a72534220be25f876dd93f186c38519bd9bd14e`, adapter SHA-256
  `07cd3499f4c087bb668897eea8f40692cf1fafd415d7d81afa743594a6201793`, and unchanged shared MPD
  SHA-256 `711afd257508fec80583399de416c9cd89a52f1713c265ca9b84ebd9ff99c529`.
- **Verification:** `AlisEditor` builds; the exact SharedDefinition filter passes 2/2 tests; the
  independent ProjectMaterial layout-receipt filter passes 1/1; the forced-failure inventory is
  byte-identical across 25 packages. Overlay v3 differs from v2 in exactly the four reviewed files,
  and those files match the live tree and materialized workspace byte-for-byte.
- **Isolated authority:** overlay receipt SHA-256
  `f66c3cabe7b5eee735448fbdd6b1b5fe1f76b6060000438f6c2755287da7af93`, source identity
  `f77687bf12b88ec076c2eab98eb8fb35435069df702fdeb6d7859cfa7ecb406c`. The workspace receipt is
  retained at `Saved/Validation/WorldRealization/mesh-terrain-comparison/phase2-workspace.json` and
  one foreground PowerShell process owns the workspace lifetime.
- **Deferred gates:** launcher-distribution Client/Server executables, production Landscape
  terrain-owner bytes/collision/Shipping, and the full same-source Kazan comparison belong to
  Phase 2/3 evidence; they are not Phase 2 entry blockers.

### 2026-09-25 - Phase 2/3 controlled execution packet (PATCH)

- **Verdict:** PATCH. Phase 2 structural and Development package evidence is accepted; the Phase 3
  packaged Mesh runtime arm rejects before center collision, so Phase 4 is blocked.
- **Final isolated source:** base commit
  `14cd7aead3a84c0371fcedc42a3ae7143802eeff`, overlay identity
  `cc9d6f5fc03ff40b84fe5b890c2af404f0c60d9c88a3315af2cb46c2e13b18b0`.
  The workspace receipt was retained and cleanup confirms the workspace was removed.
- **Structural/package proof:** AlisEditor build, complete-Kazan realization, the 16-scope authority
  audit, Development package, archive integrity, and IoStore policy pass. The audit verifies 2,817
  artifacts totaling 166,363,302 bytes and enumerates 318 sections, 106 per platform variant. The
  count is observed, not enforced, because the transaction audit used
  `ExpectedSectionsPerVariant=0`. The final Mesh Development payload is 2,249,885,511 bytes.
- **Landscape control:** three packaged center-edge-return runs are accepted with 45,674 samples,
  pooled frame p95 13.747 ms, and zero streaming failures. This replaces the stale 17.333 ms result.
- **Runtime blocker:** the final packaged Mesh gate reports World Partition enabled and one explicit
  153,600 cm center source, then rejects after 36.926 seconds with
  `runtime_gate_phase_timeout`. No tagged center section loads and no center collision is proved.
  The current receipt cannot distinguish a trace miss from an untagged hit, so the root cause is
  unverified. Product-route performance, memory, navigation, VSM, height, and visual evidence did
  not run and remain unverified.
- **Comparison limitation:** runtime-harness corrections changed the Mesh overlay identity after the
  Landscape arm, so the incomplete arms do not satisfy the one-source-identity comparison rule.
- **Material decision:** T1 is complete but remains PATCH for exact texture provenance and a
  machine-readable ProjectTexture public-asset sidecar. T3-T5 remain gated by this task.
- **Authority:** production profiles and generated authority are untouched. No staging, commit,
  remote write, or Phase 4 cutover occurred.

### 2026-09-25 - independent final R2 review (PATCH; Phase 4 BLOCKER)

- **Accepted architecture:** CanonicalCompilation's terrain -> Water -> hydro -> authored hand
  sequence is acyclic; `ground` and `hydro_transition` remain provenance-honest; slope-to-`rock`
  rejects; the generator-ID seam contains MeshPartition inside the adapter; one shared MPD, private
  channel indices, the plain-data receipt, and zero ProjectWorld -> ProjectMaterialEditor dependency
  remain sound. No architecture rewrite is warranted.
- **Scope correction:** the final overlay includes a narrow SourceIngestion filter correction needed
  for reference-complete OSM boundary output. It is now declared as an admitted prerequisite rather
  than falsely listed as untouched. Its direct regression is included in the 38/38 targeted Python
  result; provider-neutral output semantics and provider payloads remain unchanged.
- **Bounded Phase 0/1 verdict:** PATCH. E12 consumer classification and launcher-blocked Client and
  Server compile/link seams remain explicit Phase 4 preflight gates. The isolated seam proof is
  usable, but an unqualified Phase 0/1 PASS is not claimed.
- **Phase 2 verdict:** PATCH. Build, realization, authority, Development package, IoStore inspection,
  and cleanup are accepted. The 106 sections per variant are observed because the audit disabled an
  expected-count assertion; a derived expected count plus wrong-count control remains required. The
  final packet also lacks authenticated final-source rollback/no-op receipts, so those claims were
  narrowed.
- **Evidence correction:** Kazan loads `MI_ProjectTerrain_Default`. The SM5 sampler overflow belongs
  to legacy City17 `M_Landscape` and is not evidence against the generated Kazan control.
- **Phase 3 verdict:** PATCH and proven no-go for cutover, not a completed comparison. The Mesh gate
  rejects before center load/collision, omits the final trace/tag distinction, and the two arms have
  different source identities. Instrument the probe outcome, diagnose the load/tag/collision path,
  and rerun both arms from one authenticated identity before another decision review.
- **Material T1 verdict:** PATCH. Its architecture is accepted. Exact texture revision/hash/license/
  lineage and ProjectTexture sidecar authority remain production gates; T3-T5 stay gated.
- **Authority:** Phase 4 is BLOCKED and remains unauthorized. No production authority, Git index,
  commit, or remote state changed.

### 2026-09-25 - same-repository MeshDevelopment execution (PATCH; correctness accepted)

- **Workflow correction:** D11 is applied. The stale comparison worktree was removed through the
  project helper. One normal repository now owns a non-production Mesh realization profile/map and
  a non-menu `KazanMeshDevelopment` experience. Production Kazan and Manhattan profiles are
  byte-unchanged.
- **Package proof:** Development v7 at source-state
  `acef6dda46f0a63e60d58c3e15e1fce40c993190ac34d331909ff62478972193` passed editor/game build,
  full cook, archive integrity, IoStore policy, and post-package source equality. Payload:
  2,327,612,286 bytes across 53 files.
- **Product correctness:** the normal menu -> ProjectLoading route accepted the exact Mesh map,
  production game mode/pawn, grounded movement, terrain/road/building collision, gameplay
  interaction, and center -> edge -> center unload/reload. The previous runtime timeout is closed.
- **Performance blocker:** two deterministic runs stopped initial real-input ascent at about 4.9 m.
  The v7 diagnostic proved flying mode, enabled movement input, consumed Up input, near-zero final
  velocity, and 486.8 cm rise. A v8 forward/up real-input workaround also stopped at 470.8 cm and
  was removed. Current inference is a blocking Mesh collision surface; exact actor/component/
  triangle provenance remains unverified and is the next investigation boundary.
- **Decision:** keep the adapter and development profile. Do not cut over production. Dynamic nav,
  full-tour performance/residency, VSM reload, compiled height fidelity, fixed-view parity, and
  exact terrain-owner bytes remain open behind the collision diagnosis.
- **Authority:** no staging, commit, push, production-profile change, or Phase 4 action occurred.

### 2026-09-25 - first-blocking placement correction and complete tour (PATCH)

- **Reviewer finding:** valid in direction, but its collision conclusion was only a hypothesis. The
  real-character capsule sweep identified a generated building, not a Mesh compiled section, as the
  blocker above the buried start. The earlier 470-487 cm ascent measurements did not establish
  collision ownership by themselves.
- **Placement fix:** removed the terrain-only repeated-trace/actor-ignore path. Product-route ground
  placement now uses the first Pawn-channel blocker and keeps `ProbeTerrainCollision()` as the
  independent terrain proof. A bounded eight-direction endpoint check preserves grounded movement
  when the safe first blocker is a roof.
- **Profile fix:** `landscape` is conditional on `project_landscape` terrain. The MeshDevelopment
  profile no longer carries fake `logical_landscape_id` or `components_per_proxy` values, and the
  loader rejects Landscape identity on non-Landscape terrain profiles.
- **Focused proof:**
  `Project.World.Presentation.ProductRouteGroundPlacementPolicy`,
  `Project.World.Realization.Layers.MeshProfileOmitsLandscape`, and the existing Landscape
  `Project.World.Realization.Layers.ProfileContract` pass. Inline schema validation passes all 329
  documents, and the editor build passes.
- **Packaged proof:** Development v11 passes BuildCookRun, archive/IoStore verification, the normal
  ProjectLoading correctness route, and the complete playable-tour behavior. The tour reaches four
  waypoints, completes ascent/descent/collision/input/streaming phases, unloads and reloads 23 center
  cells, and records zero streaming failures.
- **Remaining blocker:** the 11,493-sample performance receipt rejects at 20.483 ms frame p95 against
  the unchanged 16.670 ms budget. Render p95 is 20.461 ms; game p95 is 9.931 ms and GPU p95 is
  9.469 ms. Installed UE 5.8 source proves cook removes MeshPartition source modifier actors and
  components. The next bounded investigation owns runtime compiled-section/render-thread cost; this
  evidence does not authorize changing the shared MPD, precise collision variant, or v1 section and
  channel constants.
- **Temporary content:** the full MeshDevelopment twin remains required Phase 3 evidence. Remove it
  only after an approved production cutover has regenerated the normal production roots. City17 is
  authored Landscape and remains outside this cleanup.
- **Authority:** Phase 4 remains blocked. Production profiles, generated production authority, Git
  index, commits, remotes, and Epic source are unchanged.

### 2026-09-25 - current-source package, render, and ownership closure (PATCH)

- **Topology decision:** closed. The fixed paired three-child gate records Mesh at 14.016 ms pooled
  frame p95 across 46,152 samples and Landscape at 14.841 ms across 44,752 samples, with zero
  streaming failures. Both pass 16.670 ms. Keep `MaxSectionComplexity=2048`; do not run Insights,
  try 4096, or split sections to the World Partition runtime grid without a new reproducible
  regression and explicit evidence.
- **Authenticated packages:** Development and Shipping were built from revision
  `050af7a9679cc99dd3d6772c1e253cd5923ac392` and source-state
  `bbc12c12b7266e1c62ee455702e82b07a8cef4a6f3b274a14caaa0ff1b0fb0e2`. Development is
  2,335,879,434 bytes across 53 files; Shipping is 2,116,918,557 bytes across 38 files. Both package
  gates and ordinary D3D12 MeshDevelopment product-route smokes accept collision, interaction, and
  center -> edge -> center streaming.
- **Render/material diagnosis:** the authenticated runtime gate observes 212 compiled sections,
  composed of 106 main-pass Nanite components and 106 non-main-pass virtual-texture helpers. It
  observes collision and Nanite variants and exactly one effective chain:
  `MI_ProjectTerrain_Default -> M_ProjectTerrain`. A Landscape-map negative control rejects with
  zero compiled/renderable Mesh sections. The current blue material is intentionally minimal; it
  is not evidence of missing Mesh geometry or default-material fallback.
- **Height and VSM:** the source-side compiled audit matches all 202,710 expected canonical samples
  in each Nanite/fallback surface with zero mismatches. Packaged center collision matches Z 8566 cm.
  The Development route renders 30 frames after center reload; its screenshot is stable against the
  prior accepted capture (SSIM 0.999867, PSNR 57.456). Packaged multi-point height and a deliberately
  stale-shadow control are still required for the strongest runtime/VSM claims.
- **Cooked-byte boundary:** 24 MeshDevelopment section-owner packages contain all 212 compiled-
  section exports; their 36 IoStore files occupy 18,490,057 compressed stored bytes. The candidate
  Landscape set occupies 14,386,071 bytes but is co-packed with 404 generated static-mesh actors/
  components. It is not a terrain-only measurement, and physical Landscape terrain bytes remain
  UNVERIFIED until ownership can be isolated.
- **Navigation blocker:** `DefaultEngine.ini` selects dynamic generation around navigation
  invokers, but `ProjectWorldRuntimeRealization.cpp` returns from the `territory_product` branch
  after PlayerStart setup. Only the bounded-route branch creates the NavMeshBoundsVolume,
  NavigationInvoker, Recast state, and path query. This is a real neutral ProjectWorld product-
  realization gap, not a proved MeshPartition-adapter defect. Epic's collision transformer has a
  true-by-default `bCanEverAffectNavigation` setting and applies it to the dedicated collision
  component, while the non-main-pass fallback helper opts out. Phase 4 cannot start until the shared
  MPD value and both Landscape and Mesh product routes prove the resulting navigation contract in a
  package.
- **Reviewer corrections:** the 2048-fragmentation theory is refuted by the accepted paired frame
  gate; 4096 is therefore not admitted. The 212 component count does not mean two visible terrain
  surfaces. Exact material-object equality is also the wrong runtime contract because Epic wraps
  the configured material in per-section MICs; the effective parent chain is the verified contract.
  Legacy City17 sampler failures are unrelated to the Kazan Mesh/Landscape pair.
- **Reviewable receipts:** paired performance is under
  `tmp/world/mesh_terrain_performance/baseline-current/comparison-evidence/`; current package
  summaries and product routes are under
  `tmp/world/mesh_terrain_performance/material-current/`; authenticated runtime and height receipts
  are under `Saved/Validation/WorldRealization/mesh-terrain-runtime-current-050af/`; IoStore describe
  and list inputs are under `tmp/world/mesh_terrain_performance/material-current/`.
- **Remaining verdict:** PATCH. Product dynamic navigation, packaged multi-point height, a VSM
  negative control, exact Landscape terrain-byte attribution (or explicit UNVERIFIED disposition),
  final exact-source paired performance/fixed-view evidence, and fresh independent review remain.
  No worktree, framework, subsystem, production cutover, stage, commit, push, or Epic-source edit
  was made.
