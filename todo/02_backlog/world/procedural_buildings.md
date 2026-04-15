# Procedural Building System

**JSON-driven procedural building generation using UE5 PCG + C++.**

Replace InstanceArrayTool (marketplace editor plugin) with a project-owned
procedural pipeline that generates buildings from data, not manual placement.

## Concept

Buildings are defined by composable JSON descriptors:

```
Content/Data/Schema/World/Buildings/
  types/          <-- building archetypes (house, barn, ruin, bunker)
  layouts/        <-- floor plans, room grids, story counts
  materials/      <-- material palettes per biome/era
  parts/          <-- modular pieces (wall, roof, door, window, balcony)
```

A single building = type + layout + material palette + part overrides.
Multiple JSON parts combine into one building definition at load time.

### Example descriptor (minimal)

```json
{
  "$schema": "../../Schemas/building.schema.json",
  "type": "residential_2story",
  "layout": "layout_rect_6x8",
  "materials": "palette_soviet_brick",
  "parts": {
    "roof": "roof_flat_concrete",
    "windows": "window_broken_random"
  }
}
```

## Architecture

```
JSON descriptors
    |
    v
BuildingDefinition UAsset (auto-generated or runtime-parsed)
    |
    v
PCG Graph (C++ subgraph nodes for wall/roof/floor placement)
    |
    v
HISM components (walls, roofs, props) -- runtime instanced meshes
```

### Why PCG + C++

- PCG framework handles spatial rules, density, randomization natively
- C++ PCG nodes give full control over placement math (no BP overhead)
- HISM output integrates with UE5 streaming and Nanite
- Deterministic seed per building -> reproducible across clients
- Supports runtime generation (procedural world) and editor baking

### Key C++ components

| Component | Responsibility |
|-----------|----------------|
| `UBuildingDefinition` | Parsed JSON -> typed struct (type, layout, palette, parts) |
| `UPCGBuildingGraph` | PCG subgraph: takes definition, emits HISM instances |
| `UPCGNode_WallPlacer` | C++ PCG node: wall segments along layout edges |
| `UPCGNode_RoofPlacer` | C++ PCG node: roof type selection + placement |
| `UPCGNode_PropScatter` | C++ PCG node: interior/exterior prop scatter |
| `ABuildingGenerator` | Actor that owns PCG component + building definition ref |

## Replaces

- **InstanceArrayTool** marketplace plugin (procedural ISM/HISM placement)
- Manual per-building instance editing in editor
- Hardcoded building layouts

## Scope

- Phase 1: JSON schema + `UBuildingDefinition` loader
- Phase 2: Basic PCG graph (walls + roof from layout)
- Phase 3: Material palette system, part variants
- Phase 4: Runtime generation, LOD, streaming integration
- Phase 5: Editor tooling (preview, bake, override per-instance)

## Dependencies

- UE5 PCG framework (built-in since 5.2)
- ProjectCore `FProjectPaths` for data path resolution
- ProjectObject layer contract for mesh Kind/Role tagging

## Related

- Current plugin: `Plugins/InstanceArrayTool/` (remove after migration)
- ISM/HISM docs: UE5 instanced rendering pipeline
- Data schema convention: `Content/Data/Schema/` + inline `$schema`
