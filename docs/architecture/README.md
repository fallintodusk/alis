# Architecture

Project-level ALIS architecture and repository-wide engineering policy.

| Concern | Owner |
|---|---|
| Boundaries, composition, invariants, and accepted decisions | [Structure](structure.md) |
| Relationships among project-level owners | [Diagrams](diagrams/README.md) |
| Reusable engineering principles | [Principles](principles.md) |
| Plugin category and dependency policy | [Plugin rules](plugin_rules.md) |
| Naming and implementation conventions | [Conventions](conventions.md) |

Component internals belong to the component reached through
[the plugin router](../../Plugins/README.md), not to project architecture.
