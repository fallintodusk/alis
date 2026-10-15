# ProjectMaterial pitfalls

## Semantic metric preview rejected independent instance values

**Symptom:** A generated terrain instance was valid, but authenticated preview
rejected its appearance when roughness differed from the shader parent's default.

**Root cause:** Preview treated every terrain family as the accepted inline
triplanar template. Metric graph verification instead compares connected graph
defaults, so substituting the instance recipe for the parent recipe was incorrect.

**Fix:** Authenticate the actual saved parent recipe for both metric height/mask
families, independently of edited instance values. Keep the accepted inline
triplanar verification path scoped to its real templates.

**Regression test:** `Project.Material.HeightMasks.AuthenticatedMetricSample`
generates distinct parent/instance defaults, tunes the instance, rejects invalid
JSON without replacing the last good consumer, reverts and validates saved assets.

## Compacted-ground detail replaced terrain semantics

**Symptom:** A matched river becomes brown after a full-terrain soil override, while
the same water material over the original terrain remains green.

**Root cause:** The isolated metric soil family consumes no canonical terrain channels.
Applying it everywhere discards ground/hydro composition. Native SingleLayerWater
also samples scene color behind water, so terrain changes affect the water result.
The color comparison alone does not prove a geometric clearance defect.

**Fix:** Use `terrain_height_masks_metric` for semantic terrain composition, reusing
the original three detail maps and one Slab. Authenticate the native channel layout
and retain the actual renderer channel texture and primitive context. Keep the direct
metric family scoped to compacted-ground samples. Preserve accepted water parameters.

**Regression tests:** `Project.Material.HeightMasks.SemanticTerrainRecipe` and
`Project.Material.HeightMasks.AuthenticatedMetricSample`. Actual compiled-channel
rendering, packaged consumption and product visual acceptance are separate checks.

## A generated geometry instance repaired its usage flag in the Editor

**Symptom:** A city road preview dirtied its accepted asphalt instance when Unreal
automatically added Nanite usage. The same missing flag selects a fallback in Game.

**Root cause:** The family graph and texture bindings were correct, but generation
had not declared its vertex-factory use before compilation. In UE 5.8, constant
instances derive their own usage flags from the parent during native updates.

**Fix:** Declare Nanite use on opaque geometry parents before compilation. Use the
normal native instance update; authenticate direct generated-bundle parent and
instance flags, including refusal of an explicit false override. Preserve the
real accepted triplanar preview contract, whose compiled section instances belong
to Mesh Partition. Single Layer Water retains its non-Nanite
geometry contract. Do not rely on Editor auto-repair or add per-instance compilers.

**Regression tests:** `Project.Material.Asphalt.AuthenticatedSample` and
`Project.Material.HeightMasks.AuthenticatedMetricSample`. City verification must
also prove no usage warning and no dirtied generated material packages.

## Fixture cleanup held packages across owner regeneration

**Symptom:** An authenticated sample crashed inside native package unload at cleanup.

**Root cause:** Raw package pointers survived an owner save/unload/reload and garbage
collection. Multiple output slots could also insert the same package more than once.

**Fix:** Retain unique owned package names, resolve the currently loaded packages
at cleanup, and unload that live set. Do not pin obsolete packages across owner reload.

**Regression tests:** The same two authenticated samples exercise regeneration,
refusal, reload and normal test shutdown.

## Shared helpers collided in a Unity build

**Symptom:** The normal Editor build reported duplicate `Mask` helpers when
Unreal combined aggregate and water source files. Renamed compiler files also
failed the matching-header-first check.

**Root cause:** Anonymous namespaces from separate files shared a translation
unit, while imported expression helpers made lookup ambiguous. Adaptive builds
of individual modified files did not exercise that combined translation unit.

**Fix:** Each compiler implementation uses its own named private namespace and
function-local imports. Each source includes its matching header first.

**Regression proof:** The normal project build, with the combined material module,
and focused authenticated aggregate/water material tests.

## A material instance inherited the wrong structural source

**Symptom:** A valid appearance recipe selected another compatible pattern but its
saved instance still sampled the parent's default. World-XY recipes also parsed
into the object projection enum, losing the declared frame in semantic identity.

**Root cause:** Instance generation set scalar/vector values only, while the parser
treated every non-triplanar spelling as object projection. Disk-only source checks
also deferred loaded-source refusal until individual builds.

**Fix:** Admit world XY explicitly. Authenticate compatible source slots, bind the
recipe-selected textures on instances and verify effective values after reload.
Validate every source and declared parent family before output writes or skips.
Preview authenticates saved bindings separately from edited candidate values.

**Files:** [pattern bindings](../Source/ProjectMaterialEditor/Private/ProjectMaterialPatternBindings.cpp),
[recipe contract](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceRecipe.cpp),
[generation service](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceGenerationService.cpp)
and [preview authority](../Source/ProjectMaterialEditor/Private/ProjectMaterialPreviewAuthority.cpp).

**Regression tests:** `Project.Material.Asphalt.AuthenticatedSample` covers shared
parents, independent selected patterns, edited preview values, known-wrong binding
and scalar refusal, and a later dirty source with zero earlier output writes.
`Project.Material.Preview.LiveRecipeRevert` retains the actual accepted triplanar
inline-source content route without forcing a production material migration.
`Project.Material.River.AuthenticatedNativeSample` covers live JSON/MID preview and
exact restoration on an authenticated water fixture, without a fabricated terrain
layout dependency. The aggregate test exercises the same public fixture operation
with an independently bound pattern and repeated parameter preview.

## A custom scratch mount had no native reference domain

**Symptom:** An otherwise valid live fixture intermittently failed automatic asset
validation with "Could not find a matching domain" for its parent reference.

**Root cause:** Mount registration does not define an asset-reference domain.
Earlier passing runs had not reliably gathered the saved dependency before the
native validator ran. `/Temp/` prevents Asset Registry synchronous scans.
The nested `/Extra/ProjectMaterialTest/` mount also loses dependency data because
the registry expects its top-level mount; neither preserves unloaded referencers.
Removing a fixture's policy immediately after saving also invalidates queued
reference validation: native `ValidateSavedPackage` schedules it for the next tick.

**Fix:** Keep `/ProjectMaterialTest/` and register its missing native reference
domain after fixture confinement. Only self/Engine and ProjectTexture are visible.
The module does not save configuration and removes its unchanged owned rule on
shutdown. The settings object is config-backed; isolated fixture work must not
save those settings through another route. Production policy remains enabled.
The [fixture lifecycle](material_generation.md#isolated-graph-fixtures) keeps test
cleanup after queued save validation and restores the original policy state.

**Files:** [generation service](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceGenerationService.cpp)
and [fixture domain](../Source/ProjectMaterialEditor/Private/ProjectMaterialFixtureDomain.cpp).

**Regression test:** `Project.Material.Preview.AuthenticatedTerrainDetailFixture`
synchronously gathers its saved parent, instance and an unloaded on-disk
referencer, then checks dependencies, referencers, native allowed/denied references
and policy removal, early-return cleanup and restoration at test completion.
The exact host unloaded-referencer test covers cleanup in a
fresh commandlet process.

## An unmapped fixture probe asserted instead of refusing

**Symptom:** A cold Editor crashed while checking a fixture before its mount existed.

**Root cause:** `LongPackageNameToFilename` asserts on an unmapped root.

**Fix:** Boundary probes use `TryConvertLongPackageNameToFilename` and refuse
unknown mount state before loading assets. Use an explicit FString to select the
installed engine overload.

**Files:** [preview authority](../Source/ProjectMaterialEditor/Private/ProjectMaterialPreviewAuthority.cpp)
and [generation service](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceGenerationService.cpp).

**Regression test:** `Project.Material.Preview.AuthenticatedTerrainDetailFixture`
checks the unmapped preview in a fresh Editor.

## A test mount changed while its old materials stayed loaded

**Symptom:** A fixture in another directory reused the first fixture's loaded
material objects despite authenticating the new directory's package bytes.

**Root cause:** Unreal caches packages by package name. Changing a mount does not
change an existing package's loaded file origin; named asset creation or loading
can reuse those objects.

**Fix:** Preflight all requested output package origins before retargeting the
test mount or generating assets. Refuse dirty, unknown and different origins.
Fixture preview checks the loaded instance and parent against their authenticated
physical files before binding the transient material.

**Files:** [generation service](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceGenerationService.cpp)
and [preview authority](../Source/ProjectMaterialEditor/Private/ProjectMaterialPreviewAuthority.cpp).

**Regression test:** `Project.Material.Preview.AuthenticatedTerrainDetailFixture`
copies a disposable fixture to another root with identical IDs and verifies both
generation and preview refusals preserve the mount, objects, bytes and binding.

## Native function input display names include types

**Symptom:** A present native bump function is rejected as having an unexpected input.

**Root cause:** `UMaterialExpressionMaterialFunctionCall::GetInputName()` returns the
display name including its type, such as `Bump (S)`. It is not the stored contract name.

**Fix:** Validate the function identity and input count, then compare the stored
`FunctionInputs[index].Input.InputName` before connecting it.

**File:** [ProjectMaterialSurfaceBuilder.cpp](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp).

**Regression test:** `Project.Material.Preview.AuthenticatedTerrainDetailFixture`.

Native material-expression connections also use the Editor library's shortened pin
names. Query `GetMaterialExpressionInputNames` before connecting; a texture sample's
explicit `MipLevel` input is exposed as `Level`. The installed MaterialEditingLibrary
uses `UMaterialGraphNode::GetShortenPinName` for this mapping. Connecting the raw name
refuses; the enumerated name compiles and supports explicit mip readback.

## Editor property assignment trims scratch material holders

**Symptom:** Restoring a disposable component's complete `OverrideMaterials` through
`set_editor_property` reduces its array to the mesh's actual material-slot count.

**Root cause:** `UMeshComponent::PostEditChangeChainProperty` calls
`CleanUpOverrideMaterials` for that property. Out-of-range references held by older
diagnostic scripts are removed; a successful property write is not exact restoration.

**Fix:** Preflight all retained objects, restore existing scratch references with
native `SetMaterial`, and compare the full resulting array against its snapshot.
Extra slots are a disposable experiment detail, not a production resource owner;
do not disable the engine cleanup or use this retention pattern in shipped content.

**Native owner:** `Components/MeshComponent.cpp`.

**Proof:** The property route reduces 245 existing scratch entries to one and the
exact-array probe rejects it. Native restoration recovers all 245 original identities;
source hashes, selected material, lighting and protected packages remain unchanged.

## Soil color comparisons confused material, lighting and display output

**Symptom:** A soil sample appears beige under one light and nearly black/red under
grazing light, although its source maps and normal calculations are unchanged.

**Root cause:** An atmosphere-connected sun changes spectral transmission when
rotated toward the horizon. Fog, ambient lighting, exposure, grading and output
encoding also affect the final image. `RTF_RGBA8` uses linear display gamma;
displaying its final-LDR bytes as ordinary sRGB exaggerates darkness. PNG RGB
values do not identify a material defect or establish scene-linear clipping.

Real-time Skylight freshness is a separate concern. Native
`FScene::AllocateAndCaptureFrameSkyEnvMap` returns for SceneCapture views;
repeated scene captures alone do not refresh that sky data. A property snapshot
or queued recapture is not evidence that the rendered ambient light is current.

**Fix:** Freeze all source targets and copy all native instance parameters into
diagnostic MIDs, including repeated texture bindings such as `SoilMacro`. Inspect
lighting and postprocessing, then separate unlit Base Color, float scene HDR and
display-encoded final LDR. Use native `RTF_RGBA8_SRGB` for display captures while
keeping Structure/Normal/Masks linear. Use gray/white surfaces and flat-normal/AO
controls. A controlled specified cubemap avoids evolving ambient data during a
palette comparison; main-view redraw is needed when testing/restoring real-time
sky. Exclude fog/atmosphere through diagnostic capture flags, not project settings.
Restore original bindings, lighting and full scratch reference arrays explicitly.

**Native owners:** `TextureRenderTarget2D.cpp` (`GetDisplayGamma`),
`SceneCapturePixelShader.usf`, `ReflectionEnvironmentRealTimeCapture.cpp`
(`AllocateAndCaptureFrameSkyEnvMap`) and `SceneCaptureComponent.cpp`.

**Proof:** Native unlit gray/white controls recover their specified values within
GBuffer quantization. Gray under the warm grazing setup rejects the neutral-color
check; the controlled rig passes. Matched linear/display output shows the native
display transfer without changing any numeric source target. Full-binding checks
reject a MID that silently inherits a different macro target.

## A copied Substrate graph refuses conversion to a UI diagnostic

**Symptom:** A normal diagnostic renders black despite a connected emissive output
and successful material compilation; its domain reads back as Surface after setting UI.

**Root cause:** UE's connected Front Material participates in shading-model sanitation
and restores the Surface domain. Waiting or rebuilding does not remove that connection.
The native render-target draw path already ensures material compilation is complete.

**Fix:** In the isolated diagnostic copy, disconnect `MP_FRONT_MATERIAL` before
selecting UI and connecting the data output to emissive. Check the resulting domain
and actual pixels. Keep the production Slab connected.

**Native owners:** `UMaterial` shading-model sanitation in `Material.cpp` and
`UKismetRenderingLibrary::DrawMaterialToRenderTarget` in `KismetRenderingLibrary.cpp`.

**Proof:** Flat and X/Y normal readbacks match the independently calculated encoded
vectors; removing the secondary metric-scale correction produces a rejected control.

## A flat-plane normal prototype flattens terrain slopes

**Symptom:** A soil prototype has relief on a horizontal plane but lights a terrain
slope as though its geometric normal were world up.

**Root cause:** The prototype supplies its world-up-relative detail directly to a
world-space Substrate Normal input. Native `BlendAngleCorrectedNormals_WS` is not
an interchangeable fix: its quaternion rotates from `VertexNormalWS` to BaseNormal,
so supplying `VertexNormalWS` as BaseNormal makes that rotation an identity.

**Fix:** In the isolated terrain material, compose the preserved world-up-relative
detail with `VertexNormalWS` using native `BlendAngleCorrectedNormals` and normalize
the result. Keep the existing world-space Slab and metric projection. This does
not add texture samples or establish triplanar projection for steep faces.

**Proof:** Native rendered zero-relief controls at 0, +30, -30 and +60 degrees
retain the geometric normal; the horizontal full-relief control retains its
original pixels. Check the installed function inputs and rendered controls before
promoting a prototype into the compiler.

## Native cleanup of transient diagnostic actors dirties the map

**Symptom:** Material bindings, actor count and lighting are restored, but the
original map package remains dirty after removing disposable capture actors.

**Root cause:** `EditorActorSubsystem::DestroyActors` calls
`EditorDestroyActor(..., true)`, which modifies the level even for this diagnostic
cleanup. A transient spawn is not proof that its later removal preserves the
package's dirty flag.

**Fix:** Never save the diagnostic map. After restoring the original references
and removing only owned actors, a native original-map reload without save can
recover a baseline that was initially clean. Verify bindings, geometry bounds,
actor count, lighting, postprocessing and dirty packages afterward. This recovery
must not discard unrelated unsaved work or become the parameter-iteration loop.

Release diagnostic Python wrappers before a native map switch as well. A native
array returned by the dirty-package query can retain the old map package through
`FPyWrapperArrayMetaData` and `FPyReferenceCollector`; `NewBlankMap` then fails its
World GC-leak guard. Keep the preflight in a short function, return only plain
data, release its wrappers and collect Python garbage before destroying the
world. Preserve the dirty-state guard. The rejection stack and a subsequent
successful native map switch discriminate this lifetime failure from corrupt
generated World data.

## Autosave sees a registered diagnostic material as a persistent asset

**Symptom:** Native autosave attempts to save a scratch material and refuses its
references to private render targets belonging to an unsaved diagnostic map.

**Root cause:** Asset duplication registers a persistable package even when the
experiment never calls Save. A scratch mount does not make live World-owned
render targets legal persistent default-texture references.

**Fix:** Keep a live-render-target diagnostic parent under the native transient
package and retain it through its owned MID/component. Remove only explicitly
owned unsaved experimental assets; never clear dirty flags, disable autosave or
weaken validation to make the graph save. Persistent generation must use the
existing authenticated generation route and its supported dependency bindings.

## Substrate water lacks volume outputs required by the ray-tracing shader

**Symptom:** A Single Layer Water BSDF graph fails compilation with undeclared
`GetSingleLayerWaterMaterialOutput0`, `1` and `2` in `RayTracingMaterialHitShaders.usf`.

**Root cause:** The installed UE 5.8 ray-tracing path still reads conventional
volume outputs rather than the connected Substrate water data. Epic tracks the
matching 5.7/5.8 defect as [UE-384718](https://issues.unrealengine.com/issue/UE-384718),
with a target fix in 6.0.

**Diagnostic:** In an isolated graph, add native `SingleLayerWaterMaterialOutput`
and project the same parameter expressions: scattering = albedo * extinction,
absorption = (1 - albedo) * extinction, with shared phase and behind-water scale.
Keep albedo within 0-1 and extinction nonnegative. Do not add independent duplicate
coefficients, custom HLSL, disable validation or change global rendering settings.
The extra output is an engine-consumer projection, not a second water authority.

**Native owners:** `UMaterialExpressionSingleLayerWaterMaterialOutput::Compile` in
`MaterialExpressions.cpp` and the water payload branch in `RayTracingMaterialHitShaders.usf`.

**Proof:** The isolated UE 5.8.3 graph without the volume output fails at the named
symbols; the graph with shared native outputs compiles and renders depth/bed response.
This does not certify packaged or ray-traced visual parity. Production adoption must
prove the exact compiled graph and its consumer paths before promotion.

## Native water-wave averaging amplifies packed normal channels

**Symptom:** The water prototype produces large broken reflection regions;
changing ripple strength barely distinguishes the candidates.

**Root cause:** The installed UE 5.8 `WaterTextureWaves` function's input named
`Divisor` multiplies its connected three-sample sum. Passing 3 amplifies packed
normal channels instead of averaging them. A fourth texture expression exists
but does not contribute to this output. Texture channel semantics also matter:
`T_Water_TilingNormal_With_Height_02_Softened` is linear packed data, not a native
Normal-compressed RGB vector; RG encodes XY and B is height.

**Fix:** Inspect the installed function's connected expressions. Use weight 1/3
for this three-sample output, sample the packed data as linear color, decode RG
and derive Z through the native function. Keep the height channel out of normal
decoding. Do not infer function behavior from an input label or texture name.

**Proof:** An authenticated flat input with RG in [127,128]/255 yields mean RG
4.48047 for weight 3 versus 0.497803 for weight 1/3 in a float target. The wrong
weight fails the encoded-flat check; the corrected weight passes. Matched native
rendering then shows coherent ripples. This isolated proof does not certify a
production water graph or packaged performance.

## Pale close water can be a shallow bright bed rather than missing ripples

**Symptom:** A close river sample looks pale and nearly featureless while distant
water looks dark and reflective with the same material.

**Cause:** Native Single Layer Water reads scene color and depth behind the water.
A bright bed near the surface can dominate transmitted color. Final lit pixels
alone do not distinguish this from a missing surface or normal input.

**Diagnosis:** Authenticate a native top-surface ray, then compare rendered water
depth, bed-only depth, shader normals and flat/ripple arms at the same camera,
lighting and optics. Change bed depth only in disposable controls. Never move
production terrain or deepen water merely to make a material screenshot darker.

**Proof:** Matched shallow/deep native controls retain the same water target and
normal but discriminate bed depth and transmitted color. Their output is a visual
diagnostic, not a city geometry or packaged-performance acceptance.

An opaque building comparison likewise requires front-facing rendered surfaces.
Diagnose the consumed compiled mesh before changing normal projection; the
[World facing pitfall](../../../World/ProjectWorld/docs/pitfalls.md#47-building-collision-and-debug-shading-mask-reversed-exterior-winding)
owns that geometry failure and its correction.

## A SceneCapture omits the lighting used by the live viewport

**Symptom:** A reflective material looks incorrect in screenshots even though
the live viewport uses Lumen.

**Root cause:** A standalone capture without inherited main-view postprocess
settings defaults to GI and reflections None in `SceneCaptureRendering.cpp`.
Copying exposure or repeatedly capturing does not opt into Lumen.

**Fix:** Set the capture's native postprocess overrides for dynamic GI and
reflection method, retain its rendering state and warm the scene before reading
pixels. Record capture lighting and extra fill lights separately from city
lighting. A local multi-view GPU snapshot is not a city or packaged frame budget.

## Preview revert changed an inherited slot's serialized representation

**Symptom:** Preview reverted to the same displayed material, but a component
whose override array was empty now contained one explicit override.

**Root cause:** Restoring `GetMaterial(slot)` with `SetMaterial` cannot distinguish
an inherited material from an explicit override or recover preview-created padding.

**Fix:** The preview retains the original selected-slot override presence and array
length. Revert restores that representation and removes only empty preview padding;
external changes in other slots remain intact. It also reports and preserves an
external replacement of the selected binding.

**File:** [ProjectMaterialSurfacePreview.cpp](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfacePreview.cpp).

**Regression tests:** `Project.Material.Preview.LiveRecipeRevert` and
`Project.Material.Preview.RefusalPreservesLiveState`.

## GroundColor readback changed while cold Editor pixels did not

**Symptom:** A live recipe preview reports the new GroundColor, but the terrain
image remains unchanged in a freshly opened plain Editor.

**Root cause:** The terrain graph blends GroundColor through the reusable
CoverPatch signal. ProjectTexture initializes that runtime output in PIE/Game/
GamePreview, rather than an ordinary Editor world. A cold cleared pattern can
eliminate the GroundColor contribution.

**Fix:** Establish a Ready runtime pattern through its existing owner before
judging cover, detail or scale. Verify the rendered component and actual pixels;
do not change the graph or initialize textures through a second compiler.

**Files:** [surface builder](../Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp)
and [pattern contract](../../ProjectTexture/docs/pattern_generation.md).

**Proof:** A native simulation session initialized the two-node pattern; matched
Editor captures then showed the JSON GroundColor change and its visual revert.
This is a rendered development experiment, not an automated aesthetic assertion.

## Material instance setter reports false after writing

**Symptom:** A generated instance contains the expected value, but an
`UMaterialEditingLibrary` setter reports failure.

**Root cause:** Installed UE 5.8 scalar and vector instance helpers may perform the
write while returning `false`.

**Fix:** The compiler uses native Editor-only setters and verifies typed values after
a fresh package load.

**Regression test:** `Project.Material.Generation.SurfaceAssetCompilerContract`.

## Existing material reload keeps stale expressions

**Symptom:** Replacement saves, but same-process reload sees an old or mixed graph.

**Root cause:** Rebuilding expressions on a loaded material can retain stale UObject
lifecycle state.

**Fix:** The host snapshots the exact output, the compiler reconstructs at the final
identity, then saves, unloads, garbage collects, reloads, and verifies. Rejection
restores the host snapshot.

**Regression tests:** the surface generation reload test and host transaction rollback
test.

## Schema edit leaves the compiler fingerprint stale

**Symptom:** A schema changes but a standard launcher build remains up to date.

**Root cause:** UBT does not infer JSON schema files as C++ makefile inputs.

**Fix:** `ProjectMaterialEditor.Build.cs` registers and hashes one sorted compiler
source/schema set. Do not maintain separate dependency and fingerprint lists.

**Regression proof:** a schema-only edit invalidates the standard makefile and changes
the embedded fingerprint; test-only source does not.

## Producer provenance in surface identity forced cross-owner rebuilds

**Symptom:** A ProjectTexture change that kept the pattern contract, even one to its
manifest or cleanup code, regenerated every surface and recompiled their shaders; a Mesh
Terrain adapter change did the same for terrain surfaces.

**Root cause:** Surface semantic identity hashed the pattern's semantic identity, which
carries ProjectTexture's compiler fingerprint, its package bytes, and the whole layout
receipt identity, although the builder reads only the pattern path and contract and the
channel map.

**Fix:** `ComputeArtifactSemanticIdentity` binds the pattern object path and output
contract and the layout ID, version, and `layout_sha256`. The pattern hashes and receipt
identity stay in the record as authenticated provenance. Do not add producer provenance
back to the identity.

**Regression tests:** `Project.Material.Generation.SurfaceIdentityFirewall` and the host
case for a pattern regenerated under the same output contract.

## A failed rollback deleted its own snapshot

**Symptom:** After a rejected run whose restore also failed, for example on an output
file held open by another process, the next run removed the output root and then could
not find the snapshot to restore.

**Root cause:** The host's final cleanup deleted the operation folder holding the snapshot
while the journal that names it survived.

**Fix:** `run_material_generation.ps1` keeps the operation folder while the journal exists
and reports both the rejection and the rollback failure.

**Regression tests:** the host cases for a failed orphan deletion with recovery, in both
the surface and pattern tests.

## Adaptive unity compilation makes an aggregate helper ambiguous

**Symptom:** The standard Editor build fails with C2668 on `Unit()` in the aggregate
surface builder after the adaptive working set changes.

**Root cause:** Unity compilation places the projection's anonymous-namespace helper
and the surface builder's private helper in one translation unit. The surface builder's
unqualified call imports both overload candidates.

**Fix:** Qualify the surface helper with its owning private namespace. Do not disable
unity compilation or change the material graph to resolve C++ name lookup.

**File:** [aggregate surface builder](../Source/ProjectMaterialEditor/Private/ProjectMaterialAggregateSurface.cpp).

**Regression proof:** the standard Editor build reproduces C2668 and then succeeds
with the qualified call. `Project.Material.Aggregate.TriplanarContract` exercises
the retained material projection contract.
