# ProjectLoading

Runtime service that resolves an experience request, executes loading phases,
tracks progress, and owns terminal loading state.

| Concern | Owner |
|---|---|
| Durable component guidance | [Documentation](docs/README.md) |

Consumers start a load through `ILoadingService`; ProjectUI subscribes to the
loading state when presentation is required.
