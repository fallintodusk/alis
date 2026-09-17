# Structure

## Purpose

City17 owns the supported hand-authored Old City 17 World in the full
development checkout while generated Kazan and Manhattan routes mature
independently. Its binary content is excluded from the public source projection.

## Owns

- The `City17_Persistent_WP` map and City17 plugin content namespace.
- The `City17` experience descriptor and its map scan specification.
- City17-specific data, configuration, and bug fixes.

## Does not own

- Runtime loading execution, owned by
  [ProjectLoading](../../../../Systems/ProjectLoading/README.md).
- Gameplay-mode behavior, owned by gameplay components.
- Reusable generation or realization, owned by
  [ProjectWorld](../../../ProjectWorld/README.md).
- Generated Kazan or Manhattan authority and protected authored overlays,
  owned by [ProjectWorldData](../../../ProjectWorldData/README.md).

## Composition

| Part | Responsibility |
|---|---|
| `City17` runtime module | Registers the experience descriptor |
| Experience descriptor | Names the map and Asset Manager scan root |
| `Content/Maps/City17_Persistent_WP` | Supported hand-authored playable World |
| Plugin configuration | Registers City17 maps for asset discovery |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

## Invariants

1. City17 remains a supported menu, load, cook, and package route in the full
   development checkout until its retirement gate passes.
2. City17 is frozen for new authored product work; only fixes needed to retain
   its supported route belong here.
3. New authored World work belongs in the generated World's protected overlay
   with canonical anchors that survive regeneration. It is not duplicated into
   City17.
4. Retirement requires an accepted generated playable scenario, migration or
   retirement of every retained reference, repository reference checks, and
   build, test, cook, package, and runtime evidence without City17.
5. Retirement is one coherent removal with no compatibility alias or second
   World authority.
