# Build a globally recognizable showcase city

**Status:** COMPLETE - D0 through D6 accepted, operator visual acceptance granted 2026-09-03
**Created:** 2026-09-02 12:18 Europe/Moscow
**Owners:** generic ProjectWorld pipeline plus the selected concrete territory data owner

## Goal

Create a second recognizable real-world territory that demonstrates ALIS is a reusable
reconstruction system rather than a Kazan-specific pipeline. It must use the exact
accepted Kazan physical footprint and the existing generic product route.

## Operator decisions

- Manhattan/New York City is the selected first candidate hypothesis.
- Characterize the exact footprint first. Persistent Manhattan generation starts only
  after the generic Building v2 and corrected D0 gates pass.
- D0 must compare Manhattan and Kazan read-only before canonical/generated mutation.
- Use only currently admitted open-source providers in the first slice.
- Do not assume famous-city satellite imagery is better. Test whether current admitted
  building/height/level/road/water and other semantics are materially richer.
- No city-specific runtime generation logic, hardcoded landmark heights, per-landmark
  actors, proprietary SDK, or hidden generic Building expansion.
- Paris and Tokyo remain future provider-research alternatives, not first-slice inputs.
- Old City 17 owns survival onboarding. This and other reconstructed showcase maps are
  demo/scale experiences using the real character in `PreviewFlight`; do not add a
  city-specific survival scenario, cache, shelter, objective graph, or tutorial.
- The generic structural prerequisite is provider-neutral canonical logical Buildings
  with effective vertical volumes consumed by `project_building_massing:v2`. Provider
  `building:part` and relation semantics terminate in Canonical Compilation; ProjectWorld
  sees no OSM roles/tags or territory identity.
- Building v2 is massing only: outline/parts, height/min-height, qualified level fallback,
  association, topology, deterministic identity, and cell-local realization. Materials,
  roofs, facades, windows, interiors, and landmark art remain separate concerns.

## Stable routes

- [Territory generation](../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory authority contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [World tooling](../../tools/World/README.md)
- [World Buildings concern](../02_backlog/world/world_present_buildings.md)

## Required product shape

If later selected, the territory must:

- use exactly the accepted Kazan width, height/radius, grid, and cell dimensions;
- appear as a separate Main Menu choice;
- use Main Menu -> ProjectLoading -> SinglePlayer -> real `ADefinitionCharacter`;
- start with the existing `PreviewFlight` presentation unless later evidence selects
  otherwise;
- present reconstruction scale and World technology rather than duplicating Old City
  17 survival onboarding;
- reuse generic ProjectWorld compilation, realization, World Partition, and evidence;
- add only concrete city data/profile and product/menu projection;
- contain no Kazan or Manhattan branch in generic runtime code.

## D0 - mandatory read-only comparison

Before generating any new authority:

1. Resolve the accepted Kazan physical footprint from authoritative profile/data. Do
   not approximate "same radius."
2. Center that exact footprint on a reviewer-selected Manhattan location containing
   recognizable dense urban structure.
3. Through only the current ALIS providers/admission rules, census equivalent Manhattan
   and Kazan inputs for:
   - terrain/DEM availability;
   - buildings, heights, levels, and building parts;
   - roads and classes;
   - water;
   - vegetation;
   - every other feature class currently consumed by the pipeline.
4. Record current provider provenance and license constraints.
5. Quantify the known `building:part` risk: outline/part/relation counts; height and
   min-height coverage; relation/containment association; orphan/ambiguous parts;
   complete/incomplete outline coverage; v1 logical count; and prospective v2 logical
   and effective-volume counts.

This is a source census only. No canonical bundle, generated layer, manifest, profile,
map, plugin, product descriptor, or menu entry may be created during D0.

Research input: [OpenStreetMap New York City](https://wiki.openstreetmap.org/wiki/New_York_City).

## Proceed gate

Proceed with Manhattan only if D0 proves:

```text
exact accepted Kazan-sized footprint
+ same existing providers
+ same generic ALIS compilers/realizers
-> materially denser and globally recognizable showcase territory
```

If `building:part` omission is confirmed, implement the bounded generic canonical-volume
and `project_building_massing:v2` prerequisite inside this active task. Stop only if real
source cases require materially broader association, coverage, provider, roof/facade, or
other semantics than the approved massing contract; do not guess or add a city branch.
If current providers do not support the hypothesis, reviewer selects another city or
opens separate provider research.

## Data ownership decision after D0

Audit the current ProjectWorldData contract before implementation:

- if it is intentionally multi-territory, add a sibling city profile/data subtree;
- if it is Kazan-specific, use the smallest content/data-only city owner following the
  existing World data pattern;
- do not add a runtime/source module merely to hold coordinates.

There remains one generic ProjectWorld implementation.

## Smallest implementation path

### D0-A - source characterization

Resolve the exact Kazan footprint and census Kazan/Manhattan through current admitted
providers without persistent Manhattan canonical or generated authority.

### B1 - generic structural fidelity prerequisite

When D0-A confirms the gap, implement provider-neutral logical Building effective
volumes plus `project_building_massing:v2`; prove focused synthetic behavior, v1
compatibility, cell locality, idempotence, and a Kazan control migration first.

### D0-B - corrected Manhattan decision

Repeat the same-footprint census through Building v2 and proceed only if current providers,
fallback/rejection rates, metric CRS controls, and authenticated previews support a useful
showcase.

### D1 - concrete authority

Add the selected city identity/profile/data through the established compilation path.

### D2 - generic realization

Compile and realize through existing ProjectWorld owners without a city branch.

### D3 - product projection

Add one separate menu/experience descriptor.

### D4 - real route

Prove ProjectLoading, SinglePlayer, `ADefinitionCharacter`, and PreviewFlight.

### D5 - focused authority proof

Run generation, schema, provenance, idempotence, and locality checks appropriate to
the changed layers.

### D6 - prototype package

Build one smoke Candidate and prove menu choice, coordinates/scale, World Partition,
generic ownership, and reasonable physical RTX 4070 behavior. Do not automatically
repeat the full historical Kazan stabilization campaign unless this city is selected
for a public release.

## D6 evidence (2026-09-03)

Receipt-owned truth. Paths below are the authority; this section restates only identity.

- Source: revision `3a3ea5647cd78faaddc6d644b374cc518107938b`,
  dirty-state digest `96d810e8711ccd042194b2fab9357538c08be1cf8548e4dffbb349e767e3050f`,
  verified unchanged after each package and again before promotion.
- Runtime profile `manhattan_showcase_512_1536_v1`
  (`6e1732b7060b663432350407f3ccd4a1d9524e8cdf072e857daa65b8cb139ca1`).
- Development and Shipping both `accepted` through the real
  Main Menu -> ProjectLoading -> SinglePlayer route with the real `ADefinitionCharacter`:
  loading provenance, possession, grounding, movement, terrain/road/Building collision,
  centre unload at edge, edge load, centre reload, and PreviewFlight restore all true on
  RTX 4070 / D3D12.
- `gameplay_interaction_required = false` in both receipts. The showcase has no
  gameplay-placement layer, so the non-interactive policy is selected by the showcase runner
  alone; Kazan and default runners pass no skip flag and stay strict.
- Candidate is the SHIPPING package
  (`Saved/PackageRelease/ManhattanShowcase/Candidate`, executable
  `4b70785c863e94a478bb66a7832762044f0fe2de5580fdf3dd9796b4f0bed015`,
  payload `f9c9e920b8083eb21d92cffd4c122195dc79972d8a771c015a369ff301d3cd1a`).
  Development is retained in staging as evidence and is not promoted.
- Evidence root:
  `Saved/Validation/WorldRealization/manhattan-showcase/455f4fbe5df14964ac0834ae0be5d667/`.

Canonical and generated Manhattan authority were reused unchanged; no recompilation or
realization was performed for this gate.

Experience composition moved to configured data during this slice: concrete territories are
JSON records in `Plugins/Resources/ProjectExperienceData` generated into DataAssets, resolved
by one generic descriptor in ProjectLoading. `Source/Alis` holds no territory knowledge, so a
future showcase city needs no C++.

Operator visual acceptance: GRANTED 2026-09-03. Manhattan reconstruction quality accepted,
and World Partition cells were confirmed to load/unload correctly at distance.

Two visual observations recorded but explicitly NOT in scope for this slice, both pre-existing
realization behaviour untouched by this diff: water bodies are stair-stepped at cell
resolution (clearly visible along the Hudson), and one extremely slender needle tower near
Midtown may be a bad height/footprint pairing.

Runtime performance: Manhattan Development is healthy at the controlled envelope, and long
traversal accumulates nothing (bounded cells ~24-25, flat ~2.2 GB, no drift, zero streaming
failures, GPU at or below accepted Kazan). A Shipping-only discrepancy the operator reported
did not reproduce in any Development configuration and is deferred to backlog with one
watch item that does not gate the release. No World optimization is justified.

Nothing remains for this concern. It is complete and does not gate Release 2.0.0.

## Future acceptance

- The same exact footprint is authenticated for Kazan and the selected city.
- Current provider data proves the city hypothesis before generation.
- Canonical/generated authority is reproducible and owner-correct.
- The normal packaged product route works with the real character.
- No city-specific generic-code fork exists.
- Rollback removes the concrete city projection/data without mutating Kazan.

## Stop conditions

- Stop on an approximate footprint or unknown provider/license identity.
- Stop if Manhattan requires hardcoded heights, landmarks, or a new provider in v1.
- Stop on material `building:part` loss and route the generic limitation first.
- Stop if implementation propagates into city-specific ProjectWorld runtime code.
- Stop before generation until D0 receives reviewer PASS and operator implementation
  priority.

## Current authorization boundary

Execute D0-A, the bounded generic Building v2 prerequisite when confirmed, Kazan control
re-accreditation, corrected Manhattan D0-B, and D1-D6 only after their gates pass. Preserve
the historical Kazan Candidate/receipts. Do not touch cinematic work, add providers, stage,
commit, publish, or bypass the final operator visual walkthrough.
