# ALIS SOURCE OF TRUTH (one file)
ASCII only. KISS. SOLID. YAGNI.

Vision (C4 + Backstage + Telemetry) - one glance
 - C4 format - Structurizr DSL (text file: architecture/diagrams/workspace.dsl). Viewer - Structurizr Lite (java -jar or Docker) or Structurizr CLI export. Edit the DSL in VS Code with a DSL extension. No images kept in repo.
- Backstage - catalog and docs. Each plugin has a tiny catalog-info.yaml for ownership/links. APIs are Backstage API entities that point to OpenAPI or AsyncAPI files next to the plugin.
- Monitoring/Telemetry - OpenTelemetry traces -> Collector service-graph -> Grafana or Jaeger. Used to verify runtime edges match the declared relations and flows in THIS FILE.
- Single edit surface - THIS FILE is authoritative for the whole map and flows. Per-plugin YAML must match it. Diagrams are derived, drift is auto-checked later.
 - KISS - start with DSL + this file. Add Backstage and Telemetry when needed for discovery and verification.

Authoritative Layout Mapping (plugins)
- Boot: Orchestrator
- Foundation: ProjectCore, ProjectSharedTypes
- Systems: ProjectLoading, ProjectSave, ProjectSettings
- World: ProjectWorld, City17, MainMenuWorld, ProjectPCG, ProjectForestBiomesPack, ProjectUrbanRuinsPCGRecipe
- UI: ProjectUI, ProjectMenuMain, ProjectMenuGame, ProjectSettingsUI
- Gameplay: ProjectGameplay, ProjectMenuPlay, ProjectSinglePlay, ProjectOnlinePlay
- Features: ProjectFeature, ProjectInventory, ProjectCombat, ProjectDialogue

View Index (C4)
- Do not duplicate view lists here.
- Source of truth: `docs/architecture/diagrams/workspace.dsl` (includes all views under `docs/architecture/diagrams/views/`).
- To browse current views, run `scripts/utils/structurizr/start_structurizr.bat` and open `http://localhost:8080`.


1) What this is (dead simple)
- This file is the first and only place to define the whole system before any code.
- Humans and agents edit ONE schema here to see the big picture.
- Per-plugin files are lower level and must match this file.
- Diagrams, checks, and drift alerts are generated from this file.

2) What you do first (always)
- Edit the Universal Project Schema block below (systems, components, relations, flows).
- Commit. CI validates it and generates diagrams.
- Then add or update per-plugin YAML and API specs to match it.

------------------------------------------------------------
UNIVERSAL PROJECT SCHEMA (authoritative)
File: THIS FILE

Rules
- IDs are ASCII, kebab-case, unique.
- IDs use kebab-case; UE plugin names are PascalCase without "GF" suffix.
  Example: project-menu-core -> ProjectMenuCore
- Refer to plugins by Backstage metadata.name using source: plugin:<name>.
- Refer to API contracts by Backstage API entity name.
- Do not describe plugin internals here.
- Keep it small. Only what the system and flows need.

SOLID Guardrails (applied at architecture level)
- SRP: BootROM does one thing -> load and hand off to Orchestrator. Orchestrator does updates and activation. No overlap.
- OCP: Orchestrator is data-driven; adding plugins extends via manifest without modifying orchestrator code.
- LSP: Feature plugins must respect the same activation contract; swapping versions preserves invariants.
- ISP: Client code depends on small interfaces (IModule, IPluginManager) not monolithic subsystems.
- DIP: High-level boot logic depends on abstractions (Orchestrator API) not concrete features.


------------------------------------------------------------
LOWER LEVEL: PER-PLUGIN RULES (encapsulation)
- One Backstage YAML per plugin: Plugins/Features/<PluginName>/catalog-info.yaml
- Keep it tiny: ownership, minimal relations, annotations.
- Contracts live next to the plugin: api/openapi.yaml or events/asyncapi.yaml
- The universal schema above must reference your plugin by metadata.name

Minimal plugin YAML template
apiVersion: backstage.io/v1alpha1
kind: Component
metadata:
  name: project-menu-main
  description: Front-end menu (logic + screens)
  annotations:
    alis.io/activation: auto
    alis.io/security: no-unauthenticated-writes
    alis.io/slo.p95_latency_ms: "200"
    alis.io/slo.error_rate: "<0.5%"
spec:
  type: service
  lifecycle: production
  owner: team-ui
  system: alis-platform
  dependsOn: []
  providesApis: []
  consumesApis: []

Optional API entity (when you provide or consume HTTP or events)
apiVersion: backstage.io/v1alpha1
kind: API
metadata:
  name: boot-api
spec:
  type: openapi   # or asyncapi
  lifecycle: production
  owner: team-core
  definition:
    $text: ./api/openapi.yaml

------------------------------------------------------------
ASCII DIAGRAMS (for quick review)
Source-of-truth flow
[THIS FILE: Universal Project Schema]
  -> C4 context and container views (generated)
  -> Dynamic views from flows (generated)
[Per-plugin YAML and API specs]
  -> Backstage catalog and docs
[Runtime traces]
  -> OTel Collector service-graph -> Grafana or Jaeger

Drift check
[Declared deps] from THIS FILE (relations and flows)
        VS
[Observed deps] from service-graph
        -> if different -> create issue "ARCH-DRIFT:<service>"
           - attach YAML snippet and graph snapshot
           - assign owners from catalog
           - propose fix (update schema or fix routing)

------------------------------------------------------------
CI AND VALIDATION (planned enforcement - implement incrementally)
- Universal schema: parse and validate IDs and references
  - every component system exists
  - every source plugin exists in catalog
  - every contract resolves to an API entity
- Per-plugin YAML: validate required fields, resolve dependsOn and API refs
- Contracts: lint OpenAPI or AsyncAPI, block publish on errors
- Generation: fail PR if C4 generation from THIS FILE fails
- Drift: nightly compare relations vs service-graph, open ARCH-DRIFT on mismatch
- Docs: TechDocs build must pass
- ASCII-only: fail PR if non-ASCII appears in docs or scripts

------------------------------------------------------------
START HERE (checklist)
1) Edit THIS FILE: add or update components, relations, and flows
2) Add or update per-plugin YAML with matching metadata.name and annotations
3) Add API entity and openapi.yaml or asyncapi.yaml when needed
4) Run generators: C4 views and TechDocs
5) Enable basic OTel traces for affected services
6) Commit and open PR; CI will validate and generate

Consistency checks (quick)
- ASCII-only in docs and scripts (project rule)
- DSL compiles and Structurizr Lite renders views
- No generic container view included; use focused views above

------------------------------------------------------------
ANCHORS (reference material)
- Update system: docs/loading/boot_chain.md
- Plugin architecture: docs/architecture/plugin_architecture.md
- Loading pipeline: docs/systems/loading_pipeline.md
- CDN prototype repo (MinIO + manifest schema source): <cdn-repo>
  - id: cdn
    name: CDN Service
    kind: external
    system: alis-platform
