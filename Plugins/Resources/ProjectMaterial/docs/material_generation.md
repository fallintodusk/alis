# Surface generation

## Contract

ProjectMaterial compiles closed surface recipes into persistent Unreal assets. The
terrain/object recipe schema is v4 with graph compiler v5. Aggregate PBR,
height/normal/masks PBR and native SingleLayerWater recipes use schema v7 with
graph compiler v8. Metric height/mask composition uses schema v8 and graph compiler
v9. Semantic metric terrain uses schema v9 and graph compiler v10. The schema owns the admitted
fields and values:

- `Data/Schemas/surface-recipe.schema.json`
- `Data/Schemas/surface-manifest.schema.json`

Recipes live under `Data/SurfaceRecipes/`. Their relative folder and safe material ID
deterministically map to `/ProjectMaterial/Surfaces/`. Unknown fields, unsupported
families or archetypes, path escapes, duplicate identities, unowned parents, and
unmatched semantic bindings fail before mutation.

Generated-pattern surface recipes name a stable ProjectTexture `pattern_id`. The compiler resolves
that ID through the accepted ProjectTexture manifest and authenticates the resolved
output path, output contract, semantic identity, and exact package bytes. It depends
on a named output ABI (`surface_structure_rgb_v1` for terrain/object,
`soil_structure_normal_masks_v1` for height/masks PBR, or
`aggregate_signals_normal_v1` for aggregate PBR), not a concrete ProjectTexture algorithm
or compiler version. The accepted ProjectMaterial manifest records the full resolved
dependency. Additive ProjectTexture recipe, algorithm, compiler, and other producer-only
manifest fields do not change this consumer projection.

`single_layer_water` consumes a fixed set of Epic Water/Engine resources instead of a
generated pattern. It rejects a `pattern` field and does not read a ProjectTexture
manifest or create a texture cache entry. ProjectMaterial remains the compiler
and material authority; Epic owns the native texture and material functions.

Surface variety belongs to recipes and instances sharing reusable shader parents.
Compiler families express physical shading and output ABI requirements, such as
opaque Substrate Slab versus SingleLayerWater, rather than every surface's content
name. Reuse native material functions or cohesive compiler helpers for common
projection, normal orientation and macro composition. A new asphalt, earth or
concrete style does not itself justify another builder, compiler or cache.
The admitted asphalt and river graphs expose their palettes and appearance controls
through JSON; their presence does not establish recipe-driven structural generation.
ProjectTexture owns that separate contract.

Terrain recipes also consume the plain-data Mesh Terrain layout receipt. The compiler
maps semantic names to adapter channel indices only at this boundary. ProjectWorld
never calls the compiler.

The terrain manifest's `terrain_layout_receipt_sha256` stores the receipt's authenticated
`receipt_sha256` payload identity, not a SHA-256 of the JSON serialization bytes. The
compiler verifies that identity against the receipt payload before using it.

`world_metric_triplanar_2d` projects the reusable tile in world metres for terrain
and the aggregate family on stationary walls, roofs and slopes.
`world_metric_xy_2d` projects in world XY metres for `aggregate_metric` and
`single_layer_water` and `height_masks_metric`. It is a distinct admitted frame,
not a triplanar alias.
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

## Metric height/mask PBR composition

`height_masks_metric` consumes the same authenticated three-output bundle as the
UV sample below. Its world-metric projection samples two tile scales and a broad
macro scale. Native HeightLerp selects the aggregate composition; secondary normal
slopes account for the secondary physical scale. Geometry normals and detail
normals are composed through the installed native world-normal function. Palette,
roughness, cavity influence and the distance-dependent macro contribution are
independent recipe controls. Mip filtering remains active at every distance.

The parent keeps one opaque Substrate Slab and shared texture parameters. Macro
and detail samples reuse the same resources. Appearance edits require no source
texture generation, displacement or World geometry changes. The closed graph
verifier compares connected native expressions, samplers and defaults against the
recipe without recompiling or saving during verification. The complete parent and
instance examples are under `Data/TestFixtures/MetricHeight/`.

The positive macro-distance interval is validated before graph construction.
This family consumes no canonical land-cover channels and does not assert universal
soil semantics. Packaged candidate cost and the actual consuming terrain remain
separate acceptance surfaces.

`terrain_height_masks_metric` reuses that detail graph and the same three outputs
inside the existing `ground`/`hydro_transition` composition. It requires the terrain
layout receipt and native Mesh Partition channel receivers. Pure ground retains the
original soil diffuse response; hydro weights select the recipe's `HydroTint`, with
the existing `HydroDarkening` transition. One Substrate Slab, the normal frame and
source maps remain unchanged. The direct `height_masks_metric` family is a scoped
compacted-ground material, not a semantic replacement for `terrain.default`.

The native channel texture and custom primitive data belong to the terrain adapter.
A transient terrain override must retain the actual renderer's channel texture and
primitive context; the parent expression's default void array is not canonical data.
Bind only ProjectTexture's structural slots from the authenticated pattern. Do not
overwrite terrain context while copying or authenticating those structural bindings.
Actual compiled-channel rendering and packaged consumption remain separate proofs.

## Isolated soil PBR sample

`height_masks_uv` with projection `uv_unit_meter` consumes the authenticated three-output
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
or pigment routing. No parallel material compiler or persistent live writer is
introduced. Authenticated fixture preview has parameter/rejection/revert coverage
for metric height/mask instances, including the semantic terrain variant and
independent parent/instance defaults. Real terrain evaluation also requires the
adapter's actual channel texture and primitive context; an isolated control does
not establish their rendered content. Reversible native city overrides and
persistent recipe/backend adoption remain distinct acceptance boundaries.

## Metric aggregate surfaces

`aggregate_metric` consumes `aggregate_signals_normal_v1`. Its closed world-XY
graph uses the generated Signals and Normal outputs with a separate broad
pigment/roughness sample of the same Signals texture. Epic's
BlendAngleCorrectedNormals aligns the decoded detail with the geometry normal;
one opaque Substrate Slab receives the final shading inputs. This does not
displace vertices or change collision.

The same physical family also admits world-metric triplanar projection. Its
encoded-normal adapter samples the existing linear output in three planes,
decodes it, preserves derivative signs, reverses only each outward axis and
blends against geometric normals. Pigment and roughness use the same projection
with an independently scaled macro sample. This is a projection choice, not a
Concrete-specific generator or another output/cache authority. XY remains the
cheaper accepted road route; parent and instance projection must agree.

The tested installed Epic WorldAlignedNormal route expects signed normals after
native normal-sampler decoding. Merely compiling it with a LinearColor texture
object does not decode ALIS's encoded RGB output. Flat and signed-flat rendered
controls distinguish that mismatch. Do not change the shared texture ABI to
accommodate a shader function. Native expression-based adaptation belongs here;
six-axis, slope and independently expected directional-normal proof accompany
any adoption. A generated aggregate variation is not exact replay of another
source algorithm merely because its palette looks similar.

`Data/TestFixtures/Asphalt/` owns its isolated parent and instance recipes.
Loaded source admission requires a clean matching same-package catalog and
complete linear/mip/wrap/filter descriptors. Disk hashes cannot authenticate
unsaved loaded texture changes. Save/reload verification checks connected metric,
palette, roughness, macro and normal routes, including recipe defaults.
Public ABI admission and Editor source fidelity are separate from opt-in backend
execution, mixed-producer memory policy and packaged frame-cost acceptance.

Instances may select a different authenticated pattern from their shared parent's
default when its output ABI is compatible. The selected stable slots override the
parent texture parameters. Save/reload verification checks effective textures,
scalars and vectors, including inherited values. A content name or appearance
variation does not create a new shader family. The existing height/masks family's
palette parameter names remain its admitted ABI; its UV fixture is not the frozen
BrownEarth world-projection candidate.

The production [Concrete parent](../Data/SurfaceRecipes/Aggregate/M_ProjectAggregateTriplanar.surface.json)
and [default instance](../Data/SurfaceRecipes/Aggregate/MI_ProjectConcrete_Default.surface.json)
use this triplanar family for stationary building massing. The instance publishes
the semantic binding consumed by World presentation profiles. Fine relief and broad
pigment/roughness variation share the same generated bundle and one Slab; ordinary
mip filtering retains distance filtering without an additional shading layer.
These defaults provide concrete massing, not facade geometry or material diversity.

## Native river sample

`Data/TestFixtures/River/` defines the parent and instance for a world-metric calm
river. They are outside the production census. The opaque Single Layer Water
graph uses Epic's WaterTextureWaves, DeriveNormalZ and BlendAngleCorrectedNormals
with its native softened water normal texture. It reproduces the connected wave,
world-normal and optical routes through the existing surface compiler. It does
not alter water geometry or canonical elevation.

The compiler refuses dirty loaded native packages before writing. Accepted
manifests authenticate the exact native resource paths and disk hashes instead
of pattern provenance. Native resource bytes participate in this material's
artifact identity because its graph consumes them. Save/reload verification
checks connected normal, optical, metric, time and parameter-default routes;
merely finding the expected expression classes is insufficient.

Transient JSON preview authenticates the accepted parent recipe and its native
resources independently of the edited instance parameters. Persistent generation
uses the same standalone writer and rollback boundary as other surfaces. Editor
compiler acceptance does not establish packaged water-pass cost or authorize
serialized World material-reference migration.

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

Before writing or accepting an unchanged skip, whole-request preflight checks
declared parent/instance family and projection compatibility, every selected
output ABI, loaded package origin and clean state, and complete source descriptors.
A later invalid source refuses before an earlier parent is saved. Dirty in-memory
resources cannot pass solely because their disk hashes still match. This preflight
does not replace the host's rollback for failures during actual saving.

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

Copied renderer MID controls cannot retarget structural textures: preview explicitly
binds the authenticated candidate's effective texture parameters after copying
overrides. Missing authenticated textures refuse before the visible binding changes.
The original MID and its overrides are retained exactly for revert.

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
project.material.preview.fixture "<candidate.surface.json>" "<candidate-recipe-root>" "<fixture-root>" "<loaded-mesh-component-path>" 0 "<layout-receipt.json>" ["<pattern-fixture-root>"]
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
authenticates the selected renderer binding. A renderer already bound to the fixture's
authenticated instance can preview that family's parameter changes directly. The
optional pattern fixture root is confined to `tmp/texture/generation/<run>/` and
requires the material fixture authority; it does not retarget production authority.
Native water needs no pattern fixture root or terrain layout; pass `unused` in the
layout position for that non-terrain family. Substitution on a production terrain
renderer still admits compatible terrain contracts only and separately authenticates
its original accepted production binding. Non-terrain fixture substitution on
another renderer refuses. Both routes reuse the same transient override/revert lifecycle.
`FIXTURE_READY`, `APPLIED_FIXTURE`, `REFUSED` and `RESTORED` are the terminal results;
console dispatch is not proof. The ordinary production preview still requires its own
accepted instance. Fixture saving authenticates disposable assets, not production adoption
or persistent updates of retained live consumer references.

Generated-pattern surface families sample the accepted ProjectTexture runtime output directly.
The expensive structural DAG runs once per unique identity; visible consumers perform
bounded texture samples rather than repeating that procedural work per pixel.
Runtime pixels and cache entries are disposable adapter state, never material or world
authority.

The material consumer adds no Material Cache, RVT, Texture Graph dependency, custom
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
