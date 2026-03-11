# Common Mistakes and Fixes (C4 + DSL)

Purpose
- Capture frequent pitfalls when editing the Structurizr DSL and architecture views.
- Keep diagrams clean, consistent, and useful.

CRITICAL RULES
- ASCII-ONLY DOCUMENTATION (ABSOLUTE REQUIREMENT!)
  - NEVER use Unicode characters in ANY .dsl or .md files
  - Unicode arrows -> (U+2192) are the MOST COMMON violation - use ASCII -> instead
  - Common violations:
    - [X] Unicode arrow: -> (appears when copying from rendered docs/AI output)
    - [X] Smart quotes: "" ''
    - [X] En/em dashes: - -
    - [X] Math symbols: >= <= x etc.
    - [X] Emoji/symbols: checkmarks, warning signs, etc.
  - Use ASCII alternatives ONLY:
    - [OK] Arrow: -> (hyphen + greater-than)
    - [OK] Quotes: " ' (straight quotes)
    - [OK] Dash: - (single hyphen)
    - [OK] Markers: [X] [OK] [!] instead of emoji
  - Why critical:
    - Structurizr DSL parser may fail
    - Git diffs become unreadable
    - CI/CD encoding issues
    - Cross-platform compatibility breaks
  - Check before commit: grep -P "[^\x00-\x7F]" docs/architecture/diagrams/**/*.dsl
  - Files affected: ALL .dsl files, ALL .md documentation

- NEVER EDIT workspace.json MANUALLY
  - workspace.json is AUTO-GENERATED from the DSL source files
  - ALWAYS edit the .dsl source files instead:
    - Component definitions: model_plugins.dsl
    - Relationships: model_plugin_relationships.dsl
    - Views: views/*.dsl
    - Styles: styles.dsl
  - Structurizr will regenerate workspace.json from the DSL
  - Editing workspace.json directly will cause conflicts and data loss
  - This is FORBIDDEN - treat workspace.json as read-only

- AVOID ACTIONS THAT RESET MANUAL LAYOUT (x,y coordinates live in workspace.json)
  - Layout positions are stored in workspace.json, NOT in .dsl files
  - Structurizr uses a merging algorithm to preserve layout when DSL changes
  - Merging attempts to match elements by name first, then internal ID
  - Actions that BREAK layout matching and LOSE positions:
    - Renaming elements AND changing their creation order in .dsl (changes internal IDs)
    - Changing view keys (causes entire diagram layout reset)
    - Reordering element definitions in .dsl while also renaming them
  - Safe practices to PRESERVE layout:
    - Keep element creation order stable in .dsl files
    - Use explicit view keys (never let DSL auto-generate them)
    - When renaming elements, do NOT reorder their lines in the .dsl
    - Test DSL changes incrementally - check layout preserved after each edit
  - If layout is lost, you must manually reposition elements in Structurizr UI
  - Reference: https://docs.structurizr.com/ui/diagrams/manual-layout

Mistakes and fixes
- Mixing compile-time and runtime views
  - Mistake: Showing runtime flows in a compile-time map.
  - Fix: At C2, keep only the Start/Update Game flow and use dynamic views for runtime. Do not maintain a C2 static-deps view; track compile-time deps in per-plugin docs if needed.

- Parent-child relationships in DSL
  - Mistake: Adding a relationship directly from a container to its child component (Structurizr rejects parent->child edges).
  - Fix Options:
    1. If the parent-side concept is a real class/module, model it as a component (e.g., create explicit entry point component)
    2. If the parent-side concept is implicit (UE subsystem auto-init), document in comments instead of relationships
  - Current approach: BootROM is initialized by UE engine at PostConfigInit (implicit), documented via comments in model_plugin_relationships.dsl
  - Entry point flow: user -> projectClient (model.dsl) then BootROM auto-starts (comment-only)

- Including Orchestrator in compile-time dependencies
  - Mistake: Orchestrator appears in compile-time dependency diagrams.
  - Fix: Orchestrator does not participate in build-time linking; do not model it in compile-time graphs.

- BootROM/Orchestrator responsibility split
  - Mistake: Confusing who manages what (BootROM vs Orchestrator).
  - Fix: Clear separation of concerns:
    - BootROM manages Orchestrator lifecycle: verify version, download latest orchestrator, load orchestrator
    - Orchestrator manages plugin dependencies: verify plugins, download plugins, load plugins BEFORE engine init
    - BootROM does NOT self-update (immutable)
    - Orchestrator does NOT self-update (BootROM updates it)
    - Both provide user feedback via UI widgets
    - Always fetch latest orchestrator version (no baseline/fallback versions)
  - Anti-pattern: "Orchestrator self-updates" - Wrong! BootROM owns orchestrator updates.
  - Correct pattern: BootROM.OrchestratorVerifier checks remote -> BootROM.OrchestratorDownloader downloads + SHA-256 verify -> BootROM.OrchestratorLoader registers/mounts/loads

- Generic "everything" container view
  - Mistake: A single dense view with all containers and relations.
  - Fix: Keep only the focused C2 Start/Update Game flow. The generic and plugin-tier container views are removed.

- Using engine-specific terms (GameFeatures) in architecture naming
  - Mistake: Tying the architecture to UE subsystems.
  - Fix: Use project-level "Features" and a Feature Activation API; keep the mechanism swappable.

- Committing generated images
  - Mistake: Checking in rendered images from Structurizr.
  - Fix: Do not commit images; render on demand.

- Inconsistent naming (ALIS vs Project)
  - Mistake: Mixed display names across views.
  - Fix: Use "ALIS Platform" and "ALIS Client" consistently in the model.

- Modeling plugin internals in the source-of-truth block
  - Mistake: Over-specifying internals in `../source_of_truth.md`.
  - Fix: Keep that file at the system/relations/flows level; details belong in per-plugin docs.

- Orchestrator knows specific plugins
  - Mistake: Hardcoded relationships from Orchestrator to individual features.
  - Fix: Keep Orchestrator generic and data-driven; dependency resolution is manifest-based.

- Referencing non-existent containers in views
  - Mistake: Views reference containers that don't exist in model (e.g., old names like projectContentPacks, projectUI, projectMenuUI).
  - Fix: Keep view includes/excludes in sync with model_plugins.dsl. Update views when plugin names change.

- Missing relationships for dynamic views
  - Mistake: Dynamic views reference relationships that don't exist in the model (e.g., projectClient -> projectExperience).
  - Fix: All relationships used in dynamic views must be declared in model.dsl first. Add missing relationships before using them in views.

- Inline properties in styles
  - Mistake: Multiple properties on one line in styles.dsl (e.g., element "Actor" { background "#fff" color "#000" }).
  - Fix: Each property must be on its own line in Structurizr DSL syntax. Use multiline format for element definitions.

- Missing relationship tags for DIP pattern
  - Mistake: Not distinguishing "registers interface" from "resolves interface" relationships.
  - Fix: Use relationship tags to show semantic meaning:
    - `tags "Implements"` - System plugin REGISTERS interface in ServiceLocator (green dashed arrow)
    - `tags "Uses"` - Feature plugin RESOLVES interface from ServiceLocator (blue solid arrow)
  - Example:
    ```dsl
    project.projectClient.projectLoading -> project.projectClient.projectCore "Registers ILoadingService in ServiceLocator, uses ILoadingHandle" "C++ API" {
        tags "Implements"
    }
    project.projectClient.projectMenuMain -> project.projectClient.projectCore "Uses ILoadingService via FProjectServiceLocator" "C++ API" {
        tags "Uses"
    }
    ```
  - Note: Arrow direction is CORRECT (both point TO ProjectCore) because:
    - Both must include ProjectCore headers (compile-time)
    - Both interact with ProjectCore's ServiceLocator (runtime: Register vs Resolve)
    - ServiceLocator registry lives IN ProjectCore, arrows show who interacts with it

Quick checks
- Views compile in Structurizr Lite.

---

New element not showing (e.g., Claims Service)
- Symptom: You add a new system/container in `model.dsl`, but it doesn't appear in the UI/PNG.
- Fix order (fast -> accurate):
  1) View include missing
     - Ensure the target view includes the element (`include *` or `include claimsService`). Context views commonly require explicit includes for externals.
  2) Missing model edges for dynamic views
     - Add the container-level relationships in `model.dsl` before referencing them in `views/dynamic_*.dsl`.
  3) Hidden by excludes
     - Check the view doesn't `exclude claimsService` (sometimes copied from other views).
  4) Quick sanity rename (forces visible change)
     - Temporarily rename a box in `model.dsl` (e.g., "CDN Service TEST"). Save and refresh. If you don't see the change, continue.
  5) Confirm Lite is reading the expected path
     - Check `tmp/structurizr.log` for: `Workspace path:` and `Workspace filename:`. They should point to `docs/architecture/diagrams` and `workspace.dsl`.
  6) Force DSL load explicitly (no JSON hand-edits)
     - In the UI: File -> Open and select `workspace.dsl`, or add `?workspace=workspace.dsl` to the URL once. Do not hand-edit JSON; it is a generated snapshot.
  7) External->external arrows on C1
     - C1 focuses on the system-external context. If you keep an external->external note (e.g., CDN -> Claims token check), do not add more; show details in the C2 Start/Update flow.

- Identifiers vs display names
  - Keep DSL identifiers aligned with display names (e.g., `cdnService` <-> "CDN Service", `monitoringService` <-> "Monitoring Service"). Mixing old identifiers like `eventBus`/`updateCdn` with new display names causes grep/edit confusion and view drift.

Additional best practices
- Treat DSL as the source of truth. Let Structurizr regenerate workspace.json automatically.
- If local churn is noisy, mark the JSON assume-unchanged locally (git update-index --assume-unchanged).
- ASCII-only in docs and DSL.
- Clear separation: compile-time vs runtime; boot vs update.
- Tiers tagged and readable.
