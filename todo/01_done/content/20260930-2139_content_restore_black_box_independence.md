# Restore Black-Box Independence Between Texture, Material, and World

**Status:** DONE - PASS (2026-10-01). Implemented and independently reviewed; the operator applied
the world-engineering skill change, which matches the reviewed copy byte for byte
**Scope:** cross-owner generation identity between ProjectTexture, ProjectMaterial, the Mesh Terrain
layout receipt, and ProjectWorld; the permanent tests that prove each owner's invalidation locality;
the engineering rule and testing policy that enforce it; the material task's paused T11 cleanup
corrections land in the same sweep (D10)
**Stable documentation owner:** [Architecture principles](../../../docs/architecture/principles.md)
(the cross-owner rule), [World pipeline test layers](../../../docs/testing/world_pipeline_layers.md)
(proof selection), [world-engineering skill](../../../.claude/skills/world-engineering/SKILL.md)
(workflow), [Surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)
and [Pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md) (owner
identities)

Agent route: read this file, then only the owner sections it cites. Reviewers start with
`## Decision` and `## Mutation matrix`.

## Contents

- Goal - Authority register - Non-goals
- Verified evidence: dependency graph, mutation matrix, runtime boundary, architecture document and
  diagram audit, refuted
- Current architecture - Problem and root cause - Decision - Required invariants
- Implementation tasks - Test-first and verification plan - Documentation plan
- Completion criteria - Review record

## Goal

An internal ProjectTexture or ProjectMaterial change recomputes at most its own owner's artifacts.
A consumer regenerates only when a producer's public contract changes. A provenance change refreshes
records and acceptance evidence, never another owner's outputs. Texture and material work never needs
a World regeneration, and permanent cheap tests plus the engineering workflow enforce all of this.

## Authority register

Operational access authorizations are execution context and are not recorded here.

### Operator decisions

- **D2** This correction runs before release integration and other material work; the material
  task's closure is paused.
  - Effect: the material task's T11 waits (A4).
  - Reason: "we need more urgently stuff".
  - Date/source: 2026-09-30, operator request.
- **D3** During investigation, and in this change: no World Apply, no Kazan or Manhattan
  regeneration, no cook, no package, no production enrollment, and no release-authority change.
  - Effect: evidence comes from owner tests, isolated fixtures, existing generated World content,
    and read-only audits.
  - Reason: not stated.
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D4** Target principle: "A black box may recompute because its implementation changed. Its
  consumers invalidate only when its public result/contract changed. Provenance changes invalidate
  acceptance, not unrelated production outputs." Three identities are kept apart: owner/action
  identity (what makes the owner itself recompute), public contract identity (what a consumer can
  observe), and provenance/release identity (exact bytes, source digests, compiler fingerprints,
  release manifests; for integrity, reproducibility, distribution, and drift detection).
  - Effect: governs `## Decision`, the mutation matrix, and the rule text.
  - Reason: stated as information hiding plus result-based propagation.
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D5** KISS: keep owner-local invalidation conservative. Do not classify every owner source file
  as output-producing or not now; the fix is the projection firewall between black boxes. No general
  incremental build framework, new build system, service, dependency engine, compatibility layer,
  or alternate generator; derive the contract projection from existing manifests and identities.
  - Effect: owner compiler fingerprints stay as they are. It supersedes the earlier supplied
    instruction, the same day, to narrow fingerprints to output-producing sources; that remains a
    possible later owner-local optimization.
  - Reason: "Five ProjectMaterial/Texture assets occasionally regenerating is irrelevant. 2,000
    World assets regenerating because five assets were reconsidered is the problem."
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D6** Workflow rule: every black box is developed and tested independently by default; cross-box
  testing happens only when a public contract changes or a focused test finds an integration
  failure. Default proof order: owner unit tests, isolated representative fixture, existing World
  integration only when necessary, targeted Development runtime if needed, release candidate. An
  expensive operation must name the changed dependency that makes cheaper evidence insufficient. An
  unexpected need to regenerate World for an internal texture or material change is an architectural
  investigation trigger, not permission. The world-engineering skill stays small and gains one rule;
  detailed procedure lives in `world_pipeline_layers.md` or the owner contract.
  - Effect: `## Documentation plan`.
  - Reason: documentation alone has not prevented the coupling.
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D7** A permanent mutation matrix, one cheap fixture per row, is the executable architecture. An
  effect outside a row's expected scope is ARCHITECTURAL RED. Only RED rows are refactor targets; no
  general abstraction for rows already green. Referential safety is kept: World must still consume
  the correct material interface, and cleanup must not delete World-referenced assets.
  - Effect: `## Mutation matrix` and `## Implementation tasks`.
  - Reason: stable boundary guarantees are proved once by permanent tests, not by rebuilding cities.
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D8** The investigation answers eight questions (dependency graph with the field on each edge;
  edge classification; false transitive edges; a cheap runtime boundary proof; the mutation matrix
  against current code; the skills and SOTs; the smallest coherent cleanup; nothing expensive),
  audits every architecture document and diagram, and returns to the operator's reviewer before
  implementation. The finished change gets an independent architectural review.
  - Effect: this todo's structure and `## Completion criteria`.
  - Reason: cut only the bad edges instead of another large refactor.
  - Date/source: 2026-09-30, reviewed direction text the operator supplied.
- **D9** Q1: P1 runs as the very first implementation action, before any firewall source change,
  as a characterization gate: existing compiled Kazan, a transient unsaved change to the current
  terrain material through the normal material interface, no new Mesh Terrain diagnostic adapter,
  and no World Apply, save, cook, or package. It must show the rendered terrain changing, zero
  World dirty packages, zero World on-disk byte changes, and restoration on discard; a transiently
  dirty ProjectMaterial package is acceptable. If it fails, stop and report the violated boundary;
  do not implement the firewall.
  - Effect: closes Q1; task T0.
  - Reason: a characterization gate for the premise, not a late acceptance test.
  - Date/source: 2026-09-30, reviewed decision text the operator supplied with the instruction to
    adopt it after verification.
- **D10** Q2: the material task's T11 lands in this sweep, after the firewall guards are green and
  before one combined production refresh. That refresh may regenerate the one pattern and the
  four surfaces because their own owner implementations changed; World stays at zero. The
  Texture-to-Material locality proof comes only from G1 and G2, never from that refresh.
  - Effect: closes Q2; replaces the earlier plan's "pattern untouched" expectation.
  - Reason: one owner-local refresh instead of two.
  - Date/source: 2026-09-30, same supplied text.
- **D12** Sweep boundary: no build system, dependency engine, cache framework, compatibility
  layer, second authority, or new generic abstraction; owner compiler fingerprints stay
  conservative; the existing pending-restore protection stays without new foreign-directory
  knowledge, and projection locking stays with the projection-recovery task; no work on the 45
  developer-payload rejections; only affected focused tests, the combined owner refresh, and
  read-only audits; no staging, commit, or push. When independent review returns PASS, update
  both todos to the final evidence and stop.
  - Effect: `## Implementation tasks`, `## Non-goals`, `## Completion criteria`.
  - Reason: the 45 rejections are release ownership work.
  - Date/source: 2026-09-30, same supplied text.

- **D13** Texture and Material are independent black boxes while their public contract is
  unchanged. Contract changes propagate only to actual consumers. Internal changes never justify
  full World regeneration.
  - Supersedes: D1 and D11, whose combined meaning this states cleanly.
  - Effect: the Goal and every task read this entry in place of D1 and D11.
  - Reason: D1 read as absolute; D11 narrowed it with supersession prose.
  - Date/source: 2026-09-30, reviewed decision text the operator supplied with the GO.

### No longer binding

- **D1** [SUPERSEDED by D13] Texture and material development are fully separate black boxes. A
  texture or material change applies into the existing material and the existing World in one
  stage, without a full-regeneration proof. Operator's words: "texture and material dev should be
  fully separate and no need regeneration full prove, since we could apply it into existing
  material - like just use one stage".
  - Effect: defines the Goal; World regeneration is not an admissible proof for internal texture or
    material work.
  - Reason: "in code architecture and our domain driven we are using black box pattern".
  - Date/source: 2026-09-30, operator request.
- **D11** [SUPERSEDED by D13] D1's separation holds under an unchanged public contract: a changed
  texture ABI, material path or binding, or channel ABI still propagates to its consumers.
  - Supersedes: D1 in part (its unconditional reading); the one-stage clause stands.
  - Effect: D1 reads with this scope; D4 and D7 already implied it.
  - Reason: a contract change must still reach its consumer.
  - Date/source: 2026-09-30, same supplied text.

### Operator gates

- **Q1 [CLOSED by D9]:** Run the runtime substitution probe (P1) before implementation, or
  as the first implementation step? It needs the rendered Editor on existing compiled Kazan terrain
  with a transient, unsaved material parameter change; the investigation phase is read-only. -
  default while open: A5. - answered by D9: first, as a characterization gate.
- **Q2 [CLOSED by D10]:** Should the paused material T11 cleanup corrections land after this
  firewall so both share one owner-local regeneration? - default while open: A4. - answered by
  D10: yes, in this sweep.

### Working assumptions

- **A1 [ACTIVE]:** `(pattern_id, output_object_path, output_contract)` is a sufficient public
  pattern contract, because the producer's `Verify` enforces the contract's render-target
  properties (`ProjectTextureRuntimeAssetBuilder.cpp:341-357`: RGBA8, linear, wrap addressing,
  mips). A change to those semantics must publish a new `output_contract`. - Verified further:
  `Verify` also checks the contract name, slot, mip addressing, node formats, and RGB-only
  generators (`:329-375`), but no test proves a rejection (G5). Channel meaning is not
  machine-checkable and rests on the versioning rule.
- **A2 [ACTIVE]:** `layout_id`, `layout_version`, and `layout_sha256` are the complete channel
  interface ProjectMaterial consumes. `layout_sha256` hashes exactly the channel ABI payload
  (`ProjectWorldMeshTerrainLayoutReceipt.cpp:47-52`), and the builder reads only the semantic
  channel map (`ProjectMaterialSurfaceBuilder.cpp:275-279`).
- **A3 [ACTIVE]:** Package saves are not assumed byte-deterministic, and the pattern catalog embeds
  its own semantic identity, so an owner recompute may change exact bytes. After the firewall that is
  a provenance change only.
- **A4 [RESOLVED by D10]:** T11 of the material task runs after this change; the firewall changes the
  material identity formula once, so one owner-local regeneration serves both.
- **A5 [RESOLVED by D9]:** P1 runs as implementation step T1. E25a and T9 already evidence the boundary
  (see Runtime boundary), so the plan does not depend on it.
- **A6 [ACTIVE]:** The material generation request can point at a ProjectTexture test-mount
  authority, since it takes explicit pattern manifest and content paths
  (`ProjectMaterialSurfaceGenerationTests.cpp:65-67`). Its allowed-root validation is unverified
  for a texture test root.

## Non-goals

- Narrowing or reclassifying owner compiler fingerprints, and removing schemas from them (D5). The
  ProjectMaterial pitfall "Schema edit leaves the compiler fingerprint stale" stays true.
- Any dependency engine, build system, service, or framework (D5).
- World realization receipts, World generator fingerprints, the World authority audit, and the
  active set: all are GREEN (graph edges W1-W4).
- Parent-to-instance identity inside ProjectMaterial: owner-local, instances compile no shaders.
- The runtime ProjectTexture module: runtime pixels and cache entries are disposable.
- Mesh Terrain adapter internals, canonical compilation, and geographic dirty units.
- The 45 developer-payload rejections and the audit exporter's registry-gather fix (3.0.0 plan,
  D12).

## Verified evidence

### Dependency graph (questions 1-3)

Class key: **owner** = owner/action identity; **contract** = public contract; **ref** = runtime
reference by path; **prov** = validation/provenance; **release** = release-only.

| Id | Edge | Field or mechanism | Code | Class | Verdict |
|---|---|---|---|---|---|
| T1 | texture sources -> pattern identity | `compiler_fingerprint`: every non-test `.cpp`/`.h` of ProjectTextureEditor, both schemas, `Build.cs` | `ProjectTextureEditor.Build.cs` `GetCompilerFingerprintFiles`; `ProjectTexturePatternRecipe.cpp:320-330` | owner | accepted (D5) |
| T2 | pattern recipe -> pattern identity | `recipe_sha256`, family, algorithm, compiler version, output contract, output path, engine Major.Minor | `ProjectTexturePatternRecipe.cpp:320-330` | owner | accepted |
| T3 | pattern identity -> catalog bytes | `Catalog->SemanticIdentity`, also the runtime cache key | `ProjectTextureRuntimeAssetBuilder.cpp:275`; `ProjectTextureRuntimeSubsystem.cpp:231,273` | owner | GREEN (owner-internal; A3) |
| M1 | pattern manifest -> material generation | resolves `pattern_id`; requires `surface_structure_rgb_v1`; authenticates package bytes against `package_sha256` | `ProjectMaterialPatternAuthority.cpp:119-167` | contract + prov | GREEN (keep) |
| M2 | pattern -> material identity | `pattern_semantic_identity` (includes T1) and `pattern_package_sha256` inside `ComputeArtifactSemanticIdentity` | `ProjectMaterialSurfaceRecipe.cpp:439-467` | prov used as a generation key | **RED: false transitive edge** |
| M3 | pattern -> material bytes | the builder uses only `OutputObjectPath` and `OutputContract` | `ProjectMaterialSurfaceBuilder.cpp:360-381,559` | contract | GREEN |
| M4 | layout receipt -> terrain material identity | `terrain_layout_receipt_sha256` = `receipt_sha256` over the canonical surface contract hash, adapter compiler fingerprint, engine identity with changelist, `layout_sha256`, MPD package hash, build policy hash | `ProjectMaterialTerrainLayoutContract.cpp:98-188`; `ProjectWorldMeshTerrainLayoutReceipt.cpp:47-69,133-147` | prov used as a generation key | **RED: false transitive edge** |
| M5 | layout -> terrain material bytes | the semantic channel map only | `ProjectMaterialSurfaceBuilder.cpp:275-279` | contract | GREEN |
| M6 | parent -> instance identity | `dependency_package_sha256` | `ProjectMaterialSurfaceRecipe.cpp:461-462` | owner (same owner) | accepted (D5) |
| M7 | material sources -> material identity | `compiler_fingerprint`: every non-test `.cpp`/`.h` of ProjectMaterialEditor, both schemas, `Build.cs` | `ProjectMaterialEditor.Build.cs` | owner | accepted (D5) |
| W1 | material manifest -> World presentation binding | reads `terrain.default`; authenticates instance and parent bytes; loads the instance; checks its parent path | `ProjectWorldPresentationMaterialBinding.cpp:69-193` | prov (precondition) | GREEN |
| W2 | material hashes -> World realization result | `terrain_material_package_sha256`, `_semantic_identity`, `_manifest_sha256` written to the result JSON; no script, tool, or dirty input reads them | `ProjectWorldRealizationService.cpp:700-703`; repository search | prov (receipt) | GREEN |
| W3 | material object path -> World packages and compiled sections | 428 external-actor packages reference `MI_ProjectTerrain_Default` by path; `UMeshPartitionDefinition::GatherDependencies` adds only the material path | material task E11, E25; `MeshPartitionDefinition.cpp:81-115` | ref | GREEN, with a one-off proof only (E25a) |
| W4 | material path -> Mesh Terrain build policy | the build-policy payload carries the MPD's material path | `ProjectWorldMeshTerrainLayoutReceipt.cpp:54-69` | ref | GREEN |
| R1 | owner manifests -> developer payload | `recipe_source_sha256`, `package_sha256`, and every recorded `*_object_path` / `*_package_sha256` pair must match selected packages | `scripts/git/mirror/compose_developer_payload.py:256-312` | release | GREEN; keeps provenance honest after the firewall |
| X1 | material -> runtime texture | samples the render target by object path; the runtime subsystem fills it from the catalog | `ProjectTextureRuntimeSubsystem.cpp` | ref | GREEN |

Observed chain from T10 (commit `5ec561b38`), with only manifest, recipe-digest, and service edits in both
owners: the pattern changed `compiler_fingerprint`, `semantic_identity`, and `package_sha256`. All 4
surfaces changed `compiler_fingerprint`, `pattern_semantic_identity`, `pattern_package_sha256`,
`semantic_identity`, and `package_sha256`, and both instances changed `dependency_package_sha256`
(`tmp/material/t10/migration/` against the committed manifests). Both owners changed in that round,
so it cannot attribute the material rebuilds to ProjectTexture alone; M2 proves the texture-only path
from code, and T1 below makes it executable.

World-side evidence: same-input Kazan and Manhattan applies after the RGB-contract material
regeneration had zero dirty units and zero actor changes, and the read-only World authority audit
accepted 3,096 unchanged artifacts after T10's material regeneration (material task Current state
and T10 record).

### Mutation matrix (question 5)

| Row | Mutation | Expected scope | Current | Evidence | Permanent proof |
|---|---|---|---|---|---|
| X1 | recipe whitespace, BOM, or CRLF | manifest provenance refresh only | GREEN | both owners' host case "refreshes only the manifest after a formatting-only recipe edit" | existing |
| X2 | validation, manifest, or orchestration code in one owner | that owner may recompute (D5); consumers untouched | GREEN (was RED via M2) | G1 | G1 |
| X3 | texture algorithm or recipe change, same contract | pattern regenerates; material provenance refresh with 0 rebuilds and 0 compiles; no World effect | GREEN (was RED via M2) | G1, G2 | G1, G2 |
| X4 | texture contract change (`output_contract` or object path) | pattern and dependent materials | GREEN (must stay) | M2 keeps path and contract | G1 control |
| X5 | material instance scalar | that instance only; 0 compiles; 0 World writes | GREEN | G3; G4; P1 | G3, G4, P1 |
| X6 | material graph change, same binding ABI | that parent and its instances; 0 World writes | GREEN | surface host case: parent edit gives generated 2, skipped 2; W3 | existing, G4 |
| X7 | material object path or binding ABI | material, World binding, MPD sections | GREEN | W1 reads the path; E25a path control | G4 control |
| X8 | terrain channel layout change | Mesh Terrain and terrain-archetype materials only | GREEN (must stay) | `layout_sha256` feeds the receipt; object recipes have no layout | G1 control |
| X9 | Mesh Terrain adapter code, MPD bytes, engine changelist, or build policy; same layout | terrain materials untouched | GREEN (was RED via M4) | G1 | G1 |
| X10 | geographic or canonical change | affected World units only | owned by World | [World pipeline test layers](../../../docs/testing/world_pipeline_layers.md) planner | not re-proven here |
| X11 | release provenance (exact bytes, source digest) | release validation and manifest refresh only | GREEN (was RED via M2) | R1; G2 | payload suite |

### Runtime boundary (question 4)

- **E25a (earlier round, builder/cook):** changing material contents at the same object path
  reused every affected compiled section and preserved World package hashes; changing the path
  rebuilt them.
- **T9 (earlier round, packaged Development):** 48/96/192 m and 64/96/128 m material scalars
  applied on loaded compiled Mesh Terrain sections changed the image as expected, with no World
  regeneration. The probe's Mesh Terrain adapter was removed afterwards.
- Neither is a permanent test. P1 runs first in the implementation as a characterization gate
  (D9), and G4 is the permanent cheap guard.

### Architecture document and diagram audit (question 6)

| Owner | State observed during investigation | Consistent? | Change |
|---|---|---|---|
| [principles.md](../../../docs/architecture/principles.md) Component Ownership | black boxes; "Depend on a public contract, never a neighbor's private implementation" | yes; M2 and M4 violate it | add one corollary bullet: the three identities (D4) |
| [world_pipeline_layers.md](../../../docs/testing/world_pipeline_layers.md) | lowest layer first; higher gate only when the changed dependency can alter what it proves; "A material or validation-only edit must not invalidate geography"; "World fingerprints should identify byte- or manifest-producing behavior" with a known-nonproducing locality proof | yes, but scoped to World fingerprints; silent on consumer identity and generated resource owners | extend Impact-Driven Development: the propagation rule, owner mutation matrices as the locality proof, ARCHITECTURAL RED on unexpected cross-owner regeneration, and the D6 proof order mapped onto L0-L4 |
| [world-engineering SKILL.md](../../../.claude/skills/world-engineering/SKILL.md) | CHANGED/UNTOUCHED; ARCHITECTURAL RED when an UNTOUCHED owner must be edited; cheapest proof; rerun only on invalidation | mostly; its trigger omits generated resources consumed by World, and RED covers editing, not regenerating | description covers them; one rule line (D6); RED also covers an UNTOUCHED owner's outputs needing regeneration |
| [territory_contract.md](../../../Plugins/World/ProjectWorld/docs/territory_contract.md) "Ownership is distinct from consumption" | ProjectMaterial artifacts consumed by semantic soft identity; consumer references separate | yes | none |
| [material_generation.md](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md) | Contract: depends on the `surface_structure_rgb_v1` ABI, not the ProjectTexture algorithm; Semantic identity: binds the pattern's semantic identity, package hash, and layout receipt | **self-contradictory** (M2, M4) | Semantic identity names the public contract terms and lists the rest as provenance with manifest-only refresh |
| [pattern_generation.md](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md) Identity | consumers resolve a stable pattern ID through the manifest | yes | add: consumers bind path and output contract; a contract semantic change publishes a new `output_contract` |
| [ProjectMaterial README](../../../Plugins/Resources/ProjectMaterial/README.md) flow | pattern manifest + layout receipt + recipes -> compiler | yes | none |
| [ProjectWorldMeshTerrain README](../../../Plugins/World/ProjectWorldMeshTerrain/README.md) | adapter fingerprint excludes diagnostics | yes | none; the consumer side owns the directed dependency |
| [ProjectWorld structure](../../../Plugins/World/ProjectWorld/docs/architecture/structure.md) | ProjectWorld never depends on ProjectMaterialEditor | yes | none |
| [ProjectWorld main view](../../../Plugins/World/ProjectWorld/docs/architecture/diagrams/main.md):21 | `ProjectMaterial authority -->|authenticated saved material identity| Adapter` | imprecise under D4 | label the consumed contract (material object path) and bytes as authenticated provenance |
| [Repository main view](../../../docs/architecture/diagrams/main.md) | project-level owners only | yes | none |
| Schemas: `surface-manifest`, `pattern-manifest`, `mesh-terrain-layout-receipt` | field shapes, no identity prose | yes | none; no field is added or removed |
| [canonical.md section 7](../../../docs/agents/canonical.md) | gate budget and checkpoint scope | yes | none; routes to world_pipeline_layers |

### Refuted

- "The World skill has no rule for testing black boxes independently": both the skill and
  `world_pipeline_layers.md` already require the cheapest proof and a changed-dependency reason for
  a higher gate. The real gaps are narrower (audit table): no consumer-identity rule, no locality
  test for the resource owners, and a skill trigger that misses them.
- "Narrowing compiler fingerprints fixes cross-box regeneration": the cross-box edges are M2 and M4
  (identity composition). A narrowed texture fingerprint still leaves X3 and X9 RED.
- "Material changes need World regeneration": W1-W4 are GREEN; World pins no material bytes or
  identity.
- Confirmed, not refuted: ProjectMaterial includes a ProjectTexture package hash in its generation
  identity (M2), and depends on the whole layout receipt where only the channel contract matters
  (M4).

## Current architecture and source of truth

ProjectTexture compiles pattern recipes into a persistent catalog and render-target descriptor whose
pixels are produced at runtime ([ProjectTexture README](../../../Plugins/Resources/ProjectTexture/README.md)).
ProjectMaterial resolves a stable pattern ID through ProjectTexture's accepted manifest, consumes the
Mesh Terrain layout receipt for terrain recipes, and compiles surface parents and instances
([Surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)).
ProjectWorld binds the terrain instance by data and authenticates its bytes; Mesh Terrain references
the material by path ([ProjectWorld main view](../../../Plugins/World/ProjectWorld/docs/architecture/diagrams/main.md)).
The developer payload authenticates every recorded dependency at release
([mirror README](../../../scripts/git/mirror/README.md)). Each owner skips an output only when its
semantic identity and exact package bytes match the accepted manifest, and it rewrites the manifest
whenever serialized record bytes would change.

## Problem and root cause

ProjectMaterial's generation identity composes its producers' provenance instead of their public
contracts. It hashes the pattern's semantic identity, which carries ProjectTexture's compiler
fingerprint, and the pattern's exact package bytes (M2). It also hashes the full layout receipt,
which carries the Mesh Terrain adapter fingerprint, engine changelist, and MPD bytes (M4). So any
texture code edit, texture recompute, adapter code edit, or MPD resave re-stales every dependent
material, although the builder reads only the pattern path and contract and the channel map. Owner
conservatism is not the defect (D5); the missing piece is the projection firewall. On the process
side, the rules already exist, but nothing tests the resource owners' locality, the skill does not
trigger for them, and locality was repeatedly re-proved with expensive World applies instead of a
permanent cheap guard.

## Decision

1. **Pattern firewall.** ProjectMaterial's semantic identity binds `pattern_id`,
   `pattern_object_path`, and `pattern_output_contract`, and stops binding
   `pattern_semantic_identity` and `pattern_package_sha256`. Both stay in the record as provenance,
   authenticated by `FProjectMaterialPatternAuthority` before generation and by the payload composer
   at release.
2. **Layout firewall.** Terrain identity binds the layout interface (`layout_id`, `layout_version`,
   `layout_sha256`) instead of `receipt_sha256`. The receipt payload stays verified, and
   `terrain_layout_receipt_sha256` stays recorded as provenance.
3. **Provenance refresh.** When a producer's provenance changes, the next `Regenerate` skips every
   asset and rewrites only the manifest (the existing save-on-changed-bytes rule); `Validate`
   reports the stale manifest until then.
4. **Closed pattern ABI.** `output_contract` stays the whole consumer contract: the producer's
   `Verify` rejects render-target state the contract does not admit, and G5 makes that a
   permanent known-bad test. A material-relevant property the check misses is fixed in the
   contract and its check, never by putting producer identity back into ProjectMaterial. Channel
   meaning (which signal is in R, G, B) is not machine-checkable; changing it publishes a new
   `output_contract`.
5. **Owner identities unchanged (D5).** The firewall changes the material identity formula once,
   and T11 changes both owners' implementations (D10), so one combined owner-local refresh
   regenerates the pattern (1) and the surfaces (4, 2 shader compiles); World stays at zero. G1
   and G2, not this refresh, prove that ProjectTexture no longer drives ProjectMaterial.
6. **Permanent guards (D7):** G1-G5 below, after the P1 characterization gate (D9).
7. **Rule and workflow:** per `## Documentation plan`.

### Premise / KISS gate

- **Owner:** ProjectMaterial's identity function already owns what a material depends on; the
  manifests already carry every contract and provenance field. No new field, file, service, or
  framework is added.
- **Removed:** two provenance terms from the pattern part of the identity, and one receipt term
  replaced by its interface subset. Gone with them: cross-owner regeneration and the habit of
  re-proving locality with World applies.
- **Knowingly given up:** a material asset is no longer automatically rebuilt when its pattern's
  bytes change under an unchanged contract. That rebuild never changed material bytes (M3); the
  exact-byte check moves wholly to authentication (M1, R1).

### Alternatives considered

- **Narrow both owner fingerprints to output-producing files** (the Mesh Terrain precedent): does not
  cut M2 or M4, and D5 defers it.
- **A separate `contract_sha256` field on the pattern manifest:** a second representation of path
  plus contract; A1 makes the existing fields sufficient. Revisit only if the contract gains
  properties that are not in `output_contract`.
- **Result comparison after regeneration (red-green per artifact):** the contract projection already
  is the compared result. Hashing rendered pixels would add an instrument with no consumer.

## Required invariants

1. Pattern bytes are authenticated against the accepted ProjectTexture manifest before any material
   generation.
2. The payload still requires every recorded pattern and parent package hash to equal the selected
   package's accepted hash.
3. The layout receipt payload is still verified, and channel semantics are unchanged.
4. A pattern output-contract or object-path change still invalidates dependent materials.
5. A channel-layout change still invalidates terrain-archetype materials only.
6. Owner-local identities are unchanged, and a skip still needs both identity and exact package
   bytes to match.
7. `Validate` rejects stale provenance, and `Regenerate` refreshes it without asset writes.
8. No World package is written, and the read-only World authority audit accepts unchanged.
9. Material contents are never a World dirty input; the material path is.
10. Orphan referential safety is unchanged.
11. `output_contract` is a closed consumer ABI: producer `Verify` rejects contract-incompatible
    output (G5), and the consumer rejects any other contract name.
12. A required orphan deletion that fails to resolve, stay confined, or delete rejects the run
    and saves no new manifest; an already-absent file is success; a failed rollback keeps journal
    and snapshot together for the next recovery (material task T11).

## Implementation tasks

- [x] **T0 - P1 characterization gate (D9):** rendered Editor (`UnrealEditor.exe` launched from
      PowerShell with the Kazan map) with compiled terrain sections loaded in view; record the
      loaded section identities. Change one terrain-instance scalar through the material instance
      interface without saving; capture before, after, and after discard from one fixed camera.
      Census dirty packages: the changed ProjectMaterial instance must appear (the instrument's
      control) and no World package may. Hash every World package on disk before and after (a
      guard against an accidental save). Stop on failure; if the Editor cannot show loaded
      compiled sections, report UNVERIFIED instead of changing envelope (D3). - Passed; see
      Results.
- [x] **T1 - guards G1-G5:** G1 red for X2, X3, and X9 with green controls; G2 red (surfaces
      regenerate); G3, G4, and G5 characterization. G2 needs A6 verified, otherwise the smallest
      test-fixture plumbing and no production API. - A6 failed only on the hard-coded
      `/ProjectTexture/` prefix; the plumbing is an admitted pattern package root plus a test-only
      `-patterntestroot`. G5 was red on filtering (see Results).
- [x] **T2 - firewall:** change `ComputeArtifactSemanticIdentity` and its callers; expose
      `layout_id`, `layout_version`, and `layout_sha256` from the layout contract. Record fields
      and schemas are unchanged. G1 and G2 turn green. - Done, plus trilinear filtering in
      the producer's `Verify`.
- [x] **T3 - material task T11 (D10):** fail-closed orphan deletion in both owners; the host keeps
      journal and snapshot together after a failed rollback and reports the original rejection;
      retained and deleted disposition in the existing receipts; the retained-orphan lifecycle in
      both owner docs; the pending-restore guard unchanged (D12). Red-first cases per that task.
- [x] **T4 - combined owner-local refresh:** pattern regenerate (1), surfaces regenerate (4, 2
      compiles), immediate reruns with 0 generated and 0 compiles, both `Validate` accept, the
      payload focused suite, and the read-only World authority audit with every artifact
      unchanged. No World Apply.
- [x] **T5 - documentation, skill, and fresh-agent reach:** per `## Documentation plan`; confirm
      `.agents/skills` is still the junction to `.claude/skills` and AGENTS.md still routes to
      principles.md and world_pipeline_layers.md; add a router line only if that check fails.
      - The operator applied the skill edit (version 1.1). Routing and the junction were
        confirmed; no router change.
- [x] **T6 - review and close:** final diff; independent architectural review of false
      transitive identities removed, legitimate contract propagation preserved, provenance still
      authenticated, no World dirty dependency introduced, T11 rollback and delete correctness, no
      new framework, and fresh-agent routing to the rule. On PASS update both todos to the final
      evidence and stop (D12). - Review PATCH (G5 missed V and U-mip addressing; the skill patch
      narrowed D6), corrected, then PASS; the material task is done.

## Test-first and verification plan

### Red evidence

| Guard | Kind | Case | Wrong behavior today, or expected result |
|---|---|---|---|
| P1 runtime characterization (T0) | acceptance, one-off, before any source change | T0 | expected: the image changes; the ProjectMaterial instance is dirty (control); 0 World dirty packages; 0 World files changed; discard restores. A failure stops the sweep |
| G1 `Project.Material.Generation.SurfaceIdentityFirewall` (new exact C++ test) | permanent regression guard | same pattern path and contract with a different pattern semantic identity and package hash; terrain identity from synthetic receipts differing only in adapter fingerprint, MPD hash, engine identity, or build policy, with valid recomputed receipt payloads | identities differ (X2, X3, X9) |
| G1 controls | permanent | changed output contract or object path; changed `layout_sha256` | must differ today and after |
| G2 texture-then-surface L1 pair (service test mounts) | acceptance | regenerate the pattern with a changed seed and the same contract in the ProjectTexture test mount, then regenerate surfaces against it | surfaces regenerate. Required after: pattern regenerated; surfaces generated 0, compiles 0; all four surface package hashes byte-identical; surface semantic identities unchanged; provenance refresh only; `Validate` accepts; no World path touched |
| G3 instance-only edit (existing surface host test) | characterization | edit one instance scalar | expected green: generated 1, compiles 0, parent untouched |
| G4 `Project.World.MeshTerrain.MaterialDependencyIsPathOnly` (new exact C++ test in ProjectWorldMeshTerrainEditor) | permanent characterization guard | record `UMeshPartitionDefinition::GatherDependencies` for the shared MPD | expected green: the material enters only as its object path, never as a package or content dependency; a transient scalar change leaves the record identical; a transient path change alters it. No rendering; P1 owns that |
| G5 `Project.Texture.Generation.OutputContractClosed` (new exact C++ test) | permanent characterization guard | producer `Verify` on a render target breaking one consumer-visible condition at a time (color space, format, U and V addressing, U and V mip addressing, mip generation, filter, mip filter, slot, contract name) under the unchanged `surface_structure_rgb_v1` name; the consumer's contract-name rejection is `Project.Material.Generation.PatternAuthority` | each rejected; the red run, where `Verify` lacked the filter checks, failed exactly those two cases |

### Results (2026-09-30)

- **P1 (T0):** three unattended Editor sessions on Kazan through the same driver and settle
  timing (`tmp/material/blackbox/p1/`). With `HydroDarkening` 0.24 -> 1.0 on the terrain
  instance, the low oblique changed only at hydro-transition rims: 1,818 pixels by more than 24
  levels (restored session: 0), 97.9% of them darker. The top-down view also carried sky and
  cloud differences in the changed session, so the terrain conclusion rests on the oblique. Dirty delta: the instance only; World newly dirty: none; control: a
  touched Kazan external-actor package does register as dirty. 3,293 package files unchanged;
  nothing saved. Two findings: pattern-driven parameters are invisible in an Editor world because
  runtime pattern pixels are generated only in Game, PIE, and game-preview worlds (a first run
  with `MacroStrength` showed no change); and `ProjectWorld.CaptureEvidence` can capture before
  Mesh Terrain sections render (a first baseline showed no terrain; a later run of the capture
  route alone reproduced it with an accepted receipt, `tmp/material/blackbox/capture_readiness/`).
  Both traps are recorded in their owners. The capture readiness fix belongs to the World capture
  route, is tracked as the prototype `20261001-1003_world_fix_capture_readiness.md`, and is not
  part of this change.
- **Red (pre-firewall build):** G1 failed only its two provenance assertions; G5 failed only
  filter and mip filter; host G2 failed with `Accepted surface is stale` on the parent that
  samples the pattern; the T11 deletion case accepted the run. G3 and G4 passed.
- **Green (final build):** 19 exact tests 1/1 each (all `Project.Material.*`, all
  `Project.Texture.*`, and G4); host tests 9/9 surface and 4/4 pattern, including G2 with 0
  generated, 0 compiles, byte-identical surfaces, unchanged identities, refreshed pattern
  provenance, and `Validate` rejecting then accepting.
- **T4:** pattern generated 1; surfaces generated 4 with 2 compiles; reruns 0 and 0; both
  `Validate` accept; manifests changed only fingerprint, semantic identity, package, pattern
  provenance, and parent hashes. World authority audit accepted, 3,096 artifacts byte-identical,
  active set and every generator fingerprint identical to the T10 audit. Payload suite 19/19;
  `validate_all.py` 478 files; `Project.World.Realization.Presentation.ProfileContract` 1/1.

### Green evidence

- G1-G5 pass. The pre-firewall red run is G1's mutation evidence (it failed exactly its two
  provenance assertions) and the pre-fix red run is G5's (it failed exactly filter and mip
  filter).
- T11's host cases per the material task.
- Existing exact tests: `Project.Material.Generation.SurfaceRecipeContract`,
  `SurfaceManifestContract`, `SurfaceAssetCompilerContract`, `PatternAuthority`,
  `Project.Material.MeshTerrain.LayoutReceipt`, `Project.Texture.Generation.AssetCompilerContract`,
  and the Mesh Terrain seam tests.
- Host tests `run_surface_generation.Tests.ps1` and `run_pattern_generation.Tests.ps1`;
  `generator_fingerprint.Tests.ps1` unchanged and passing.
- T4 refresh counts, zero-write reruns, both `Validate`; the payload suite; the read-only World
  authority audit with every artifact unchanged.
- `python scripts/ue/check/data/validate_all.py`; ASCII check on changed docs.

## Documentation plan

- **[principles.md](../../../docs/architecture/principles.md) Component Ownership:** one bullet
  stating D4's three identities as a repository rule.
- **[world_pipeline_layers.md](../../../docs/testing/world_pipeline_layers.md) Impact-Driven
  Development:** widen the fingerprint paragraph from World to every generated-content owner. State
  that a consumer's identity binds its producer's public contract, and that each owner's mutation
  matrix at L0/L1 is its locality proof. An unexpected cross-owner regeneration is ARCHITECTURAL
  RED. An expensive gate names the changed dependency it authenticates: the owner's own producing
  input or a producer's public contract. Default order per black box: owner tests, isolated
  fixture, a consumer's integration only when the producer's public contract changed or a focused
  test found an integration failure, targeted runtime, release candidate (D6).
- **[world-engineering SKILL.md](../../../.claude/skills/world-engineering/SKILL.md):** the
  description covers generated resources that World consumes; add one rule line (D6); ARCHITECTURAL
  RED also applies when an UNTOUCHED owner's outputs would need regeneration. The mutation matrix
  is not copied into the skill. `.agents/skills` is a junction to `.claude/skills` (one body).
- **Fresh-agent reach:** AGENTS.md already routes to principles.md (line 35) and
  world_pipeline_layers.md (line 144); no router change unless T5's check fails.
- **[material_generation.md](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)
  Semantic identity:** contract terms versus provenance terms; manifest-only refresh. T11's
  retained-orphan lifecycle text is owned by the material task.
- **[pattern_generation.md](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md)
  Identity:** consumers bind path and output contract; `Verify` closes the contract; a channel
  meaning change publishes a new `output_contract`.
- **[ProjectWorld main view](../../../Plugins/World/ProjectWorld/docs/architecture/diagrams/main.md):**
  edge label.
- **[Visual Verification](../../../tools/World/VisualVerification/README.md) capture stage:** the
  readiness wait does not cover Mesh Terrain section rendering, and Editor captures cannot show
  pattern-driven appearance (citing pattern_generation.md).
- **Duplication avoided:** the rule lives once in principles.md; the testing doc and the skill state
  only their own consequences and link to it.
- Stable docs, code, comments, tests, and configuration must not reference this todo.

## Completion criteria

- P1 passed before any source change, or the sweep stopped with the violated boundary.
- G1 and G2 red then green, with G1's mutation check; G3, G4, and G5 green, with G5's mutation
  check.
- T11 complete per the material task.
- Matrix rows X2, X3, X9, and X11 GREEN; the other rows unchanged.
- Combined refresh: pattern 1, surfaces 4 with 2 compiles; zero-write reruns; both `Validate`
  accept; no World package written; the World authority audit accepts unchanged.
- Documentation plan and fresh-agent reach check complete; independent architectural review PASS;
  both todos updated to the final evidence.
- No staging, commit, or push.

## Review record

### 2026-09-30 - investigation (REVIEW REQUIRED)

- **Trigger:** the operator: "texture and material dev should be fully separate and no need
  regeneration full prove ... refactor world skill and our approaches now immediately", with two
  supplied direction texts (D3-D8).
- **Root cause:** consumer generation identity composes producer provenance (M2, M4); the process
  had no permanent locality guard for the resource owners.
- **Fix:** proposed; not implemented.
- **Verification:** read-only code and document reading; the T10 manifest diff; a repository search
  for consumers of World's material receipt fields. No code, asset, stable doc, stage, or commit
  changed.
- **Authority:** D1-D8, Q1-Q2, A1-A6 added.

### 2026-09-30 - operator's reviewer: APPROVED WITH PATCHES

- **Trigger:** the operator supplied the review and asked for its answers to be recorded as
  decisions after verification, with an argument where the evidence disagrees.
- **Verified and accepted:** Q1 as a first characterization gate (D9). Q2 (D10), including that
  the combined refresh also regenerates the pattern, because T11 edits
  `ProjectTextureGenerationService.cpp` and the texture commandlet, which the conservative
  fingerprint covers. D1's scope (D11). A permanent closed-ABI control (G5, invariant 11):
  producer `Verify` checks the contract name, slot, RGBA8, linear color, wrap and mip addressing,
  mip generation, node format, and RGB-only generators (`ProjectTextureRuntimeAssetBuilder.cpp:329-375`),
  but no test proves a rejection. The strict G2 result. Fresh-agent reach: `.agents/skills` is a
  junction to `.claude/skills`, and AGENTS.md routes both rule owners.
- **Argued and changed:** the expensive-gate rule names the changed dependency the gate
  authenticates, not only a changed public dependency, since an owner's own producing change still
  needs that owner's gates (a Mesh Terrain producer change needs terrain realization proof). P1's
  zero-World-dirty result discriminates only if the census also reports the transiently changed
  ProjectMaterial instance as dirty; the on-disk census guards against an accidental save and does
  not prove the boundary. G4 already recorded dependency data rather than rendering; only its
  wording changed. Channel meaning is not machine-checkable, so it stays a documented versioning
  rule outside G5.
- **Authority:** D9-D12 added; D1 superseded in part by D11; Q1 closed by D9, Q2 by D10; A4
  resolved by D10, A5 by D9.

### 2026-10-01 - implementation and independent review (PASS)

- **Trigger:** the operator's GO with final refinements (D13 and the P1, G1, G2, G5, T11 clarifications).
- **Root cause:** confirmed as M2 and M4; P1 confirmed the World boundary already held.
- **Fix:** the identity firewall; trilinear filtering enforced by the producer's `Verify`; test-only
  pattern test-mount plumbing; T11; the combined owner-local refresh; the rule in principles.md with
  its testing and skill consequences.
- **Verification:** see `### Results`. Independent review returned PATCH: G5 covered only two of the
  four addressing conditions while claiming all, and the prepared skill line narrowed D6 with an
  exclusive "only". Both were corrected (G5 1/1 after rebuild, skill patch reworded and rechecked);
  the same reviewer then returned PASS.
- **Authority:** D13 added, superseding D1 and D11.

### 2026-10-01 - rule wording by use; world-engineering gap pass

- **Trigger:** the operator's reviewer asked for the canonical rule to classify provenance by use;
  the operator asked to close the world-engineering skill's gaps with an architect pass.
- **Accepted:** the absolute "provenance is never another owner's rebuild key" could make an agent
  strip a genuinely consumed producer value (Mesh Partition's own dependency interface hashes
  `UStaticMesh` and `UTexture` content for modifiers). principles.md and world_pipeline_layers.md
  now say a consumer invalidates when an input or public contract it actually consumes changes,
  and information used only as provenance must not become another owner's rebuild key.
- **Refined:** the owner clause keeps "its own inputs or implementation", not "producing inputs",
  because D5 keeps owner fingerprints conservative; the em dashes became ASCII; "focused evidence
  proves the boundary wrong" is not a regeneration licence (D6) but ARCHITECTURAL RED: fix the
  boundary so the consumer regenerates through its corrected identity.
- **Skill gaps closed in the patch:** trigger scope, RED for regenerated outputs, the by-use
  consequence, UNTOUCHED outputs in Implementation and in the review checklist, version 1.1; the
  skill's backticked repo-root path style replaces the relative link. A route-table row for
  principles.md was not added, because the inline citation already reaches it.
- **Durable sweep:** the capture readiness trap and the Editor pattern-visibility trap moved to the
  Visual Verification capture section; the capture route alone reproduced the missing terrain.
- **Authority:** no change.

### 2026-10-01 - closure review: PASS with one condition

- **Trigger:** the operator's reviewer returned PASS on condition that the skill patch is applied
  and inspected, and asked that the capture readiness defect be owned rather than only documented;
  the operator asked for that owner as a prototype in `00_current` that needs
  `/investigate-change` first.
- **Verified:** commit `bfc8ff818` holds the rule wording, the testing consequence, and the Visual
  Verification paragraph, but not the skill patch: the canonical skill is still version 1.0 (236
  lines). `.agents/skills` is still a junction to `.claude/skills`, with no second skill body.
- **Changed:** checked against the reviewer's own list, the patch's boundary sentence ("not by
  hand") did not name the broader regeneration this sweep was about. It now reads: fix the boundary
  so the consumer's corrected identity triggers its regeneration; do not cover the gap with a
  manual or broader regeneration. The patch still applies to HEAD and reproduces the verified copy.
- **Owned:** the capture readiness defect is the prototype
  `20261001-1003_world_fix_capture_readiness.md`. No 3.0.0 release gate uses the Editor capture
  route; R2 step 6 is a packaged property of the frozen candidate.
- **Refined:** after a texture-only change, "Material stays untouched" holds for the surface assets
  (no rebuild, no compile, byte-identical), not for the accepted manifest: its recorded pattern
  provenance makes `Validate` report it stale until a `Regenerate` rewrites only the manifest
  (Decision 3).
- **Authority:** no change.

### 2026-10-01 - closure (PASS)

- **Trigger:** the operator applied the skill patch; the reviewer returned PASS.
- **Verified:** the canonical skill is byte-identical to the reviewed copy (version 1.1, 239 lines,
  ASCII, LF) and meets each item of the reviewer's semantic list; `.agents/skills` is still a
  junction to `.claude/skills` and resolves to the same bytes, with one skill body; the skill's
  cited paths resolve; `git diff --check` is clean; this file's 29 relative links were rebased for
  `01_done/content/` and resolve. No C++, Unreal, regeneration, World Apply, cook, or package check
  was rerun: only the skill, docs, and todos changed after the green run.
- **Follow-up:** the prototype `20261001-1003_world_fix_capture_readiness.md` stays in
  `00_current`, uninvestigated. Its Water consumer is the Editor route only when fed Editor
  captures; the release's Shipping Water proof uses packaged playable-tour frames.
- **Authority:** no change.
