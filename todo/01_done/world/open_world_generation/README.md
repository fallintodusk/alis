# Open World Generation

Status: complete. Source ingestion, canonical compilation, Unreal realization,
clean bootstrap, and cooked-package validation pass for the bounded P0 inputs.

## Goal

Define a scalable, regenerable ALIS open-world generation pipeline based on
structured world data, Unreal Engine systems, and legally usable geospatial
sources.

## Execution

- [Execution router](todo.md) - completed order and responsibility ownership.
- [Source ingestion](01_source_ingestion.md) - Kazan provider admission,
  snapshots, and provider-preserving decoding.
- [Canonical world compilation](02_canonical_world_compilation.md) - ALIS JSON,
  deterministic cells, overlays, and reports.
- [Unreal world realization](03_unreal_world_realization.md) - target UE 5.8
  adapter after the separately owned engine gate.
- [End-to-end validation](04_end_to_end_validation.md) - two-cell acceptance,
  failure cases, and metrics.

## Research Materials

- [Working system map](system_map.md) - visual relationship and spatial-mapping
  router. Detailed status, decisions, and tasks remain in their owning files.
- [Component licensing](../../../../docs/legal/component_license_policy.md) -
  canonical UE, standalone-tool, protocol, and authored-asset boundary.
- [World data and Unreal asset licensing](../../../../docs/legal/world_data_and_asset_policy.md)
  - provenance graph, source suitability, public artifacts, `.uasset`
  publication, and reproducible developer acquisition.
- [Research audit](research_audit.md) - repository and source evidence,
  corrected rationale, and deferred research; no implementation checklist.
- [Main research report](research_main.md) - initial strategy, candidate technologies,
  risks, and proposed phases. Numbered source links were recovered from its
  PDF; the audit still governs claim validation.
- [Agentic research report](research_agentic.md) - agent-driven architecture,
  Unreal MCP and PCG tooling, automation boundaries, and integration flow.
  Numbered source links were recovered from its PDF; tool maturity and repo
  assumptions still require adoption checks.
- [Unreal Engine capabilities report](research_ue.md) - modern UE worldbuilding,
  streaming, procedural generation, editor automation, and cooked-content
  delivery capabilities. Numbered source links were recovered from its PDF;
  Individual optional-feature claims still require adoption-time validation;
  the engine, fixture, build, packaging, plugin, and P0 functional gates pass.
- [Implementation research report](research_impl.md) - versioned Earth-data
  compiler architecture, ALIS World Fabric layers, deterministic generation,
  failure modes, permanent stack, and phased implementation order. Source
  references are included but still require validation.

## Result

The project-owned `p0` validation route proves the synthetic and Kazan paths
from an initially absent tool/data/output state through D0-D3 and a cooked
installed prototype.
