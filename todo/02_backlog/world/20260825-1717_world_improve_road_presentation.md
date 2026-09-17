# Improve Generated Road Presentation

Status: uninvestigated

## Current gap

Accepted generated road geometry uses a debug-quality surface. Presentation
needs a reusable authenticated road material while topology, class widths,
terrain drape, junctions, collision, and generated identity remain fixed.

## Investigation boundary

- Keep ProjectWorld as semantic consumer and ProjectMaterial as resource owner.
- Diagnose topology or drape defects separately and stop if either reproduces.
- Evaluate one material candidate with fixed ground, junction, boundary, and
  aerial views.
- Preserve package size and the fixed frame budget.
- Use Kazan as the first fixture, never as a reusable-code branch.

Start from [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md).
