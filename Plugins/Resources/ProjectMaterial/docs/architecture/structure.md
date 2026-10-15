# ProjectMaterial structure

ProjectMaterial maps authenticated resources and surface recipes to reusable Unreal
material parents and instances. The [generation contract](../material_generation.md)
owns authoring, transactional mutation, preview and saved-resource authentication.

## Immediate components

| Component | Responsibility | Implementation |
|---|---|---|
| Surface recipe contract | Admit physical shader families, projections and appearance controls | [Recipe contract](../../Source/ProjectMaterialEditor/Public/ProjectMaterialSurfaceRecipe.h) |
| Family composition | Assemble reviewed native PBR/SingleLayerWater graphs using shared primitives | [Surface compiler](../../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp) |
| Resource bindings | Authenticate output ABIs, bind instance textures and verify effective values | [Pattern bindings](../../Source/ProjectMaterialEditor/Private/ProjectMaterialPatternBindings.h) |
| Generation service | Compile, save/reload and authenticate the accepted manifest | [Generation service](../../Source/ProjectMaterialEditor/Public/ProjectMaterialSurfaceGenerationService.h) |
| Preview | Apply reversible instance parameters to existing rendered bindings | [Preview contract](../../Source/ProjectMaterialEditor/Public/ProjectMaterialSurfacePreview.h) |

## Composition and variety

Families describe shading and resource contracts: aggregate PBR, encoded
height/normal/masks PBR, native single-layer water, and terrain/object triplanar
semantics. Surface names such as asphalt or river belong to recipes and bindings.
A different name, palette, scale or admitted structural pattern does not require
another C++ builder or universal shader with many static switches.

The shared [native expression helpers](../../Source/ProjectMaterialEditor/Private/ProjectMaterialExpressionUtils.h)
own graph construction primitives, world XY coordinates and native normal
orientation. Families retain their distinct channel meanings and physical models.
SingleLayerWater is not an opaque Substrate Slab with a color substitution.

Opaque geometry families declare native Nanite usage before compilation. Constant
instances inherit it through Unreal's normal update path. Direct generated-bundle
families authenticate effective parent and instance flags. The accepted triplanar
templates retain their existing authentication: Mesh Partition owns their compiled
section instances and vertex-factory use. Editor auto-repair is not runtime proof.
Water remains a separate non-Nanite geometry contract.

The metric height/mask family composes the same three outputs at two physical
scales with native HeightLerp, independently scaled macro variation and native
world-normal orientation. This family consumes no Mesh Partition semantic
channels. Its palette and distance controls belong to the recipe; source maps
remain shared across these appearance variants. The connected native graph is
reconstructed for authentication without shader compilation or asset saving.

For whole-terrain use, the semantic metric variant wraps the same detail in the
existing ground/hydro composition. Its native channel/layout dependency is explicit.
The shader receives the adapter's existing channel texture and primitive context;
it does not generate masks or redefine canonical weights. The compacted-ground
variant remains useful for isolated samples and does not claim universal land cover.

An instance's pattern may differ from its parent's default when the output ABI is
compatible. Bind the selected stable texture slots on the instance; do not rebuild
the parent or silently inherit a different source. Verify effective texture, scalar
and vector values after save/reload. Unsupported ABI, missing parameters, foreign
or dirty resources refuse before output mutation.

The one-output triplanar family also has accepted parents with inline texture
samples. Their inherited source can be authenticated for parameter preview. They
cannot be retargeted to another pattern until their owner generates a parameterized
template. This verifies actual accepted content without a compatibility compiler,
new reflected legacy fields or World regeneration.

Preview authenticates the saved baseline and its resource bindings separately
from the edited candidate values. Candidate values are validated and applied to
a transient MID. Metric families authenticate their real parent recipe, including
when terrain semantics are present; an instance's defaults can differ from the
parent. A changed preview value is not a mismatch in the saved baseline.
Invalid candidates retain the last good preview; revert restores the original binding.
Copied MID overrides cannot select other structural resources: the transient preview
rebinds the authenticated candidate textures and retains the original MID for revert.
The existing fixture console operation also accepts a renderer already bound to its
authenticated fixture instance, including aggregate PBR and native water. Isolated
pattern roots remain confined and require isolated material authority. Substitution
on production terrain retains its separate compatibility/restore guard.

## Dependencies

Generated structures come through the
[ProjectTexture output contract](../../../ProjectTexture/docs/pattern_generation.md).
Native water resources are authenticated directly; a texture that is not generated
by ProjectTexture does not acquire a fictitious pattern or cache identity.
ProjectTexture owns structural composition and texture lifetime. ProjectMaterial
owns metric projection, palette and shading. World owns geometry and semantic
assignment, and consumes material references without calling the compiler.

Shader parameters do not mutate canonical facts, elevations, winding, collision,
World Partition or generated mesh descriptions. Saved material-reference migration
and production backend activation require their existing acceptance boundaries.

## Authoring another surface

1. Select an existing physical family and compatible resource ABI. Start from its
   complete [fixture recipe](../../Data/TestFixtures); preserve schema admission.
2. Reuse an accepted structural pattern where possible. Change its bounded JSON
   controls through ProjectTexture only when the structure itself must differ.
3. Author another instance recipe with a unique material/binding ID, shared parent,
   selected pattern and appearance parameters. A different selected pattern must
   use the same output ABI; source textures are bound on the instance.
4. Verify in an isolated mount and transient preview. Promote through the existing
   writer/manifest transaction and the affected consumers' acceptance boundary.

An unsupported physical model or resource ABI requires a reviewed extension of its
owner, rather than a C++ builder named after the new content. Production references
and World packages are not changed by this authoring procedure.

Compiler-source changes still use the existing shared owner fingerprint and may
invalidate several material artifacts at promotion. This is distinct from routine
recipe/instance iteration and does not justify canonical or World regeneration.
Do not claim that this refactor makes all acceptance tooling dependency-minimal.

The [component diagram](diagrams/main.md) shows the authoring and runtime routes.

## Native references

Epic's [Material Instances](https://dev.epicgames.com/documentation/unreal-engine/instanced-materials-in-unreal-engine)
provide shared parents with scalar, vector and texture parameters. Parameter changes
reuse the parent shader; static switches can produce extra permutations. Use admitted
instance controls for style variation, and a distinct family only for a distinct
physical shading/resource contract.

Epic's [Material Functions](https://dev.epicgames.com/documentation/unreal-engine/material-functions-in-unreal-engine?lang=en-US)
provide reusable native graph composition. The installed `MaterialEditingLibrary`,
`UMaterialExpressionTextureSampleParameter2D`, `BlendAngleCorrectedNormals` and
SingleLayerWater expressions are the implementation reference. Availability does
not waive connected-route verification or exact packaged-candidate acceptance.
