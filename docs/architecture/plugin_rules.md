# Plugin Rules

Project-level policy for first-party plugin categories and cross-plugin
dependencies. The current component inventory is routed from
[Plugins](../../Plugins/README.md); descriptors and `Build.cs` files own the
executable dependency graph.

## Categories

| Category | Responsibility |
|---|---|
| Foundation | Cross-component contracts, discovery, and primitive types |
| Boot | Work required before normal engine, World, and UI lifecycles |
| Systems | Reusable runtime services |
| Features | Optional or experience-scoped domain capabilities |
| Gameplay | Game-mode and gameplay composition |
| UI | Shared presentation infrastructure and domain presentation adapters |
| Resources | Reusable content-facing definitions and resource behavior |
| World | World generation, realization, concrete World data, and fixtures |
| Editor | Editor-only authoring and synchronization |
| Test | Cross-component automation and fixtures |

## Dependency Rules

1. Foundation does not depend on a higher-level first-party category.
2. A consumer of a capability uses its ProjectCore interface when it does not
   need to construct or own the provider's concrete type.
3. Concrete composition may declare a direct dependency on the component that
   owns the constructed type.
4. UI may depend on a feature for serialized domain types; behavior still
   crosses through the owning public contract.
5. Runtime modules never depend on Editor or Test modules.
6. World-data plugins consume ProjectWorld contracts and do not fork reusable
   generation or realization logic.
7. Machine-local and bundled third-party plugins are dependencies, not
   first-party architecture owners.

## Validation

Declare dependencies in both `Build.cs` and `.uplugin` where Unreal requires
them. Use the repository build and governance checks rather than maintaining a
dependency list in prose.
