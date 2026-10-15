# ProjectTexture structure

ProjectTexture turns reviewed structural recipes into shared texture resources.
Its consumer contract is a named output ABI and stable native render-target slots.
The [generation contract](../pattern_generation.md) owns admission, identity,
runtime policy and the supported producer envelopes.

## Immediate components

| Component | Responsibility | Implementation |
|---|---|---|
| Editor compiler | Validate recipes, assemble admitted native kernels and authenticate generated packages | [ProjectTextureEditor](../../Source/ProjectTextureEditor/ProjectTextureEditor.Build.cs) |
| Runtime resource owner | Admit generation, share identities and retain stable outputs under one budget | [Runtime subsystem](../../Source/ProjectTexture/Public/ProjectTextureRuntimeSubsystem.h) |
| Optional execution adapter | Execute an authenticated native graph within a declared resource envelope | [Bundle producer contract](../../Source/ProjectTexture/Public/ProjectTextureBundleProducer.h) |

Recipes select structural styles; consumer population does not select algorithms
or allocate new texture identities. Repeated use of the same accepted pattern
shares its resources. Distinct structures consume distinct bounded reservations.
Do not assign an independent seed or bundle to every World object.

## Native composition

The particle family consumes explicit structural controls from its
[recipe schema](../../Data/Schemas/pattern-recipe.schema.json). Its native
kernel uses Unreal VectorNoise Voronoi, Cellnoise and gradient/vector noise;
NormalFromHeightmap derives the coherent encoded normal. Schema-controlled
values determine content. C++ owns validation, native expression assembly,
output contracts and deterministic identity, not one algorithm per surface name.
ABI constants, numeric decoding and unit conventions remain implementation rules.

Noise tile periods are native expression properties. Changing a structural period
requires compiling the affected generator material. Palette and surface-scale
changes belong to ProjectMaterial and do not regenerate this structure.

Texture Graph provides reusable graphs/subgraphs and typed input parameter maps
through `UTG_Graph::GetInputParamsVarMap` and `SetInputParamsFromVarMap` in the
installed engine's `TextureGraph/Public/TG_Graph.h`. Its stock Noise expression
provides Simplex/Perlin/Worley variants, rather than the Voronoi seed-position and
Cellnoise composition used by the accepted source. The admitted material kernels
therefore reuse the native material expressions; an optional Texture Graph adapter
can execute material expressions through its existing execution contract.

The public runtime owner has no Texture Graph dependency. The private pinned
adapter's cooked proof establishes its admitted fixture only. New templates,
parameters or output ABIs require their own authenticated proof before enrollment.
Native parameter setters may warn instead of fail; validate pin names, types and
ranges, then verify readback. Editor-only render-target conversion is not a
packaged GPU-resource bridge. Stable output ownership stays in ProjectTexture.

## Change boundaries

Changing a kernel changes its owned structural resources and provenance. A material
consumer reacts to an output path/ABI it actually consumes, rather than copying
producer internals into its rebuild key. World geometry does not consume particle
composition or palette. Production promotion, resource retirement and any actual
reference migration remain separate from isolated compiler verification.

The current compiler fingerprint covers the admitted owner source set. A compiler
refactor can therefore stale several pattern artifacts even when their default
pixels are equal. Recipe-only structural variation and runtime sharing are distinct
from that one-time provenance update. Do not bypass authentication or rebuild World
geometry to hide it; use the existing resource-owner promotion boundary.

The [component diagram](diagrams/main.md) shows these boundaries.

## Native references

Epic's [Texture Graph introduction](https://dev.epicgames.com/documentation/en-us/unreal-engine/getting-started-with-texture-graph-in-unreal-engine)
describes native node graphs and reusable parameters. Installed `TG_Graph.h`,
`TG_Expression_Noise.h` and the native material VectorNoise implementation determine
the actual extension and compilation limits. Prefer these capabilities through the
existing producer contract; a graph asset is not an independent cache authority.
