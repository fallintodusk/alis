# Pattern generation

## Contract

ProjectTexture defines reusable structural pattern recipes. Its admitted variants are
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
every consumer. Packaged targets do not parse JSON. Automatic generation runs only in Game, PIE,
and game-preview worlds; a cold Editor target keeps its clear color. Focused native
fixtures may explicitly request generation in an isolated Editor world. A capture
without actual Ready pixels cannot prove pattern quality.

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

## Soil bundle

The closed soil variant uses schema v4, compiler v6, family `soil_structure`, and
`soil_structure_normal_masks_v1`. It exports three same-package linear RGBA8
render targets in fixed order; alpha is undefined and unused:

| Slot | RGB data | Generator |
|---|---|---|
| Structure | shading height, clump structure, grain/pore structure | `soil_clump_structure_native` |
| Normal | encoded tangent-space unit vector | `soil_normal_native` |
| Masks | local cavity approximation, roughness structure, pigment structure | `soil_masks_native` |

Normal and Masks each consume Structure. Their layouts are
`soil_normal_rgb_v1` and `soil_cavity_roughness_pigment_rgb_v1`; Structure uses
`soil_height_clump_grain_rgb_v1`. Parsing rejects another order, topology or
export correspondence before asset mutation. The accepted manifest authenticates
the primary Structure path and the exact bytes of the package containing all three
outputs. It does not publish three independent manifests or a second authority.

The shape kernel uses stock material expressions, periodic native noise, irregular
Voronoi clumps/grains and pores. Broad patch weights mix coarse and fine aggregate
populations and sparse grain; restrained warp avoids uniformly stretched ridges.
Pigment and roughness include separate periodic variation rather than sharing only
the elevation signal. Native noise periods are powers of two so the
floating-point wrap does not change integer lattice hashes at interior cell edges.
The normal kernel takes wrapped central height differences at one texel, scales
normalized-UV derivatives by a reference height/tile ratio of 0.025, normalizes and
encodes RGB as `0.5 + 0.5 * N`. ProjectMaterial reconstructs those slopes and sets
the physical height/tile ratio; the generator does not choose a surface's metres,
color, wetness or final roughness. The cavity channel is a local height-neighbor
approximation, not measured occlusion, cast shadows or a prelit color map.

All three outputs have stock full box-filtered mips, wrap addressing and trilinear
filtering. Consumers decode and normalize sampled normals after filtering. This
is a linear RGBA8 resource contract, not an imported BC5 normal texture or an
assertion that two textures are enough for every PBR material. Readback checks use
native byte pixels interpreted as linear units; UE's RGBA8 float readback performs
sRGB decoding and is unsuitable for these numeric controls.

Complete topology, outputs and budget are admitted before any draw. The cache
reserves the whole bounded DAG before drawing targets, and publishes readiness
for the bundle together after the render-thread flush. Readiness and warm reuse
both require every exported target and native RHI resource to match. Recreating
one resource makes the entire bundle unready. Sequential GPU writes are not an
atomic pixel transaction; the flush is not GPU-complete timing evidence.

Three 1024-square RGBA8 mip chains occupy 16,777,212 logical bytes; three
512-square chains occupy 4,194,300. GPU allocation overhead and cold GPU latency
require separate measurement. Export residency remains pinned under the existing
cache lifetime; unlimited structural fixture retargeting is not established.
Ordinary material-parameter previews reuse one frozen bundle without allocating
another structural identity.

`Data/TestFixtures/Soil/` exercises native generation, authenticated material
consumption, save/reload and no-op behavior under disposable test mounts. It is
outside the production recipe census. Schema/compiler implementation fingerprints
change when their owner changes; that does not authorize regenerating production
resources or any World output during fixture development.

For test-mount validation and mutation, every requested output must resolve through
the actual mount to its declared physical file. An already loaded package must be clean and
originate from that same known file. Mismatched mounts, dirty packages and foreign
or unknown loaded origins refuse before fixture policy registration, package writes
or manifest promotion. Retargeting a mount cannot retarget cached UObjects.

## Opt-in asynchronous bundle producers

`UProjectTextureBundleProducer` is an execution adapter for an already admitted
catalog. `UProjectTextureRuntimeSubsystem` remains the only public admission,
identity, sharing, pinning and eviction authority. The ordinary material-DAG
producer remains the default; a native adapter is selected only by an explicit
`RequestBundle` caller. ProjectTexture has no public Texture Graph dependency.

An adapter validates its immutable structural identity and declares a conservative
peak reservation covering its working resources, native retention and stable output
mips before `Begin`. The owner reserves that entire amount until eviction. It does
not reduce the reservation merely because generation completed. Unsupported inputs,
identity collisions, a competing default writer, or insufficient budget refuse
before generation and preserve accepted pinned resources.

Requests for the same identity share one pending operation and the same stable
output slots. Readiness requires all sibling resources and their descriptor to
match. The owner copies valid adapter outputs into the existing slots, generates
native mips and waits for a GPU fence before publishing the whole bundle. Incomplete,
invalid or cancelled results publish nothing. A descriptor change refuses warm
reuse; resource replacement invalidates Ready. Failure can be retried with a fresh
producer after draining and releasing the failed operation.

Output keys and destination references are frozen at admission so cancellation,
eviction and subsystem shutdown remain safe if a caller edits the catalog. Engine
subsystem lifetime, rather than unrelated World cleanup, owns shared bundles. Pins
protect live consumers; callers must restore or detach material bindings before
releasing their final pin. Releasing a pin alone does not evict. Explicit eviction
releases stable resources and asks the adapter to drain and release its own outputs.
Teardown waits for outstanding GPU copies before release. This synchronous recovery
boundary is not a promise of stall-free generation or shutdown.

Physical allocation evidence must distinguish logical reservation, stable RHI
textures, adapter intermediate/export resources, native pools and other engine
GPU allocations. Metadata eviction or zero tracked render-target allocation does
not establish zero total GPU residency. An adapter must bound its actual supported
layout and account for retained intermediates rather than accept arbitrary graphs
under a guessed fixed reservation.

Experimental Texture Graph support is a private project-local plugin candidate,
not an enrolled production backend. Retaining a native fork requires an exact UE
version and upstream file hashes, a minimal private diff, cooked-runtime regression
proof and an upgrade procedure: compare upstream hashes, rebase each justified
change, rebuild, then re-prove shader availability, numeric output ABI, GC, pending
shutdown, cancellation and physical release. Keep Epic source and patch artifacts
out of the public repository; resolve distribution rights before promotion.

The current private adapter supports a closed pinned graph layout and serialized
native execution only. Graph duplication freezes graph-owned state; external
cooked material objects remain shared and must be immutable in that envelope.
Its exclusive native pool release requires scheduler quiescence and completed GPU
uses, including the copies into stable slots. That release is not a concurrency
contract for arbitrary Texture Graph users. A broader adapter must establish its
own safe lifetime and input authentication before admission.

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
