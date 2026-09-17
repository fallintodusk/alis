# Experience Asset Registration

ProjectLoading turns an experience descriptor into a validated
`FLoadRequest`. The descriptor owns experience intent; ProjectLoading owns the
runtime Asset Manager interaction.

## Current Contract

1. A plugin registers a `UProjectExperienceDescriptorBase` with the global
   experience registry.
2. `ILoadingService::BuildLoadRequestForExperience` resolves that descriptor.
3. `FInitialExperienceLoader` applies the descriptor's asset scan specs in
   cooked builds. Editor runs use config-registered asset types.
4. The loader resolves the map identity and copies critical and warmup asset
   identities into the request.
5. Pipeline executors preload critical assets, travel to the map, and load
   warmup assets.

The map itself is not preloaded by the critical-asset executor. Travel owns
World loading.

## Ownership

| Concern | Owner |
|---|---|
| Experience identity, map, scan specs, and asset groups | Experience descriptor |
| Descriptor discovery | ProjectCore experience registry |
| Scan execution and request construction | `FInitialExperienceLoader` |
| Phase execution and progress | ProjectLoading pipeline |
| User-facing loading presentation | ProjectUI |

Callers request an experience by identity through `ILoadingService`; they do
not reproduce descriptor lookup, scanning, or load-request assembly.

## Invariants

- Invalid or unresolved experience identities fail before pipeline execution.
- Scan specs come from the resolved descriptor, not from a caller-maintained
  path list.
- Editor and cooked registration routes converge on the same primary asset
  identities.
- ProjectLoading reports lifecycle state; it does not own loading widgets.

Implementation authority remains in
`Source/ProjectLoading/Private/Experience/InitialExperienceLoader.cpp` and the
public ProjectCore loading contracts.
