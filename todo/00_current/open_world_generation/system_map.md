# Open World Generation Working System Map

Status: visual router for the active todo, not an architecture source of
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
    Inputs["1. IMMUTABLE INPUTS<br/><br/>Source snapshot: DEM, roads, footprints<br/>Source ledger: license, attribution, hashes<br/>Authored data overlay: patches and hero exclusions"]
    Compile["2. ENGINE-INDEPENDENT BUILD<br/><br/>External deterministic compiler<br/>CRS, stable IDs, geometry, boundary ownership<br/>Compiler cells, manifests, and validation reports"]
    Unreal["3. UE 5.7 REALIZATION<br/><br/>ProjectWorld contracts and narrow import adapter<br/>ProjectPCG integration and world-specific recipes<br/>Generated content plus protected authored content<br/>World plugin map under stock World Partition"]
    Runtime["4. RUNTIME<br/><br/>ProjectLoading loads the built map and content<br/>World Partition streams engine cells<br/>Dynamic persistence overlays the loaded world<br/>using stable feature IDs"]

    Inputs -->|"normalize, validate, compile"| Compile
    Compile -->|"cell manifest and typed artifacts"| Unreal
    Unreal -->|"built map and native content"| Runtime
```

## Spatial Mapping

```mermaid
flowchart LR
    Source["Source partition"]
    Compiler["Compiler cell"]
    Artifact["Render or import artifact"]
    WP["World Partition cell"]
    Simulation["Optional simulation cell"]
    Delivery["Optional delivery bundle<br/>may aggregate artifacts"]

    Source -->|"normalized into"| Compiler
    Compiler -->|"publishes"| Artifact
    Artifact -->|"mapped to"| WP
    Compiler -.->|"mapped independently"| Simulation
    Artifact -.->|"packaged independently"| Delivery
```

Each arrow is an explicit mapping, not shared identity. Addressing and size
policy belongs to [audit C-04](todo.md#c-04---define-stable-identity-coordinates-and-separate-spatial-grids).

## Detail Sources

- [Implemented baseline](todo.md#c-02---correct-the-implemented-baseline)
- [Unresolved architecture decisions](todo.md#world-gate-w0---architecture-decisions)
- [Exact first-slice tasks and acceptance](first_slice.md)
- [Research reports and evidence routes](README.md#materials)

Stable component references:

- [ProjectWorld](../../../Plugins/World/ProjectWorld/README.md)
- [ProjectDefinitionGenerator](../../../Plugins/Editor/ProjectDefinitionGenerator/README.md)
- [City17](../../../Plugins/World/City17/README.md)
