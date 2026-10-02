# Build Service Structure

## Boundary

The CLI composes eight workspace crates. It reads project descriptors and
configured local build state, invokes Unreal tooling, writes staged artifacts
and a manifest, and optionally calls the sibling CDN Control API.

The sibling CDN owns remote storage, server-side verification, promotion, and
rollback semantics. Build Service owns only the request and local receipt gates
implemented in this repository.

## Components

| Component | Current responsibility |
|---|---|
| CLI | Command parsing and pipeline composition |
| scanner | `BuildUnit.yaml` discovery, fingerprints, and changed-unit selection |
| order_planner | `.uplugin` dependency graph and topological order |
| executor | Engine-pin checks, base release state, and Unreal DLC commands |
| state | Atomic local build-state persistence and dirty propagation |
| packager | ZIP/IoStore discovery, copying, and SHA-256 metadata |
| manifest | Manifest parsing, validation, mutation, and atomic local writing |
| engine_config | Engine-root resolution and source-release receipt verification |
| publisher | Authenticated Control API upload, promote, and rollback calls |

## Invariants

- Engine identity is checked before build work.
- `BuildUnit.yaml` fingerprints, not Git diffs, select changed units.
- Promotion requires a receipt that verifies current source identity and the
  staged artifact set.
- CDN credentials are read from the configured environment-variable name; the
  token is not stored in committed configuration.
- Failed or unsupported operations remain errors; documentation does not claim
  remote atomicity beyond the Control API response.

## Current limitations

Manifest signing and DLL Authenticode signing are not implemented by the
current pipeline. Some manifest fields are still synthesized rather than read
from `.uplugin` metadata. These gaps mean Build Service is not the authority
for the public signed GitHub release.
