# Open World Generation Working System Map

Status: visual router for the completed task, not an architecture source of
truth. It intentionally does not repeat implementation status, decision
details, or execution tasks from the owning files.

## One-Sentence Model

ALIS should compile immutable, licensed source data into deterministic,
engine-independent world cells, then use a narrow Unreal adapter to create
disposable World Partition content while keeping authored and runtime state
separate.

## Target Relationship Map

This diagram shows proposed boundaries, not implementation status or approved
contracts. Follow the detail links below before treating a box as implemented.

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 24, "rankSpacing": 32, "curve": "basis"}}}%%
flowchart TD
    Inputs["IMMUTABLE INPUTS<br/><br/>Source snapshot: DEM, roads, footprints<br/>Source ledger: license, attribution, hashes<br/>Authored data overlay: patches and hero exclusions"]
    Compile["ENGINE-INDEPENDENT BUILD<br/><br/>External deterministic compiler<br/>One aligned terrain grid, core plus halo<br/>Stable IDs, staged promotion, boundary hashes<br/>Compiler cells, manifests, and reports"]
    Unreal["TARGET UE 5.8 REALIZATION<br/><br/>Narrow canonical import adapter<br/>One logical Kazan Landscape with generated<br/>and protected authored layers<br/>Baked into the installed prototype"]
    Runtime["RUNTIME<br/><br/>World Partition streams installed cells<br/>Dynamic persistence overlays the loaded world<br/>using stable feature IDs"]

    Inputs -->|"normalize, validate, compile"| Compile
    Compile -->|"cell manifest and typed artifacts"| Unreal
    Unreal -->|"cooked local content"| Runtime
```

## Spatial Mapping

```mermaid
flowchart LR
    Source["Source partition"]
    Compiler["Compiler cell"]
    Artifact["Render or import artifact"]
    WP["World Partition cell"]
    Simulation["Optional simulation cell"]

    Source -->|"normalized into"| Compiler
    Compiler -->|"publishes"| Artifact
    Artifact -->|"mapped to"| WP
    Compiler -.->|"mapped independently"| Simulation
```

Each arrow is an explicit mapping, not shared identity. Addressing and size
policy belongs to [audit C-04](research_audit.md#c-04---define-stable-identity-coordinates-and-separate-spatial-grids).

## Detail Sources

- [Execution router](todo.md)
- [Implemented baseline](research_audit.md#c-02---correct-the-implemented-baseline)
- [Source ingestion tasks](01_source_ingestion.md)
- [Canonical compilation tasks](02_canonical_world_compilation.md)
- [Unreal realization tasks](03_unreal_world_realization.md)
- [Cross-layer acceptance](04_end_to_end_validation.md)
- [Research reports and evidence routes](README.md#research-materials)

Stable component references:

- [ProjectWorld](../../../../Plugins/World/ProjectWorld/README.md)
- [ProjectDefinitionGenerator](../../../../Plugins/Editor/ProjectDefinitionGenerator/README.md)
- [City17](../../../../Plugins/World/City17/README.md)
