# ProjectMaterial

ProjectMaterial owns reusable material appearance and the Editor-only compiler that
produces closed material families from reviewed surface recipes.

## Ownership

Canonical data owns world facts and semantics. ProjectMaterial maps those semantics
to visual appearance; it does not infer geography, classification, physics, or
gameplay truth. ProjectWorld consumes the accepted material manifest through a stable
binding and never depends on `ProjectMaterialEditor`.

Reusable material behavior belongs here. A visual that is meaningful only to one
concrete object remains with that object's owner until reuse is demonstrated.

## Runtime boundary

The plugin has no Runtime source module. Packaged targets cook and load ordinary
Unreal material assets. They do not parse recipes, compile graphs, or call an Editor
service. Generated packages contain no unique authored authority and are
reconstructible from recipes plus authenticated upstream inputs.

## Generated surface flow

```text
ProjectTexture accepted pattern manifest
+ Mesh Terrain layout receipt
+ Data/SurfaceRecipes/**/*.surface.json
-> ProjectMaterialEditor closed surface compiler
-> Content/Surfaces/**
-> Data/Manifests/Surfaces/accepted.surface-manifest.json
```

The current families use explicit Substrate Slab graphs. Terrain uses world-metric
triplanar projection and the Mesh Terrain semantic layout; movable objects use
object-metric triplanar projection so their pattern does not swim. Both consume the
same accepted ProjectTexture runtime output. Asset, shader, and generated-resource
structure scales with declared families and unique reusable structure, not cells,
cities, colors, wetness, instance transforms, or consumer count.

See [surface generation](docs/material_generation.md) for the owned contract and
operation route.

## Content

Existing authored/imported resources remain in their established folders. Compiler
outputs live only under `Content/Surfaces/`. Authored changes enter recipes or their
upstream authorities, never generated package state.

## Dependencies

`ProjectMaterialEditor` depends on ProjectTexture authority and Unreal Editor
facilities. It has no gameplay, ProjectWorld, ProjectObject, or runtime consumer
dependency.
