# Surface generation

## Contract

ProjectMaterial compiles closed surface recipes into persistent Unreal assets. The
terrain/object recipe schema is v4 with graph compiler v5; the isolated soil
variant uses schema v5 with graph compiler v6. The schema owns the admitted
fields and values:

- `Data/Schemas/surface-recipe.schema.json`
- `Data/Schemas/surface-manifest.schema.json`

Recipes live under `Data/SurfaceRecipes/`. Their relative folder and safe material ID
deterministically map to `/ProjectMaterial/Surfaces/`. Unknown fields, unsupported
families or archetypes, path escapes, duplicate identities, unowned parents, and
unmatched semantic bindings fail before mutation.

Surface recipes name only a stable ProjectTexture `pattern_id`. The compiler resolves
that ID through the accepted ProjectTexture manifest and authenticates the resolved
output path, output contract, semantic identity, and exact package bytes. It depends
on a named output ABI (`surface_structure_rgb_v1` for terrain/object or
`soil_structure_normal_masks_v1` for soil), not a concrete ProjectTexture algorithm
or compiler version. The accepted ProjectMaterial manifest records the full resolved
dependency. Additive ProjectTexture recipe, algorithm, compiler, and other producer-only
manifest fields do not change this consumer projection.

Terrain recipes also consume the plain-data Mesh Terrain layout receipt. The compiler
maps semantic names to adapter channel indices only at this boundary. ProjectWorld
never calls the compiler.

The terrain manifest's `terrain_layout_receipt_sha256` stores the receipt's authenticated
`receipt_sha256` payload identity, not a SHA-256 of the JSON serialization bytes. The
compiler verifies that identity against the receipt payload before using it.

`world_metric_triplanar_2d` projects the reusable tile in world metres for terrain.
`object_metric_triplanar_2d` projects it in local metres for movable objects so motion
does not change the pattern. The `terrain_metric` and `object_metric` archetypes use
three axis samples of the same stable runtime render target. ProjectTexture owns
pattern shape, frequency, DAG topology, and seed;
ProjectMaterial owns metric scale, color, roughness, slope response, hydro response, and
per-channel influence. The material samples the declared RGB signals as linear data;
`GroundDetail` modulates roughness only in those two archetypes. They use mesh geometry
normals without a texture-derived normal perturbation. Alpha is not a material input.

`terrain_detail_metric` is a closed material experiment with the same terrain channels
and structural output contract. Its three macro/cover samples retain `PatternScaleMeters`;
three additional samples use `FineScaleMeters` for fine color, roughness and visual bump.
`FineColorStrength` controls fine brightness variation. `BumpHeightCentimeters` bounds
the artistic shading height to 0-1 cm; it never changes canonical elevation, positions,
collision or geometry normals. Epic's installed `PerturbNormalLQ` derives a world-space
normal from that height. The Substrate normal uses world space, while slope and projection
semantics keep geometry normals. This derivative function has known 2x2 pixel artifacts;
its availability is not visual or performance acceptance. Evaluate aliasing and cost before
adoption or adding more expensive sampling. A zero bump amplitude is the matched control.

Terrain treats `CoverPatch` as visual evidence only. Slope can reduce cover and bias
the appearance toward exposed ground, while `hydro_transition` can darken and wet the
surface. Neither rule asserts canonical grass or rock semantics. CanonicalCompilation
remains the owner of world facts.

## Isolated soil PBR sample

`soil_uv` with projection `uv_unit_meter` consumes the authenticated three-output
[soil bundle](../../ProjectTexture/docs/pattern_generation.md#soil-bundle). It is a
closed opaque Substrate Slab, with three linear texture samples. It does not consume
Mesh Terrain semantic channels. `Data/TestFixtures/Soil/` provides its parent and
instance; those recipes are outside the production surface census.

One UV unit must represent one metre of surface. The unscaled 100 cm BasicShapes
plane satisfies this fixture contract. Its tile period is `PatternScaleMeters`.
A scaled plane or the native BasicShapes sphere's stretched UVs do not establish
physical scale acceptance; the sphere is only a tangent-basis diagnostic. This
UV family is not a landscape/triplanar adoption path.

The material owns soil/grain palette, tile metres, relief centimetres, base
roughness, roughness variation, cavity influence and grain-color influence. It
reconstructs the generated normal's XY/Z slopes, scales by
`(BumpHeightCentimeters / 100) / PatternScaleMeters / 0.025`, then normalizes.
The Slab receives a tangent-space normal; Unreal's Substrate compiler performs
the tangent-to-world conversion. Zero relief is the flat-normal control.
Relief affects shading only; it never changes positions, collision or canonical
geometry and does not generate displaced-geometry cast shadows.

Dry soil is represented as a dielectric: Slab F0 is 0.04, with no metallic map.
Masks G independently modulates clamped roughness; Masks R feeds the native
material AO input through an independent influence. Masks B and grain structure
supply color variation without directional lighting. Specular/metallic textures
are not required for these constants, but no fixed two-texture limit applies to
other families. AO response depends on the actual lighting/rendering path;
wiring or parameter readback alone is not evidence of a visible AO effect.

Generation uses the existing ProjectMaterial compiler and fixture authority.
Material and source package authentication, connected texture/channel routes,
parameter defaults, linear samplers and exact sibling bindings are verified after
save/reload. Texture membership alone cannot establish normal, roughness, cavity
or pigment routing. No
parallel material compiler or persistent live writer is introduced. The transient
JSON preview command below remains the authenticated terrain operation; its soil
extension is not verified. Isolated soil diagnostics may tune native MIDs on the
frozen JSON-compiled sample without saving. Such values are provisional controls,
not persistent recipe acceptance or a proved soil JSON hot-update loop.

## Semantic identity

Accepted identity binds normalized recipe semantics, the compiler fingerprint,
engine Major.Minor compatibility, final object path, the parent package hash for
instances, and only the public contracts it consumes from other owners: the resolved
ProjectTexture pattern ID, object path, and output contract, and, for terrain recipes, the
Mesh Terrain layout ID, version, and `layout_sha256`.

The pattern's semantic identity and package hash and the layout receipt identity are
provenance. Before generation the compiler authenticates the pattern's package bytes and
the receipt payload and records all three, and the developer payload checks the recorded
pattern and parent hashes, but they are not rebuild keys. A ProjectTexture recompute or a
Mesh Terrain adapter or definition change that keeps those contracts therefore rebuilds no
surface: the next `Regenerate` skips every output and rewrites only the manifest's
provenance, and until then `Validate` reports the manifest stale.

The UBT compiler fingerprint covers the sorted non-test compiler source and schema
set through one shared `ExternalDependencies` list. A skip is accepted only when the
semantic identity and current package SHA-256 both match the accepted manifest.

Each record also stores `recipe_source_sha256`: SHA-256 of the recipe's UTF-8 bytes
after dropping one byte-order mark and converting CRLF and CR to LF. Public binary
transport authenticates a published recipe against it without reproducing the recipe
parser. A formatting-only edit therefore rebuilds no asset and compiles no shader, but
regeneration rewrites the manifest's source digest; until then `Validate` reports the
manifest stale.

Manifest paths are owner-relative. Machine paths and personal host identities are not
stored in repository authority.

## Mutation lifecycle

Use the host wrapper; do not invoke the commandlet directly:

```powershell
scripts/ue/material/run_surface_generation.ps1 `
  -Mode Validate `
  -LayoutReceipt <mesh-terrain-layout-receipt.json>

scripts/ue/material/run_surface_generation.ps1 `
  -Mode Regenerate `
  -LayoutReceipt <mesh-terrain-layout-receipt.json>
```

The host owns the project-wide generated-content lock, same-project Editor exclusion,
bounded timeout, exact output/manifest snapshot, transaction journal, authenticated
child receipt, rollback after rejection or interruption, and bounded Current/Previous
evidence. Each accepted regeneration retains the exact state it replaced as one rollback
bundle bound to its operation id, which only the host's `RestorePrevious` mode restores
([host transaction](../../../../scripts/ue/material/README.md)).

`Validate` is read-only: it accepts only when a regeneration would skip every output and
leave the manifest bytes unchanged.

Normal generation reports orphaned compiler outputs without deleting them. Explicit
`-CleanupOrphans` removes only packages inside `Content/Surfaces/` that no on-disk
package references. The commandlet does not gather the Asset Registry at startup, so
cleanup first runs one full synchronous search and refuses if gathering is still in
progress; references held only in memory or in an unrecovered transaction snapshot are
outside what the registry can report. A deletion that cannot resolve, stay inside the
output root, or complete rejects the run, and the host restores the prior outputs and
manifest; an orphan whose file is already absent is not an error.

A referenced orphan stays on disk but is no longer accepted authority: it leaves the
accepted manifest, is never selected for the public payload, and a later cleanup does
not reconsider it. The run lists it in `retained_orphans` of the host receipt and names
its referencers in the commandlet log. Disposing of it is a manual, reviewed change:
remove the references, then delete the package.

## Evaluation policy

### Live instance preview

ProjectMaterialEditor owns a one-recipe transient terrain preview operation. Use an
explicit candidate JSON copy with the same relative folder/material ID as an
accepted instance. The Editor console entry point is:

```text
project.material.preview "<candidate.surface.json>" "<candidate-recipe-root>" "<loaded-mesh-component-path>" 0 "<layout-receipt.json>"
project.material.preview.revert
```

The explicit command is the update trigger; it does not install a filesystem
watcher. MCP can dispatch the command and capture the viewport. It does not parse
recipes or compile materials. `APPLIED`, `REFUSED` and `RESTORED` in
`LogProjectMaterialPreview` are the operation result; console dispatch success
alone does not prove that the candidate was accepted or rendered.

The operation reuses the surface parser, manifest/package authentication, pattern
authority, graph verification, layout contract and instance parameter validation.
It authenticates the accepted instance and parent bytes, admits only compatible
instance parameters, and refuses unsaved baseline asset changes. It compares the
authenticated layout's semantic channels against the actual accepted parent graph;
receipt provenance alone does not require regenerating that graph. This is a
preview of an existing accepted baseline, not current-compiler generation or
promotion. The original compiler fingerprint remains recorded in the unchanged
manifest; preview does not certify it as the current generation identity.

Exactly one selected loaded mesh slot receives a transient MID. If its original
binding is a renderer MID, the preview uses its constant parent and copies its
overrides, including native preview channels. No Mesh Partition private types,
World rebuild request, actor replacement or persistent package mutation is used.
Updating the same slot replaces only the transient candidate; invalid input leaves
the last good binding intact. Revert restores the exact original binding and
selected-slot override representation, removing only padding created by preview. Save
and world-cleanup events discard the preview; external binding changes are reported
and preserved. Existing dirty package state is neither cleared nor saved.

Use real fixed-camera rendered captures to verify the selected component. An
Editor view may render a different transient binding from the saved terrain
provider. After opening a map, establish the rendered baseline and stable binding
before applying a candidate: native preview initialization may still replace a
material after the map-loading API returns. Recover through revert and a fresh
explicit selection after initialization, rather than rebuilding World outputs.
Camera and component-count equality alone do not establish a settled scene:
geometry, preview channels and lighting may still appear asynchronously. Inspect
consecutive unchanged-baseline renders and a rendered revert control. Reject a
comparison if geometry or other unexplained scene changes occur between frames.
Runtime pattern quality still requires a Ready pattern in PIE/game
preview. Persistent generation continues through the locked commandlet host above;
this operation does not enable persistent in-editor saving or production adoption.

### Isolated graph fixtures

Structural trials use the existing surface compiler under an explicit
`tmp/material/generation/<run>/` root. Place parent/instance recipes in `recipes/`;
generated assets and authenticated records belong to `content/` and `manifests/`.
Disposable packages use `/ProjectMaterialTest/Surfaces`, mounted only at
`/ProjectMaterialTest/`. After confinement checks, the Editor module registers a
native asset-reference domain for that root. It admits self/Engine references and
ProjectTexture only; production domains cannot reference fixtures. Restriction
switches and existing rules stay unchanged. No configuration is saved by this route.
The rule is held in the native settings object during fixture work and removed at
engine pre-exit or module unload only if its owned entry is unchanged. Cleanup runs
before UObject teardown; late module teardown never recreates a settings object.
The native fixture test schedules cleanup on every return through the automation
latent queue, after the next-tick native save validation has run. It restores the
entry settings, retaining an owned fixture domain only when that domain was already
present before the test. Immediate teardown after saving would invalidate the
queued reference checks; validation remains enabled throughout this lifecycle.
This settings object is config-backed: an unrelated save of those settings could
persist the rule. Use
this route in isolated fixtures and do not save native policy settings during it.
`/Temp/` is excluded from Asset Registry synchronous scans, and the nested
`/Extra/ProjectMaterialTest/` mount loses gathered dependency data because the
registry expects the top-level mount. Both undermine unloaded-referencer safety.
The Editor commands are:

```text
project.material.fixture.generate "<fixture-root>" "<layout-receipt.json>"
project.material.preview.fixture "<candidate.surface.json>" "<candidate-recipe-root>" "<fixture-root>" "<loaded-mesh-component-path>" 0 "<layout-receipt.json>"
project.material.preview.revert
```

Generation refuses an active preview and requires a confined test root. Before
retargeting the test mount, the compiler checks every requested output identity
against the actual loaded package file. Dirty packages and packages loaded from
another or unknown file refuse without changing the mount or assets. Generation neither
replaces existing outputs without their transaction owner nor mutates production.
The physical output directory must end in `Surfaces`, matching the admitted package
root; malformed direct requests refuse before mount changes or output writes.
Keep candidate parameter copies separate from the fixture's accepted `recipes/`.
Fixture preview validates the unchanged compiled fixture through the current owner,
then authenticates its instance, parent, pattern, bytes, channels and mount. It checks
the instance and parent packages' loaded file origins against the authenticated files;
mount selection alone cannot authenticate cached UObjects. It separately
authenticates the selected renderer's accepted production terrain binding. Substitution
admits compatible terrain contracts only and reuses the transient override/revert lifecycle.
`FIXTURE_READY`, `APPLIED_FIXTURE`, `REFUSED` and `RESTORED` are the terminal results;
console dispatch is not proof. The ordinary production preview still requires its own
accepted instance. Fixture saving authenticates disposable assets, not production adoption
or persistent updates of retained live consumer references.

Generated surface families sample the accepted ProjectTexture runtime output directly.
The expensive structural DAG runs once per unique identity; visible consumers perform
bounded triplanar texture samples rather than repeating that procedural work per pixel.
Runtime pixels and cache entries are disposable adapter state, never material or world
authority.

The current design does not add Material Cache, RVT, runtime Texture Graph, custom
virtual-texture producers, or persistent disk caches. Add another cache layer only for
a measured duty the ProjectTexture cache does not already own.

Generated resource cost scales with unique reusable structure, not world population or
consumer count. Appearance-only regeneration must not rewrite ProjectWorld packages.

## Verification

Focused C++ tests own schema, semantic and source identity, pattern authority, layout
binding, graph, reload, integrity, idempotence, and dependency locality;
`Project.Material.Generation.SurfaceIdentityFirewall` proves that producer provenance
leaves surface identity unchanged while a changed pattern contract or channel layout
changes it. The host transaction test owns the real fresh-process zero-write rerun,
injected failure rollback, the formatting-only manifest refresh, an instance-only edit,
a pattern regenerated under the same contract in a ProjectTexture test mount (no surface
rebuild, provenance refresh only), referenced-orphan retention, a failed orphan deletion
with recovery, and the cleanup refusal while a restorable snapshot exists. Test
generation is confined to `tmp/material/generation/`.
