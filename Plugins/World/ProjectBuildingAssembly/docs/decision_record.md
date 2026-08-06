# ProjectBuildingAssembly - Standing Decision Record

Status: accepted target architecture. The plugin is scaffold-stage: a module
skeleton and empty data directories exist; no described behavior is
implemented yet. Present-tense statements below describe the ACCEPTED
CONTRACT, not shipped code.

## Decision

ALIS buildings will be produced by fully procedural modular assembly, not by
monolithic hand-built building meshes:

- JSON building blueprints will describe each building; planned contract
  locations are the scaffold directories `Data/Buildings/` and
  `Data/Schemas/` (empty today);
- reusable mesh kits will supply the modular pieces; planned location
  `Data/Kits/` (empty today);
- C++ assembly in the `ProjectBuildingAssembly` module will realize a
  blueprint into placed geometry;
- reference photography drives blueprint authoring, not asset sculpting.

## Authority split with ProjectWorld

SOT for the full split: territory doc section "Building geometry authority
split" (`Plugins/World/ProjectWorld/docs/territory_generation.md`).
Summary:

- ProjectWorld owns canonical footprints, height evidence, massing
  constraints, and disposable territory blockouts.
- ProjectBuildingAssembly owns optional modular assemblies for SELECTED
  stable building feature IDs, consumed through a stable public
  footprint/massing contract; it never couples to ProjectWorld internals
  and never owns geographic truth. The contract's OWNER is selected after
  checking `docs/architecture/plugin_rules.md` - a direct module
  dependency on ProjectWorld is not presumed; a neutral shared contract
  or data-document boundary may be required instead.
- An assembled feature explicitly replaces or suppresses its ProjectWorld
  blockout; competing building geometry is forbidden. Unselected buildings
  keep ProjectWorld massing. Authored hero buildings remain protected
  overlays and are never forced through the assembly generator.

## Regeneration governance

Assembly output is a generated layer under the ProjectWorld layered
regeneration contract: profile-owned, disposable, manifest-owned, and
subject to per-layer admission (synthetic implementation, protected
fixtures, full regeneration matrix) before its first territory or playable
use.

## Conditional intake

Legacy art kits enter only after a licensing/provenance audit and a
measured integration prototype; kit intake is conditional even though the
procedural direction itself is not.

## Owed before first production use

- Plugin README per project plugin-documentation requirements.
- `.uplugin` descriptor and module registration on the supported path.
- Dependency direction and the footprint/massing contract's owning
  location decided against `docs/architecture/plugin_rules.md` BEFORE any
  ProjectWorld coupling exists; the contract may live in a neutral shared
  location or as a data-document boundary rather than a module dependency.
- Deterministic input/output and regeneration evidence wired into the
  world validation route.
