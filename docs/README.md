# Documentation

Public documentation router for the ALIS source tree.

This folder is the public-facing documentation layer for the repository. It focuses on architecture, build and release flow, testing, UI, and text-based data workflows.

Start here if you want a stable public overview before drilling into plugin-level READMEs.

## Best Entry Points

- Architecture: [architecture/README.md](architecture/README.md)
- Build and release: [build/README.md](build/README.md)
- Public packaging and verification: [build/packaging_guide.md](build/packaging_guide.md)
- Data architecture: [data/README.md](data/README.md)
- UI router: [ui/README.md](ui/README.md)
- Testing: [testing/README.md](testing/README.md)
- Animation system: [animation/README.md](animation/README.md)
- Scripts and automation: [../scripts/README.md](../scripts/README.md)

## What These Docs Cover

- architecture and plugin boundaries
- text-first, JSON-driven workflows
- build, validation, and release packaging
- testing strategy and automation
- UI framework, hot reload, and descriptor-driven rendering
- public trust and release verification flow

## Public Scope

These docs are selected for the public source mirror.

Intentionally not routed from here:
- private machine setup
- internal CI wiring
- local-only agent instructions
- delivery or infrastructure notes that are not part of the public code tree

## Reading Pattern

Use category routers first, then move into the owning plugin README or focused guide.

Examples:
- architecture router -> plugin README
- build router -> workflow or package guide
- UI router -> ProjectUI docs or feature UI plugin README
- data router -> generator or object-definition docs
