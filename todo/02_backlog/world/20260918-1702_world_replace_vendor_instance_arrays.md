# Replace Vendor Instance Arrays With Project-Owned World Tooling

Status: investigated; implementation not selected

## Authority register

### Decisions

- D1: ALIS needs project-owned PCG/building tooling in the future, but that
  work is not part of the current Shipping package-size cleanup.

### Assumptions

- A1 - ACTIVE: the replacement should extend the existing World authoring and
  realization owners rather than create a parallel building-definition system.
  Revalidate this against the World architecture before implementation.

## Goal

Replace the remaining vendor instance-array dependency and repeated
first-party construction logic with one project-owned World authoring boundary,
without changing City17 placements or visual output.

## Verified current seam

- `/Script/InstanceArrayTool` has one current asset referencer:
  `BP_BuildingHrushev`.
- `/Script/ActorArrayTool` has no current asset referencer.
- Six current ProjectObject Blueprints contain repeated HISM/construction
  behavior worth reconciling with the future World tooling:
  - `BP_PaletteTrash`;
  - `HruSide_Bp`;
  - `HruSideDetailed_Bp`;
  - `HruFrontDetailed_Bp`;
  - `HruBack_Bp`;
  - `BP_Naz60K1_Dormitory`.
- `BP_BuildingHrushev` is a data-only subclass of the vendor actor and is the
  migration bridge, not evidence that every ProjectObject Blueprint should
  become C++.
- The vendor plugin's tracked source is about 1.62 MiB. Its ignored local
  `Binaries/` and `Intermediate/` directories are checkout/build artifacts, not
  player-package savings. Package size is not the reason to do this work.

## Investigation before implementation

1. Route through the current World architecture and identify the existing
   owner of deterministic building/instance realization.
2. Capture the six Blueprints' inputs and realized HISM outputs: meshes,
   transforms, materials, collision, cull distances, actor identity, and World
   Partition placement.
3. Determine whether the smallest owner is a reusable first-party C++ actor,
   a component, existing PCG infrastructure, or a combination. Do not assume
   the old speculative JSON building platform.
4. Design a one-pass editor migration for current City17 actors. Do not edit
   vendor source and do not keep compatibility redirects after the tracked
   content is migrated.
5. Remove the vendor plugin only after both script modules have zero asset
   referencers and City17 visual/runtime acceptance passes.

## Non-goals

- Shipping package-size reduction claims without measured IoStore evidence;
- conversion of simple composition Blueprints;
- changes to `BP_Piano_1` runtime light behavior;
- a new catalog, registry, or parallel content authority;
- implementation during the current package-size cleanup.

## Acceptance

- one project-owned World tooling owner replaces the vendor runtime/editor
  dependency;
- current placed actors preserve exact realized output and World Partition
  identity;
- both vendor script modules have zero current asset referencers;
- City17 visual and packaged runtime checks pass;
- durable ownership is recorded in the existing World documentation tree.
