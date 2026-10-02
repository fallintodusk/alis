# Build State and CDN Manifest

Build state and the CDN manifest have different owners.

| Data | Owner | Purpose |
|---|---|---|
| Build ID, pins, dirty units, prior local builds | `crates/state` local state | Incremental build decisions |
| Engine identity, plugin versions, dependencies, artifact URLs/hashes/sizes | `crates/manifest` manifest | Runtime delivery metadata |

The build ID is not part of the CDN manifest schema. The manifest crate embeds
and validates the schema shape used by this repository; the sibling CDN owns
the remote service contract.

Current manifest signing limitations are stated in
[Build Service publishing](signing_and_publishing.md).
