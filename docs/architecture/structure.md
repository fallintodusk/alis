# Structure

## Purpose

ALIS combines a small project-branded Unreal Engine game module with reusable
plugins and standalone development tools. This owner defines only how those
top-level parts divide responsibility.

## Owns

- Project-level composition and dependency direction.
- The distinction between branded game integration and reusable components.
- Repository-wide invariants that no narrower component can own.

## Does not own

- Plugin internals, which belong to each component under
  [Plugins](../../Plugins/README.md).
- Game-specific startup implementation, which belongs to
  [Source](../../Source/README.md).
- Build, test, packaging, or generation procedures, which belong to
  [scripts](../../scripts/README.md) and [tools](../../tools/README.md).
- Work status and proposed changes, which are not architecture facts.

## Composition

| Part | Responsibility |
|---|---|
| [Source](../../Source/README.md) | ALIS-branded game targets and composition |
| [Plugins](../../Plugins/README.md) | Runtime, editor, UI, test, and World components |
| [Scripts](../../scripts/README.md) | Repository development and release automation |
| [Tools](../../tools/README.md) | Standalone build, delivery, and World tooling |
| [Documentation](../README.md) | Durable technical guidance and routing |

## Relationships

The project-level relationships are drawn once in
[the main view](diagrams/main.md). Each lower owner documents its own graph.

Through ProjectCore contracts, reusable consumers request capabilities without
depending on provider internals. Concrete compile-time dependencies remain
valid when one component composes a type or consumes data owned by another;
[plugin rules](plugin_rules.md) own that distinction.

## Invariants

1. Reusable first-party code uses the `Project*` prefix; ALIS branding stays in
   game composition, content, configuration, and user-facing text.
2. A component owns its behavior, data contracts, configuration, tests, and
   architecture at the narrowest responsible boundary.
3. Parents treat children as black boxes and never copy their internal graph.
4. Executable dependencies are declared in `.uproject`, `.uplugin`, and
   `Build.cs`; prose does not maintain a second component inventory.
5. Unsupported operations fail explicitly rather than reporting success.
6. Generated World authority, public release identity, and trust checks remain
   under their existing owners.

## Accepted decisions

- Reusable naming boundary - see [Invariants](#invariants).
- Recursive component ownership - see [Composition](#composition).
- Interface consumption versus concrete composition - see
  [Relationships](#relationships).
