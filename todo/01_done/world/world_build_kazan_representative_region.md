# Kazan Representative Technical Region

Status: complete. P0 proved the deterministic data-to-Unreal path; this
milestone certifies one bounded Kazan region as a reproducible, measured
technical environment.

Acceptance evidence (2026-08-05): both top-level profiles accepted with the
corrected packaging map argument, the unfiltered p95 sampler with immediate
invalid-timing rejection, per-tick accumulated runtime-role validation, and
Shipping-executable identity proof. Live receipts: `run-20260805T193447Z`
(p0) and `run-20260805T194205Z` (representative_v1) under
`Saved/Validation/WorldPipeline/`. The representative Presentation Gate
measured the staged Shipping executable (SHA-256 recorded) on D3D12 at
1920x1080 scalability 3, three complete 180-frame fixed-camera windows,
worst p95 11.42 ms against the 33.34 ms budget, with hashed PNG captures.
The gate decision policy has focused C++ coverage in
`ProjectWorldPresentationSamplingTests.cpp` independent of the packaged run.

## Permanent contracts

- [ProjectWorld world-build boundary](../../../Plugins/World/ProjectWorld/README.md#world-build-boundary)
- [ProjectWorld Unreal realization contract](../../../Plugins/World/ProjectWorld/README.md#unreal-realization-contract)
- [ProjectWorld verification contract](../../../Plugins/World/ProjectWorld/README.md#14-validation)
- [Unreal Editor MCP control policy](../../../docs/ue_engine/mcp_editor_control.md)
- [Representative acceptance profile](../../../tools/World/EndToEndValidation/profiles/representative_v1.validation.json)
- [End-to-end evidence contract](../../../tools/World/EndToEndValidation/README.md)
- [World data and generated-asset policy](../../../docs/legal/world_data_and_asset_policy.md)

## Scope boundary

- One representative Kazan region, not full-city or republic-scale Unreal
  realization.
- Canonical JSON remains authoritative; generated Unreal assets remain
  disposable.
- No required paid, Fab, Marketplace, hosted GIS, or Experimental UE feature.
- No native content delivery, nationwide generation, final interiors, or
  final city-wide art claim.

## Execution slices

### 1. Visual geometry correctness

- [x] Terrain-drape generated road previews so accepted road geometry remains
  visibly continuous.
- [x] Mass disjoint building polygon parts independently instead of bridging
  a MultiPolygon through one aggregate box.
- [x] Refresh matching World Partition external-actor payloads in place during
  same-identity regeneration.
- [x] Add focused automation regressions and live multi-angle evidence.

### 2. Representative environment

- [x] Select approved reusable terrain, road, and building presentation
  presets.
- [x] Establish reproducible sky, lighting, exposure, and capture viewpoints.
- [x] Add provider-neutral water, land-cover, vegetation, and foliage inputs.
- [x] Protect authored hero-place overlays during regeneration.

### 3. Playable runtime profile

- [x] Enforce the selected World Partition roles and executable Nanite,
  instancing, and HLOD policies.
- [x] Add the required collision, navigation, interaction, and streaming
  behavior for one gameplay route.
- [x] Restore exact map and presentation files after any rejected Apply,
  including an initially absent target.
- [x] Freeze scoped source, procedural-buffer, mesh-section upper-bound,
  frame-time, and regeneration targets before acceptance.

Implementation is complete. Rendered frame-time acceptance waits for the
packaged non-NullRHI evidence owned by the Presentation Gate.

### 4. Presentation gate

- [x] Rebuild the region from accepted inputs without manual repair.
- [x] Pass gameplay-route and packaged-map smoke checks.
- [x] Record fixed-camera packaged p95 frame time with machine, GPU, driver,
  RHI, resolution, scalability, warmup, and sample-window identity via a
  live accepted receipt.
- [x] Capture repeatable overview, terrain-oblique, and road-oblique evidence
  from the accepted packaged run.

## Completion

Complete when one reproducible Kazan technical region renders from the
approved capture angles, supports the bounded gameplay route, stays inside
the frozen runtime budgets, and preserves authored content on regeneration.
Territory-scale water, vegetation, richer roads, building massing, and game
presentation belong to the next world-generation milestone.
