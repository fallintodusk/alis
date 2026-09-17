# Improve Foliage Lumen Quality

Status: uninvestigated

## Current gap

Instanced foliage is excluded from the ray-tracing scene to preserve packaged
GPU stability, which reduces indirect-lighting and reflection quality.

## Investigation boundary

- Capture the current stable packaged route and fixed foliage views first.
- Test mesh-distance-field and asset-scoped candidates before any global
  ray-tracing re-entry.
- Re-enable instanced-geometry ray tracing only in a bounded experiment with
  GPU-crash, geometry-segment, memory, visual, and frame-budget evidence.
- Preserve the existing stable setting on every rejected candidate.

Visual improvement cannot trade away packaged stability.
