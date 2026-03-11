# C4 Diagrams (Structurizr DSL)

Quick reference: see `../GLOSSARY.md` and `../RESPONSIBILITIES.md` for terms and boundaries.

Purpose
- Single source for C4 views (no images in repo)
- Edit the workspace DSL and render on demand
- Do not duplicate view lists here; the DSL is authoritative

Source of truth
- Workspace: `docs/architecture/diagrams/workspace.dsl`
- Views live under: `docs/architecture/diagrams/views/`
- For the current set of views, open `workspace.dsl` or browse in Structurizr Lite

How to view
- Start viewer: `scripts/utils/structurizr/start_structurizr.bat`
- URL: `http://localhost:8080`

Notes
- The generic container view was removed to reduce clutter; use focused views.
- Tier colors and tags are defined in `styles.dsl`.
- See also: `docs/architecture/diagrams/COMMON_MISTAKES.md`.
- Autolayout is disabled in committed DSL. Keep it off; enable only locally while iterating.
- Do not edit `workspace.json` manually. Treat `workspace.dsl` + `views/*.dsl` as the only edit surface.
- Boot chain responsibilities live in `docs/architecture/boot_chain.md`; see the `C3 - Boot Chain (BootROM -> Orchestrator)` view for manifest guard, baseline installer, and status overlay components.

Visual Legend - Complete Color System

**Element Colors (Boxes):**
- **Dark Blue** (#0b3d91) - `Tier:Foundation` (ProjectCore)
  - Zero dependencies, defines interfaces (ILoadingService, ISettingsService, etc.)
- **Light Blue** (#1f77b4) - `Tier:Systems` (ProjectLoading, ProjectSession, etc.)
  - Implement Foundation interfaces, provide core services
- **Purple** (#8e44ad) - `Tier:UI` (ProjectUI)
  - Reusable UI framework (MVVM widgets, screen stack, theming)
- **Green** (#229954) - `Tier:Features` (ProjectMenuMain, ProjectMenuGame, etc.)
  - Use Foundation interfaces via ServiceLocator, consume UI framework

**Relationship Colors (Arrows):**
- **Green dashed** - System REGISTERS interface in ServiceLocator
- **Blue solid** - Feature RESOLVES interface from ServiceLocator
- **Gray solid** - Regular module dependency (not interface-related)

See `model_plugin_relationships.dsl` header for full arrow semantics (compile-time + runtime)
