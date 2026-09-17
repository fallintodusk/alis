# Project Conventions

Stable implementation conventions shared by ALIS components. A component may
specialize them in its own architecture only when its boundary genuinely
differs.

## Naming

- Reusable first-party identifiers use `Project*`; see
  [the naming principle](principles.md#universal-naming-convention).
- Unreal reflected types use their engine prefix (`U`, `A`, `F`, `E`, or `I`).
- Script names use lowercase words separated by underscores.
- Public paths and documentation use portable repository-relative locations.

## GameFeature Requirements

For a GameFeature plugin:

1. The `GameFeatureData` asset name matches the plugin name.
2. The asset lives at the plugin content root.
3. The `.uplugin` `BuiltInInitialFeatureState` and asset-manager registration
   match the supported activation route.
4. Runtime dependencies are declared by the plugin and module descriptors.

Use the existing registration and primary-asset validators; documentation does
not duplicate their inventories.

## API Macros

Every public symbol that crosses an Unreal module boundary uses that module's
uppercase generated API macro. Private implementation does not export symbols
without a current external consumer.

## Cross-Plugin Use

- Resolve optional capabilities at a stable lifecycle boundary and handle
  absence explicitly.
- Prefer ProjectCore interfaces for capability consumption.
- A class that constructs or owns a concrete component may depend directly on
  its provider.
- Do not introduce a global event bus or dependency-injection framework for one
  consumer.

## Unreal Assets and Data

- JSON runtime data declares its schema and is staged by the plugin that reads
  it.
- Agents edit text authority and use the owning generator/editor route for
  binary assets.
- Moving a reflected class with real serialized consumers uses the procedure in
  [class migration](../editor/class_migration.md); public code does not retain
  hypothetical redirects.

## Server Code

Server-only implementation and secrets are excluded from client targets with
`WITH_SERVER_CODE`. Client-safe contracts contain no credentials or private
server configuration.

## Comments and Documentation

Comments explain only non-obvious local intent. Stable documents own
cross-file boundaries and invariants. Neither durable surface links to a todo or
narrates implementation history.
