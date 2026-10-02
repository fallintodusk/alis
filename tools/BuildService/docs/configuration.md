# Build Service Configuration

The CLI reads `config/build_service.toml` unless `--config` selects another
file.

## Owned values

| Section | Values |
|---|---|
| `paths` | Project, staging, manifest, build workspaces, and local state paths |
| `cdn` | Control API URL and the name of the environment variable holding its token |
| `build` | Platform, configuration, channel, and reported parallel slot count |

The Unreal source-engine root is deliberately absent. It resolves from the
project's `scripts/config/ue_path.conf`; a process-local `UE_SOURCE_PATH`
overrides it for one invocation. The engine identity gate runs before build
work.

The token value must exist only in the named environment variable. Do not put a
token, signing key, or private endpoint credential in this file.

`parallel_builds` is currently reported by the CLI but does not schedule
parallel Unreal builds.

See the [CLI reference](api_reference.md) for command behavior and the
[architecture](architecture/README.md) for ownership.
