# Accepted Canonical World Authority

This directory stores accepted, engine-independent world definitions. It is
not a raw provider cache, a compiler scratch directory, or Unreal runtime
content.

```mermaid
flowchart LR
    SOURCE["Admitted geographic sources"] --> COMPILE["Canonical compiler"]
    COMPILE --> AUTHORITY["Accepted canonical bundle"]
    AUTHORITY --> REALIZE["Unreal realization"]
    REALIZE --> UE["Generated maps and assets"]
```

## Layout

```text
Canonical/<profile_id>/
  active.json
  bundles/<content_identity>.zip
```

- `active.json` is the small, reviewable authority pointer. It authenticates
  the selected bundle by identity, byte size, and SHA-256.
- `bundles/` contains immutable canonical JSON plus admission evidence. The
  ZIP is a storage container; it does not change the JSON data model.
- Bundles are tracked through Azure Git LFS so a clean internal checkout can
  materialize the accepted world without downloading provider data or
  recompiling it.
- The public GitHub source branch is text-only and excludes ZIP files and Git
  LFS pointers. The mirror's developer release packages the exact selected ZIP
  together with active generated Unreal packages, identities, and an installer.
  A public `active.json` must not be left without that authenticated release.
- Packaged games do not load these ZIP files. They consume the Unreal packages
  generated from the selected authority.

## Scale boundary

The current territory-sized artifact stays in Azure Git LFS for internal use
and is distributed separately as an immutable public data-release asset. Do
not move it to a Git submodule: that would move the same binary-history problem
while making source-to-authority revisions harder to keep atomic.

If promotion churn or world size makes whole-bundle LFS transport expensive,
preserve `active.json` as the root authority and migrate payload transport to
content-addressed, independently reusable territory/cell-block artifacts in
approved object storage. That future transport change must preserve hashes,
offline caching, atomic activation, and deterministic materialization.
