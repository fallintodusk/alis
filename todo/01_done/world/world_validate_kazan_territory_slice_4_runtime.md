# World slice 4 - automated partition design and runtime proof

## Outcome

First prove that the cold-started packaged product can select generated Kazan
through the real menu/loading route, spawn and control the player on generated
ground, interact with its generated blockout, and unload/reload spatial cells.
Then measure that baseline and select the passing World Partition runtime
profile through the frozen ordering without changing geographic or
generated-layer authority.

## Stable routes

- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World pipeline layers](../../../docs/testing/world_pipeline_layers.md)
- [MCP/editor evidence policy](../../../docs/ue_engine/mcp_editor_control.md)
- [ProjectLoading contract](../../../Plugins/Systems/ProjectLoading/README.md)
- [Single-player contract](../../../Plugins/Gameplay/ProjectSinglePlay/README.md)
- [Main-menu contract](../../../Plugins/UI/ProjectMenuMain/README.md)
- [Initiative roadmap](../../02_backlog/world/world_generate_kazan_territory_roadmap.md)

## Initial audit facts (closed by section A)

- The accepted territory map is
  `/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory`, but
  `Config/DefaultGame.ini` does not scan or cook that map.
- The descriptor registry and menu currently expose MainMenuWorld and City17,
  not a Kazan-territory experience or selection.
- The reusable product route already exists:
  `MenuMainComposerSubsystem -> MenuPlayPlayerController ->
  ILoadingService::BuildLoadRequestForExperience -> ProjectLoading travel`.
  Slice 4 extends that route; it does not replace it.
- The initial gap was no territory runtime profile. The first applied baseline
  was `256/768`; the bounded comparison selected and persisted `512/1536`.
- Existing 3E Editor evidence proves three generated actors and Map Check, not
  packaged player spawn, possession, movement, interaction, or streaming.

## Frozen boundary

Orchestration owner: Slice 4 runtime acceptance.

Behavior remains with its existing owners: ProjectWorld owns runtime-profile
policy; the menu/loading stack owns experience selection and travel;
ProjectSinglePlay/character owners own GameMode, spawn, possession, and normal
movement; ProjectWorldData owns the concrete Kazan map/profile and package
inclusion. Slice 4 may add the smallest missing projection through those
public contracts, but it must not create a parallel loading or gameplay path.

Candidate baseline:

```text
128 m cells / 768 m range
256 m cells / 768 m range
512 m cells / 1536 m range
+ declared Landscape proxy bundles
```

Expected changed components:

- Kazan runtime profile/schema and read-back validation;
- the minimal Kazan experience/menu/cook projection required by the existing
  product loading path, if the initial audit confirms it is missing;
- packaged product-route and deterministic traversal evidence, using stable
  World Partition telemetry with Insights as optional diagnostics;
- focused validation tests, selected profile, and stable runtime contract docs;
- packaged acceptance receipts and only derived packages required by the
  selected profile.

Expected untouched components:

- source ingestion, canonical compilation, and accepted geography;
- terrain, water, road, vegetation, building, and gameplay-placement inputs or
  generator semantics;
- ProjectLoading phase semantics beyond the owner-local travel-provenance and
  late boot re-entry correction, ProjectSinglePlay mechanics, character spawn
  semantics, ObjectDefinition, inventory, vitals, dialogue, and scenario logic;
- HLOD policy, which remains explicitly disabled.

Stop if the product baseline requires changing a frozen generated layer,
inventing a second loading/gameplay route, adding scenario mechanics, or
changing character/interaction semantics. Also stop if no candidate passes or
evidence requires a new performance/quality tradeoff not already decided by
the roadmap.

## Execution

### A. Product baseline - mandatory before candidate comparison

- [x] Audit the actual product route and record its current gaps: Kazan
  experience registration/menu selection, AssetManager scan, cook inclusion,
  travel URL/GameMode, player start/grounding, interaction path, and stable
  runtime telemetry. Characterize before changing an owner.
- [x] Pin and read back the intended `256 m` / `768 m` Kazan baseline through
  the existing HashSet-native runtime-profile path. Add only the smallest
  owner-local experience/menu/cook projection proven missing by the audit.
- [x] Cold-start a packaged/current-game build at its default entry and prove:

  ```text
  main menu -> select Kazan -> ProjectLoading -> correct map + GameMode
  -> possessed player grounded on generated terrain -> normal movement
  -> terrain/road/building collision -> gameplay-object interaction/collision
  -> centre -> edge -> centre -> real cell unload + reload
  ```

  Direct `.umap` opening, Editor fly-through, or a synthetic pawn is diagnostic
  only. Fix a failure in its actual owner and do not begin the tournament until
  this path passes. Candidate packaging uses the installed launcher-engine
  route; Slice 4 never switches to or rebuilds a source engine.

  Accepted baseline receipt:
  `Saved/Validation/WorldRealization/slice4-baseline-256-768/product-route/shipping-exposure-fixed.json`.
  It records physical RTX 4070/D3D12 identity, Shipping executable identity,
  ProjectLoading provenance, collision/interaction owners, unload/reload, and
  the validated rendered frame. Source/launcher cache switching is now blocked
  before UAT by `test_engine_route_admission.ps1`; the owner cleanup retained
  rollback/cache inputs while reclaiming superseded scratch.
- [x] Measure the passing baseline through the same packaged harness: loaded
  and activated cells, readiness/streaming failures, Frame/Game/Render/GPU p95,
  useful p99 hitch evidence, peak memory, residency, and hardware/preset ID.

### B. Bounded profile selection

- [x] Audit actor bounds, reference bundles, per-cell/package weight,
  Landscape ownership, source-speed coverage, and HLOD absence; reject an
  invalid candidate before an expensive package when possible.

  Accepted static audit receipt:
  `Saved/Validation/WorldRealization/runtime-profile-tournament/7ca2f836cf9a4f6fbfb4b4218f078ad1/static-partition-audit.json`.
  The read-only launcher-Editor audit accepted all candidates against the same
  850 actors and 210 Landscape proxies. It found zero actor-reference bundles,
  missing packages, Data Layer memberships, Landscape ownership failures, or
  HLOD participation, and attributed 11,755,861 external-package bytes. The
  selected profile reduced conservative spatial cell assignments to 4,899.
- [x] Pin the three machine-readable candidates and focused twins, then apply
  and read back one concern at a time without source or generated-layer drift.
- [x] Before the tournament, prove an isolated runtime-only switch advances
  map/runtime state while all six layer manifest entries and artifact sets stay
  byte-identical. Treat any layer propagation as architectural red.

  Accepted locality receipt:
  `Saved/Validation/WorldRealization/runtime-profile-locality/e84ec65155ae4741ba6144dc9b511779/summary.json`.
  It changes the actual copied realization SOT field through
  `512/1536 -> 128/768 -> 512/1536`, records zero dirty units for all six
  layers, and preserves one exact layer artifact-set SHA-256.
- [x] Run dense-centre, diagonal, perimeter, backtrack, and higher-speed
  streaming-source routes through the same product harness. The higher-speed
  route must not add a vehicle or another traversal system. Correctness
  teleports are not performance traversal; collect time-resolved route data.
- [x] Select using hard correctness/budgets first, then p99 hitch, peak memory,
  and activation churn as required by the stable SOT. Persist the winner as the
  stable default; a passing baseline alone does not waive this comparison.
- [x] Prove the primary prototype gate on the physical RTX 4070 at High,
  1440p/60. Record RTX 3060-class Medium 1080p/60 plus its declared 30 FPS
  fallback as UNQUALIFIED in the release plan until a named physical adapter
  is available; never extrapolate or emulate it. The secondary release gate
  does not block Slice 4 archive or prototype promotion.

  Accepted tournament receipt:
  `Saved/Validation/WorldRealization/runtime-profile-tournament/7ca2f836cf9a4f6fbfb4b4218f078ad1/summary.json`.
  One byte-identical packaged Development executable compared all profiles on
  the physical RTX 4070/D3D12 at High 1440p. All passed; `512/1536` won with
  `14.999 ms` p95, `17.773 ms` p99, `3761.6 MiB` peak process memory, and zero
  streaming failures. Separate launcher Shipping product-route evidence
  passed because launcher Shipping does not provide native CSV profiling:
  `Saved/Validation/WorldRealization/runtime-profile-tournament/7ca2f836cf9a4f6fbfb4b4218f078ad1/shipping-final/product-route.json`.
- [x] Run the required focused and end-of-slice automated gates and audit the
  unchanged generated authority. The final audit accepted all 1,372 artifacts
  byte-identical under active set `1ae7df79427c6a1f0c8e3c9048ae7e12c882deb826401b281a68b089812abedf`.
- [x] Operator product walkthrough and promotion checkpoint accepted 2026-08-25.
  The operator judged the generated blockout, including roads, good enough for
  prototype presentation. Water, vegetation, and other visual improvements are
  routed to a separate research campaign and do not reopen Slice 4.

## Out of scope

HLOD, new geography, new generated layers, art polish, scenario objectives,
gameplay balancing, multiplayer, combat, dialogue, a new vehicle/traversal
system, hardware emulation, and manual fly-throughs as technical acceptance
authority.
