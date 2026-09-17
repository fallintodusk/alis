# ProjectPCG

Enabled runtime module shell reserved for a possible Unreal PCG adapter.

The current plugin contains module startup/shutdown only. Generated-World P0
realization uses ProjectWorld's deterministic adapters and does not depend on a
ProjectPCG service, registry, node, or recipe.

Any future PCG integration must consume accepted canonical records through the
ProjectWorld boundary rather than create a second source or import authority.
See [ProjectWorld](../../ProjectWorld/README.md).
