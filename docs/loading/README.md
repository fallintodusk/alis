# Boot, Delivery, and Loading

These concerns are separate ownership boundaries.

| Need | Owner |
|---|---|
| Understand the implemented Unreal boot sequence | [Boot chain](boot_chain.md) |
| Inspect the missing external Launcher handoff | [Launcher integration](launcher_integration.md) |
| Prepare and verify a public GitHub release | [Release packaging](../build/packaging_guide.md) |
| Understand Build Service/CDN publication boundaries | [Content publishing](content_publishing.md) |
| Resolve experiences and register primary assets | [ProjectLoading asset registration](../../Plugins/Systems/ProjectLoading/docs/asset_manager_registration.md) |
| Execute runtime experience loading | [ProjectLoading](../../Plugins/Systems/ProjectLoading/README.md) |
| Present loading state | [ProjectUI](../../Plugins/UI/ProjectUI/README.md) |

Boot activation, artifact delivery, runtime loading, and presentation do not
share a fallback authority.
