# Open World Generation

Status: research audited; bounded first-slice decisions and implementation not
started.

## Goal

Define a scalable, regenerable ALIS open-world generation pipeline based on
structured world data, Unreal Engine systems, and legally usable geospatial
sources.

## Materials

- [Working system map](system_map.md) - visual relationship and spatial-mapping
  router. Detailed status, decisions, and tasks remain in their owning files.
- [Component licensing](../../../docs/legal/component_license_policy.md) -
  canonical UE, standalone-tool, protocol, and authored-asset boundary.
- [World data and Unreal asset licensing](../../../docs/legal/world_data_and_asset_policy.md)
  - provenance graph, source suitability, public artifacts, `.uasset`
  publication, and reproducible developer acquisition.
- [Research audit and decision register](todo.md) - comprehensive critical,
  important, and additional findings; decision gates; and completion criteria.
- [Active first-slice plan](first_slice.md) - the bounded 19-task execution
  queue for a two-cell compiler and UE 5.7 adapter proof.
- [Main research report](research_main.md) - initial strategy, candidate technologies,
  risks, and proposed phases. Numbered source links were recovered from its
  PDF; the audit still governs claim validation.
- [Agentic research report](research_agentic.md) - agent-driven architecture,
  Unreal MCP and PCG tooling, automation boundaries, and integration flow.
  Numbered source links were recovered from its PDF; tool maturity and repo
  assumptions still require the audit gates.
- [Unreal Engine capabilities report](research_ue.md) - modern UE worldbuilding,
  streaming, procedural generation, editor automation, and cooked-content
  delivery capabilities. Numbered source links were recovered from its PDF;
  UE 5.8 claims remain upgrade-gated for the UE 5.7 project.
- [Implementation research report](research_impl.md) - versioned Earth-data
  compiler architecture, ALIS World Fabric layers, deterministic generation,
  failure modes, permanent stack, and phased implementation order. Source
  references are included but still require validation.

## Next Decision Gate

Close the four remaining World Gate W0 decisions, then execute the two-adjacent-cell
proof. It must test terrain, roads, building massing, boundary ownership,
georeferencing, provenance, authored overrides, invalid-feature rejection,
partial rebuilds, and deterministic regeneration.
