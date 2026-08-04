# Canonical World Realization

`realize_canonical_world.ps1` is the supported noninteractive UE entry point
for ProjectWorld canonical validation, application, and owned-output deletion.

It accepts one exact Canonical Compilation `compile_result.json`, resolves the
configured launcher engine, and writes machine-readable evidence under
`Saved/Validation/WorldRealization/<receipt-sha256>/`.

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
- `Delete` removes owned feature and GeoReferencing actors, clears generated
  Landscape layers, and preserves the `Authored Corrections` layer.

`-MaxRoads` and `-MaxBuildings` bound source feature identities, not generated
mesh fragments. P0 defaults to one road and four buildings so a real provider
snapshot cannot accidentally expand the prototype content budget.

Each result records verified inputs, the grid-owned vertical origin, coordinate
error, actor/section/component counts, changed Landscape components, protected
authored-layer identity, generated source bytes, and an actual-world semantic
fingerprint. Repeating an unchanged Apply must keep the fingerprint stable and
update zero Landscape components.

Every run removes its previous result before launching Unreal, requires a
newly emitted accepted result, and propagates any nonzero Unreal process exit.

For the complete source-to-cooked-package P0 gate, use the single command
owned by [`tools/World/EndToEndValidation/`](../../../tools/World/EndToEndValidation/README.md).
