# Surface generation

## Contract

ProjectMaterial compiles closed surface recipes into persistent Unreal assets. The
current recipe schema is v4 and its graph compiler is v5. The schema owns the admitted
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
on the stable `surface_structure_rgb_v1` ABI, not a concrete ProjectTexture algorithm
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
does not change the pattern. Both use three axis samples of the same stable runtime
render target. ProjectTexture owns pattern shape, frequency, DAG topology, and seed;
ProjectMaterial owns metric scale, color, roughness, slope response, hydro response, and
per-channel influence. The material samples the declared RGB signals as linear data;
`GroundDetail` modulates roughness only. The generated surface uses mesh geometry
normals without a texture-derived normal perturbation. Alpha is not a material input.

Terrain treats `CoverPatch` as visual evidence only. Slope can reduce cover and bias
the appearance toward exposed ground, while `hydro_transition` can darken and wet the
surface. Neither rule asserts canonical grass or rock semantics. CanonicalCompilation
remains the owner of world facts.

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
evidence. Production regeneration retains one exact prior rollback bundle.

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
