# Orchestrator

Early-boot plugin for installed manifest validation, dependency ordering, and
Unreal plugin/module activation.

| Concern | Owner |
|---|---|
| Durable component guidance | [Documentation](docs/README.md) |
| Runtime manifest and boot data | [Data](Data/README.md) |

The runtime entry point is `FOrchestratorCoreModule::StartupModule()` in the
`OrchestratorCore` module.
