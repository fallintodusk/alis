# ProjectTexture pitfalls

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
