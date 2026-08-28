# ProjectMaterial

Universal material resources and the Editor-only compiler that produces reviewed
material families from closed JSON recipes.

## Ownership

ProjectMaterial owns reusable material behavior and resources that are meaningful
outside one concrete object or world dataset. Examples include terrain, water,
asphalt, foliage, metal, plastic, glass, material functions, and shared effects.

Concrete consumers select these assets through stable soft object paths:

- ProjectWorld maps world semantics such as `terrain` or `road.default` to a
  universal material identity.
- ProjectWorldData owns geography and sourced semantic facts, never material graphs,
  recipes, parameters, or manifests.
- ProjectObject composes universal resources into concrete entities. A genuinely
  entity-specific visual may remain ObjectId-owned; it does not move compiler or
  archetype logic out of ProjectMaterial.

Reuse is the promotion boundary. A resource stays with one concrete owner until it
is genuinely reusable, then it is generalized here rather than copied.

## Runtime boundary

The plugin has no Runtime source module. Packaged games cook and load material assets
through normal Unreal soft references. They do not parse recipes, compile graphs, or
call a ProjectMaterial service.

`ProjectMaterialEditor` is an Editor module. Consumer `Build.cs` files must not depend
on it. Content dependencies are declared only when cook evidence shows that a
consumer plugin needs a descriptor dependency.

## Generated material flow

```text
Data/Materials/**/*.material.json
-> ProjectMaterialEditor closed recipe compiler
-> Content/Generated/** at final stable identity
-> Data/Manifests/Materials/accepted.material-manifest.json
```

V1 discovers ProjectMaterial recipes only. It supports the exact
`surface_opaque/landscape_basic_v1` parent and instance contract; it is not a general
material graph DSL. See [material generation](docs/material_generation.md).

The accepted terrain family is:

```text
Terrain/M_ProjectTerrain
-> /ProjectMaterial/Generated/Terrain/M_ProjectTerrain

Terrain/MI_ProjectTerrain_Default
-> /ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default
```

The parent exposes low-slope color, steep-slope color, slope contrast, and roughness.
Same-path recipe changes advance ProjectMaterial authority only. Consumers keep the
stable asset identity and must not rebuild World geography for material-only tuning.

Use the host wrapper for mutation:

```powershell
scripts/ue/material/run_material_generation.ps1 -Mode Validate
scripts/ue/material/run_material_generation.ps1 -Mode Regenerate
```

Do not invoke the commandlet directly. The wrapper owns the shared generated-content
lock, same-project Editor exclusion, exact `-1` rollback, timeout handling, receipt
authentication, and scratch cleanup.

## Content

Authored and imported assets remain in their existing folders. Generated assets live
only under `Content/Generated/`. Adding the compiler does not automatically migrate
authored, imported, third-party, or existing generated materials.

## Dependencies

The Editor module depends only on Unreal Editor facilities. There is no gameplay,
World, ProjectObject, or consumer dependency in the compiler direction.
