# Canonical World Realization

`realize_canonical_world.ps1` is the supported noninteractive UE entry point
for ProjectWorld canonical validation, application, and owned-output deletion.

It accepts one exact Canonical Compilation `compile_result.json`, resolves the
configured launcher engine, and writes machine-readable evidence under
`Saved/Validation/WorldRealization/<invocation-sha256>/`. The invocation
identity covers the compile receipt, presentation and optional runtime
profiles, target map, realization budgets, and Landscape requirement so
distinct runs cannot overwrite each other's evidence.

The default presentation profile is
`Plugins/World/ProjectWorld/Data/Presentation/kazan_representative_v1.json`.
It is the editable SOT for approved engine-provided material references,
outdoor lighting, fixed exposure, and named capture viewpoints. Override it
with `-PresentationProfile <path>` for a different bounded region profile.
Use `-RuntimeProfile <path>` only when that generated map must realize and
prove the profile-pinned gameplay route. The runtime document owns route
identity, collision/navigation extent, explicit optimization policies, and
structural budgets; omitting it keeps synthetic and non-playable fixtures free
of world-specific runtime behavior.
Automation orchestrators may provide `-EvidencePath <path>` under
`Saved/Validation/WorldRealization/`; both the wrapper and commandlet reject
other roots. Orchestrators therefore do not need to reproduce the script's
invocation identity algorithm without weakening evidence containment.

```powershell
.\scripts\ue\world\realize_canonical_world.ps1 `
  -CompileResult <path-to-compile_result.json> `
  -Mode Validate
```

Use `-Mode Apply` only for the generated ProjectWorld map root. Add
`-RequireLandscape` when validating a real Landscape envelope; incompatible
canonical dimensions fail rather than resample.

Modes:

- `Validate` verifies the full receipt and runs the real GeoReferencing probe
  for EPSG inputs without saving a map.
- `Apply` creates or regenerates owned actors and assets. Compatible grids use
  one stock Landscape; incompatible fixtures retain exact procedural terrain.
- `Delete` removes owned feature, presentation, and GeoReferencing actors,
  clears generated Landscape layers, and preserves the `Authored Corrections`
  layer. It does not load or require the former presentation profile.

`-MaxRoads` and `-MaxBuildings` bound source feature identities, not generated
mesh fragments. P0 defaults to one road and four buildings so a real provider
snapshot cannot accidentally expand the prototype content budget.

Each result records verified inputs, the grid-owned vertical origin, coordinate
error, actor/section/component counts, changed Landscape components, protected
authored-layer identity, presentation profile identity, generated source bytes,
and an actual-world semantic fingerprint. Repeating an unchanged Apply must
keep the fingerprint stable and update zero Landscape components.

Runtime evidence additionally records the exact profile hash, route identity,
one collision probe per route-fragment cell, Recast path length, generated
source bytes, allocated procedural mesh buffer bytes, exact streaming-role
counts, a mesh-section draw-call upper bound, and executable Nanite, instancing,
and HLOD policy results. The frozen frame-time value is a target, not a
`-NullRHI` measurement. A result is rejected when any structural policy or
budget fails.

Every run removes its previous result before launching Unreal, requires a
newly emitted accepted result, and propagates any nonzero Unreal process exit.
Apply and Delete also snapshot only the target map, its World Partition
external actor/object roots, and generated presentation material. Acceptance
discards that temporary snapshot; any rejected result, engine failure, or
missing receipt restores the exact prior files. An initially absent target is
restored to absence. A failed restoration preserves the recovery snapshot and
reports its absolute path. If a nonzero engine exit rolls back a child result
marked `accepted`, the wrapper removes that stale receipt. The focused
transaction regression lives under `test/`.

For the complete source-to-cooked-package P0 gate, use the single command
owned by [`tools/World/EndToEndValidation/`](../../../tools/World/EndToEndValidation/README.md).
