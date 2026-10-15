# ProjectTexture pitfalls

## Cold-loaded sibling output cleared after premature generation

- Symptom: loading an aggregate material during an existing Game/PIE world reports
  cache Ready, but its Normal target is black and the surface shades dark.
- Root cause: nested output-slot PostLoad notification synchronously generated
  all siblings. A later native texture PostLoad recreated a sibling resource,
  invalidating its generated pixels and Ready identity. Loading before PIE and
  manually requesting afterward did not exercise this ordering.
- Fix: queue PostLoad notifications and drain through the existing ticker outside
  native PostLoad and incomplete dependency loading. Preserve default single-writer
  admission, budgets, resource-identity readiness and explicit bundle ownership.
- Owner: `Source/ProjectTexture/Private/ProjectTextureRuntimeSubsystem.cpp`.
- Regression: `Project.Texture.Runtime.ColdLoadedAggregate` cold-loads a saved
  two-output catalog in a generation world, checks actual encoded Normal pixels,
  warm resource reuse, native resource-recreation recovery and unchanged package
  bytes. Existing default/cache and opt-in ownership tests remain required.

## Internal stage mistaken for an exported output

- Symptom: an explicit filtered bundle rejects or allocates an output using the
  internal raw stage's dimensions when exports start after that stage.
- Root cause: output validation/configuration used the export index as the node
  index, although output slots identify their consumed node explicitly.
- Fix: resolve each exported descriptor by its slot NodeId. Internal stages remain
  producer-owned intermediates; only public outputs receive stable targets.
- Owner: `Source/ProjectTexture/Private/ProjectTextureRuntimeBundles.cpp`.
- Regression: `Project.Texture.Runtime.FilteredBundleExports` uses different raw
  and exported sizes, validates copied pixels/mips, and checks released eviction.

## Mutable catalog during asynchronous bundle teardown

- Symptom: cooked subsystem shutdown crashed after a pending caller emptied its
  catalog node array. A changed Ready descriptor could also remain visible through
  the public readiness query even though a new request refused it.
- Root cause: cleanup rebuilt output keys from mutable node indices; readiness
  checked stable resources without comparing the admitted descriptor.
- Fix: freeze output keys and target references at admission, cancel changed pending
  descriptors and compare the frozen descriptor before reporting Ready. Preserve
  accepted pixels on refusal; drain GPU work before teardown releases resources.
- Owner: `Source/ProjectTexture/Private/ProjectTextureRuntimeBundles.cpp`.
- Regression: `Project.Texture.Runtime.BundleMutationShutdown` reproduces explicit
  cooked shutdown after descriptor mutation; `Project.Texture.Runtime.BundleFailureRecovery`
  verifies failed-sibling recovery and Ready invalidation without resource replacement.

The [asynchronous bundle contract](pattern_generation.md#opt-in-asynchronous-bundle-producers)
owns the standing lifetime and admission rules.
