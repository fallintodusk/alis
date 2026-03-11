# Documentation - Alis UE 5.5 Project

Agent-friendly documentation organized by topic. For quick agent routing, see [../AGENTS.md](../AGENTS.md).

---

## Doc Hygiene (keep implementation in plugins)
- Use the index files below first; keep category docs concise and focused on routing.
- Do not paste code into docs. Point to the implementation by class and method (for example `IOrchestratorModule::Start` in `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Public/OrchestratorAPI.h`).
- Plugin docs live next to their code under `Plugins/<Category>/<Name>/` (README or docs folder). Cross-link there instead of duplicating content here.
- C4 DSL diagrams are mandatory: `docs/architecture/diagrams/workspace.dsl` (views under `diagrams/views/`). If a diagram drifts, call it out and sync after discussion.

## Quick Category Indexes (Progressive Disclosure)
 
 **Start here for fast navigation:**
 - **[../scripts/README.md](../scripts/README.md)** - Script automation tasks with decision trees
 - **[testing/](testing/README.md)** - Testing strategies and debugging/troubleshooting
 - **[docs/loading/](loading/README.md)** - Content Delivery Ecosystem (Build -> CDN -> Launcher -> Engine)
 - **[architecture/README.md](architecture/README.md)** - High-level architecture index
 - **[animation/README.md](animation/README.md)** - Animation domain SOT
 - **[build/README.md](build/README.md)** - Build & compile tasks with decision trees
 - **[build/packaging_guide.md](build/packaging_guide.md)** - Public release packaging and GitHub Releases pipeline
 - **[docs/diagrams/](architecture/diagrams/)** - C4 Architecture Diagrams
 
 **Pattern:** Category index -> Detailed docs (load only what you need)
 
 ---
 
 ## Category Routers (keep lean)

 - **Loading:** `loading/` (Master Ecosystem Guide)
 - **Data:** `data/` (Workflow, Specs, Router)
 - **Architecture:** `plugins.md` (Plugin rules), `principles.md` (Core philosophy)
 - **Systems:** folder `systems/` (Runtime Game Systems)
 - **Gameplay:** `gameplay/` (Characters, Modes, Features)
 - **UI:** `ui/README.md` (routes to ProjectUI plugin docs)
 - **Editor:** `editor/` (Place Actors panel, editor-only features)
 - **Testing:** `testing/` (+ `troubleshooting.md`)
 - **Tools:** `../tools/` (BuildService, Launcher docs co-located with code)
 - **Config:** `config/` (Rendering, INI)
 - **Reference:** `reference/`
 - **Manual:** `manual/`
 
 ---
 
**Token budget:** Load only relevant category. Don't load entire reference/ unless needed.
