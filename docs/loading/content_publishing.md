# Content Publishing Boundaries

The supported public ALIS distribution is the signed GitHub release flow owned
by [release packaging](../build/packaging_guide.md).

Build Service contains a separate Rust artifact pipeline and owns its current
code and tests. Its component boundary is documented in
[Build Service architecture](../../tools/BuildService/docs/architecture/README.md).
The sibling CDN repository owns its service schema and promotion behavior.

No Launcher executable is implemented in this repository, so stable ALIS docs
must not describe Build Service/CDN output as a proven player delivery route.

Do not duplicate artifact schemas or publication state here; verify them at
the producer and consumer boundaries that actually execute them.
