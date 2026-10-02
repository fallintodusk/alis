# Build Service Configuration

`build_service.toml` is the checked-in local-development configuration consumed
by the CLI. Run from `tools/BuildService` so its relative paths resolve to the
repository and Build Service workspaces.

Select another complete configuration with the global `--config <path>`
option. Do not commit environment credentials or copy the Unreal engine path
into a Build Service config; the project engine configuration owns that value.

See [configuration](../docs/configuration.md) for field ownership.
