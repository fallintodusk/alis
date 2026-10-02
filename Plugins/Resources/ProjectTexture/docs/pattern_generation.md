# Pattern generation

## Contract

ProjectTexture defines reusable structural pattern recipes. Version 3 is deliberately
closed. The schema owns the admitted fields and values; recipes select reviewed native
algorithms and a bounded acyclic graph rather than serializing Unreal expression
graphs.

| Field | Current value |
|---|---|
| Schema | `3` |
| Family | `surface_structure` |
| Algorithm envelope | `runtime_dag_native` |
| Compiler | `5` |
| Output contract | `surface_structure_rgb_v1` |
| Export | named runtime output slots |

The terrain graph generates `natural_noise_basis_native` once, then feeds that cached
parent into `natural_ground_structure_native`. Its exported `Structure` slot is one
linear RGBA8 render target with a full mip chain, wrap addressing, and trilinear
filtering. Its declared
RGB signals are `R=MacroVariation`, `G=CoverPatch`, and `B=GroundDetail`; alpha is
undefined and unused. The basis packs its medium structure with high-frequency detail
into RGB before the child derives the exported tile, so no DAG edge reads parent alpha.
The current `GroundDetail` combines basis aggregate noise at 24 repeats per tile,
basis detail at 64 repeats per tile, and medium noise at 12 repeats per tile. A
consumer's metric tile scale therefore maps to signal periods of that scale
divided by 24, 64, and 12, respectively; those embedded frequencies matter
when choosing a material sampling scale.
The render target is configured as non-sRGB data and DAG parents are sampled with
`LinearColor`; the RGB channels are not display-color values.
The Canvas material route uses blend semantics rather than a raw RGBA data copy, so
ProjectTexture keeps opacity constant and does not contract on destination alpha. These
are visual structural signals, not canonical land-cover facts.

The runtime catalog is generated as an ordinary data asset. A stock
`UTextureRenderTarget2D` is the stable material-facing object, with output-slot user
data linking it to the catalog and DAG node. The engine subsystem requests that slot,
generates each structural identity once on the GPU, and makes the same object ready for
every consumer. Packaged targets do not parse JSON. Generation runs only in Game, PIE,
and game-preview worlds; in an Editor world the target keeps its clear color, so
appearance driven by the pattern signals is visible only in those worlds.

The cache has one writer per structural identity, explicit generating/ready/failed
states, pin counts, a byte budget, and least-recently-used eviction of unpinned entries.
An intermediate DAG target may be recycled after its descendants are complete. The
current 512x512 RGBA8 mip chain occupies 1,398,100 resident bytes; the two-node DAG
peaks at 2,796,200 bytes while both nodes are live.

Packaged Development runs can opt into `-ProjectTextureColdWarmProof`. The
runtime log records the first output request-to-Ready duration, immediately
requests the same slot again, and records the warm duration, generation counts,
cache hit, resource reuse, and resident/peak bytes. Ready follows the existing
render-thread flush, but this timing does not claim GPU-complete latency. The
proof flag changes no recipe, identity, generated asset, or cache policy.

Shape, frequency, seed, graph topology, and output layout belong here. Tint, roughness,
blend weight, wetness, metric scale, and per-output influence remain ProjectMaterial
parameters and do not change pattern identity.

Unknown fields, algorithms, versions, path escapes, or out-of-range structural values
fail before mutation. A new kernel requires a reviewed algorithm and schema/compiler
version; recipes cannot add Unreal nodes.

## Identity

The recipe folder and safe catalog/descriptor IDs map to concern-named objects under
`/ProjectTexture/Patterns`. There is no `Generated` path segment. Semantic identity
contains normalized recipe semantics, graph topology, the compiler fingerprint, final
output identity, and UE Major.Minor compatibility. A launcher changelist on the same
engine line does not change identity by itself; a different engine line does.

The accepted manifest is sorted by output identity, uses LF bytes, and records the
output contract with recipe, compiler, engine-compatibility, semantic, and package
hashes. Consumers resolve a stable pattern ID through this manifest; they do not copy
the generated object path or semantic hash into their own recipes. A package becomes
accepted only after its exact saved bytes are recorded. Recipe validation alone does
not claim that a runtime DAG has generated or rendered.

A consumer's identity binds only the object path and `output_contract`; the semantic
identity and package hash authenticate the pattern it consumed but never force it to
rebuild. The contract name therefore carries every consumer-visible property: the
compiler's reload verification rejects a saved slot whose format, color space,
addressing, filtering, or mips differ from what `surface_structure_rgb_v1` admits. What
each RGB channel means cannot be checked by code, so a change to it, like a change to
any of those properties, publishes a new `output_contract`.

Each record also stores `recipe_source_sha256`, the SHA-256 of the recipe's UTF-8 bytes
after dropping one byte-order mark and converting CRLF and CR to LF, which public binary
transport uses to authenticate a published recipe. A formatting-only edit rebuilds no
asset; regeneration rewrites only the manifest. `Validate` is read-only and accepts only
when a regeneration would skip every output and leave the manifest bytes unchanged.

Explicit orphan cleanup deletes a pattern package only when no on-disk package references
it, after one full synchronous Asset Registry search; it refuses if gathering is still in
progress. A deletion that cannot resolve, stay inside the output root, or complete rejects
the run and the host restores the prior state. A referenced orphan is retained on disk but
leaves the accepted manifest; the host receipt lists it in `retained_orphans`, later
cleanup does not reconsider it, and its disposal is a manual, reviewed change.

## Boundaries

- ProjectTexture owns structural recipes, native DAG kernels, runtime generation,
  output slots, cache policy, readiness, and disposable generated pixels.
- ProjectMaterial consumes accepted output identities and owns appearance and
  projection policy.
- Canonical domain owners define semantic facts.
- ProjectWorld and realization adapters do not call or own the texture cache.
- Editor compiler code and manifests never load in packaged targets.

Generated resource cost scales with unique reusable structure, not cells, cities,
actors, material instances, or other consumer population.
