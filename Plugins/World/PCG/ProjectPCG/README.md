# ProjectPCG

Reusable UE-side PCG integration: nodes, registries, runtime generation
services, and provider-neutral extension points.

ProjectPCG does not acquire provider data, validate Canonical Compilation
receipts, own the deterministic import commandlet, or contain world-specific
recipes and content packs. Those responsibilities belong to the external
World Compiler, ProjectWorld, and concrete world/content plugins respectively.

P0 realization uses deterministic ProjectWorld adapters and stock engine
surfaces; it does not require PCG or Experimental features. A later PCG adapter
may consume accepted canonical records through the same ProjectWorld boundary
without creating a second import algorithm.

See [ProjectWorld](../../ProjectWorld/README.md) for the world build and
ownership contract. `ProjectLoadingSubsystem` remains the reference pattern
for always-on reusable services.
